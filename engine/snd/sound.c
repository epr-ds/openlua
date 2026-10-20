/* OpenLua - audio system implementation
 *
 * Built on the native PSP audio path (pspaudio.h / sceAudio*):
 *
 *   - sound.load()  decodes 16-bit PCM WAV (mono or stereo, any rate)
 *     and PSP VMA/VOX (22050 Hz mono, 4-byte sample-count header),
 *     normalising to the fixed 22050 Hz mono hardware output rate.
 *   - sound.play()  reserves a hardware channel and starts pumping.
 *   - sound_pump()  is the per-frame driver (hooked into screen.flip()):
 *     feeds one 1024-sample chunk whenever the channel buffer has drained
 *     below the chunk size - a self-limiting feed rate that cannot outpace
 *     the hardware drain even if the frame loop slows down.
 *   - loop / pause / stop / vol map onto the channel state + flags.
 *
 * Volume range: 0..100% externally, mapped to 0..PSP_AUDIO_VOLUME_MAX.
 * Channels: 8 hardware channels (PSP_AUDIO_CHANNEL_MAX). OneLua's
 * sound.play(snd, channel) passes 1..8; 0/auto = next free channel.
 */

#include "engine/snd/sound.h"

#if defined(__PSP__)
#include <pspaudio.h>
#else
/* Host stubs for PSP audio APIs */
typedef int SceAudioChannel;
#define PSP_AUDIO_FORMAT_MONO        0
#define PSP_AUDIO_VOLUME_MAX         128
#define PSP_AUDIO_NEXT_CHANNEL       (-1)
#define PSP_AUDIO_CHANNEL_MAX        8
static int host_ch_used[PSP_AUDIO_CHANNEL_MAX];
static inline int sceAudioChReserve(int c, int l, int f)
{
    (void)l; (void)f;
    if (c == PSP_AUDIO_NEXT_CHANNEL) {
        for (c = 0; c < PSP_AUDIO_CHANNEL_MAX; c++)
            if (!host_ch_used[c])
                break;
        if (c == PSP_AUDIO_CHANNEL_MAX)
            return -1;
    }
    if (c < 0 || c >= PSP_AUDIO_CHANNEL_MAX || host_ch_used[c])
        return -1;
    host_ch_used[c] = 1;
    return c;
}
static inline void sceAudioChRelease(int c)
{
    if (c >= 0 && c < PSP_AUDIO_CHANNEL_MAX)
        host_ch_used[c] = 0;
}
static inline int sceAudioOutput(int c, int v, void *p) { (void)c; (void)v; (void)p; return 0; }
static inline int sceAudioGetChannelRestLen(int c)      { (void)c; return 0; }
#endif

#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <limits.h>
#include <stdio.h>
#include "engine/core/olreg.h"
#if !defined(__PSP__)
#include <errno.h>
#endif

/* PSP audio uses reserved channels; on host builds we mock the channel API. */
#ifndef PSP_AUDIO_CHANNEL_MAX
#define PSP_AUDIO_CHANNEL_MAX 8
#endif
#ifndef PSP_AUDIO_FORMAT_MONO
#define PSP_AUDIO_FORMAT_MONO 0
#endif
#ifndef PSP_AUDIO_VOLUME_MAX
#define PSP_AUDIO_VOLUME_MAX 128
#endif
#ifndef PSP_AUDIO_NEXT_CHANNEL
#define PSP_AUDIO_NEXT_CHANNEL (-1)
#endif

#define SND_MAX    8
#define HW_RATE    22050
#define CHUNK      1024              /* samples per sceAudioOutput call (~46 ms) */
#define CHUNK_ALIGN 64               /* PSP_AUDIO_SAMPLE_ALIGN granularity */

typedef struct {
    int      used;
    int      channel;      /* reserved PSP audio channel (-1 = none)   */
    int16_t *pcm;          /* 22050 Hz mono, interleaved-free mono     */
    int      len;          /* total samples                            */
    int      pos;          /* next sample index to output              */
    int      loop;
    int      paused;
    int      playing;
    int      finished;
    int      vol;          /* 0..100 (0xF = unset default -> 100)      */
    int      channels;     /* original channel count (1 or 2)          */
    int      orig_rate;    /* source sample rate (Hz)                  */
} OLS_Sound;

