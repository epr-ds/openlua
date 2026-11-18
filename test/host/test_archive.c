/* Host tests for engine/fs/archive.c (ZIP/TAR/TAR.GZ/GZIP). */

#include "engine/fs/archive.h"
#include "engine/fs/olfs.h"

#include "miniz.h"
#include "miniz_zip.h"

#include <stdio.h>
#include <stdlib.h>
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

#define MAX_SEEN 32

typedef struct {
    OLArchiveEntry entries[MAX_SEEN];
    int count;
} Seen;

static void collect(void *user, const OLArchiveEntry *e)
{
    Seen *s = user;

    if (s->count < MAX_SEEN)
        s->entries[s->count] = *e;
    s->count++;
}

static const OLArchiveEntry *find_entry(const Seen *s, const char *name)
{
    for (int i = 0; i < s->count && i < MAX_SEEN; i++)
        if (!strcmp(s->entries[i].name, name))
            return &s->entries[i];
    return NULL;
}

static int read_file(const char *path, char *buf, size_t cap)
{
    FILE *f = fopen(path, "rb");
    size_t n;

    if (!f)
        return -1;
    n = fread(buf, 1, cap - 1, f);
    fclose(f);
    buf[n] = '\0';
    return (int)n;
}

static int write_file(const char *path, const void *data, size_t len)
{
    FILE *f = fopen(path, "wb");
    int ok;

    if (!f)
        return -1;
    ok = fwrite(data, 1, len, f) == len;
    if (fclose(f) != 0)
        ok = 0;
    return ok ? 0 : -1;
}

static int make_zip(const char *path, const char *name, const char *data)
{
    mz_zip_archive z;
    int ok = 0;

    memset(&z, 0, sizeof(z));
    if (!mz_zip_writer_init_file(&z, path, 0))
        return -1;
    if (mz_zip_writer_add_mem(&z, name, data, strlen(data),
                              MZ_DEFAULT_LEVEL) &&
        mz_zip_writer_finalize_archive(&z))
        ok = 1;
    mz_zip_writer_end(&z);
    return ok ? 0 : -1;
}

static void test_safe_names(void)
{
    check(ol_archive_is_safe_name("hello.txt"), "safe simple name");
    check(ol_archive_is_safe_name("dir/nested.txt"), "safe nested name");
    check(ol_archive_is_safe_name("dir/"), "safe trailing separator");
    check(!ol_archive_is_safe_name(""), "empty name rejected");
    check(!ol_archive_is_safe_name("/etc/passwd"), "absolute path rejected");
    check(!ol_archive_is_safe_name("../evil"), "leading traversal rejected");
    check(!ol_archive_is_safe_name("a/../../evil"), "nested traversal rejected");
    check(!ol_archive_is_safe_name("a\\..\\evil"), "backslash traversal rejected");
    check(!ol_archive_is_safe_name("C:/evil"), "drive prefix rejected");
    check(!ol_archive_is_safe_name("ms0:/evil"), "psp device prefix rejected");
    check(!ol_archive_is_safe_name("a//b"), "empty component rejected");
    check(ol_archive_is_safe_name("a..b/c"), "dots inside a name are allowed");
}

