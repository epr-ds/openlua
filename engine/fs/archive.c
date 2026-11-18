/* OpenLua - compressed archive reader.
 *
 * Supports ZIP (stored/deflate through vendored miniz), TAR (ustar, GNU
 * long names and pax extended headers), TAR.GZ/TGZ, TAR.BZ2/TBZ2/TBZ and
 * single-file GZIP or BZIP2.  The gzip member is parsed here so the raw
 * deflate payload can be handed to miniz without a second compression
 * library; bzip2 streams use the vendored libbzip2 decompressor, which
 * verifies the block and combined CRCs itself.
 *
 * RAR, 7z and xz are detected by magic and reported as
 * OL_ARCHIVE_ERR_UNSUPPORTED.  The format dispatcher is deliberately small
 * so those containers can be added without touching the Lua layer.
 *
 * Only stdio and the portable filesystem layer are used, so the whole
 * module builds on the host as well as under psp-gcc.
 */

#include "engine/fs/archive.h"
#include "engine/fs/olfs.h"

#include "miniz.h"
#include "miniz_zip.h"
#include "bzlib.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ARC_IO_BUF      16384
#define ARC_PATH_MAX    512
#define ARC_PAX_MAX     (1024 * 1024)
#define ARC_TAR_BLOCK   512
#define ARC_GZ_PEEK     512

/* ------------------------------------------------------------------ */
/* Small helpers                                                       */
/* ------------------------------------------------------------------ */

static unsigned int arc_le16(const unsigned char *p)
{
    return (unsigned int)p[0] | ((unsigned int)p[1] << 8);
}

static unsigned long long arc_le32(const unsigned char *p)
{
    return (unsigned long long)p[0] | ((unsigned long long)p[1] << 8) |
           ((unsigned long long)p[2] << 16) |
           ((unsigned long long)p[3] << 24);
}

static unsigned long long arc_round_block(unsigned long long n)
{
    return (n + ARC_TAR_BLOCK - 1) & ~(unsigned long long)(ARC_TAR_BLOCK - 1);
}

static int arc_zero_block(const unsigned char *p, size_t n)
{
    for (size_t i = 0; i < n; i++)
        if (p[i])
            return 0;
    return 1;
}

/* Copy an archive member name, normalising separators, dropping leading
 * "./" and trailing slashes.  Returns -1 when the result would not fit. */
static int arc_copy_name(char *dst, size_t cap, const char *src, size_t len)
{
    size_t i = 0;

    while (len >= 2 && src[0] == '.' && src[1] == '/') {
        src += 2;
        len -= 2;
    }
    while (len > 0 && (src[len - 1] == '/' || src[len - 1] == '\\'))
        len--;
    if (len == 0 || len >= cap)
        return -1;

    for (size_t j = 0; j < len; j++) {
        char c = src[j];
        if (c == '\\')
            c = '/';
        if (c == '\0')
            return -1;
        dst[i++] = c;
    }
    dst[i] = '\0';
    return 0;
}

int ol_archive_is_safe_name(const char *name)
{
    const char *p;

    if (!name || !*name)
        return 0;
    if (name[0] == '/' || name[0] == '\\')
        return 0;
    if (isalpha((unsigned char)name[0]) && name[1] == ':')
        return 0;
    if (strchr(name, ':'))
        return 0;                      /* device prefix inside a member */

    p = name;
    for (;;) {
        const char *q = p;
        size_t len;

        while (*q && *q != '/' && *q != '\\')
            q++;
        len = (size_t)(q - p);
        if (len == 0) {
            if (*q == '\0')
                break;                 /* trailing separator */
            return 0;                  /* empty component */
        }
        if (len == 1 && p[0] == '.')
            return 0;
        if (len == 2 && p[0] == '.' && p[1] == '.')
            return 0;
        if (*q == '\0')
            break;
        p = q + 1;
    }
    return 1;
}

static int arc_join_path(char *out, size_t cap, const char *dir,
                         const char *name)
{
    size_t dl = strlen(dir);
    size_t nl = strlen(name);
    int sep = dl > 0 && dir[dl - 1] != '/' && dir[dl - 1] != '\\';

    if (!ol_archive_is_safe_name(name))
        return -1;
    if (dl + (size_t)sep + nl + 1 > cap)
        return -1;

    memcpy(out, dir, dl);
    if (sep)
        out[dl++] = '/';
    memcpy(out + dl, name, nl + 1);
    return 0;
}

static int arc_ensure_parent(const char *path)
{
    char dir[ARC_PATH_MAX];

    if (olfs_path_dir(path, dir, sizeof(dir)) != 0)
        return -1;
    if (dir[0] && !olfs_exists(dir))
        (void)olfs_mkdir_p(dir);
    return 0;
}

static int arc_dst_for(const char *dst, const char *member, char *out,
                       size_t cap)
{
    size_t dl = strlen(dst);
    int as_dir = 0;

    if (dl > 0 && (dst[dl - 1] == '/' || dst[dl - 1] == '\\'))
        as_dir = 1;
    else if (olfs_isdir(dst))
        as_dir = 1;

    if (!as_dir) {
        if (dl + 1 > cap)
            return -1;
        memcpy(out, dst, dl + 1);
        return 0;
    }

    const char *base = strrchr(member, '/');
    base = base ? base + 1 : member;
    return arc_join_path(out, cap, dst, base);
}

/* ------------------------------------------------------------------ */
/* Gzip reader                                                         */
/* ------------------------------------------------------------------ */

typedef struct {
    FILE *f;
    mz_stream zs;
    unsigned char in[ARC_IO_BUF];
    int initialized;
    int stream_end;
    int error;
    unsigned long crc;
    unsigned long long produced;
} OLGz;

static int gz_skip_cstr(FILE *f)
{
    int c;

    do {
        c = fgetc(f);
        if (c == EOF)
            return -1;
    } while (c != 0);
    return 0;
}

