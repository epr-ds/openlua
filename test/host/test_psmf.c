#include "engine/data/psmf.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

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

static void put32(unsigned char *p, uint32_t v)
{
    p[0] = (unsigned char)(v >> 24);
    p[1] = (unsigned char)(v >> 16);
    p[2] = (unsigned char)(v >> 8);
    p[3] = (unsigned char)v;
}

static void put_ts(unsigned char *p, uint64_t v)
{
    p[0] = (unsigned char)(v >> 40);
    p[1] = (unsigned char)(v >> 32);
    put32(p + 2, (uint32_t)v);
}

static void make_header(unsigned char *h)
{
    memset(h, 0, PSMF_HEADER_SIZE);
    memcpy(h, "PSMF", 4);
    memcpy(h + 4, "0014", 4);
    put32(h + 8, PSMF_HEADER_SIZE);
    put32(h + 12, 0x40800);
    put_ts(h + 0x54, 90000);
    put_ts(h + 0x5A, 660570);
    h[142] = 9;   /* 144x80 */
    h[143] = 5;
}

int main(void)
{
    unsigned char h[PSMF_HEADER_SIZE];
    OL_PsmfInfo info;
    const uint64_t file_size = PSMF_HEADER_SIZE + 0x40800u;

    make_header(h);
    memset(&info, 0xA5, sizeof(info));
    check(psmf_parse_header(h, sizeof(h), file_size, &info) == 0,
          "valid header accepted");
    check(memcmp(h + 4, "0014", 4) == 0 && info.version == 0x30303134u,
          "version parsed");
    check(info.stream_offset == PSMF_HEADER_SIZE, "stream offset parsed");
    check(info.stream_size == 0x40800u, "stream size parsed");
    check(info.first_ts == 90000u, "first timestamp parsed");
    check(info.last_ts == 660570u, "last timestamp parsed");
    check(info.duration_90k == 570570u, "duration computed");
    check(info.width == 144 && info.height == 80, "dimensions parsed");

    check(psmf_parse_header(h, sizeof(h), file_size - 1, &info) == -1,
          "stream beyond file rejected");
    check(psmf_parse_header(h, sizeof(h) - 1, file_size, &info) == -1,
          "short header rejected");
    check(psmf_parse_header(NULL, sizeof(h), file_size, &info) == -1,
          "null data rejected");
    check(psmf_parse_header(h, sizeof(h), file_size, NULL) == -1,
          "null output rejected");

    h[0] = 'X';
    check(psmf_parse_header(h, sizeof(h), file_size, &info) == -1,
          "bad magic rejected");
    make_header(h);
    h[7] = 'x';
    check(psmf_parse_header(h, sizeof(h), file_size, &info) == -1,
          "non-numeric version rejected");
    make_header(h);
    put32(h + 8, PSMF_HEADER_SIZE - 1);
    check(psmf_parse_header(h, sizeof(h), file_size, &info) == -1,
          "offset below header rejected");
    make_header(h);
    put32(h + 8, PSMF_HEADER_SIZE + 1);
    check(psmf_parse_header(h, sizeof(h), file_size, &info) == -1,
          "unaligned offset rejected");
    make_header(h);
    put32(h + 12, 0);
    check(psmf_parse_header(h, sizeof(h), file_size, &info) == -1,
          "zero stream size rejected");

    make_header(h);
    put_ts(h + 0x54, 0x100000000ull);
    put_ts(h + 0x5A, 0x100010000ull);
    check(psmf_parse_header(h, sizeof(h), file_size, &info) == 0 &&
          info.first_ts == 0x100000000ull &&
          info.last_ts == 0x100010000ull &&
          info.duration_90k == 0x10000u,
          "64-bit timestamps parsed");

    make_header(h);
    put_ts(h + 0x54, 200);
    put_ts(h + 0x5A, 100);
    check(psmf_parse_header(h, sizeof(h), file_size, &info) == 0 &&
          info.duration_90k == 0,
          "reversed timestamps keep zero duration");

    make_header(h);
    h[142] = 0;
    h[143] = 0;
    check(psmf_parse_header(h, sizeof(h), file_size, &info) == 0 &&
          info.width == 0 && info.height == 0,
          "missing dimensions allowed");

    printf("psmf: %d checks, %d failed\n", checks, fails);
    return fails ? 1 : 0;
}