static void test_zip(const char *fixtures, const char *work)
{
    char src[512];
    char path[512];
    Seen s = {0};
    int count = 0;
    int found = 0;
    char body[128];
    int rc;

    snprintf(src, sizeof(src), "%s/archive.zip", fixtures);

    check(!strcmp(ol_archive_detect(src), "zip"), "zip detected");
    rc = ol_archive_list(src, collect, &s, &count);
    check(rc == OL_ARCHIVE_OK, "zip listing succeeds");
    check(count == 6 && s.count == 6, "zip member count");

    const OLArchiveEntry *e = find_entry(&s, "hello.txt");
    check(e != NULL && e->size == 16 && !e->is_dir, "zip hello size");
    e = find_entry(&s, "dir/nested.txt");
    check(e != NULL && e->size == 14, "zip nested size");
    e = find_entry(&s, "stored.txt");
    check(e != NULL && e->size == 18, "zip stored size");
    e = find_entry(&s, "empty.txt");
    check(e != NULL && e->size == 0, "zip empty size");
    e = find_entry(&s, "dir");
    check(e != NULL && e->is_dir, "zip directory entry");

    snprintf(path, sizeof(path), "%s/one.txt", work);
    rc = ol_archive_extract_entry(src, "hello.txt", path, &found);
    check(rc == OL_ARCHIVE_OK && found, "zip extractfile succeeds");
    check(read_file(path, body, sizeof(body)) == 16 &&
          !strcmp(body, "Hello, OpenLua!\n"), "zip extracted bytes");

    rc = ol_archive_extract_entry(src, "missing.txt", path, &found);
    check(rc == OL_ARCHIVE_ERR_NOTFOUND && !found, "zip missing member");

    snprintf(path, sizeof(path), "%s/all_zip", work);
    rc = ol_archive_extract_all(src, path, &count);
    check(rc == OL_ARCHIVE_OK && count == 6, "zip extract all");
    snprintf(path, sizeof(path), "%s/all_zip/dir/nested.txt", work);
    check(read_file(path, body, sizeof(body)) == 14 &&
          !strcmp(body, "nested member\n"), "zip nested extracted");
}

static void test_tar(const char *fixtures, const char *work)
{
    char src[512];
    char path[512];
    Seen s = {0};
    int count = 0;
    int rc;
    char body[128];

    snprintf(src, sizeof(src), "%s/archive.tar", fixtures);
    check(!strcmp(ol_archive_detect(src), "tar"), "tar detected");
    rc = ol_archive_list(src, collect, &s, &count);
    check(rc == OL_ARCHIVE_OK, "tar listing succeeds");
    check(count == 5 && s.count == 5, "tar member count");

    const OLArchiveEntry *e = find_entry(&s, "hello.txt");
    check(e != NULL && e->size == 16, "tar hello size");
    e = find_entry(&s, "dir");
    check(e != NULL && e->is_dir, "tar directory entry");
    e = find_entry(&s, "very/long/path/that/needs/gnu/long/name/support/"
                        "abcdefghijklmnopqrstuvwxyz0123456789.txt");
    check(e != NULL && e->size == 12, "tar GNU long name");

    snprintf(path, sizeof(path), "%s/all_tar", work);
    rc = ol_archive_extract_all(src, path, &count);
    check(rc == OL_ARCHIVE_OK && count == 5, "tar extract all");
    snprintf(path, sizeof(path), "%s/all_tar/hello.txt", work);
    check(read_file(path, body, sizeof(body)) == 16, "tar hello extracted");
}

static void test_tgz(const char *fixtures, const char *work)
{
    char src[512];
    char path[512];
    Seen s = {0};
    int count = 0;
    int rc;
    char body[128];

    snprintf(src, sizeof(src), "%s/archive.tgz", fixtures);
    check(!strcmp(ol_archive_detect(src), "tar.gz"), "tar.gz detected");
    rc = ol_archive_list(src, collect, &s, &count);
    check(rc == OL_ARCHIVE_OK, "tar.gz listing succeeds");
    check(count == 5 && s.count == 5, "tar.gz member count");
    check(find_entry(&s, "hello.txt") != NULL, "tar.gz hello listed");

    snprintf(path, sizeof(path), "%s/all_tgz", work);
    rc = ol_archive_extract_all(src, path, &count);
    check(rc == OL_ARCHIVE_OK && count == 5, "tar.gz extract all");
    snprintf(path, sizeof(path), "%s/all_tgz/dir/nested.txt", work);
    check(read_file(path, body, sizeof(body)) == 14, "tar.gz nested extracted");
}

static void test_tbz2(const char *fixtures, const char *work)
{
    char src[512];
    char path[512];
    Seen s = {0};
    int count = 0;
    int rc;
    char body[128];

    snprintf(src, sizeof(src), "%s/archive.tar.bz2", fixtures);
    check(!strcmp(ol_archive_detect(src), "tar.bz2"), "tar.bz2 detected");
    rc = ol_archive_list(src, collect, &s, &count);
    check(rc == OL_ARCHIVE_OK, "tar.bz2 listing succeeds");
    check(count == 5 && s.count == 5, "tar.bz2 member count");
    check(find_entry(&s, "hello.txt") != NULL, "tar.bz2 hello listed");

    snprintf(path, sizeof(path), "%s/all_tbz2", work);
    rc = ol_archive_extract_all(src, path, &count);
    check(rc == OL_ARCHIVE_OK && count == 5, "tar.bz2 extract all");
    snprintf(path, sizeof(path), "%s/all_tbz2/dir/nested.txt", work);
    check(read_file(path, body, sizeof(body)) == 14, "tar.bz2 nested extracted");
}