static int gz_open(OLGz *g, FILE *f)
{
    unsigned char h[10];
    unsigned int flg;

    memset(g, 0, sizeof(*g));
    g->f = f;
    g->crc = mz_crc32(0, NULL, 0);

    if (fread(h, 1, sizeof(h), f) != sizeof(h))
        return -1;
    if (h[0] != 0x1f || h[1] != 0x8b || h[2] != 8)
        return -1;

    flg = h[3];
    if (flg & 0xe0)
        return -1;                     /* reserved flags */
    if (flg & 0x04) {                  /* FEXTRA */
        unsigned char x[2];
        if (fread(x, 1, 2, f) != 2)
            return -1;
        if (fseek(f, (long)arc_le16(x), SEEK_CUR) != 0)
            return -1;
    }
    if ((flg & 0x08) && gz_skip_cstr(f) != 0)  /* FNAME */
        return -1;
    if ((flg & 0x10) && gz_skip_cstr(f) != 0)  /* FCOMMENT */
        return -1;
    if ((flg & 0x02) && fseek(f, 2, SEEK_CUR) != 0) /* FHCRC */
        return -1;

    memset(&g->zs, 0, sizeof(g->zs));
    if (mz_inflateInit2(&g->zs, -MZ_DEFAULT_WINDOW_BITS) != MZ_OK)
        return -1;
    g->initialized = 1;
    return 0;
}

/* Returns bytes produced, 0 at end of stream, -1 on error. */
static long gz_read(OLGz *g, void *dst, size_t cap)
{
    unsigned char *out = dst;
    size_t got = 0;

    if (cap == 0)
        return 0;

    while (got < cap) {
        if (g->zs.avail_in == 0 && !g->stream_end) {
            size_t r = fread(g->in, 1, sizeof(g->in), g->f);
            if (r == 0) {
                g->error = 1;
                break;
            }
            g->zs.next_in = g->in;
            g->zs.avail_in = (mz_uint)r;
        }

        g->zs.next_out = out + got;
        g->zs.avail_out = (mz_uint)(cap - got);

        int rc = mz_inflate(&g->zs, MZ_NO_FLUSH);
        size_t made = (cap - got) - g->zs.avail_out;
        if (made) {
            g->crc = mz_crc32(g->crc, out + got, made);
            g->produced += made;
            got += made;
        }

        if (rc == MZ_STREAM_END) {
            g->stream_end = 1;
            break;
        }
        if (rc != MZ_OK && rc != MZ_BUF_ERROR) {
            g->error = 1;
            break;
        }
        if (rc == MZ_BUF_ERROR && made == 0) {
            if (g->zs.avail_in == 0)
                continue;              /* top of loop refills the input */
            g->error = 1;
            break;
        }
    }

    if (got == 0)
        return g->error ? -1 : 0;
    return (long)got;
}

static int gz_finish(OLGz *g)
{
    int ok = 1;

    if (!g->initialized)
        return 0;

    if (g->stream_end) {
        unsigned char t[8];
        size_t have = g->zs.avail_in < 8 ? g->zs.avail_in : 8;

        memcpy(t, g->zs.next_in, have);
        g->zs.avail_in -= (mz_uint)have;
        if (have < 8 && fread(t + have, 1, 8 - have, g->f) != 8 - have)
            ok = 0;
        if (ok) {
            unsigned long long crc = arc_le32(t);
            unsigned long long isize = arc_le32(t + 4);
            if ((g->crc & 0xffffffffUL) != crc)
                ok = 0;
            if ((g->produced & 0xffffffffULL) != isize)
                ok = 0;
        }
    }

    mz_inflateEnd(&g->zs);
    g->initialized = 0;
    return ok ? 0 : -1;
}

/* ------------------------------------------------------------------ */
/* Bzip2 reader                                                        */
/* ------------------------------------------------------------------ */

typedef struct {
    bz_stream zs;
    unsigned char in[ARC_IO_BUF];
    int initialized;
    int stream_end;
    int error;
} OLBz;

static int bz2_open(OLBz *b)
{
    memset(b, 0, sizeof(*b));
    if (BZ2_bzDecompressInit(&b->zs, 0, 0) != BZ_OK)
        return -1;
    b->initialized = 1;
    return 0;
}

/* Returns bytes produced, 0 at end of stream, -1 on error.  BZ2_bzDecompress
 * verifies every block CRC and the combined stream CRC before reporting
 * BZ_STREAM_END, so no trailer parsing is needed here. */
static long bz2_read(OLBz *b, FILE *f, void *dst, size_t cap)
{
    unsigned char *out = dst;
    size_t got = 0;
    int stalled = 0;

    if (cap == 0)
        return 0;
    if (b->error)
        return -1;
    if (b->stream_end)
        return 0;

    while (got < cap) {
        if (b->zs.avail_in == 0 && !b->stream_end) {
            size_t r = fread(b->in, 1, sizeof(b->in), f);
            if (r == 0) {
                b->error = 1;
                break;
            }
            b->zs.next_in = (char *)b->in;
            b->zs.avail_in = (unsigned int)r;
        }

        b->zs.next_out = (char *)out + got;
        b->zs.avail_out = (unsigned int)(cap - got);

        int rc = BZ2_bzDecompress(&b->zs);
        size_t made = (cap - got) - b->zs.avail_out;
        if (made)
            got += made;

        if (rc == BZ_STREAM_END) {
            b->stream_end = 1;
            break;
        }
        if (rc != BZ_OK) {
            b->error = 1;
            break;
        }
        if (made == 0) {
            if (b->zs.avail_in == 0)
                continue;              /* top of loop refills the input */
            if (++stalled > 1) {
                b->error = 1;
                break;
            }
        } else {
            stalled = 0;
        }
    }

    /* Report a failed stream even when earlier blocks produced bytes: the
     * caller must delete its partial output rather than accept it. */
    if (b->error)
        return -1;
    if (got == 0)
        return 0;
    return (long)got;
}

static int bz2_finish(OLBz *b)
{
    int ok = b->stream_end;

    if (!b->initialized)
        return 0;

    BZ2_bzDecompressEnd(&b->zs);
    b->initialized = 0;
    return ok ? 0 : -1;
}

/* ------------------------------------------------------------------ */
/* Sequential reader (plain file, gzip stream or bzip2 stream)         */
/* ------------------------------------------------------------------ */

typedef enum {
    RD_PLAIN = 0,
    RD_GZIP,
    RD_BZIP2
} OLReadKind;

typedef struct {
    FILE *f;
    OLReadKind kind;
    OLGz gz;
    OLBz bz;
    unsigned long long size_hint;      /* gzip ISIZE (0 when unknown)      */
    unsigned char peek[ARC_GZ_PEEK];
    size_t peek_len;
    size_t peek_pos;
} OLArcReader;

static int rd_init_plain(OLArcReader *r, const char *path)
{
    memset(r, 0, sizeof(*r));
    r->kind = RD_PLAIN;
    r->f = fopen(path, "rb");
    return r->f ? 0 : -1;
}

