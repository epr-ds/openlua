#include "engine/data/pbp.h"

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

static void put32le(unsigned char *p, unsigned int v)
{
    p[0] = (unsigned char)v;
    p[1] = (unsigned char)(v >> 8);
    p[2] = (unsigned char)(v >> 16);
    p[3] = (unsigned char)(v >> 24);
}

static unsigned int get32le(const unsigned char *p)
{
    return (unsigned int)p[0] | ((unsigned int)p[1] << 8) |
           ((unsigned int)p[2] << 16) | ((unsigned int)p[3] << 24);
}

static void put16le(unsigned char *p, unsigned int v)
{
    p[0] = (unsigned char)v;
    p[1] = (unsigned char)(v >> 8);
}

static void make_pbp(unsigned char *h)
{
    memset(h, 0, PBP_HEADER_SIZE);
    memcpy(h, "\0PBP", 4);
    put32le(h + 4, 0x00010000u);
    put32le(h + 8 + PBP_SECTION_SFO * 4, 40);
    put32le(h + 8 + PBP_SECTION_ICON0 * 4, 200);
    put32le(h + 8 + PBP_SECTION_PIC0 * 4, 300);
    put32le(h + 8 + PBP_SECTION_PIC1 * 4, 400);
}

static void test_pbp(void)
{
    unsigned char h[PBP_HEADER_SIZE];
    OL_Pbp pbp;
    unsigned int off, size;

    make_pbp(h);
    check(pbp_parse(h, sizeof(h), 500, &pbp) == 1, "valid PBP accepted");
    check(pbp_section(&pbp, PBP_SECTION_SFO, &off, &size) == 1 &&
          off == 40 && size == 160, "SFO runs to next section");
    check(pbp_section(&pbp, PBP_SECTION_ICON0, &off, &size) == 1 &&
          off == 200 && size == 100, "ICON0 range parsed");
    check(pbp_section(&pbp, PBP_SECTION_PIC0, &off, &size) == 1 &&
          off == 300 && size == 100, "PIC0 range parsed");
    check(pbp_section(&pbp, PBP_SECTION_PIC1, &off, &size) == 1 &&
          off == 400 && size == 100, "PIC1 runs to EOF");
    check(pbp_section(&pbp, PBP_SECTION_ICON1, NULL, NULL) == 0,
          "absent ICON1 reported missing");
    off = size = 0;
    check(pbp_section(&pbp, PBP_SECTION_ICON1, &off, &size) == 0 &&
          off == 0 && size == 0, "missing section leaves outputs alone");

    check(pbp_parse(NULL, sizeof(h), 500, &pbp) == 0, "null header rejected");
    check(pbp_parse(h, sizeof(h), 500, NULL) == 0, "null output rejected");
    check(pbp_parse(h, sizeof(h) - 1, 500, &pbp) == 0,
          "short header rejected");
    check(pbp_parse(h, sizeof(h), PBP_HEADER_SIZE - 1, &pbp) == 0,
          "too-small file rejected");

    h[0] = 'X';
    check(pbp_parse(h, sizeof(h), 500, &pbp) == 0, "bad magic rejected");

    make_pbp(h);
    put32le(h + 8 + PBP_SECTION_SFO * 4, 1);
    check(pbp_parse(h, sizeof(h), 500, &pbp) == 0,
          "offset below header rejected");

    make_pbp(h);
    put32le(h + 8 + PBP_SECTION_PIC1 * 4, 501);
    check(pbp_parse(h, sizeof(h), 500, &pbp) == 0,
          "offset past EOF rejected");

    make_pbp(h);
    put32le(h + 8 + PBP_SECTION_SFO * 4, 250);
    check(pbp_parse(h, sizeof(h), 500, &pbp) == 0,
          "unsorted offsets rejected");

    make_pbp(h);
    put32le(h + 8 + PBP_SECTION_SFO * 4, 0);
    check(pbp_parse(h, sizeof(h), 500, &pbp) == 1 &&
          pbp_section(&pbp, PBP_SECTION_SFO, NULL, NULL) == 0,
          "missing SFO accepted as absent");

    make_pbp(h);
    put32le(h + 8 + PBP_SECTION_PIC0 * 4, 200);
    check(pbp_parse(h, sizeof(h), 500, &pbp) == 1 &&
          pbp_section(&pbp, PBP_SECTION_ICON0, NULL, &size) == 1 &&
          size == 0, "duplicate offset gives empty section");

    check(pbp_section(NULL, 0, &off, &size) == 0, "null PBP section rejected");
    check(pbp_section(&pbp, -1, &off, &size) == 0, "negative index rejected");
    check(pbp_section(&pbp, PBP_SECTION_COUNT, &off, &size) == 0,
          "index past count rejected");
}

