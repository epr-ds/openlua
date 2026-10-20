#define _GNU_SOURCE
#include "engine/snd/sound.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int checks;
static int fails;

static void check(int cond, const char *msg)
{
    checks++;
    if (!cond) {
        fails++;
        printf("  FAIL: %s\n", msg);
    }
}

static void put16(unsigned char *p, uint16_t v)
{
    p[0] = (unsigned char)(v & 0xff);
    p[1] = (unsigned char)(v >> 8);
}

static void put32(unsigned char *p, uint32_t v)
{
    p[0] = (unsigned char)(v & 0xff);
    p[1] = (unsigned char)((v >> 8) & 0xff);
    p[2] = (unsigned char)((v >> 16) & 0xff);
    p[3] = (unsigned char)(v >> 24);
}

static int write_bytes(const char *path, const void *data, size_t len)
{
    FILE *f = fopen(path, "wb");
    if (!f)
        return -1;
    int ok = fwrite(data, 1, len, f) == len;
    if (fclose(f) != 0)
        ok = 0;
    return ok ? 0 : -1;
}

static int write_wav(const char *path, int channels, int rate, int frames)
{
    unsigned int data_len = (unsigned int)frames * (unsigned int)channels * 2u;
    size_t total = 44u + data_len;
    unsigned char *buf = (unsigned char *)calloc(1, total);
    if (!buf)
        return -1;

    memcpy(buf + 0, "RIFF", 4);
    put32(buf + 4, (uint32_t)(total - 8));
    memcpy(buf + 8, "WAVE", 4);
    memcpy(buf + 12, "fmt ", 4);
    put32(buf + 16, 16);
    put16(buf + 20, 1);
    put16(buf + 22, (uint16_t)channels);
    put32(buf + 24, (uint32_t)rate);
    put32(buf + 28, (uint32_t)(rate * channels * 2));
    put16(buf + 32, (uint16_t)(channels * 2));
    put16(buf + 34, 16);
    memcpy(buf + 36, "data", 4);
    put32(buf + 40, data_len);

    for (int i = 0; i < frames * channels; i++) {
        int16_t sample = (int16_t)((i * 97) % 2000 - 1000);
        buf[44 + (size_t)i * 2] = (unsigned char)(sample & 0xff);
        buf[45 + (size_t)i * 2] = (unsigned char)((uint16_t)sample >> 8);
    }

    int rc = write_bytes(path, buf, total);
    free(buf);
    return rc;
}

static int write_vma(const char *path, uint32_t samples)
{
    size_t total = 4u + (size_t)samples * 2u;
    unsigned char *buf = (unsigned char *)calloc(1, total);
    if (!buf)
        return -1;
    put32(buf, samples);
    for (uint32_t i = 0; i < samples; i++)
        put16(buf + 4 + (size_t)i * 2u, (uint16_t)(i * 1234u));
    int rc = write_bytes(path, buf, total);
    free(buf);
    return rc;
}