static int rd_init_gzip(OLArcReader *r, const char *path)
{
    long size;

    memset(r, 0, sizeof(*r));
    r->kind = RD_GZIP;
    r->f = fopen(path, "rb");
    if (!r->f)
        return -1;

    if (fseek(r->f, 0, SEEK_END) != 0 || (size = ftell(r->f)) < 18) {
        fclose(r->f);
        r->f = NULL;
        return -1;
    }
    if (fseek(r->f, size - 4, SEEK_SET) != 0) {
        fclose(r->f);
        r->f = NULL;
        return -1;
    }
    unsigned char tail[4];
    if (fread(tail, 1, 4, r->f) != 4 || fseek(r->f, 0, SEEK_SET) != 0) {
        fclose(r->f);
        r->f = NULL;
        return -1;
    }
    r->size_hint = arc_le32(tail);

    if (gz_open(&r->gz, r->f) != 0) {
        fclose(r->f);
        r->f = NULL;
        return -1;
    }
    return 0;
}

static int rd_init_bzip2(OLArcReader *r, const char *path)
{
    memset(r, 0, sizeof(*r));
    r->kind = RD_BZIP2;
    r->f = fopen(path, "rb");
    if (!r->f)
        return -1;

    if (bz2_open(&r->bz) != 0) {
        fclose(r->f);
        r->f = NULL;
        return -1;
    }
    return 0;
}

static int rd_read_some(OLArcReader *r, void *dst, size_t cap)
{
    unsigned char *out = dst;
    size_t got = 0;

    while (got < cap && r->peek_pos < r->peek_len) {
        out[got++] = r->peek[r->peek_pos++];
    }
    if (got == cap)
        return (int)got;

    if (r->kind == RD_GZIP) {
        long n = gz_read(&r->gz, out + got, cap - got);
        if (n < 0)
            return -1;
        got += (size_t)n;
    } else if (r->kind == RD_BZIP2) {
        long n = bz2_read(&r->bz, r->f, out + got, cap - got);
        if (n < 0)
            return -1;
        got += (size_t)n;
    } else {
        size_t n = fread(out + got, 1, cap - got, r->f);
        got += n;
    }
    return (int)got;
}

static int rd_read(OLArcReader *r, void *dst, size_t n)
{
    unsigned char *out = dst;
    size_t got = 0;

    while (got < n) {
        if (r->peek_pos < r->peek_len) {
            out[got++] = r->peek[r->peek_pos++];
            continue;
        }
        if (r->kind == RD_GZIP) {
            long g = gz_read(&r->gz, out + got, n - got);
            if (g <= 0)
                return -1;
            got += (size_t)g;
        } else if (r->kind == RD_BZIP2) {
            long g = bz2_read(&r->bz, r->f, out + got, n - got);
            if (g <= 0)
                return -1;
            got += (size_t)g;
        } else {
            size_t g = fread(out + got, 1, n - got, r->f);
            if (g == 0)
                return -1;
            got += g;
        }
    }
    return 0;
}

static int rd_peek(OLArcReader *r, void *dst, size_t n)
{
    unsigned char *out = dst;

    while (r->peek_len < n) {
        size_t want = n - r->peek_len;
        int got;

        if (r->kind == RD_GZIP) {
            long g = gz_read(&r->gz, r->peek + r->peek_len, want);
            if (g <= 0)
                break;
            got = (int)g;
        } else if (r->kind == RD_BZIP2) {
            long g = bz2_read(&r->bz, r->f, r->peek + r->peek_len, want);
            if (g <= 0)
                break;
            got = (int)g;
        } else {
            size_t g = fread(r->peek + r->peek_len, 1, want, r->f);
            if (g == 0)
                break;
            got = (int)g;
        }
        r->peek_len += (size_t)got;
    }

    size_t avail = r->peek_len - r->peek_pos;
    size_t copy = avail < n ? avail : n;
    if (copy)
        memcpy(out, r->peek + r->peek_pos, copy);
    return (int)copy;
}

static int rd_skip(OLArcReader *r, unsigned long long n)
{
    unsigned char buf[4096];

    if (r->peek_pos >= r->peek_len && r->kind == RD_PLAIN) {
        if (n > 0x7fffffffull)
            return -1;
        return fseek(r->f, (long)n, SEEK_CUR) == 0 ? 0 : -1;
    }

    while (n > 0) {
        size_t want = n < sizeof(buf) ? (size_t)n : sizeof(buf);
        if (rd_read(r, buf, want) != 0)
            return -1;
        n -= want;
    }
    return 0;
}

/* Verify the gzip trailer (CRC32 + ISIZE) or bzip2 stream CRC.  Safe to call
 * twice. */
static int rd_finish(OLArcReader *r)
{
    if (r->kind == RD_GZIP)
        return gz_finish(&r->gz);
    if (r->kind == RD_BZIP2)
        return bz2_finish(&r->bz);
    return 0;
}

static int rd_close(OLArcReader *r)
{
    int ok = rd_finish(r);

    if (r->f)
        fclose(r->f);
    r->f = NULL;
    return ok;
}

/* ------------------------------------------------------------------ */
/* TAR                                                                 */
/* ------------------------------------------------------------------ */

static int tar_number(const unsigned char *p, size_t n,
                      unsigned long long *out)
{
    size_t i = 0;
    unsigned long long v = 0;

    while (i < n && (p[i] == ' ' || p[i] == '\0'))
        i++;
    if (i == n) {
        *out = 0;
        return 0;
    }

    if (p[i] & 0x80) {                 /* GNU base-256 for large values */
        v = p[i] & 0x7f;
        for (i++; i < n; i++)
            v = (v << 8) | p[i];
        *out = v;
        return 0;
    }

    for (; i < n && p[i] >= '0' && p[i] <= '7'; i++)
        v = v * 8 + (unsigned long long)(p[i] - '0');
    while (i < n && (p[i] == ' ' || p[i] == '\0'))
        i++;
    if (i != n)
        return -1;
    *out = v;
    return 0;
}

static int tar_checksum_ok(const unsigned char *h)
{
    unsigned long long stored;
    unsigned long long usum = 0;
    long long ssum = 0;

    if (tar_number(h + 148, 8, &stored) != 0)
        return 0;

    for (size_t i = 0; i < ARC_TAR_BLOCK; i++) {
        unsigned char c = (i >= 148 && i < 156) ? ' ' : h[i];
        usum += c;
        ssum += (long long)(signed char)c;
    }
    return (unsigned long long)usum == stored ||
           (unsigned long long)ssum == stored;
}