/* Build one SFO with a string, a UTF-8 string with max length and an int. */
static size_t make_sfo(unsigned char *b, size_t cap)
{
    const int count = 3;
    const size_t keys_at = SFO_HEADER_SIZE + (size_t)count * SFO_ENTRY_SIZE;
    const char *keys[3] = { "TITLE", "TITLE_ID", "PARENTAL_LEVEL" };
    const char *vals[3] = { "OpenLua Demo", "OPENLUA01", NULL };
    size_t key_off[3], key_len[3], data_off[3], data_len[3];
    size_t pos = 0, data_pos = 0;
    unsigned int parental = 5;

    memset(b, 0, cap);

    for (int i = 0; i < 3; i++) {
        key_off[i] = pos;
        key_len[i] = strlen(keys[i]) + 1;
        memcpy(b + keys_at + pos, keys[i], key_len[i]);
        pos += key_len[i];
    }
    const size_t data_at = keys_at + pos;

    for (int i = 0; i < 3; i++) {
        data_off[i] = data_pos;
        if (i == 2) {
            data_len[i] = 4;
            put32le(b + data_at + data_pos, parental);
            data_pos += 4;
        } else {
            data_len[i] = strlen(vals[i]) + 1;
            memcpy(b + data_at + data_pos, vals[i], data_len[i]);
            data_pos += data_len[i];
        }
        data_pos = (data_pos + 3) & ~(size_t)3;
    }

    if (data_at + data_pos > cap)
        return 0;

    memcpy(b, "\0PSF", 4);
    put32le(b + 4, 0x00000101u);
    put32le(b + 8, (unsigned int)keys_at);
    put32le(b + 12, (unsigned int)data_at);
    put32le(b + 16, (unsigned int)count);

    for (int i = 0; i < 3; i++) {
        unsigned char *e = b + SFO_HEADER_SIZE + (size_t)i * SFO_ENTRY_SIZE;
        put16le(e, (unsigned int)key_off[i]);
        put16le(e + 2, i == 2 ? 0x0404u : 0x0004u);
        put32le(e + 4, (unsigned int)data_len[i]);
        put32le(e + 8, (unsigned int)data_len[i]);
        put32le(e + 12, (unsigned int)data_off[i]);
    }

    return data_at + data_pos;
}

