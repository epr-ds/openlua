/* OpenLua - audio system implementation
 *
 * Built on the native PSP audio path (pspaudio.h / sceAudio*):
 *
 *   - sound.load()  decodes 16-bit PCM WAV (mono or stereo, any rate)
 *     and PSP VMA/VOX (22050 Hz mono, 4-byte sample-count header),
 *     normalising to the 44100 Hz mono hardware output rate.  Note that
 *     sceAudioChReserve() has no rate argument: channels 0..7 always play
 *     at the system rate (44100 Hz by default), so anything else must be
 *     resampled before it reaches the hardware.
 *     MP3 (`.mp3`, ID3v2, or an MPEG frame sync) is streamed through
 *     minimp3 instead of being decoded up front: menu music is 3-10 MB,
 *     so holding whole files in RAM (and the 8 MB WAV cap) is not viable.
 *   - sound.play()  reserves a hardware channel, then starts a small output
 *     thread for that sound.
 *   - The output thread fills one of two buffers and hands it over with
 *     sceAudioOutputBlocking(), which doubles as the playback clock.  A
 *     channel only holds one buffer, so feeding it from the frame loop
 *     left an audible gap between chunks; and because the hardware reads
 *     straight out of the caller's memory, a buffer must never be written
 *     while it is still playing - hence the double buffer plus thread.
 *   - loop / pause / stop / vol map onto the channel state + flags.
 *
 * Volume range: 0..100% externally, mapped to 0..PSP_AUDIO_VOLUME_MAX.
 * Channels: 8 hardware channels (PSP_AUDIO_CHANNEL_MAX). OneLua's
 * sound.play(snd, channel) passes 1..8; 0/auto = next free channel.
 */

#include "engine/snd/sound.h"

#if defined(__PSP__)
#include <pspaudio.h>
#include <pspthreadman.h>
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

/* Loaded sounds live in slots that are created on demand: a game keeps its
 * whole SFX catalogue resident and calls sound.play() on those same handles
 * for the whole session, so there is no useful fixed ceiling.  Handles are
 * stable 1-based slot indexes; a freed slot is reused by the next load and a
 * failed allocation just fails that one load. */
#define SND_GROW   8
#define HW_RATE    44100             /* PSP system channel rate (fixed) */
#define CHUNK      1024              /* samples per sceAudioOutput call (~23 ms) */
#define CHUNK_ALIGN 64               /* PSP_AUDIO_SAMPLE_ALIGN granularity */

/* MP3 streaming: a compressed input buffer plus a staging buffer holding
 * already-resampled 44100 Hz mono samples.  One decoded MPEG1 frame is at
 * most 1152 samples/channel; at the lowest MP3 rate (8 kHz) resampling to
 * 44100 yields ~6350 samples, so 8192 leaves room for one frame on top of
 * a partially-consumed chunk. */
#define MP3_IN_BUF  (64 * 1024)
#define MP3_STAGE   8192

/* Largest possible MPEG frame (body + 4-byte header).  minimp3's
 * MAX_FREE_FORMAT_FRAME_SIZE is 2304, and it needs the next header too, so
 * keeping this many bytes in hand guarantees a whole frame before decoding. */
#define MP3_MAX_FRAME  (2304 + 4)

