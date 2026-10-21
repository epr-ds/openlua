/* OpenLua - portable PSMF / PMF container header parser.
 *
 * PSMF is the container used by PSP gameboots and movies: a 2048-byte
 * header followed by an MPEG program stream carrying H.264 video and
 * ATRAC3+ audio.  This module only validates and describes the header;
 * decoding stays behind the PSP sceMpeg backend so the parser can be
 * unit-tested on the host.
 */

#ifndef OPENLUA_PSMF_H
#define OPENLUA_PSMF_H

#include <stddef.h>
#include <stdint.h>

#define PSMF_HEADER_SIZE 2048u

typedef struct {
    uint32_t version;        /* ASCII version field, e.g. "0014" */
    uint32_t stream_offset;  /* byte offset of MPEG data (usually 2048) */
    uint32_t stream_size;    /* bytes of MPEG data after the header */
    uint64_t first_ts;       /* first presentation timestamp, 90 kHz */
    uint64_t last_ts;        /* last presentation timestamp, 90 kHz */
    uint32_t duration_90k;   /* last_ts - first_ts (0 when unknown) */
    int width;               /* decoded frame width, 0 when unavailable */
    int height;              /* decoded frame height, 0 when unavailable */
} OL_PsmfInfo;

/* Parse and validate a PSMF header.  `data` must point at the start of the
 * file and `len` is the number of header bytes available (at least 2048).
 * `file_size` is the total file size used to bound the stream range.
 * Returns 0 on success, -1 for a malformed/truncated header. */
int psmf_parse_header(const unsigned char *data, size_t len,
                      uint64_t file_size, OL_PsmfInfo *out);

#endif