static int tar_header_looks_like(const unsigned char *h)
{
    if (arc_zero_block(h, ARC_TAR_BLOCK))
        return 1;
    if (!memcmp(h + 257, "ustar", 5))
        return 1;
    if (!tar_checksum_ok(h))
        return 0;
    return h[156] == '0' || h[156] == '\0' || h[156] == '5' ||
           h[156] == '7' || h[156] == 'L' || h[156] == 'x';
}

static void tar_name_from_header(const unsigned char *h, char *out, size_t cap)
{
    const unsigned char *prefix = h + 345;

    if (!memcmp(h + 257, "ustar", 5) && prefix[0])
        snprintf(out, cap, "%.155s/%.100s", (const char *)prefix,
                 (const char *)h);
    else
        snprintf(out, cap, "%.100s", (const char *)h);
}

static int pax_apply(const unsigned char *data, size_t n, char *name,
                     size_t name_cap, unsigned long long *size,
                     int *have_size)
{
    size_t off = 0;

    while (off < n) {
        size_t start = off;
        size_t rec_len = 0;

        while (off < n && data[off] >= '0' && data[off] <= '9') {
            rec_len = rec_len * 10 + (size_t)(data[off] - '0');
            off++;
        }
        if (off >= n || data[off] != ' ')
            return -1;
        off++;
        if (rec_len == 0 || start + rec_len > n || rec_len <= off - start)
            return -1;

        size_t rec_end = start + rec_len;
        size_t eq = off;
        size_t value_end = rec_end;

        while (eq < rec_end && data[eq] != '=')
            eq++;
        if (value_end > off && data[value_end - 1] == '\n')
            value_end--;

        if (eq < rec_end) {
            size_t key_len = eq - off;
            size_t value_start = eq + 1;

            if (key_len == 4 && !memcmp(data + off, "path", 4)) {
                if (arc_copy_name(name, name_cap,
                                  (const char *)data + value_start,
                                  value_end - value_start) != 0)
                    return -1;
            } else if (key_len == 4 && !memcmp(data + off, "size", 4)) {
                unsigned long long v = 0;
                for (size_t i = value_start; i < value_end; i++) {
                    if (data[i] < '0' || data[i] > '9')
                        return -1;
                    v = v * 10 + (unsigned long long)(data[i] - '0');
                }
                *size = v;
                *have_size = 1;
            }
        }
        off = rec_end;
    }
    return 0;
}

/* Read the next real (regular file or directory) entry.  Extended headers
 * are consumed internally, and unsupported member types are skipped. */
static int tar_next(OLArcReader *r, OLArchiveEntry *e, int *eof)
{
    char long_name[OL_ARCHIVE_NAME_MAX];
    int have_long_name = 0;
    unsigned long long size_override = 0;
    int have_size = 0;

    for (;;) {
        unsigned char h[ARC_TAR_BLOCK];
        unsigned long long block_size;
        int type;

        if (rd_read(r, h, sizeof(h)) != 0)
            return -1;

        if (arc_zero_block(h, sizeof(h))) {
            *eof = 1;
            return 0;
        }
        if (!tar_checksum_ok(h))
            return -1;

        if (tar_number(h + 124, 12, &block_size) != 0)
            return -1;

        type = h[156];

        if (type == 'L') {             /* GNU long name */
            if (block_size == 0 || block_size >= sizeof(long_name))
                return -1;
            if (rd_read(r, long_name, (size_t)block_size) != 0)
                return -1;
            long_name[block_size] = '\0';
            have_long_name = 1;
            if (rd_skip(r, arc_round_block(block_size) - block_size) != 0)
                return -1;
            continue;
        }

        if (type == 'x' || type == 'g') { /* pax extended header */
            char *data;

            if (block_size == 0 || block_size > ARC_PAX_MAX)
                return -1;
            data = malloc((size_t)block_size);
            if (!data)
                return -1;
            if (rd_read(r, data, (size_t)block_size) != 0) {
                free(data);
                return -1;
            }
            if (type == 'x' && !have_long_name) {
                if (pax_apply((const unsigned char *)data, (size_t)block_size,
                              long_name, sizeof(long_name),
                              &size_override, &have_size) != 0) {
                    free(data);
                    return -1;
                }
            }
            free(data);
            if (rd_skip(r, arc_round_block(block_size) - block_size) != 0)
                return -1;
            continue;
        }

        if (type == 'K') {             /* GNU long link name */
            if (rd_skip(r, arc_round_block(block_size)) != 0)
                return -1;
            continue;
        }

        if (type != '0' && type != '\0' && type != '7' && type != '5') {
            /* symlink, hard link, device, fifo: never materialise these */
            if (rd_skip(r, arc_round_block(block_size)) != 0)
                return -1;
            continue;
        }

        char raw_name[OL_ARCHIVE_NAME_MAX];
        if (have_long_name)
            memcpy(raw_name, long_name, sizeof(raw_name));
        else
            tar_name_from_header(h, raw_name, sizeof(raw_name));

        if (arc_copy_name(e->name, sizeof(e->name), raw_name,
                          strlen(raw_name)) != 0)
            return -1;

        e->is_dir = type == '5';
        e->encrypted = 0;
        e->size = (have_size && !e->is_dir) ? size_override : block_size;
        if (e->is_dir)
            e->size = 0;
        *eof = 0;
        return 1;
    }
}

static int tar_list_stream(OLArcReader *r, OLArchiveEntryFn fn, void *user,
                           int *count)
{
    int n = 0;
    int ok = OL_ARCHIVE_OK;

    for (;;) {
        OLArchiveEntry e;
        int eof = 0;

        if (tar_next(r, &e, &eof) < 0) {
            ok = OL_ARCHIVE_ERR_CORRUPT;
            break;
        }
        if (eof)
            break;
        n++;
        if (fn)
            fn(user, &e);
        if (rd_skip(r, arc_round_block(e.size)) != 0) {
            ok = OL_ARCHIVE_ERR_IO;
            break;
        }
    }

    if (count)
        *count = n;
    return ok;
}