typedef struct {
    int      used;
    int      channel;      /* reserved PSP audio channel (-1 = none)   */
    int16_t *pcm;          /* 44100 Hz mono, interleaved-free mono     */
    int      len;          /* total samples                            */
    int      pos;          /* next sample index to output              */
    int      loop;
    int      paused;
    int      playing;
    int      finished;
    int      vol;          /* 0..100 (0xF = unset default -> 100)      */
    int      channels;     /* original channel count (1 or 2)          */
    int      orig_rate;    /* source sample rate (Hz)                  */

#if defined(__PSP__)
    /* Output thread: the hardware reads straight out of `obuf[cur]`, so the
     * two buffers alternate and neither is written while it is playing. */
    SceUID         thread;     /* feeding thread, -1 when idle         */
    volatile int   quit;       /* ask the thread to stop              */
    int16_t       *obuf[2];
    void          *obuf_raw[2];/* malloc() result behind each obuf    */
#endif

    /* Streaming MP3 state.  pcm/len/pos stay unused for streams. */
    int            streaming;
    FILE          *fp;         /* kept open on the memory stick          */
    mp3dec_t      *mp3;
    unsigned char *inbuf;
    int            in_len;
    int            in_pos;
    int            eof;        /* input file exhausted                   */
    int            stream_end; /* decoder produced its last frame        */
    int16_t       *stage;      /* resampled 44100 mono staging           */
    int            stage_len;
    int            stage_pos;
    int16_t       *frame;      /* one decoded frame, downmixed mono      */
    int16_t        rs_prev;    /* last source sample of previous frame   */
    int            rs_have_prev;
    double         rs_pos;     /* fractional position in current frame   */
} OLS_Sound;

static OLS_Sound **g_s;        /* slot table; NULL entry = free */
static int         g_cap;      /* slots in the table           */

/* Allocate a zeroed sound in a free slot, growing the table when needed.
 * Returns NULL when out of memory; the caller reports a failed load. */
static OLS_Sound *slot_new(void)
{
    int idx = -1;
    for (int i = 0; i < g_cap; i++) {
        if (!g_s[i]) { idx = i; break; }
    }
    if (idx < 0) {
        int cap = g_cap + SND_GROW;
        OLS_Sound **grown = (OLS_Sound **)realloc(g_s,
                                  (size_t)cap * sizeof(*g_s));
        if (!grown)
            return NULL;
        for (int i = g_cap; i < cap; i++)
            grown[i] = NULL;
        g_s   = grown;
        idx   = g_cap;
        g_cap = cap;
    }

    OLS_Sound *s = (OLS_Sound *)calloc(1, sizeof(*s));
    if (!s)
        return NULL;
    g_s[idx] = s;
    return s;
}

/* 1-based handle for a live slot, or 0. */
static int slot_handle(const OLS_Sound *s)
{
    for (int i = 0; i < g_cap; i++)
        if (g_s[i] == s)
            return i + 1;
    return 0;
}

/* Free a torn-down sound and make its slot available again. */
static void slot_drop(OLS_Sound *s)
{
    int h = slot_handle(s);
    if (h > 0)
        g_s[h - 1] = NULL;
    free(s);
}

#if defined(__PSP__)
/* Two hardware-sized buffers per sound; the output thread alternates them so
 * the buffer being played is never written to.  The audio DMA prefers
 * 64-byte aligned input, which malloc() does not guarantee, so over-allocate
 * and align by hand (newlib here has no memalign). */
static int16_t *snd_alloc_aligned(void **raw_out)
{
    size_t bytes = (size_t)CHUNK * sizeof(int16_t) + 64;
    unsigned char *raw = (unsigned char *)malloc(bytes);
    if (!raw)
        return NULL;
    *raw_out = raw;
    return (int16_t *)(((uintptr_t)raw + 63u) & ~(uintptr_t)63u);
}

