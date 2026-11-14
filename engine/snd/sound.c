/* OpenLua - audio system implementation
 *
 * Built on the native PSP audio path (pspaudio.h / sceAudio*):
 *
 *   - sound.load()  decodes 16-bit PCM WAV (mono or stereo, any rate)
 *     and PSP VMA/VOX (22050 Hz mono, 4-byte sample-count header),
 *     normalising to the fixed 22050 Hz mono hardware output rate.
 *     MP3 (`.mp3`, ID3v2, or an MPEG frame sync) is streamed through
 *     minimp3 instead of being decoded up front: menu music is 3-10 MB,
 *     so holding whole files in RAM (and the 8 MB WAV cap) is not viable.
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
#include "third_party/minimp3/minimp3.h"
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

/* MP3 streaming: a compressed input buffer plus a staging buffer holding
 * already-resampled 22050 Hz mono samples.  One decoded MPEG1 frame is at
 * most 1152 samples/channel; at the lowest MP3 rate (8 kHz) resampling to
 * 22050 yields ~3176 samples, so 8192 leaves room for one frame on top of
 * a partially-consumed chunk. */
#define MP3_IN_BUF  (64 * 1024)
#define MP3_STAGE   8192

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

    /* Streaming MP3 state.  pcm/len/pos stay unused for streams. */
    int            streaming;
    FILE          *fp;         /* kept open on the memory stick          */
    mp3dec_t      *mp3;
    unsigned char *inbuf;
    int            in_len;
    int            in_pos;
    int            eof;        /* input file exhausted                   */
    int            stream_end; /* decoder produced its last frame        */
    int16_t       *stage;      /* resampled 22050 mono staging           */
    int            stage_len;
    int            stage_pos;
    int16_t       *frame;      /* one decoded frame, downmixed mono      */
    int16_t        rs_prev;    /* last source sample of previous frame   */
    int            rs_have_prev;
    double         rs_pos;     /* fractional position in current frame   */
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
/* MP3 streaming decode (minimp3)                                     */

static int has_mp3_ext(const char *p)
{
    size_t n = strlen(p);
    if (n < 4)
        return 0;
    p += n - 4;
    return p[0] == '.' &&
           (p[1] == 'm' || p[1] == 'M') &&
           (p[2] == 'p' || p[2] == 'P') &&
           (p[3] == '3');
}

/* Rewind to the first MPEG frame, skipping an ID3v2 tag if present. */
static int mp3_seek_start(OLS_Sound *s)
{
    if (!s->fp)
        return -1;
    if (fseek(s->fp, 0, SEEK_SET) != 0)
        return -1;

    unsigned char id3[10];
    size_t n = fread(id3, 1, sizeof(id3), s->fp);
    if (n == sizeof(id3) && memcmp(id3, "ID3", 3) == 0) {
        unsigned long size = ((unsigned long)(id3[6] & 0x7f) << 21) |
                             ((unsigned long)(id3[7] & 0x7f) << 14) |
                             ((unsigned long)(id3[8] & 0x7f) << 7) |
                             ((unsigned long)(id3[9] & 0x7f));
        if (size > 0 && size < (1UL << 30))
            fseek(s->fp, (long)(10 + size), SEEK_SET);
        else
            fseek(s->fp, 0, SEEK_SET);
    } else {
        fseek(s->fp, 0, SEEK_SET);
    }

    mp3dec_init(s->mp3);
    s->in_len     = 0;
    s->in_pos     = 0;
    s->eof        = 0;
    s->stream_end = 0;
    s->stage_len  = 0;
    s->stage_pos  = 0;
    s->rs_prev    = 0;
    s->rs_have_prev = 0;
    s->rs_pos     = 0.0;
    return 0;
}

static void mp3_close(OLS_Sound *s)
{
    if (s->fp) {
        fclose(s->fp);
        s->fp = NULL;
    }
    free(s->mp3);   s->mp3   = NULL;
    free(s->inbuf); s->inbuf = NULL;
    free(s->stage); s->stage = NULL;
    free(s->frame); s->frame = NULL;
}

/* Compact consumed input and read another block. */
static void mp3_feed(OLS_Sound *s)
{
    if (s->eof)
        return;
    if (s->in_pos > 0) {
        memmove(s->inbuf, s->inbuf + s->in_pos,
                (size_t)(s->in_len - s->in_pos));
        s->in_len -= s->in_pos;
        s->in_pos = 0;
    }
    if (s->in_len >= MP3_IN_BUF)
        return;
    size_t n = fread(s->inbuf + s->in_len, 1,
                     (size_t)(MP3_IN_BUF - s->in_len), s->fp);
    if (n == 0)
        s->eof = 1;
    s->in_len += (int)n;
}

