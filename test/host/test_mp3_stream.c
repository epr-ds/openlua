/* MP3 streaming regression: the decoder must deliver every frame of a stream
 * that spans several input-buffer refills.
 *
 * The stream is built by repeating the fixture's frame data (its ID3v2 tag is
 * stripped first) until it is several times the 64 KB input window.  Reusing
 * one fixture keeps the expected sample count exact: 10 frames x 1152
 * samples per copy.
 *
 * This includes sound.c so the static streaming helpers can be driven
 * directly, which is the only way to observe dropped frames - playback just
 * skips ahead when they go missing. */
#include <unistd.h>

#include "engine/snd/sound.c"

#define COPIES      300
#define FRAMES_EACH 10
#define SAMPLES_EACH (FRAMES_EACH * 1152)

static int checks;
static int fails;

static void check(int cond, const char *msg)
{
    checks++;
    if (!cond) {
        fails++;
        fprintf(stderr, "FAIL: %s\n", msg);
    }
}

/* Copy the fixture `copies` times, dropping an ID3v2 tag from the first copy
 * and skipping it on every later copy.  Returns the number of bytes written,
 * or -1. */
static long build_stream(const char *fixture, const char *out, int copies)
{
    FILE *in = fopen(fixture, "rb");
    if (!in)
        return -1;
    if (fseek(in, 0, SEEK_END) != 0) { fclose(in); return -1; }
    long size = ftell(in);
    if (size <= 0 || fseek(in, 0, SEEK_SET) != 0) { fclose(in); return -1; }

    unsigned char *buf = malloc((size_t)size);
    if (!buf) { fclose(in); return -1; }
    size_t got = fread(buf, 1, (size_t)size, in);
    fclose(in);
    if ((long)got != size) { free(buf); return -1; }

    long start = 0;
    if (size >= 10 && !memcmp(buf, "ID3", 3)) {
        long tag = ((long)(buf[6] & 0x7f) << 21) | ((long)(buf[7] & 0x7f) << 14) |
                   ((long)(buf[8] & 0x7f) << 7) | (long)(buf[9] & 0x7f);
        tag += 10;
        if (tag < 10 || tag > size) { free(buf); return -1; }
        start = tag;
    }
    long body = size - start;
    if (body <= 0) { free(buf); return -1; }

    FILE *f = fopen(out, "wb");
    if (!f) { free(buf); return -1; }
    long written = 0;
    for (int i = 0; i < copies; i++) {
        if (fwrite(buf + start, 1, (size_t)body, f) != (size_t)body) {
            fclose(f); free(buf); return -1;
        }
        written += body;
    }
    fclose(f);
    free(buf);
    return written;
}

/* Drain the stream frame by frame.  `force_rate` overrides the source rate
 * the decoder reports, to exercise the resampler with a realistic mismatch.
 * Returns the source samples decoded; the staged count is written to
 * *staged_out when non-NULL. */
static long drain_frames(OLS_Sound *s, int force_rate, long *staged_out)
{
    mp3_seek_start(s);
    s->stage_len = 0;
    s->stage_pos = 0;
    long src = 0, staged = 0;
    for (;;) {
        int n = mp3_decode_mono(s);
        if (n <= 0)
            break;
        src += n;
        if (force_rate > 0)
            s->orig_rate = force_rate;
        int before = s->stage_len;
        mp3_append_resampled(s, n);
        staged += s->stage_len - before;
        if (s->stage_len >= CHUNK)
            s->stage_len = 0;      /* emulate a consumer draining it */
    }
    if (staged_out)
        *staged_out = staged;
    return src;
}

int main(int argc, char **argv)
{
    if (argc < 2) {
        fprintf(stderr, "mp3_stream: missing fixture path\n");
        return 1;
    }

    char tmp[] = "/tmp/openlua-mp3-XXXXXX";
    if (!mkdtemp(tmp)) {
        fprintf(stderr, "mp3_stream: setup failed\n");
        return 1;
    }
    char path[1024];
    snprintf(path, sizeof(path), "%s/repeat.mp3", tmp);

    long body = build_stream(argv[1], path, COPIES);
    check(body > 0, "repeat fixture into a multi-window stream");
    check(body > 4L * MP3_IN_BUF, "stream spans several input buffers");

    int h = sound_alloc(path);
    check(h > 0, "long stream loads");
    if (h > 0) {
        OLS_Sound *s = get(h);

        /* 1. Every frame must decode: the bit-reservoir reset that used to
         *    happen at a buffer boundary silently dropped ~1% of a track. */
        long staged = 0;
        long src = drain_frames(s, 0, &staged);
        check(src == (long)COPIES * SAMPLES_EACH, "every frame decodes");
        check(s->orig_rate == 44100, "fixture reports its source rate");
        check(staged == src, "44100 Hz source is staged 1:1");

        /* 2. The resampler must keep the duration: the same frames are fed
         *    through the 48 kHz -> 44.1 kHz path. */
        long resampled = 0;
        src = drain_frames(s, 48000, &resampled);
        long expect = (long)((double)src * 44100.0 / 48000.0);
        check(src == (long)COPIES * SAMPLES_EACH, "frames survive a device change");
        check(resampled >= expect - 4 && resampled <= expect + 4,
              "resampled length matches the source duration");

        /* 3. Feeding chunks (the PSP output-thread path) must deliver the
         *    same total. */
        mp3_seek_start(s);
        static int16_t chunk[CHUNK];
        long total = 0;
        int guard = 0;
        for (;;) {
            int n = snd_next_chunk(s, chunk, CHUNK);
            if (n <= 0)
                break;
            total += n;
            if (++guard > 200000)
                break;
        }
        check(total == (long)COPIES * SAMPLES_EACH, "chunked feeding loses nothing");
        check(guard < 200000, "chunked feeding terminates");
        check(snd_next_chunk(s, chunk, CHUNK) == 0, "ended stream stays ended");
    }
    sound_free(h);
    unlink(path);
    rmdir(tmp);

    printf("mp3_stream: %d checks, %d failed\n", checks, fails);
    return fails ? 1 : 0;
}