static int snd_alloc_outbuf(OLS_Sound *s)
{
    s->obuf[0] = snd_alloc_aligned(&s->obuf_raw[0]);
    s->obuf[1] = snd_alloc_aligned(&s->obuf_raw[1]);
    if (!s->obuf[0] || !s->obuf[1]) {
        free(s->obuf_raw[0]);
        free(s->obuf_raw[1]);
        s->obuf[0] = s->obuf[1] = NULL;
        s->obuf_raw[0] = s->obuf_raw[1] = NULL;
        return -1;
    }
    memset(s->obuf[0], 0, CHUNK * sizeof(int16_t));
    memset(s->obuf[1], 0, CHUNK * sizeof(int16_t));
    return 0;
}
#endif

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
    out->rate     = 22050;       /* source rate: sound_alloc resamples to HW_RATE */
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

        /* minimp3 wipes its decoder state - including the Layer III bit
         * reservoir - whenever the supplied buffer ends in the middle of a
         * frame.  Every frame that then refers back into the missing
         * reservoir fails to decode, which cost ~1% of a track (audible
         * skips).  Refill first so only a real end-of-file can leave a frame
         * truncated. */
        while (!s->eof && s->in_len - s->in_pos < MP3_MAX_FRAME) {
            int before = s->in_len - s->in_pos;
            mp3_feed(s);
            if (s->in_len - s->in_pos <= before)
                break;              /* window is full: decode what we have */
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
    OLS_Sound *s = slot_new();
    if (!s) {
        fclose(f);
        return 0;
    }
    int handle = slot_handle(s);

    s->channel = -1;
#if defined(__PSP__)
    s->thread  = -1;
#endif
    s->vol     = 100;

    s->mp3   = (mp3dec_t *)calloc(1, sizeof(mp3dec_t));
    s->inbuf = (unsigned char *)malloc(MP3_IN_BUF);
    s->stage = (int16_t *)malloc(MP3_STAGE * sizeof(int16_t));
    s->frame = (int16_t *)malloc(MINIMP3_MAX_SAMPLES_PER_FRAME *
                                 sizeof(int16_t));
#if defined(__PSP__)
    int outbuf_ok = (snd_alloc_outbuf(s) == 0);
#else
    int outbuf_ok = 1;
#endif
    if (!s->mp3 || !s->inbuf || !s->stage || !s->frame || !outbuf_ok) {
        fclose(f);
        free(s->mp3);
        free(s->inbuf);
        free(s->stage);
        free(s->frame);
#if defined(__PSP__)
        free(s->obuf_raw[0]);
        free(s->obuf_raw[1]);
#endif
        slot_drop(s);
        return 0;
    }

    s->used      = 1;
    s->streaming = 1;
    s->fp        = f;

    if (mp3_seek_start(s) != 0) {
        sound_free(handle);
        return 0;
    }

    /* Prime the first frame so a non-MP3 file is rejected at load time and
     * the sample rate is known before playback. */
    int n = mp3_decode_mono(s);
    if (n <= 0) {
        sound_free(handle);
        return 0;
    }
    mp3_append_resampled(s, n);
    return handle;
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

    if (fsize > (8L * 1024 * 1024)) {
        fclose(f);
        return 0;
    }

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

    OLS_Sound *s = slot_new();
    if (!s) {
        free(final);
        return 0;
    }
#if defined(__PSP__)
    if (snd_alloc_outbuf(s) != 0) {
        slot_drop(s);
        free(final);
        return 0;
    }
#endif
    s->used      = 1;
    s->channel   = -1;
    s->pcm       = final;
    s->len       = flen;
    s->pos       = 0;
    s->loop      = 0;
    s->paused    = 0;
    s->playing   = 0;
    s->finished  = 0;
    s->vol       = 100;
    s->channels  = raw.channels;
    s->orig_rate = raw.rate;
#if defined(__PSP__)
    s->thread    = -1;
#endif
    return slot_handle(s);
}

static OLS_Sound *get(int h)
{
    if (h < 1 || h > g_cap)
        return NULL;
    OLS_Sound *s = g_s[h - 1];
    return (s && s->used) ? s : NULL;
}

static void release_channel(OLS_Sound *s)
{
    if (s->channel >= 0)
        sceAudioChRelease(s->channel);
    s->channel = -1;
}

/* Copy up to `want` samples of the next chunk into dst.
 * Returns the number copied, or 0 when the sound has ended.
 *
 * A looping sound is filled to the end of the chunk, wrapping as often as
 * needed, so a loop shorter than one chunk does not insert silence at the
 * seam.  Anything else returns a short (zero-padded) final chunk. */