static int tar_write_file(OLArcReader *r, const OLArchiveEntry *e,
                          const char *path)
{
    char tmp[ARC_PATH_MAX];
    FILE *out;
    unsigned long long left = e->size;
    int ok = 1;

    if (strlen(path) + 6 > sizeof(tmp))
        return OL_ARCHIVE_ERR_PATH;
    snprintf(tmp, sizeof(tmp), "%s.part", path);

    if (arc_ensure_parent(path) != 0)
        return OL_ARCHIVE_ERR_PATH;

    out = fopen(tmp, "wb");
    if (!out) {
        (void)rd_skip(r, arc_round_block(e->size));
        return OL_ARCHIVE_ERR_IO;
    }

    while (left > 0) {
        unsigned char buf[8192];
        size_t want = left < sizeof(buf) ? (size_t)left : sizeof(buf);
        if (rd_read(r, buf, want) != 0) {
            ok = 0;
            break;
        }
        if (fwrite(buf, 1, want, out) != want) {
            ok = 0;
            break;
        }
        left -= want;
    }
    if (fclose(out) != 0)
        ok = 0;

    if (!ok) {
        remove(tmp);
        (void)rd_skip(r, arc_round_block(left));
        return OL_ARCHIVE_ERR_IO;
    }

    if (rd_skip(r, arc_round_block(e->size) - e->size) != 0) {
        remove(tmp);
        return OL_ARCHIVE_ERR_IO;
    }
    if (olfs_move(tmp, path) != 0) {
        remove(tmp);
        return OL_ARCHIVE_ERR_IO;
    }
    return OL_ARCHIVE_OK;
}

static int tar_extract_one(OLArcReader *r, const char *want, const char *dst,
                           int *found)
{
    for (;;) {
        OLArchiveEntry e;
        int eof = 0;
        int rc;

        if (tar_next(r, &e, &eof) < 0)
            return OL_ARCHIVE_ERR_CORRUPT;
        if (eof)
            return OL_ARCHIVE_ERR_NOTFOUND;

        if (strcmp(e.name, want) != 0) {
            if (rd_skip(r, arc_round_block(e.size)) != 0)
                return OL_ARCHIVE_ERR_IO;
            continue;
        }

        *found = 1;
        char path[ARC_PATH_MAX];
        if (!ol_archive_is_safe_name(e.name) ||
            arc_dst_for(dst, e.name, path, sizeof(path)) != 0)
            return OL_ARCHIVE_ERR_PATH;

        if (e.is_dir) {
            (void)olfs_mkdir_p(path);
            return OL_ARCHIVE_OK;
        }
        rc = tar_write_file(r, &e, path);
        return rc;
    }
}

static int tar_extract_all(OLArcReader *r, const char *dst_dir, int *count)
{
    int n = 0;

    for (;;) {
        OLArchiveEntry e;
        int eof = 0;
        char path[ARC_PATH_MAX];

        if (tar_next(r, &e, &eof) < 0)
            return OL_ARCHIVE_ERR_CORRUPT;
        if (eof)
            break;

        if (!ol_archive_is_safe_name(e.name) ||
            arc_join_path(path, sizeof(path), dst_dir, e.name) != 0)
            return OL_ARCHIVE_ERR_PATH;

        if (e.is_dir) {
            (void)olfs_mkdir_p(path);
            n++;
            continue;
        }

        int rc = tar_write_file(r, &e, path);
        if (rc != OL_ARCHIVE_OK)
            return rc;
        n++;
    }

    if (count)
        *count = n;
    return OL_ARCHIVE_OK;
}

/* ------------------------------------------------------------------ */
/* ZIP (miniz)                                                         */
/* ------------------------------------------------------------------ */

typedef struct {
    mz_zip_archive zip;
    FILE *f;
} OLZip;

static int zip_open(OLZip *z, const char *path)
{
    long size;

    memset(z, 0, sizeof(*z));
    z->f = fopen(path, "rb");
    if (!z->f)
        return OL_ARCHIVE_ERR_OPEN;

    if (fseek(z->f, 0, SEEK_END) != 0 || (size = ftell(z->f)) < 0 ||
        fseek(z->f, 0, SEEK_SET) != 0) {
        fclose(z->f);
        z->f = NULL;
        return OL_ARCHIVE_ERR_OPEN;
    }

    if (!mz_zip_reader_init_cfile(&z->zip, z->f, (mz_uint64)size, 0)) {
        fclose(z->f);
        z->f = NULL;
        return OL_ARCHIVE_ERR_FORMAT;
    }
    return OL_ARCHIVE_OK;
}

static void zip_close(OLZip *z)
{
    mz_zip_reader_end(&z->zip);
    if (z->f)
        fclose(z->f);
    z->f = NULL;
}

static int zip_entry(OLZip *z, mz_uint index, OLArchiveEntry *e)
{
    mz_zip_archive_file_stat st;

    if (!mz_zip_reader_file_stat(&z->zip, index, &st))
        return OL_ARCHIVE_ERR_CORRUPT;
    if (arc_copy_name(e->name, sizeof(e->name), st.m_filename,
                      strlen(st.m_filename)) != 0)
        return OL_ARCHIVE_ERR_TOOLARGE;

    e->size = st.m_uncomp_size;
    e->is_dir = st.m_is_directory ? 1 : 0;
    e->encrypted = st.m_is_encrypted ? 1 : 0;
    return OL_ARCHIVE_OK;
}

static int zip_list(const char *path, OLArchiveEntryFn fn, void *user,
                    int *count)
{
    OLZip z;
    mz_uint total;
    int rc = zip_open(&z, path);

    if (rc != OL_ARCHIVE_OK)
        return rc;

    total = mz_zip_reader_get_num_files(&z.zip);
    for (mz_uint i = 0; i < total; i++) {
        OLArchiveEntry e;
        rc = zip_entry(&z, i, &e);
        if (rc != OL_ARCHIVE_OK)
            break;
        if (fn)
            fn(user, &e);
    }

    zip_close(&z);
    if (rc == OL_ARCHIVE_OK && count)
        *count = (int)total;
    return rc;
}

static int zip_method_supported(OLZip *z, mz_uint index)
{
    mz_zip_archive_file_stat st;

    if (!mz_zip_reader_file_stat(&z->zip, index, &st))
        return 0;
    return st.m_method == 0 || st.m_method == MZ_DEFLATED;
}