int main(void)
{
    char tmp[] = "/tmp/openlua-sound-XXXXXX";
    char path[1024];

    if (!mkdtemp(tmp)) {
        fprintf(stderr, "sound setup failed\n");
        return 1;
    }

    snprintf(path, sizeof(path), "%s/one.bin", tmp);
    check(write_bytes(path, "A", 1) == 0, "write 1-byte file");
    check(sound_alloc(path) == 0, "1-byte file rejected");

    snprintf(path, sizeof(path), "%s/two.bin", tmp);
    check(write_bytes(path, "AB", 2) == 0, "write 2-byte file");
    check(sound_alloc(path) == 0, "2-byte file rejected");

    snprintf(path, sizeof(path), "%s/three.bin", tmp);
    check(write_bytes(path, "ABC", 3) == 0, "write 3-byte file");
    check(sound_alloc(path) == 0, "3-byte file rejected");

    snprintf(path, sizeof(path), "%s/four.bin", tmp);
    check(write_bytes(path, "RIFF", 4) == 0, "write 4-byte RIFF");
    check(sound_alloc(path) == 0, "4-byte RIFF rejected without over-read");

    snprintf(path, sizeof(path), "%s/mono.wav", tmp);
    check(write_wav(path, 1, 11025, 100) == 0, "write mono wav");
    int h = sound_alloc(path);
    check(h > 0, "mono wav loads");
    check(sound_rate(h) == 22050, "mono wav resampled rate");
    check(sound_channels(h) == 1, "mono wav channel count");
    sound_free(h);

    snprintf(path, sizeof(path), "%s/stereo.wav", tmp);
    check(write_wav(path, 2, 44100, 100) == 0, "write stereo wav");
    h = sound_alloc(path);
    check(h > 0, "stereo wav loads");
    check(sound_rate(h) == 22050, "stereo wav resampled rate");
    check(sound_channels(h) == 2, "stereo wav channel count");
    sound_free(h);

    snprintf(path, sizeof(path), "%s/tiny-rate.wav", tmp);
    check(write_wav(path, 1, 1000000, 1) == 0, "write tiny high-rate wav");
    check(sound_alloc(path) == 0, "zero-length resample rejected");

    snprintf(path, sizeof(path), "%s/huge-chunk.wav", tmp);
    {
        unsigned char hdr[44] = {0};
        memcpy(hdr + 0, "RIFF", 4);
        put32(hdr + 4, 36);
        memcpy(hdr + 8, "WAVE", 4);
        memcpy(hdr + 12, "fmt ", 4);
        put32(hdr + 16, 16);
        put16(hdr + 20, 1);
        put16(hdr + 22, 1);
        put32(hdr + 24, 22050);
        put32(hdr + 28, 44100);
        put16(hdr + 32, 2);
        put16(hdr + 34, 16);
        memcpy(hdr + 36, "data", 4);
        put32(hdr + 40, 0xfffffff0u);
        check(write_bytes(path, hdr, sizeof(hdr)) == 0, "write huge chunk wav");
    }
    check(sound_alloc(path) == 0, "chunk length overflow rejected");

    snprintf(path, sizeof(path), "%s/valid.vma", tmp);
    check(write_vma(path, 4) == 0, "write vma");
    int h1 = sound_alloc(path);
    check(h1 > 0, "vma loads");
    check(sound_rate(h1) == 22050, "vma rate");
    check(sound_channels(h1) == 1, "vma channel count");

    snprintf(path, sizeof(path), "%s/short.vma", tmp);
    check(write_bytes(path, "\x01\x00\x00", 3) == 0, "write short vma");
    check(sound_alloc(path) == 0, "short vma rejected");

    snprintf(path, sizeof(path), "%s/mismatch.vma", tmp);
    {
        unsigned char bad[8] = {0};
        put32(bad, 99);
        check(write_bytes(path, bad, sizeof(bad)) == 0, "write mismatch vma");
    }
    check(sound_alloc(path) == 0, "vma count mismatch rejected");

    check(sound_alloc(path) == 0, "failed vma leaves no handle");

    snprintf(path, sizeof(path), "%s/valid2.vma", tmp);
    check(write_vma(path, 8) == 0, "write second vma");
    int h2 = sound_alloc(path);
    check(h2 > 0 && h2 != h1, "second vma gets distinct handle");

    check(sound_play_channel(h1, 2) == 0, "explicit channel reserves");
    check(sound_play_channel(h2, 2) == -1, "busy explicit channel rejected");
    check(sound_stop(h1) == 0, "stop releases channel");
    check(sound_play_channel(h2, 2) == 0, "channel available after stop");
    sound_free(h2);
    check(sound_play_channel(h1, 2) == 0, "free releases channel");
    check(sound_stop(h1) == 0, "stop second reservation");
    h2 = sound_alloc(path);
    check(h2 > 0 && h2 != h1, "reload second sound after free");
    check(sound_play_channel(h1, 8) == -1, "out-of-range channel rejected");
    check(sound_play_channel(h1, -2) == -1, "invalid auto channel rejected");

    check(sound_play(h1) == 0, "auto channel reserves");
    check(sound_play_channel(h2, 0) == -1, "auto channel is owned");
    check(sound_stop(h1) == 0, "stop auto channel");
    check(sound_play_channel(h2, 0) == 0, "channel 0 available after auto stop");

    check(sound_play_channel(h2, 1) == 0, "play for natural finish");
    sound_pump();
    check(sound_endstream(h2) == 1, "pump finishes short sound");
    check(sound_playing(h2) == 0, "finished sound not playing");
    check(sound_play_channel(h1, 1) == 0, "finished sound released channel");
    sound_stop(h1);
    sound_free(h2);
    sound_free(h1);

    const char *files[] = {
        "one.bin", "two.bin", "three.bin", "four.bin", "mono.wav",
        "stereo.wav", "tiny-rate.wav", "huge-chunk.wav", "valid.vma",
        "short.vma", "mismatch.vma", "valid2.vma"
    };
    for (size_t i = 0; i < sizeof(files) / sizeof(files[0]); i++) {
        snprintf(path, sizeof(path), "%s/%s", tmp, files[i]);
        unlink(path);
    }
    rmdir(tmp);

    printf("sound: %d checks, %d failed\n", checks, fails);
    return fails ? 1 : 0;
}
