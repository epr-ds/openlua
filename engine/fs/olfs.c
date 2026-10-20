/* OpenLua - portable filesystem core (dual backend).
 *
 * PSP  : SCE Io (sceIoOpen/Dopen/Dread/Mkdir/Rmdir/Remove/Rename/Getstat)
 * host : POSIX (stat/opendir/fstat/rename/unlink/getcwd), gated by the
 *        compiler target so the same code builds under psp-gcc and gcc.
 *
 * Upper modules (files.c, ini.c, savedata.c) never touch a syscall - they
 * call this layer and are therefore unit-testable on the host unchanged.
 */

#include "olfs.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#if defined(__PSP__)
#include <pspiofilemgr.h>
#include <pspiofilemgr_dirent.h>
#include <pspiofilemgr_stat.h>
#include <pspiofilemgr_fcntl.h>
#else
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <unistd.h>
#endif

static int fill_entry_path(OLEntry *e, const char *dir, const char *name)
{
    size_t dlen = strlen(dir);
    size_t nlen = strlen(name);
    int sep = dlen > 0 && dir[dlen - 1] != '/';

    if (nlen >= sizeof(e->name))
        return -1;
    if (dlen + (size_t)sep + nlen + 1 > sizeof(e->path))
        return -1;

    memcpy(e->name, name, nlen + 1);
    memcpy(e->path, dir, dlen);
    if (sep)
        e->path[dlen++] = '/';
    memcpy(e->path + dlen, name, nlen + 1);
    return 0;
}

/* =============================================================== PSP === */
#if defined(__PSP__)

#define OL_O_RDONLY   PSP_O_RDONLY
#define OL_O_WRONLY   PSP_O_WRONLY
#define OL_O_CREAT    PSP_O_CREAT
#define OL_O_TRUNC    PSP_O_TRUNC
#define OL_O_APPEND   PSP_O_APPEND

#define OL_FD_INVALID 0xFFFFFFFFu

static int olfs_stat_ok(const char *path, unsigned int *attr, long *size)
{
    SceIoStat st;
    if (sceIoGetstat(path, &st) < 0)
        return -1;
    if (attr) *attr = st.st_attr;
    if (size) *size = (long)st.st_size;
    return 0;
}

int olfs_exists(const char *path)
{
    return olfs_stat_ok(path, NULL, NULL) == 0;
}

int olfs_isdir(const char *path)
{
    unsigned int a;
    if (olfs_stat_ok(path, &a, NULL) != 0)
        return 0;
    return FIO_S_ISDIR(a);
}

long olfs_size(const char *path)
{
    long s = -1;
    olfs_stat_ok(path, NULL, &s);
    return s;
}

int olfs_mkdir(const char *path)
{
    return sceIoMkdir(path, 0777) == 0 ? 0 : -1;
}

int olfs_mkdir_p(const char *path)
{
    char buf[512];
    size_t len;
    const char *colon;

    len = strlen(path);
    if (len == 0 || len >= sizeof(buf))
        return -1;
    if (path[0] == '/' && path[1] == 0)
        return 0;
    colon = strchr(path, ':');
    if (colon && (colon[1] == 0 ||
                  (colon[1] == '/' && colon[2] == 0)))
        return 0;

    memcpy(buf, path, len + 1);
    char *p = buf + 1;
    if (colon) {
        p = buf + (colon - path) + 1;
        if (*p == '/')
            p++;
    }
    for (; *p; p++) {
        if (*p == '/') {
            *p = 0;
            olfs_mkdir(buf);           /* best effort; parent first    */
            *p = '/';
        }
    }
    if (olfs_mkdir(buf) == 0)
        return 0;
    return olfs_isdir(buf) ? 0 : -1;
}

static int olfs_rdelete(const char *path)
{
    int fd, n, isdir = 0;
    SceIoStat st;
    SceIoDirent d;
    char child[512];

    if (sceIoGetstat(path, &st) < 0)
        return -1;
    if (FIO_S_ISDIR(st.st_attr))
        isdir = 1;
    if (!isdir)
        return sceIoRemove(path) == 0 ? 0 : -1;

    fd = sceIoDopen(path);
    if (fd < 0)
        return -1;
    while ((n = sceIoDread(fd, &d)) > 0 && d.d_name[0]) {
        if (!strcmp(d.d_name, ".") || !strcmp(d.d_name, ".."))
            continue;
        snprintf(child, sizeof(child), "%s/%s", path, d.d_name);
        olfs_rdelete(child);
    }
    sceIoDclose(fd);
    return sceIoRmdir(path) == 0 ? 0 : -1;
}

int olfs_delete(const char *path)
{
    return olfs_rdelete(path);
}