static int snd_next_chunk(OLS_Sound *s, int16_t *dst, int want)
{
    int got = 0;

    if (s->streaming) {
        int restarted = 0;
        while (got < want) {
            int avail = s->stage_len - s->stage_pos;
            if (avail <= 0) {
                if (!s->stream_end)
                    avail = mp3_fill(s, want - got);
                if (avail <= 0) {
                    /* End of stream: rewind once for a looping sound. */
                    if (!s->loop || restarted ||
                        mp3_seek_start(s) != 0)
                        break;
                    restarted = 1;
                    continue;
                }
            }
            int n = want - got;
            if (n > avail)
                n = avail;
            memcpy(dst + got, s->stage + s->stage_pos,
                   (size_t)n * sizeof(int16_t));
            got += n;
            s->stage_pos += n;
        }
        return got;
    }

    while (got < want) {
        int remaining = s->len - s->pos;
        if (remaining <= 0) {
            if (!s->loop)
                break;
            s->pos = 0;
            remaining = s->len;
        }
        int n = want - got;
        if (n > remaining)
            n = remaining;
        memcpy(dst + got, s->pcm + s->pos, (size_t)n * sizeof(int16_t));
        got += n;
        s->pos += n;
    }
    return got;
}

#if defined(__PSP__)
/* ----------------------------------------------------------------- */
/* Output thread                                                     */
/*
 * sceAudioChReserve() has no rate argument: channels 0..7 always play at
 * the system rate (44100 Hz), and each holds exactly one buffer which the
 * mixer reads straight out of our memory.  Feeding that from screen.flip()
 * left a silent gap between every chunk, and rewriting the staging buffer
 * while it played garbled MP3 streams.  A small per-sound thread with two
 * buffers fixes both: sceAudioOutputBlocking() waits for the previous
 * buffer to finish, which paces the loop and keeps the output gapless,
 * and the alternating buffer is never touched while it is playing.
 */

static int snd_thread(SceSize args, void *argp)
{
    OLS_Sound *s = *(OLS_Sound **)argp;
    int cur = 0;

    (void)args;

    while (!s->quit) {
        if (s->paused) {
            sceKernelDelayThread(2000);
            continue;
        }

        int n = snd_next_chunk(s, s->obuf[cur], CHUNK);
        if (n <= 0) {
            s->playing  = 0;
            s->finished = 1;
            break;
        }

        int full = (n >= CHUNK);
        if (!full)
            memset(s->obuf[cur] + n, 0,
                   (size_t)(CHUNK - n) * sizeof(int16_t));

        if (sceAudioOutputBlocking(s->channel, vol_hw(s->vol),
                                   s->obuf[cur]) < 0) {
            s->playing  = 0;
            s->finished = 1;
            break;
        }

        cur ^= 1;
        if (!full && !s->loop) {
            s->playing  = 0;
            s->finished = 1;
            break;
        }
    }

    /* The final chunk is still playing: wait it out before the channel goes
     * back to the pool (sceAudioChReserve() will not reuse it otherwise). */
    if (s->channel >= 0) {
        int ch = s->channel;
        s->channel = -1;               /* claim it before the drain loop */
        while (sceAudioGetChannelRestLen(ch) > 0)
            sceKernelDelayThread(500);
        sceAudioChRelease(ch);
    }
    return 0;
}

static int snd_thread_start(OLS_Sound *s)
{
    s->quit   = 0;
    /* 32 KB: minimp3 decoding plus the kernel's syscall frames, and enough
     * headroom that a short stack can never clobber the saved return. */
    s->thread = sceKernelCreateThread("openlua_audio", snd_thread,
                                      0x10, 0x8000, 0, NULL);
    if (s->thread < 0)
        return -1;
    if (sceKernelStartThread(s->thread, sizeof(s), &s) < 0) {
        sceKernelDeleteThread(s->thread);
        s->thread = -1;
        return -1;
    }
    return 0;
}