static int zip_extract_index(OLZip *z, mz_uint index, const OLArchiveEntry *e,
                             const char *dst)
{
    char path[ARC_PATH_MAX];

    if (!ol_archive_is_safe_name(e->name))
        return OL_ARCHIVE_ERR_PATH;
    if (arc_dst_for(dst, e->name, path, sizeof(path)) != 0)
        return OL_ARCHIVE_ERR_PATH;

    if (e->is_dir) {
        (void)olfs_mkdir_p(path);
        return OL_ARCHIVE_OK;
    }
    if (e->encrypted || !zip_method_supported(z, index))
        return OL_ARCHIVE_ERR_UNSUPPORTED;
    if (arc_ensure_parent(path) != 0)
        return OL_ARCHIVE_ERR_PATH;

    char tmp[ARC_PATH_MAX];
    if (strlen(path) + 6 > sizeof(tmp))
        return OL_ARCHIVE_ERR_PATH;
    snprintf(tmp, sizeof(tmp), "%s.part", path);

    if (!mz_zip_reader_extract_to_file(&z->zip, index, tmp, 0)) {
        remove(tmp);
        return OL_ARCHIVE_ERR_CORRUPT;
    }
    if (olfs_move(tmp, path) != 0) {
        remove(tmp);
        return OL_ARCHIVE_ERR_IO;
    }
    return OL_ARCHIVE_OK;
}

static int zip_extract_entry(const char *path, const char *name,
                             const char *dst, int *found)
{
    OLZip z;
    int rc = zip_open(&z, path);

    if (rc != OL_ARCHIVE_OK)
        return rc;

    mz_uint total = mz_zip_reader_get_num_files(&z.zip);
    for (mz_uint i = 0; i < total; i++) {
        OLArchiveEntry e;

        rc = zip_entry(&z, i, &e);
        if (rc != OL_ARCHIVE_OK)
            break;
        if (strcmp(e.name, name) != 0)
            continue;

        *found = 1;
        rc = zip_extract_index(&z, i, &e, dst);
        break;
    }
    if (rc == OL_ARCHIVE_OK && !*found)
        rc = OL_ARCHIVE_ERR_NOTFOUND;

    zip_close(&z);
    return rc;
}

static int zip_extract_all(const char *path, const char *dst_dir, int *count)
{
    OLZip z;
    mz_uint total;
    int rc = zip_open(&z, path);
    int n = 0;

    if (rc != OL_ARCHIVE_OK)
        return rc;

    total = mz_zip_reader_get_num_files(&z.zip);
    for (mz_uint i = 0; i < total; i++) {
        OLArchiveEntry e;
        char out[ARC_PATH_MAX];

        rc = zip_entry(&z, i, &e);
        if (rc != OL_ARCHIVE_OK)
            break;
        if (!ol_archive_is_safe_name(e.name)) {
            rc = OL_ARCHIVE_ERR_PATH;
            break;
        }
        if (arc_join_path(out, sizeof(out), dst_dir, e.name) != 0) {
            rc = OL_ARCHIVE_ERR_PATH;
            break;
        }

        if (e.is_dir) {
            (void)olfs_mkdir_p(out);
            n++;
            continue;
        }
        if (e.encrypted || !zip_method_supported(&z, i)) {
            rc = OL_ARCHIVE_ERR_UNSUPPORTED;
            break;
        }
        if (arc_ensure_parent(out) != 0) {
            rc = OL_ARCHIVE_ERR_PATH;
            break;
        }

        char tmp[ARC_PATH_MAX];
        if (strlen(out) + 6 > sizeof(tmp)) {
            rc = OL_ARCHIVE_ERR_PATH;
            break;
        }
        snprintf(tmp, sizeof(tmp), "%s.part", out);
        if (!mz_zip_reader_extract_to_file(&z.zip, i, tmp, 0)) {
            remove(tmp);
            rc = OL_ARCHIVE_ERR_CORRUPT;
            break;
        }
        if (olfs_move(tmp, out) != 0) {
            remove(tmp);
            rc = OL_ARCHIVE_ERR_IO;
            break;
        }
        n++;
    }

    zip_close(&z);
    if (rc == OL_ARCHIVE_OK && count)
        *count = n;
    return rc;
}

/* ------------------------------------------------------------------ */
/* Single-file GZIP / BZIP2                                            */
/* ------------------------------------------------------------------ */

static int arc_has_suffix(const char *s, const char *suffix)
{
    size_t n = strlen(s);
    size_t m = strlen(suffix);

    if (n < m)
        return 0;
    s += n - m;
    for (size_t i = 0; i < m; i++) {
        if (tolower((unsigned char)s[i]) != tolower((unsigned char)suffix[i]))
            return 0;
    }
    return 1;
}

static void compressed_member_name(const char *path, int bz2, char *out,
                                   size_t cap)
{
    char base[OL_ARCHIVE_NAME_MAX];
    size_t len;

    if (olfs_path_base(path, base, sizeof(base)) != 0)
        strcpy(base, "data");
    len = strlen(base);

    if (bz2) {
        if (arc_has_suffix(base, ".tbz2"))
            len -= 5;
        else if (arc_has_suffix(base, ".tbz"))
            len -= 4;
        else if (arc_has_suffix(base, ".bz2"))
            len -= 4;
        base[len] = '\0';
    } else {
        if (arc_has_suffix(base, ".tgz"))
            len -= 4;
        else if (arc_has_suffix(base, ".gz"))
            len -= 3;
        base[len] = '\0';
    }

    if (base[0] == '\0')
        strcpy(base, "data");
    snprintf(out, cap, "%s", base);
}

static int gzip_list(const char *path, OLArcReader *r, OLArchiveEntryFn fn,
                     void *user, int *count)
{
    OLArchiveEntry e;

    memset(&e, 0, sizeof(e));
    compressed_member_name(path, 0, e.name, sizeof(e.name));
    e.size = r->size_hint;
    e.is_dir = 0;
    e.encrypted = 0;

    if (fn)
        fn(user, &e);
    if (count)
        *count = 1;
    return OL_ARCHIVE_OK;
}

/* A .bz2 stream has no uncompressed-size field, so listing counts the output
 * while stream-verifying the CRCs.  This is the same order of work as listing
 * a tar.bz2, where the tar headers must be decompressed anyway. */
static int bz2_list(const char *path, OLArcReader *r, OLArchiveEntryFn fn,
                    void *user, int *count)
{
    OLArchiveEntry e;
    unsigned char buf[8192];
    unsigned long long total = 0;

    for (;;) {
        int got = rd_read_some(r, buf, sizeof(buf));

        if (got < 0)
            return OL_ARCHIVE_ERR_CORRUPT;
        if (got == 0)
            break;
        total += (unsigned long long)got;
    }
    if (rd_finish(r) != 0)
        return OL_ARCHIVE_ERR_CORRUPT;

    memset(&e, 0, sizeof(e));
    compressed_member_name(path, 1, e.name, sizeof(e.name));
    e.size = total;
    e.is_dir = 0;
    e.encrypted = 0;

    if (fn)
        fn(user, &e);
    if (count)
        *count = 1;
    return OL_ARCHIVE_OK;
}