int olfs_move(const char *src, const char *dst)
{
    return sceIoRename(src, dst) == 0 ? 0 : -1;
}

int olfs_copyfile(const char *src, const char *dst)
{
    int rs, rw;
    char buf[8192];
    int n;

    rs = sceIoOpen(src, OL_O_RDONLY, 0);
    if (rs < 0)
        return -1;
    rw = sceIoOpen(dst, OL_O_WRONLY | OL_O_CREAT | OL_O_TRUNC, 0777);
    if (rw < 0)
    { sceIoClose(rs); return -1; }

    while ((n = sceIoRead(rs, buf, sizeof(buf))) > 0)
        if (sceIoWrite(rw, buf, n) != n)
        { sceIoClose(rs); sceIoClose(rw); return -1; }

    sceIoClose(rs);
    sceIoClose(rw);
    return 0;
}

int olfs_list(const char *dir, OLEntry *out, int n, int *outn, int *trunc)
{
    int fd, i = 0;
    SceIoDirent d;

    if (outn) *outn = 0;
    if (trunc) *trunc = 0;

    fd = sceIoDopen(dir);
    if (fd < 0)
        return -1;

    while (i < n) {
        if (sceIoDread(fd, &d) <= 0 || !d.d_name[0])
            break;
        if (!strcmp(d.d_name, ".") || !strcmp(d.d_name, ".."))
            continue;
        memset(&out[i], 0, sizeof(out[i]));
        if (fill_entry_path(&out[i], dir, d.d_name) != 0) {
            if (trunc) *trunc = 1;
            continue;
        }
        out[i].is_dir     = FIO_S_ISDIR(d.d_stat.st_attr);
        out[i].size       = out[i].is_dir ? 0 : (long)d.d_stat.st_size;
        out[i].attr       = d.d_stat.st_attr;
        i++;
    }
    while (sceIoDread(fd, &d) > 0 && d.d_name[0] &&
           (strcmp(d.d_name, ".") && strcmp(d.d_name, "..")))
        if (trunc) *trunc = 1;
    sceIoDclose(fd);

    if (outn) *outn = i;
    return 0;
}

static char s_cwd[256] = "app0:/";

int olfs_cwd(char *buf, unsigned int cap)
{
    size_t len;

    if (!buf || cap == 0)
        return -1;
    len = strlen(s_cwd);
    if (len + 1 > cap)
        return -1;
    memcpy(buf, s_cwd, len + 1);
    return 0;
}

int olfs_chdir(const char *path)
{
    int r = sceIoChdir(path);
    if (r == 0) {
        if (path[0] == '/' || strchr(path, ':')) {
            snprintf(s_cwd, sizeof(s_cwd), "%s", path);
        } else {
            size_t cur = strlen(s_cwd);
            int sep = cur > 0 && s_cwd[cur - 1] != '/';
            if (cur + (size_t)sep + strlen(path) >= sizeof(s_cwd)) {
                s_cwd[0] = 0;
            } else {
                if (sep)
                    s_cwd[cur++] = '/';
                memcpy(s_cwd + cur, path, strlen(path) + 1);
            }
        }
    }
    return r;
}

const char *olfs_cwd_current(void)
{
    return s_cwd;
}

/* =============================================================== host == */
#else

#define OL_FD_INVALID (-1)

static int stat_ok(const char *path, struct stat *st)
{
    return stat(path, st) == 0;
}

int olfs_exists(const char *path)
{
    struct stat st;
    return stat_ok(path, &st);
}

int olfs_isdir(const char *path)
{
    struct stat st;
    return stat_ok(path, &st) && S_ISDIR(st.st_mode);
}

long olfs_size(const char *path)
{
    struct stat st;
    if (!stat_ok(path, &st))
        return -1;
    return (long)st.st_size;
}

int olfs_mkdir(const char *path)
{
    return mkdir(path, 0755);
}

int olfs_mkdir_p(const char *path)
{
    char buf[512];
    size_t len = strlen(path);
    int rc;

    if (len == 0 || len >= sizeof(buf))
        return -1;
    memcpy(buf, path, len + 1);
    for (char *p = buf + (buf[0] == '/' ? 1 : 0); *p; p++) {
        if (*p == '/') {
            *p  = 0;
            mkdir(buf, 0755);    /* EEXIST is OK for parent dirs */
            *p  = '/';
        }
    }
    rc = mkdir(buf, 0755);
    if (rc != 0 && errno == EEXIST)
        return olfs_isdir(buf) ? 0 : -1;
    return rc == 0 ? 0 : -1;
}

