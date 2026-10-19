/* OpenLua - portable filesystem core.
 *
 * A single small API over the PS2/PSP-style device paths ("ms0:/...",
 * "app0:/...") with two backends:
 *
 *   PSP  : SCE Io (sceIoOpen/sceIoDread/sceIoMkdir/...)
 *   host : POSIX (stat/opendir/fstat/...) via the local C library, so the
 *          same logic can be unit-tested without PSP toolchain or hardware.
 *
 * Upper modules (files, ini, savedata) use only this layer - no syscall
 * directly - and stay pure logic on either platform.
 */

#ifndef OL_OLFS_H
#define OL_OLFS_H

#include <stddef.h>

typedef struct {
    char  name[128];
    char  path[256];      /* full path (<dir>/<name>)                   */
    long  size;           /* bytes; 0 for directories                   */
    int   is_dir;
    unsigned int attr; /* platform permission bits (FIO_S_* / st_mode) */
} OLEntry;

/* ---- core state ------------------------------------------------------- */
int  olfs_exists(const char *path);
int  olfs_isdir(const char *path);
long olfs_size(const char *path);          /* -1 on error                */

int  olfs_mkdir(const char *path);
int  olfs_mkdir_p(const char *path);       /* full chain                 */
int  olfs_rmdir(const char *path);         /* empty dir only             */
int  olfs_delete(const char *path);        /* file or whole tree         */
int  olfs_move(const char *src, const char *dst);
int  olfs_copyfile(const char *src, const char *dst);

/* ---- directory listing ------------------------------------------------- */
/* 0 on success; *out receives at most n entries, sorted by name,
 * *outn = actual count, *trunc = 1 if more remained unread. */
int olfs_list(const char *dir, OLEntry *out, int n, int *outn, int *trunc);

/* ---- working directory -------------------------------------------------- */
int olfs_cwd(char *buf, unsigned int cap);      /* 0 on success          */
int olfs_chdir(const char *path);
const char *olfs_cwd_current(void);             /* last known cwd (cache) */

/* ---- path helpers (pure string logic) ---------------------------------- */
int olfs_path_dir(const char *path, char *out, unsigned int cap);  /* minus tail */
int olfs_path_base(const char *path, char *out, unsigned int cap); /* tail only  */
int olfs_path_ext(const char *path, char *out, unsigned int cap);  /* "lua" or "" */

#endif