/* Extract the single stream member (gzip or bzip2) to `dst`. */
static int stream_extract(OLArcReader *r, const char *member, const char *dst,
                          int *found)
{
    char path[ARC_PATH_MAX];
    char tmp[ARC_PATH_MAX];
    FILE *out;
    int ok = 1;

    *found = 1;
    if (arc_dst_for(dst, member, path, sizeof(path)) != 0)
        return OL_ARCHIVE_ERR_PATH;
    if (strlen(path) + 6 > sizeof(tmp))
        return OL_ARCHIVE_ERR_PATH;
    snprintf(tmp, sizeof(tmp), "%s.part", path);

    if (arc_ensure_parent(path) != 0)
        return OL_ARCHIVE_ERR_PATH;

    out = fopen(tmp, "wb");
    if (!out)
        return OL_ARCHIVE_ERR_IO;

    for (;;) {
        unsigned char buf[8192];
        int got = rd_read_some(r, buf, sizeof(buf));
        if (got < 0) {
            ok = 0;
            break;
        }
        if (got == 0)
            break;
        if (fwrite(buf, 1, (size_t)got, out) != (size_t)got) {
            ok = 0;
            break;
        }
    }
    if (fclose(out) != 0)
        ok = 0;
    if (ok && rd_finish(r) != 0)
        ok = 0;

    if (!ok) {
        remove(tmp);
        return OL_ARCHIVE_ERR_CORRUPT;
    }
    if (olfs_move(tmp, path) != 0) {
        remove(tmp);
        return OL_ARCHIVE_ERR_IO;
    }
    return OL_ARCHIVE_OK;
}

static int stream_extract_all(OLArcReader *r, const char *path, int bz2,
                              const char *dst_dir, int *count)
{
    char name[OL_ARCHIVE_NAME_MAX];
    int found = 0;
    int rc;

    (void)olfs_mkdir_p(dst_dir);
    compressed_member_name(path, bz2, name, sizeof(name));
    rc = stream_extract(r, name, dst_dir, &found);
    if (rc == OL_ARCHIVE_OK && rd_close(r) != 0)
        rc = OL_ARCHIVE_ERR_CORRUPT;
    if (rc == OL_ARCHIVE_OK && count)
        *count = 1;
    return rc;
}

/* ------------------------------------------------------------------ */
/* Format detection and public dispatch                                */
/* ------------------------------------------------------------------ */

typedef enum {
    FMT_UNKNOWN = 0,
    FMT_ZIP,
    FMT_TAR,
    FMT_GZIP,
    FMT_RAR,
    FMT_7Z,
    FMT_BZIP2,
    FMT_XZ
} OLArcFormat;

static const unsigned char sig_7z[6] = { 0x37, 0x7a, 0xbc, 0xaf, 0x27, 0x1c };
static const unsigned char sig_xz[6] = { 0xfd, '7', 'z', 'X', 'Z', 0x00 };

static OLArcFormat detect_format(FILE *f, const char *path)
{
    unsigned char h[ARC_TAR_BLOCK];
    size_t n;
    const char *p;

    if (fseek(f, 0, SEEK_SET) != 0)
        return FMT_UNKNOWN;
    n = fread(h, 1, sizeof(h), f);
    (void)fseek(f, 0, SEEK_SET);

    if (n >= 4 && h[0] == 'P' && h[1] == 'K' &&
        ((h[2] == 3 && h[3] == 4) || (h[2] == 5 && h[3] == 6) ||
         (h[2] == 7 && h[3] == 8)))
        return FMT_ZIP;
    if (n >= 6 && !memcmp(h, "Rar!\x1a\x07", 6))
        return FMT_RAR;
    if (n >= 6 && !memcmp(h, sig_7z, 6))
        return FMT_7Z;
    if (n >= 3 && !memcmp(h, "BZh", 3))
        return FMT_BZIP2;
    if (n >= 6 && !memcmp(h, sig_xz, 6))
        return FMT_XZ;
    if (n >= 2 && h[0] == 0x1f && h[1] == 0x8b)
        return FMT_GZIP;
    if (n >= ARC_TAR_BLOCK && tar_header_looks_like(h))
        return FMT_TAR;

    /* An empty tar has no magic, so fall back to the extension. */
    p = strrchr(path, '.');
    if (p && (!strcmp(p, ".tar") || !strcmp(p, ".TAR")))
        return FMT_TAR;

    return FMT_UNKNOWN;
}

static const char *format_name(OLArcFormat fmt)
{
    switch (fmt) {
    case FMT_ZIP:   return "zip";
    case FMT_TAR:   return "tar";
    case FMT_GZIP:  return "gzip";
    case FMT_RAR:   return "rar";
    case FMT_7Z:    return "7z";
    case FMT_BZIP2: return "bzip2";
    case FMT_XZ:    return "xz";
    default:        return "unknown";
    }
}

const char *ol_archive_strerror(int code)
{
    switch (code) {
    case OL_ARCHIVE_OK:              return "ok";
    case OL_ARCHIVE_ERR_OPEN:        return "cannot open archive";
    case OL_ARCHIVE_ERR_FORMAT:      return "unrecognised archive format";
    case OL_ARCHIVE_ERR_UNSUPPORTED: return "unsupported archive format or compression";
    case OL_ARCHIVE_ERR_CORRUPT:     return "corrupt archive";
    case OL_ARCHIVE_ERR_IO:          return "archive I/O error";
    case OL_ARCHIVE_ERR_TOOLARGE:    return "archive metadata too large";
    case OL_ARCHIVE_ERR_NOTFOUND:    return "archive member not found";
    case OL_ARCHIVE_ERR_PATH:        return "unsafe archive member path";
    default:                         return "unknown archive error";
    }
}

static int reader_is_tar(OLArcReader *r)
{
    unsigned char h[ARC_TAR_BLOCK];
    int n = rd_peek(r, h, sizeof(h));

    if (n < ARC_TAR_BLOCK)
        return 0;
    return tar_header_looks_like(h);
}

static int open_compressed(OLArcReader *r, const char *path, OLArcFormat fmt)
{
    return fmt == FMT_BZIP2 ? rd_init_bzip2(r, path) : rd_init_gzip(r, path);
}