/* Decode the next audio frame into s->frame, downmixed to mono.
 * Returns the number of samples per channel, or 0 at end of stream. */
static int mp3_decode_mono(OLS_Sound *s)
{
    for (;;) {
        if (s->in_pos >= s->in_len) {
            mp3_feed(s);
            if (s->in_pos >= s->in_len)
                return 0;
        }

        int avail = s->in_len - s->in_pos;
        mp3dec_frame_info_t info;
        memset(&info, 0, sizeof(info));
        int samples = mp3dec_decode_frame(s->mp3, s->inbuf + s->in_pos,
                                          avail, s->frame, &info);
        if (samples > 0) {
            s->in_pos += info.frame_bytes;
            if (info.hz > 0)
                s->orig_rate = info.hz;
            s->channels = info.channels;
            if (info.channels == 2) {
                for (int i = 0; i < samples; i++) {
                    int a = s->frame[i * 2];
                    int b = s->frame[i * 2 + 1];
                    s->frame[i] = (int16_t)((a + b) / 2);
                }
            }
            return samples;
        }

        if (info.frame_bytes > 0 && info.frame_bytes < avail) {
            /* junk or an ID3v1 trailer before the next frame */
            s->in_pos += info.frame_bytes;
            continue;
        }

        if (info.frame_bytes >= avail) {
            /* No complete frame in what is buffered. */
            if (s->eof)
                return 0;
            if (avail >= MP3_IN_BUF) {
                /* Corrupt / non-MPEG data: drop most of the window and
                 * resynchronise without scanning it byte by byte. */
                s->in_pos += avail - 1024;
                continue;
            }
            int before = avail;
            mp3_feed(s);
            if (s->in_len - s->in_pos <= before && s->eof)
                return 0;
            continue;
        }

        /* frame_bytes == 0: an incomplete frame at the buffer start. */
        int before = avail;
        mp3_feed(s);
        if (s->in_len - s->in_pos <= before && s->eof)
            return 0;
    }
}

/* Append one decoded mono frame to the staging buffer, resampling to
 * HW_RATE with linear interpolation.  rs_pos/rs_prev carry the fractional
 * position and last sample across frame boundaries. */
static void mp3_append_resampled(OLS_Sound *s, int n)
{
    if (n <= 0)
        return;

    if (s->orig_rate <= 0)
        s->orig_rate = HW_RATE;

    if (s->orig_rate == HW_RATE || n < 2) {
        int space = MP3_STAGE - s->stage_len;
        if (n > space)
            n = space;
        memcpy(s->stage + s->stage_len, s->frame,
               (size_t)n * sizeof(int16_t));
        s->stage_len += n;
        s->rs_prev = s->frame[n - 1];
        s->rs_have_prev = 1;
        s->rs_pos = 0.0;
        return;
    }

    double step = (double)s->orig_rate / (double)HW_RATE;
    if (!s->rs_have_prev) {
        s->rs_prev = s->frame[0];
        s->rs_have_prev = 1;
        s->rs_pos = 0.0;
    }

    while (s->rs_pos < (double)(n - 1) && s->stage_len < MP3_STAGE) {
        int i0 = (int)s->rs_pos;
        if ((double)i0 > s->rs_pos)
            i0--;                         /* floor() for negative carry */
        double t = s->rs_pos - (double)i0;
        int a = (i0 < 0) ? s->rs_prev : s->frame[i0];
        int b = s->frame[i0 + 1];         /* 0 <= i0+1 <= n-1 */
        s->stage[s->stage_len++] =
            (int16_t)(a + (int)((b - a) * t + (t >= 0 ? 0.5 : -0.5)));
        s->rs_pos += step;
    }

    s->rs_prev = s->frame[n - 1];
    s->rs_pos -= (double)n;
}

/* Ensure `need` staged samples are available (or the stream has ended).
 * Returns the number of staged samples remaining. */