static void test_sfo(void)
{
    unsigned char b[512];
    OL_Sfo sfo;
    const char *key;
    const unsigned char *value;
    unsigned int fmt, len;

    size_t n = make_sfo(b, sizeof(b));
    check(n > 0, "SFO fixture built");
    check(sfo_parse(b, n, &sfo) == 1, "valid SFO accepted");
    check(sfo.count == 3, "entry count parsed");

    check(sfo_entry(&sfo, 0, &key, &fmt, &value, &len) == 1 &&
          strcmp(key, "TITLE") == 0 && fmt == 0x0004 &&
          len == 13 && memcmp(value, "OpenLua Demo", 12) == 0,
          "string entry parsed");
    check(sfo_entry(&sfo, 1, &key, &fmt, &value, &len) == 1 &&
          strcmp(key, "TITLE_ID") == 0 && len == 10 &&
          memcmp(value, "OPENLUA01", 9) == 0,
          "second string entry parsed");
    check(sfo_entry(&sfo, 2, &key, &fmt, &value, &len) == 1 &&
          strcmp(key, "PARENTAL_LEVEL") == 0 && fmt == 0x0404 &&
          len == 4 && get32le(value) == 5,
          "integer entry parsed");

    check(sfo_entry(&sfo, 3, &key, &fmt, &value, &len) == 0,
          "entry past count rejected");
    check(sfo_entry(NULL, 0, &key, &fmt, &value, &len) == 0,
          "null SFO rejected");
    check(sfo_entry(&sfo, 0, NULL, NULL, NULL, NULL) == 1,
          "entry outputs optional");

    check(sfo_parse(NULL, n, &sfo) == 0, "null SFO data rejected");
    check(sfo_parse(b, n, NULL) == 0, "null SFO output rejected");
    check(sfo_parse(b, SFO_HEADER_SIZE - 1, &sfo) == 0,
          "short SFO header rejected");

    b[0] = 'X';
    check(sfo_parse(b, n, &sfo) == 0, "bad SFO magic rejected");

    n = make_sfo(b, sizeof(b));
    put32le(b + 16, SFO_MAX_ENTRIES + 1);
    check(sfo_parse(b, n, &sfo) == 0, "entry count cap enforced");

    n = make_sfo(b, sizeof(b));
    put32le(b + 16, 3);
    check(sfo_parse(b, SFO_HEADER_SIZE + SFO_ENTRY_SIZE, &sfo) == 0,
          "truncated entry table rejected");

    n = make_sfo(b, sizeof(b));
    put32le(b + 8, 1);
    check(sfo_parse(b, n, &sfo) == 0, "key table below header rejected");

    n = make_sfo(b, sizeof(b));
    put32le(b + 12, get32le(b + 8) - 1);
    check(sfo_parse(b, n, &sfo) == 0, "data before keys rejected");

    n = make_sfo(b, sizeof(b));
    put32le(b + 8, (unsigned int)n + 1);
    check(sfo_parse(b, n, &sfo) == 0, "key table past EOF rejected");

    n = make_sfo(b, sizeof(b));
    put32le(b + 12, (unsigned int)n + 1);
    check(sfo_parse(b, n, &sfo) == 0, "data table past EOF rejected");

    n = make_sfo(b, sizeof(b));
    check(sfo_parse(b, n, &sfo) == 1, "SFO fixture reparsed");
    put16le(b + SFO_HEADER_SIZE, 0xFFFF);
    check(sfo_entry(&sfo, 0, &key, &fmt, &value, &len) == 0,
          "key offset past key table rejected");

    n = make_sfo(b, sizeof(b));
    check(sfo_parse(b, n, &sfo) == 1, "SFO fixture reparsed");
    put16le(b + SFO_HEADER_SIZE + 2, 0x1234);
    check(sfo_entry(&sfo, 0, &key, &fmt, &value, &len) == 0,
          "unknown format rejected");

    n = make_sfo(b, sizeof(b));
    check(sfo_parse(b, n, &sfo) == 1, "SFO fixture reparsed");
    put32le(b + SFO_HEADER_SIZE + 2 * SFO_ENTRY_SIZE + 4, 2);
    check(sfo_entry(&sfo, 2, &key, &fmt, &value, &len) == 0,
          "short integer rejected");

    n = make_sfo(b, sizeof(b));
    check(sfo_parse(b, n, &sfo) == 1, "SFO fixture reparsed");
    put32le(b + SFO_HEADER_SIZE + 2 * SFO_ENTRY_SIZE + 12, 0xFFFF);
    check(sfo_entry(&sfo, 2, &key, &fmt, &value, &len) == 0,
          "integer past data table rejected");

    n = make_sfo(b, sizeof(b));
    check(sfo_parse(b, n, &sfo) == 1, "SFO fixture reparsed");
    put32le(b + SFO_HEADER_SIZE + SFO_ENTRY_SIZE + 4, 0xFFFF);
    check(sfo_entry(&sfo, 1, &key, &fmt, &value, &len) == 1 &&
          len <= n, "oversized string clamped to buffer");

    n = make_sfo(b, sizeof(b));
    check(sfo_parse(b, n, &sfo) == 1, "SFO fixture reparsed");
    put16le(b + SFO_HEADER_SIZE + SFO_ENTRY_SIZE + 2, 0x0204u);
    put32le(b + SFO_HEADER_SIZE + SFO_ENTRY_SIZE + 8, 4);
    check(sfo_entry(&sfo, 1, &key, &fmt, &value, &len) == 1 &&
          fmt == 0x0204 && len == 4,
          "max-length string uses max_len");

    n = make_sfo(b, sizeof(b));
    check(sfo_parse(b, n, &sfo) == 1, "SFO fixture reparsed");
    put16le(b + SFO_HEADER_SIZE + SFO_ENTRY_SIZE + 2, 0x0204u);
    put32le(b + SFO_HEADER_SIZE + SFO_ENTRY_SIZE + 8, 0xFFFF);
    check(sfo_entry(&sfo, 1, &key, &fmt, &value, &len) == 1 &&
          len <= n, "oversized max_len clamped to buffer");
}

int main(void)
{
    test_pbp();
    test_sfo();

    printf("pbp: %d checks, %d failure(s)\n", checks, fails);
    return fails ? 1 : 0;
}
