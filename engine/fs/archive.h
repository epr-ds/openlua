/* OpenLua - compressed archive reader.
 *
 * Reads the archive formats that OneLua's files.scan/extract/file API
 * exposes, plus the extra formats that are cheap to support:
 *
 *   .zip                 stored and deflate entries (miniz)
 *   .tar                 ustar, GNU long names and pax extended headers
 *   .tar.gz / .tgz       gzip-compressed tar (gzip header parsed locally)
 *   .tar.bz2 / .tbz2     bzip2-compressed tar (libbzip2)
 *   .gz                  a single gzip-compressed file
 *   .bz2                 a single bzip2-compressed file
 *
 * RAR, 7z and xz are recognised so callers get a precise
 * "unsupported" result instead of a misleading "corrupt archive" one.  The
 * parser is pure C99 over stdio, like engine/data/json.c, and is host-testable.
 */

#ifndef OPENLUA_ARCHIVE_H
#define OPENLUA_ARCHIVE_H

#include <stddef.h>

#define OL_ARCHIVE_NAME_MAX 512

typedef enum {
    OL_ARCHIVE_OK = 0,
    OL_ARCHIVE_ERR_OPEN = -1,        /* file missing/unreadable           */
    OL_ARCHIVE_ERR_FORMAT = -2,      /* not a recognised archive          */
    OL_ARCHIVE_ERR_UNSUPPORTED = -3, /* rar/7z/xz/encrypted/zip method    */
    OL_ARCHIVE_ERR_CORRUPT = -4,     /* bad header, checksum or payload   */
    OL_ARCHIVE_ERR_IO = -5,          /* read/write failed                 */
    OL_ARCHIVE_ERR_TOOLARGE = -6,    /* header/metadata limit exceeded    */
    OL_ARCHIVE_ERR_NOTFOUND = -7,    /* requested member is absent        */
    OL_ARCHIVE_ERR_PATH = -8         /* member name is unsafe             */
} OLArchiveResult;

typedef struct {
    char          name[OL_ARCHIVE_NAME_MAX];
    unsigned long long size;         /* uncompressed bytes                */
    int           is_dir;
    int           encrypted;
} OLArchiveEntry;

typedef void (*OLArchiveEntryFn)(void *user, const OLArchiveEntry *entry);

/* Human-readable description for a result code ("ok", "bad archive", ...). */
const char *ol_archive_strerror(int code);

/* Detected container type: "zip", "tar", "tar.gz", "gzip", "tar.bz2",
 * "bzip2", "rar", "7z", "xz" or "unknown".  Never returns NULL. */
const char *ol_archive_detect(const char *path);

/* Enumerate members.  `entry` names are normalised to forward slashes.
 * The callback is optional; `count` receives the number of members. */
int ol_archive_list(const char *path, OLArchiveEntryFn fn, void *user,
                    int *count);

/* Extract one member to `dst`.
 *   - If `dst` is an existing directory or ends with '/', the member's
 *     base name is appended; otherwise `dst` is the destination file.
 *   - `found` (optional) is set to 1 when the member existed.
 * Member names are validated against traversal ("../", absolute paths,
 * drive prefixes) before anything is written. */
int ol_archive_extract_entry(const char *path, const char *name,
                             const char *dst, int *found);

/* Extract every directory and regular file below `dst_dir`.  Missing
 * directories are created.  Extraction stops on the first error. */
int ol_archive_extract_all(const char *path, const char *dst_dir, int *count);

/* 1 when `name` is a safe relative member name.  Exposed for tests. */
int ol_archive_is_safe_name(const char *name);

#endif