/* Stop the feeding thread (if any), drain the channel and release it.
 * The wait is unconditional: the thread is either parked in one output
 * call (it returns within a chunk) or about to exit, and deleting a thread
 * that still owns the decoder/buffers would corrupt memory. */
static void snd_halt(OLS_Sound *s)
{
    s->playing = 0;
    if (s->thread >= 0) {
        s->quit = 1;
        if (sceKernelWaitThreadEnd(s->thread, NULL) >= 0)
            sceKernelDeleteThread(s->thread);
        s->thread = -1;
    }
    if (s->channel >= 0) {
        while (sceAudioGetChannelRestLen(s->channel) > 0)
            sceKernelDelayThread(500);
        sceAudioChRelease(s->channel);
        s->channel = -1;
    }
}
#else
static void snd_halt(OLS_Sound *s)
{
    release_channel(s);
}
#endif

int sound_play(int h)
{
    return sound_play_channel(h, -1);
}

/* Reserve channel ch (0..7) if not already holding it, then start. */
int sound_play_channel(int h, int ch)
{
    OLS_Sound *s = get(h);
    if (!s || ch < -1 || ch >= PSP_AUDIO_CHANNEL_MAX) return -1;

#if defined(__PSP__)
    /* A replay restarts from the top, so tear the old output thread down
     * before touching the position or the stream decoder. */
    snd_halt(s);
    s->channel = -1;

    int rc = sceAudioChReserve(ch >= 0 ? ch : PSP_AUDIO_NEXT_CHANNEL,
                               CHUNK, PSP_AUDIO_FORMAT_MONO);
    if (rc < 0)
        return -1;
    s->channel = rc;
#else
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
#endif

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

#if defined(__PSP__)
    if (snd_thread_start(s) != 0) {
        s->playing = 0;
        release_channel(s);
        return -1;
    }
#endif
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
    snd_halt(s);
    s->playing  = 0;
    s->pos      = 0;
    s->paused   = 0;
    s->finished = 1;
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

/* Resampled length in samples (0 for streams, whose length is unknown). */
int sound_length(int h)
{
    OLS_Sound *s = get(h);
    return s ? (s->streaming ? 0 : s->len) : 0;
}

void sound_free(int h)
{
    OLS_Sound *s = get(h);
    if (!s) return;
    snd_halt(s);
    if (s->streaming)
        mp3_close(s);
    else
        free(s->pcm);
#if defined(__PSP__)
    free(s->obuf_raw[0]);
    free(s->obuf_raw[1]);
#endif
    memset(s, 0, sizeof(*s));
    slot_drop(s);
}

/* ----------------------------------------------------------------- */
/* per-frame pump — host-build playback driver                        */
void sound_pump(void)
{
#if defined(__PSP__)
    /* The PSP build feeds each playing sound from its own output thread;
     * screen.flip() has nothing to do. */
    return;
#else
    for (int i = 0; i < g_cap; i++) {
        OLS_Sound *s = g_s[i];
        if (!s || !s->used || !s->playing || s->paused || s->channel < 0)
            continue;

        int rest = sceAudioGetChannelRestLen(s->channel);
        if (rest < 0)
            continue;                        /* channel error: skip    */

        if (s->streaming) {
            /* Reuse the same chunking the PSP output thread uses, so the host
             * suite exercises the real streaming path. */
            static int16_t chunk[CHUNK];
            while (rest < CHUNK) {
                int n = snd_next_chunk(s, chunk, CHUNK);
                if (n <= 0) {
                    s->playing  = 0;
                    s->finished = 1;
                    release_channel(s);
                    break;
                }
                if (n < CHUNK)
                    memset(chunk + n, 0,
                           (size_t)(CHUNK - n) * sizeof(int16_t));

                int r = sceAudioOutput(s->channel, vol_hw(s->vol), (void *)chunk);
                if (r < 0)
                    break;

                rest += CHUNK;
                if (n < CHUNK) {
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
#endif
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