static void test_gzip(const char *fixtures, const char *work)
{
    char src[512];
    char path[512];
    Seen s = {0};
    int count = 0;
    int found = 0;
    int rc;
    char body[64];

    snprintf(src, sizeof(src), "%s/hello.txt.gz", fixtures);
    check(!strcmp(ol_archive_detect(src), "gzip"), "gzip detected");
    rc = ol_archive_list(src, collect, &s, &count);
    check(rc == OL_ARCHIVE_OK && count == 1, "gzip single member");
    check(s.count == 1 && !strcmp(s.entries[0].name, "hello.txt") &&
          s.entries[0].size == 13, "gzip member name and size");

    snprintf(path, sizeof(path), "%s/gzip_out", work);
    rc = ol_archive_extract_entry(src, "hello.txt", path, &found);
    check(rc == OL_ARCHIVE_OK && found, "gzip extract file");
    rc = ol_archive_list(path, NULL, NULL, NULL);
    check(rc == OL_ARCHIVE_ERR_OPEN || rc == OL_ARCHIVE_ERR_FORMAT,
          "gzip output is a file, not an archive");
    check(read_file(path, body, sizeof(body)) == 13 &&
          !strcmp(body, "Hello, gzip!\n"), "gzip extracted bytes");

    snprintf(path, sizeof(path), "%s/gzip_all", work);
    rc = ol_archive_extract_all(src, path, &count);
    check(rc == OL_ARCHIVE_OK && count == 1, "gzip extract all");
}

static void test_bz2(const char *fixtures, const char *work)
{
    char src[512];
    char path[512];
    Seen s = {0};
    int count = 0;
    int found = 0;
    int rc;
    char body[64];

    snprintf(src, sizeof(src), "%s/hello.txt.bz2", fixtures);
    check(!strcmp(ol_archive_detect(src), "bzip2"), "bzip2 detected");
    rc = ol_archive_list(src, collect, &s, &count);
    check(rc == OL_ARCHIVE_OK && count == 1, "bzip2 single member");
    check(s.count == 1 && !strcmp(s.entries[0].name, "hello.txt") &&
          s.entries[0].size == 14, "bzip2 member name and size");

    snprintf(path, sizeof(path), "%s/bz2_out", work);
    rc = ol_archive_extract_entry(src, "hello.txt", path, &found);
    check(rc == OL_ARCHIVE_OK && found, "bzip2 extract file");
    check(read_file(path, body, sizeof(body)) == 14 &&
          !strcmp(body, "Hello, bzip2!\n"), "bzip2 extracted bytes");

    snprintf(path, sizeof(path), "%s/bz2_all", work);
    rc = ol_archive_extract_all(src, path, &count);
    check(rc == OL_ARCHIVE_OK && count == 1, "bzip2 extract all");
    snprintf(path, sizeof(path), "%s/bz2_all/hello.txt", work);
    check(read_file(path, body, sizeof(body)) == 14, "bzip2 extract all bytes");
}