static int mp3_fill(OLS_Sound *s, int need)
{
    for (;;) {
        int avail = s->stage_len - s->stage_pos;
        if (avail >= need || s->stream_end)
            return avail;

        if (s->stage_pos > 0) {
            if (avail > 0)
                memmove(s->stage, s->stage + s->stage_pos,
                        (size_t)avail * sizeof(int16_t));
            s->stage_len = avail;
            s->stage_pos = 0;
        }

        int n = mp3_decode_mono(s);
        if (n <= 0) {
            s->stream_end = 1;
            return s->stage_len;
        }
        mp3_append_resampled(s, n);
    }
}

/* Takes ownership of `f`; returns a 1-based handle or 0. */
static int mp3_sound_alloc(FILE *f)
{
    int slot = -1;
    for (int i = 0; i < SND_MAX; i++) {
        if (!g_s[i].used) {
            slot = i;
            break;
        }
    }
    if (slot < 0) {
        fclose(f);
        return 0;
    }

    OLS_Sound *s = &g_s[slot];
    memset(s, 0, sizeof(*s));
    s->channel = -1;
    s->vol     = 100;

    s->mp3   = (mp3dec_t *)calloc(1, sizeof(mp3dec_t));
    s->inbuf = (unsigned char *)malloc(MP3_IN_BUF);
    s->stage = (int16_t *)malloc(MP3_STAGE * sizeof(int16_t));
    s->frame = (int16_t *)malloc(MINIMP3_MAX_SAMPLES_PER_FRAME *
                                 sizeof(int16_t));
    if (!s->mp3 || !s->inbuf || !s->stage || !s->frame) {
        fclose(f);
        free(s->mp3);
        free(s->inbuf);
        free(s->stage);
        free(s->frame);
        memset(s, 0, sizeof(*s));
        return 0;
    }

    s->used      = 1;
    s->streaming = 1;
    s->fp        = f;

    if (mp3_seek_start(s) != 0) {
        sound_free(slot + 1);
        return 0;
    }

    /* Prime the first frame so a non-MP3 file is rejected at load time and
     * the sample rate is known before playback. */
    int n = mp3_decode_mono(s);
    if (n <= 0) {
        sound_free(slot + 1);
        return 0;
    }
    mp3_append_resampled(s, n);
    return slot + 1;
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
    if (fsize <= 0)
    { fclose(f); return 0; }

    /* MP3 streams; everything else is decoded fully into RAM.  Sniff the
     * extension, an ID3v2 tag, or a raw MPEG frame sync so renamed files
     * still work.  The 8 MB cap below does not apply to streams. */
    unsigned char probe[4] = {0};
    size_t probe_n = fread(probe, 1, sizeof(probe), f);
    fseek(f, 0, SEEK_SET);
    int is_mp3 = has_mp3_ext(path) ||
                 (probe_n >= 3 && memcmp(probe, "ID3", 3) == 0) ||
                 (probe_n >= 2 && probe[0] == 0xFF &&
                  (probe[1] & 0xE0) == 0xE0);
    if (is_mp3)
        return mp3_sound_alloc(f);

    if (fsize > (8L * 1024 * 1024))
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

    if (s->streaming) {
        if (mp3_seek_start(s) != 0) {
            release_channel(s);
            return -1;
        }
    } else {
        s->pos = 0;
    }
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
    if (s->streaming)
        mp3_close(s);
    else
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

        if (s->streaming) {
            while (rest < CHUNK) {
                int avail = mp3_fill(s, CHUNK);
                if (avail <= 0) {
                    if (s->loop && mp3_seek_start(s) == 0)
                        avail = mp3_fill(s, CHUNK);
                    if (avail <= 0) {
                        s->playing  = 0;
                        s->finished = 1;
                        release_channel(s);
                        break;
                    }
                }

                int n = (avail < CHUNK) ? avail : CHUNK;
                int16_t *ptr = s->stage + s->stage_pos;
                if (n < CHUNK) {
                    /* end of stream: pad the final chunk with silence */
                    static int16_t tail[CHUNK];
                    memcpy(tail, ptr, (size_t)n * sizeof(int16_t));
                    memset(tail + n, 0, (size_t)(CHUNK - n) * sizeof(int16_t));
                    ptr = tail;
                }

                int r = sceAudioOutput(s->channel, vol_hw(s->vol), (void *)ptr);
                if (r < 0)
                    break;

                s->stage_pos += n;
                rest += CHUNK;
                if (n < CHUNK) {
                    if (s->loop && mp3_seek_start(s) == 0)
                        continue;            /* refill from the top      */
                    s->playing  = 0;
                    s->finished = 1;
                    release_channel(s);
                    break;
                }
            }
            continue;
        }

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