const char *ol_archive_detect(const char *path)
{
    FILE *f;
    OLArcFormat fmt;

    if (!path)
        return "unknown";
    f = fopen(path, "rb");
    if (!f)
        return "unknown";
    fmt = detect_format(f, path);
    fclose(f);

    if (fmt == FMT_GZIP || fmt == FMT_BZIP2) {
        OLArcReader r;

        if (open_compressed(&r, path, fmt) == 0) {
            int tar = reader_is_tar(&r);
            (void)rd_close(&r);
            if (tar)
                return fmt == FMT_BZIP2 ? "tar.bz2" : "tar.gz";
        }
        return format_name(fmt);
    }
    return format_name(fmt);
}

int ol_archive_list(const char *path, OLArchiveEntryFn fn, void *user,
                    int *count)
{
    FILE *f;
    OLArcFormat fmt;

    if (!path)
        return OL_ARCHIVE_ERR_OPEN;
    if (count)
        *count = 0;

    f = fopen(path, "rb");
    if (!f)
        return OL_ARCHIVE_ERR_OPEN;
    fmt = detect_format(f, path);
    fclose(f);

    if (fmt == FMT_ZIP)
        return zip_list(path, fn, user, count);
    if (fmt == FMT_RAR || fmt == FMT_7Z || fmt == FMT_XZ)
        return OL_ARCHIVE_ERR_UNSUPPORTED;
    if (fmt == FMT_GZIP || fmt == FMT_BZIP2) {
        OLArcReader r;
        int rc;

        if (open_compressed(&r, path, fmt) != 0)
            return OL_ARCHIVE_ERR_FORMAT;
        if (reader_is_tar(&r))
            rc = tar_list_stream(&r, fn, user, count);
        else if (fmt == FMT_BZIP2)
            rc = bz2_list(path, &r, fn, user, count);
        else
            rc = gzip_list(path, &r, fn, user, count);
        (void)rd_close(&r);
        return rc;
    }
    if (fmt == FMT_TAR) {
        OLArcReader r;
        int rc;

        if (rd_init_plain(&r, path) != 0)
            return OL_ARCHIVE_ERR_OPEN;
        rc = tar_list_stream(&r, fn, user, count);
        (void)rd_close(&r);
        return rc;
    }
    return OL_ARCHIVE_ERR_FORMAT;
}

int ol_archive_extract_entry(const char *path, const char *name,
                             const char *dst, int *found)
{
    FILE *f;
    OLArcFormat fmt;
    char member[OL_ARCHIVE_NAME_MAX];
    int local_found = 0;

    if (found)
        *found = 0;
    if (!path || !name || !dst)
        return OL_ARCHIVE_ERR_OPEN;
    if (arc_copy_name(member, sizeof(member), name, strlen(name)) != 0)
        return OL_ARCHIVE_ERR_PATH;
    if (!ol_archive_is_safe_name(member))
        return OL_ARCHIVE_ERR_PATH;

    f = fopen(path, "rb");
    if (!f)
        return OL_ARCHIVE_ERR_OPEN;
    fmt = detect_format(f, path);
    fclose(f);

    if (fmt == FMT_ZIP) {
        int rc = zip_extract_entry(path, member, dst, &local_found);
        if (found)
            *found = local_found;
        return rc;
    }
    if (fmt == FMT_RAR || fmt == FMT_7Z || fmt == FMT_XZ)
        return OL_ARCHIVE_ERR_UNSUPPORTED;
    if (fmt == FMT_GZIP || fmt == FMT_BZIP2) {
        OLArcReader r;
        char sname[OL_ARCHIVE_NAME_MAX];
        int bz2 = fmt == FMT_BZIP2;
        int rc;

        if (open_compressed(&r, path, fmt) != 0)
            return OL_ARCHIVE_ERR_FORMAT;
        if (reader_is_tar(&r)) {
            rc = tar_extract_one(&r, member, dst, &local_found);
            (void)rd_close(&r);
        } else {
            compressed_member_name(path, bz2, sname, sizeof(sname));
            if (strcmp(sname, member) != 0) {
                (void)rd_close(&r);
                return OL_ARCHIVE_ERR_NOTFOUND;
            }
            rc = stream_extract(&r, sname, dst, &local_found);
            (void)rd_close(&r);
        }
        if (found)
            *found = local_found;
        return rc;
    }
    if (fmt == FMT_TAR) {
        OLArcReader r;
        int rc;

        if (rd_init_plain(&r, path) != 0)
            return OL_ARCHIVE_ERR_OPEN;
        rc = tar_extract_one(&r, member, dst, &local_found);
        (void)rd_close(&r);
        if (found)
            *found = local_found;
        return rc;
    }
    return OL_ARCHIVE_ERR_FORMAT;
}

int ol_archive_extract_all(const char *path, const char *dst_dir, int *count)
{
    FILE *f;
    OLArcFormat fmt;

    if (count)
        *count = 0;
    if (!path || !dst_dir)
        return OL_ARCHIVE_ERR_OPEN;

    f = fopen(path, "rb");
    if (!f)
        return OL_ARCHIVE_ERR_OPEN;
    fmt = detect_format(f, path);
    fclose(f);

    if (fmt == FMT_ZIP) {
        (void)olfs_mkdir_p(dst_dir);
        return zip_extract_all(path, dst_dir, count);
    }
    if (fmt == FMT_RAR || fmt == FMT_7Z || fmt == FMT_XZ)
        return OL_ARCHIVE_ERR_UNSUPPORTED;
    if (fmt == FMT_GZIP || fmt == FMT_BZIP2) {
        OLArcReader r;
        int bz2 = fmt == FMT_BZIP2;
        int rc;

        if (open_compressed(&r, path, fmt) != 0)
            return OL_ARCHIVE_ERR_FORMAT;
        if (reader_is_tar(&r)) {
            (void)olfs_mkdir_p(dst_dir);
            rc = tar_extract_all(&r, dst_dir, count);
            (void)rd_close(&r);
            return rc;
        }
        return stream_extract_all(&r, path, bz2, dst_dir, count);
    }
    if (fmt == FMT_TAR) {
        OLArcReader r;
        int rc;

        if (rd_init_plain(&r, path) != 0)
            return OL_ARCHIVE_ERR_OPEN;
        (void)olfs_mkdir_p(dst_dir);
        rc = tar_extract_all(&r, dst_dir, count);
        (void)rd_close(&r);
        return rc;
    }
    return OL_ARCHIVE_ERR_FORMAT;
}