static void test_rejections(const char *dir)
{
    char path[512];
    char outside[512];
    int found = 0;
    int rc;
    int count = 0;

    snprintf(outside, sizeof(outside), "%s/evil.txt", dir);
    snprintf(path, sizeof(path), "%s/evil.zip", dir);
    check(make_zip(path, "../evil.txt", "bad") == 0, "create traversal zip");
    rc = ol_archive_extract_all(path, dir, &count);
    check(rc == OL_ARCHIVE_ERR_PATH, "traversal extract_all rejected");
    check(!olfs_exists(outside), "traversal wrote nothing");

    found = 7;
    rc = ol_archive_extract_entry(path, "../evil.txt", outside, &found);
    check(rc == OL_ARCHIVE_ERR_PATH, "traversal extract_entry rejected");
    check(!olfs_exists(outside), "traversal extract_entry wrote nothing");

    snprintf(path, sizeof(path), "%s/fake.rar", dir);
    write_file(path, "Rar!\x1a\x07\x00", 7);
    check(!strcmp(ol_archive_detect(path), "rar"), "rar detected");
    check(ol_archive_list(path, NULL, NULL, NULL) ==
          OL_ARCHIVE_ERR_UNSUPPORTED, "rar unsupported");
    check(ol_archive_extract_all(path, dir, NULL) ==
          OL_ARCHIVE_ERR_UNSUPPORTED, "rar extract unsupported");

    snprintf(path, sizeof(path), "%s/fake.7z", dir);
    {
        static const unsigned char sig[6] = { 0x37, 0x7a, 0xbc, 0xaf, 0x27, 0x1c };
        write_file(path, sig, sizeof(sig));
    }
    check(!strcmp(ol_archive_detect(path), "7z"), "7z detected");
    check(ol_archive_list(path, NULL, NULL, NULL) ==
          OL_ARCHIVE_ERR_UNSUPPORTED, "7z unsupported");

    snprintf(path, sizeof(path), "%s/not.bin", dir);
    write_file(path, "just some bytes\n", 16);
    check(!strcmp(ol_archive_detect(path), "unknown"), "unknown detected");
    check(ol_archive_list(path, NULL, NULL, NULL) ==
          OL_ARCHIVE_ERR_FORMAT, "unknown rejected");
}

static void test_corrupt(const char *fixtures, const char *work)
{
    char src[512];
    char path[512];
    unsigned char buf[64];
    FILE *f;
    int n;
    int rc;
    int found = 0;

    snprintf(src, sizeof(src), "%s/hello.txt.gz", fixtures);
    f = fopen(src, "rb");
    check(f != NULL, "open gzip fixture");
    if (!f)
        return;
    n = (int)fread(buf, 1, sizeof(buf), f);
    fclose(f);
    check(n == 33, "gzip fixture size");
    buf[n / 2] ^= 0xff;

    snprintf(path, sizeof(path), "%s/corrupt.gz", work);
    write_file(path, buf, (size_t)n);
    rc = ol_archive_extract_entry(path, "corrupt", work, &found);
    check(rc == OL_ARCHIVE_ERR_CORRUPT, "gzip CRC/deflate error detected");

    snprintf(src, sizeof(src), "%s/hello.txt.bz2", fixtures);
    f = fopen(src, "rb");
    check(f != NULL, "open bzip2 fixture");
    if (!f)
        return;
    n = (int)fread(buf, 1, sizeof(buf), f);
    fclose(f);
    check(n == 57, "bzip2 fixture size");
    buf[n / 2] ^= 0xff;

    snprintf(path, sizeof(path), "%s/corrupt.bz2", work);
    write_file(path, buf, (size_t)n);
    rc = ol_archive_list(path, NULL, NULL, NULL);
    check(rc == OL_ARCHIVE_ERR_CORRUPT, "corrupt bzip2 rejected");

    unsigned char bad_tar[512];
    memset(bad_tar, 0, sizeof(bad_tar));
    memcpy(bad_tar + 257, "ustar", 5);  /* magic without a valid checksum */
    snprintf(path, sizeof(path), "%s/corrupt.tar", work);
    write_file(path, bad_tar, sizeof(bad_tar));
    check(ol_archive_list(path, NULL, NULL, NULL) ==
          OL_ARCHIVE_ERR_CORRUPT, "corrupt tar rejected");
}

int main(int argc, char **argv)
{
    const char *fixtures = argc > 1 ? argv[1] : "test/host/fixtures";
    const char *work = "archivework";

    (void)olfs_delete(work);
    (void)olfs_mkdir_p(work);

    test_safe_names();
    test_zip(fixtures, work);
    test_tar(fixtures, work);
    test_tgz(fixtures, work);
    test_tbz2(fixtures, work);
    test_gzip(fixtures, work);
    test_bz2(fixtures, work);
    test_rejections(work);
    test_corrupt(fixtures, work);

    printf("archive: %d checks, %d failed\n", checks, fails);
    return fails ? 1 : 0;
}