static OLS_Sound g_s[SND_MAX];

/* ---- helpers -------------------------------------------------------------- */

static int to_s(const char **out, lua_State *L, int idx)
{
    const char *s = luaL_checkstring(L, idx);
    *out = s;
    return 1;
}

static inline int vol_hw(int pct)
{
    if (pct < 0) pct = 0;
    if (pct > 100) pct = 100;
    return (pct * PSP_AUDIO_VOLUME_MAX) / 100;
}

static uint32_t le32(const unsigned char *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static uint16_t le16(const unsigned char *p)
{
    return (uint16_t)(p[0] | (p[1] << 8));
}

/* ----------------------------------------------------------------- */
/* WAV (PCM 16-bit) parsing + VMA (VOX) parsing                       */

typedef struct {
    int16_t *pcm;
    int      len;          /* samples @src_rate, mono                   */
    int      rate;
    int      channels;
} RawAudio;

static void raw_free(RawAudio *a)
{
    if (a->pcm) { free(a->pcm); a->pcm = NULL; }
    a->len = 0;
}

static int wav_parse(const unsigned char *buf, unsigned int size, RawAudio *out)
{
    if (size < 44 || memcmp(buf, "RIFF", 4) || memcmp(buf + 8, "WAVE", 4))
        return -1;

    unsigned int off = 12;
    int fmt_found = 0, data_found = 0;
    int rate = 0, channels = 0, bits = 0, fmt = 0;
    const unsigned char *data = NULL;
    unsigned int data_len = 0;

    /* walk 16-byte chunk envelopes (header: 4 id + 4 size + payload) */
    while (off + 8 <= size) {
        unsigned int clen = le32(buf + off + 4);
        unsigned int cpos = off + 8;
        if (clen > size - cpos)
            return -1;

        if (!memcmp(buf + off, "fmt ", 4)) {            /* format chunk */
            if (clen < 16) return -1;
            fmt        = le16(buf + cpos + 0);
            channels   = le16(buf + cpos + 2);
            rate       = (int)le32(buf + cpos + 4);
            bits       = le16(buf + cpos + 14);
            fmt_found  = 1;
        } else if (!memcmp(buf + off, "data", 4)) {      /* data chunk  */
            data     = buf + cpos;
            data_len = clen;
            data_found = 1;
            if (fmt_found) break;                          /* both found  */
        }
        off = cpos + clen + (clen & 1);                    /* chunk pad   */
    }

    if (!fmt_found || !data_found)
        return -1;
    if (fmt != 1 || bits != 16)                            /* PCM 16-bit only */
        return -1;
    if (channels < 1 || channels > 2 || rate <= 0)
        return -1;
    if (data_len < 2 || (data_len & 1))
        return -1;

    /* downmix to mono, keep in place (reuse allocation) */
    unsigned int nsamp = data_len / 2;
    int16_t *mono = (int16_t *)malloc(nsamp * sizeof(int16_t));
    if (!mono)
        return -1;

    uint16_t *src = (uint16_t *)data;
    if (channels == 1) {
        for (unsigned int i = 0; i < nsamp; i++)
            mono[i] = (int16_t)src[i];
        nsamp = nsamp;
    } else {
        unsigned int frames = nsamp / 2;
        for (unsigned int i = 0; i < frames; i++) {
            int a = (int16_t)src[i*2];
            int b = (int16_t)src[i*2+1];
            mono[i] = (int16_t)((a + b) / 2);
        }
        nsamp = frames;
    }

    out->pcm      = mono;
    out->len      = (int)nsamp;
    out->rate     = rate;
    out->channels = channels;
    return 0;
}

/* VMA / VOX: 4-byte LE sample count, then 16-bit PCM @ 22050 Hz mono. */
static int vma_parse(const unsigned char *buf, unsigned int size, RawAudio *out)
{
    if (size < 4)
        return -1;
    uint32_t nsamp = le32(buf);
    /* sanity: sample count must match the payload (in bytes: 2 bytes/sample) */
    if (nsamp == 0 || (unsigned long long)nsamp * 2 + 4 > size + 4)
        return -1;
    if ((unsigned long long)nsamp * 2 != size - 4)
        return -1;   /* strict: header sample count == payload */

    int16_t *pcm = malloc(nsamp * sizeof(int16_t));
    if (!pcm)
        return -1;
    memcpy(pcm, buf + 4, nsamp * sizeof(int16_t));

    out->pcm      = pcm;
    out->len      = (int)nsamp;
    out->rate     = HW_RATE;
    out->channels = 1;
    return 0;
}

/* ----------------------------------------------------------------- */
/* resample src_rate -> HW_RATE (linear interpolation, mono)         */

static int resample_m(const int16_t *src, int n, int src_rate,
                      int16_t **out, int *out_len)
{
    *out = NULL;
    *out_len = n;

    if (src_rate == HW_RATE)
        return 1;                       /* caller keeps the original    */

    long long nout_ll = (long long)n * HW_RATE / src_rate;
    if (nout_ll <= 0 || nout_ll > INT_MAX)
        return -1;
    int nout = (int)nout_ll;

    int16_t *pcm = (int16_t *)malloc(nout * sizeof(int16_t));
    if (!pcm)
        return -1;

    double ratio = (double)HW_RATE / (double)src_rate;
    for (int i = 0; i < nout; i++) {
        double pos = i / ratio;
        int i0 = (int)pos;
        int i1 = i0 + 1;
        if (i1 >= n) i1 = n - 1;
        double t = pos - i0;
        int a = src[i0];
        int b = src[i1];
        pcm[i] = (int16_t)(a + (int)((b - a) * t + (t >= 0 ? 0.5 : -0.5)));
    }

    *out = pcm;
    *out_len = nout;
    return 0;
}

/* ----------------------------------------------------------------- */
/* public API                                                         */

int sound_alloc(const char *path)
{
    FILE *f = fopen(path, "rb");
    if (!f)
        return 0;

    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (fsize <= 0 || fsize > (8L * 1024 * 1024))
    { fclose(f); return 0; }

    unsigned char *buf = malloc((size_t)fsize);
    if (!buf) { fclose(f); return 0; }
    if (fread(buf, 1, (size_t)fsize, f) != (size_t)fsize)
    { free(buf); fclose(f); return 0; }
    fclose(f);

    RawAudio raw = {0};
    int ok = 0;
    if (wav_parse(buf, (unsigned int)fsize, &raw) == 0)
        ok = 1;
    else {
        int riff = fsize >= 4 && memcmp(buf, "RIFF", 4) == 0;
        if (!riff && vma_parse(buf, (unsigned int)fsize, &raw) == 0)
            ok = 1;
    }
    free(buf);
    if (!ok) { raw_free(&raw); return 0; }

    /* resample to hardware rate; a failure must not silently keep the source rate */
    int16_t *res = NULL;
    int flen = raw.len;
    int rr = resample_m(raw.pcm, raw.len, raw.rate, &res, &flen);
    if (rr < 0) { raw_free(&raw); return 0; }
    int16_t *final = raw.pcm;
    if (rr == 0) {
        final = res;
        free(raw.pcm);
    }
    if (!final || flen <= 0) { free(final); return 0; }

    for (int i = 0; i < SND_MAX; i++) {
        if (!g_s[i].used) {
            g_s[i].used      = 1;
            g_s[i].channel   = -1;
            g_s[i].pcm       = final;
            g_s[i].len       = flen;
            g_s[i].pos       = 0;
            g_s[i].loop      = 0;
            g_s[i].paused    = 0;
            g_s[i].playing   = 0;
            g_s[i].finished  = 0;
            g_s[i].vol       = 100;
            g_s[i].channels  = raw.channels;
            g_s[i].orig_rate = raw.rate;
            return i + 1;          /* 1-based handle */
        }
    }
    free(final);
    return 0;                      /* all slots used */
}

static OLS_Sound *get(int h)
{
    if (h < 1 || h > SND_MAX || !g_s[h - 1].used)
        return NULL;
    return &g_s[h - 1];
}

static void release_channel(OLS_Sound *s)
{
    if (s->channel >= 0)
        sceAudioChRelease(s->channel);
    s->channel = -1;
}

int sound_play(int h)
{
    return sound_play_channel(h, -1);
}

/* Reserve channel ch (0..7) if not already holding it, then start. */
int sound_play_channel(int h, int ch)
{
    OLS_Sound *s = get(h);
    if (!s || ch < -1 || ch >= PSP_AUDIO_CHANNEL_MAX) return -1;

    if (ch >= 0 && s->channel != ch) {
        int old_channel = s->channel;
        int rc = sceAudioChReserve(ch, CHUNK, PSP_AUDIO_FORMAT_MONO);
        if (rc < 0)
            return -1;                 /* keep any existing reservation */
        if (old_channel >= 0)
            sceAudioChRelease(old_channel);
        s->channel = rc;
    } else if (ch < 0 && s->channel < 0) {
        int rc = sceAudioChReserve(PSP_AUDIO_NEXT_CHANNEL,
                                   CHUNK, PSP_AUDIO_FORMAT_MONO);
        if (rc < 0)
            return -1;
        s->channel = rc;
    }

    s->pos      = 0;
    s->playing  = 1;
    s->paused   = 0;
    s->finished = 0;
    return 0;
}

int sound_pause(int h, int mode)
{
    OLS_Sound *s = get(h);
    if (!s || !s->playing) return -1;
    if (mode == -1)
        s->paused = !s->paused;
    else if (mode == 0)
        s->paused = 0;
    else
        s->paused = 1;
    return 0;
}

int sound_stop(int h)
{
    OLS_Sound *s = get(h);
    if (!s) return -1;
    s->playing  = 0;
    s->pos      = 0;
    s->paused   = 0;
    s->finished = 1;
    release_channel(s);
    return 0;
}

int sound_loop(int h)
{
    OLS_Sound *s = get(h);
    if (!s) return -1;
    s->loop = !s->loop;
    return s->loop;
}

int sound_looping(int h)
{
    OLS_Sound *s = get(h);
    return s ? s->loop : 0;
}

int sound_vol(int h, int pct)
{
    OLS_Sound *s = get(h);
    if (!s) return -1;
    if (pct >= 0) s->vol = pct;
    return s->vol;
}

int sound_playing(int h)
{
    OLS_Sound *s = get(h);
    return s ? (s->playing && !s->paused ? 1 : 0) : 0;
}

int sound_endstream(int h)
{
    OLS_Sound *s = get(h);
    return s ? s->finished : 0;
}

int sound_rate(int h)
{
    OLS_Sound *s = get(h);
    return s ? HW_RATE : 0;          /* normalised output rate */
}

int sound_channels(int h)
{
    OLS_Sound *s = get(h);
    return s ? s->channels : 0;
}

void sound_free(int h)
{
    OLS_Sound *s = get(h);
    if (!s) return;
    release_channel(s);
    free(s->pcm);
    memset(s, 0, sizeof(*s));
}

/* ----------------------------------------------------------------- */
/* per-frame pump — called once per screen.flip()                    */
void sound_pump(void)
{
    for (int i = 0; i < SND_MAX; i++) {
        OLS_Sound *s = &g_s[i];
        if (!s->used || !s->playing || s->paused || s->channel < 0)
            continue;

        int rest = sceAudioGetChannelRestLen(s->channel);
        if (rest < 0)
            continue;                        /* channel error: skip    */

        /* feed exactly one chunk when the buffer has drained below   */
        /* the chunk size; self-limiting so a slow UI loop cannot    */
        /* outpace the hardware drain even if it falls below 60 fps  */
        while (rest < CHUNK) {
            int remaining = s->len - s->pos;
            if (remaining <= 0) {
                if (s->loop) {
                    s->pos = 0;
                    remaining = s->len;
                } else {
                    s->playing  = 0;
                    s->finished = 1;
                    release_channel(s);
                    break;
                }
            }

            int n = (remaining < CHUNK) ? remaining : CHUNK;
            int16_t *ptr = s->pcm + s->pos;
            if (n < CHUNK) {
                /* tail: zero-pad to full chunk length               */
                static int16_t tail[CHUNK];
                memcpy(tail, ptr, n * sizeof(int16_t));
                memset(tail + n, 0, (CHUNK - n) * sizeof(int16_t));
                ptr = tail;
            }

            int r = sceAudioOutput(s->channel, vol_hw(s->vol), (void *)ptr);
            if (r < 0)
                break;                       /* busy/not reserved: retry next frame */

            s->pos += n;
            rest += CHUNK;                    /* approximate updated rest */
            if (!(s->loop) && s->pos >= s->len) {
                s->playing  = 0;
                s->finished = 1;
                release_channel(s);
                break;
            }
        }
    }
}

static int l_sound_load(lua_State *L)
{
    const char *path;
    to_s(&path, L, 1);
    int h = sound_alloc(path);
    if (h <= 0)
        lua_pushnil(L);
    else
        lua_pushinteger(L, h);
    return 1;
}

static int l_sound_play(lua_State *L)
{
    int h = (int)luaL_checkinteger(L, 1);
    /* channel arg optional: 0/auto, or 1..8 (mapped to PSP channel 0..7) */
    int ch = -1;
    if (lua_isnumber(L, 2)) {
        int arg = (int)luaL_checkinteger(L, 2);
        if (arg < 0 || arg > PSP_AUDIO_CHANNEL_MAX)
            luaL_error(L, "sound.play: channel must be 1..%d or 0 for auto",
                       PSP_AUDIO_CHANNEL_MAX);
        if (arg > 0)
            ch = arg - 1;
    }
    if (!get(h))
        luaL_error(L, "sound.play: unknown handle %d", h);
    int r = sound_play_channel(h, ch);
    if (r < 0)
        luaL_error(L, "sound.play: failed (audio channel not available?)");
    return 0;
}

static int l_sound_pause(lua_State *L)
{
    int h  = (int)luaL_checkinteger(L, 1);
    int mode = (lua_isnumber(L, 2)) ? (int)luaL_checkinteger(L, 2) : -1;
    if (sound_pause(h, mode) < 0)
        luaL_error(L, "sound.pause: unknown handle %d", h);
    return 0;
}

static int l_sound_stop(lua_State *L)
{
    int h = (int)luaL_checkinteger(L, 1);
    if (sound_stop(h) < 0)
        luaL_error(L, "sound.stop: unknown handle %d", h);
    return 0;
}

static int l_sound_loop(lua_State *L)
{
    int h = (int)luaL_checkinteger(L, 1);
    int on = sound_loop(h);
    if (on < 0)
        luaL_error(L, "sound.loop: unknown handle %d", h);
    lua_pushboolean(L, on);
    return 1;
}

static int l_sound_looping(lua_State *L)
{
    int h = (int)luaL_checkinteger(L, 1);
    lua_pushboolean(L, sound_looping(h));
    return 1;
}

static int l_sound_playing(lua_State *L)
{
    int h = (int)luaL_checkinteger(L, 1);
    lua_pushboolean(L, sound_playing(h));
    return 1;
}

static int l_sound_endstream(lua_State *L)
{
    int h = (int)luaL_checkinteger(L, 1);
    lua_pushboolean(L, sound_endstream(h));
    return 1;
}

static int l_sound_vol(lua_State *L)
{
    int h    = (int)luaL_checkinteger(L, 1);
    int pct  = (lua_isnumber(L, 2)) ? (int)luaL_checkinteger(L, 2) : -1;
    int r    = sound_vol(h, pct);
    if (r < 0)
        luaL_error(L, "sound.vol: unknown handle %d", h);
    lua_pushinteger(L, r);
    return 1;
}

static int l_sound_rate(lua_State *L)
{
    int h = (int)luaL_checkinteger(L, 1);
    lua_pushinteger(L, sound_rate(h));
    return 1;
}

static int l_sound_free(lua_State *L)
{
    int h = (int)luaL_checkinteger(L, 1);
    sound_free(h);
    return 0;
}

static const luaL_Reg sound_funcs[] = {
    { "load",        l_sound_load },
    { "play",        l_sound_play },
    { "pause",       l_sound_pause },
    { "stop",        l_sound_stop },
    { "loop",        l_sound_loop },
    { "looping",     l_sound_looping },
    { "playing",     l_sound_playing },
    { "endstream",   l_sound_endstream },
    { "vol",         l_sound_vol },
    { "rate",        l_sound_rate },
    { "free",        l_sound_free },
    { NULL, NULL }
};

void sound_api_register(lua_State *L)
{
    lua_newtable(L);
    ol_setfuncs(L, sound_funcs);
    lua_setglobal(L, "sound");
}