static int rdelete(const char *path)
{
    struct stat st;
    if (!stat_ok(path, &st))
        return -1;
    if (!S_ISDIR(st.st_mode))
        return unlink(path);

    DIR *dir = opendir(path);
    if (!dir)
        return -1;
    struct dirent *e;
    char child[700];
    while ((e = readdir(dir)) != NULL) {
        if (!strcmp(e->d_name, ".") || !strcmp(e->d_name, ".."))
            continue;
        snprintf(child, sizeof(child), "%s/%s", path, e->d_name);
        rdelete(child);
    }
    closedir(dir);
    return rmdir(path);
}

int olfs_delete(const char *path)
{
    return rdelete(path) == 0 ? 0 : -1;
}

int olfs_move(const char *src, const char *dst)
{
    return rename(src, dst) == 0 ? 0 : -1;
}

int olfs_copyfile(const char *src, const char *dst)
{
    FILE *f = fopen(src, "rb");
    if (!f)
        return -1;
    FILE *t = fopen(dst, "wb");
    if (!t)
    { fclose(f); return -1; }

    char buf[8192];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), f)) > 0)
        if (fwrite(buf, 1, n, t) != n)
        { fclose(f); fclose(t); return -1; }

    fclose(f);
    fclose(t);
    return 0;
}

static int cmp_entry(const void *a, const void *b)
{
    return strcmp(((const OLEntry *)a)->name, ((const OLEntry *)b)->name);
}

int olfs_list(const char *dir, OLEntry *out, int n, int *outn, int *trunc)
{
    if (outn) *outn = 0;
    if (trunc) *trunc = 0;

    DIR *d = opendir(dir);
    if (!d)
        return -1;

    int i = 0;
    struct dirent *e;
    while (i < n && (e = readdir(d)) != NULL) {
        if (!strcmp(e->d_name, ".") || !strcmp(e->d_name, ".."))
            continue;
        memset(&out[i], 0, sizeof(out[i]));
        if (fill_entry_path(&out[i], dir, e->d_name) != 0) {
            if (trunc) *trunc = 1;
            continue;
        }
        struct stat st;
        if (stat(out[i].path, &st) == 0) {
            out[i].is_dir = S_ISDIR(st.st_mode);
            out[i].size   = out[i].is_dir ? 0 : (long)st.st_size;
            out[i].attr   = st.st_mode;
        }
        i++;
    }
    while ((e = readdir(d)) != NULL)
        if (strcmp(e->d_name, ".") && strcmp(e->d_name, ".."))
            if (trunc) *trunc = 1;

    closedir(d);
    if (i > 1)
        qsort(out, (size_t)i, sizeof(OLEntry), cmp_entry);
    if (outn) *outn = i;
    return 0;
}

static char s_cwd[512] = "";

int olfs_cwd(char *buf, unsigned int cap)
{
    if (!buf || cap == 0)
        return -1;
    if (!getcwd(s_cwd, sizeof(s_cwd)))
        return -1;
    if (cap < strlen(s_cwd) + 1)
        return -1;
    memcpy(buf, s_cwd, strlen(s_cwd) + 1);
    return 0;
}

int olfs_chdir(const char *path)
{
    int r = chdir(path);
    if (r == 0) {
        char b[512];
        if (getcwd(b, sizeof(b)))
            memcpy(s_cwd, b, strlen(b) + 1);
    }
    return r;
}

const char *olfs_cwd_current(void)
{
    return s_cwd;
}

#endif

/* =================================================== pure path logic ==== */

int olfs_path_dir(const char *path, char *out, unsigned int cap)
{
    const char *last = strrchr(path, '/');
    if (!last) {
        if (cap < 1) return -1;
        out[0] = 0;
        return 0;
    }
    unsigned int n = (unsigned int)(last - path);
    if (n == 0) n = 1;                   /* root: keep '/'               */
    if (n + 1 > cap) return -1;
    memcpy(out, path, n);
    out[n] = 0;
    return 0;
}

int olfs_path_base(const char *path, char *out, unsigned int cap)
{
    const char *last = strrchr(path, '/');
    const char *base = last ? last + 1 : path;
    unsigned int n = (unsigned int)strlen(base);
    if (n + 1 > cap) return -1;
    memcpy(out, base, n + 1);
    return 0;
}

int olfs_path_ext(const char *path, char *out, unsigned int cap)
{
    const char *last = strrchr(path, '/');
    const char *base = last ? last + 1 : path;
    const char *dot  = strrchr(base, '.');
    if (!dot || dot == base) {
        if (cap < 1) return -1;
        out[0] = 0;
        return 0;
    }
    unsigned int n = (unsigned int)strlen(dot + 1);
    if (n + 1 > cap) return -1;
    memcpy(out, dot + 1, n + 1);
    return 0;
}
