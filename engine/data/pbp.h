/* OpenLua - bounds-checked PBP and PARAM.SFO parsing.
 *
 * The parser is pure: callers pass a header/buffer and get offsets, sizes or
 * entries back.  File I/O and Lua integration live in engine/sys/game.c, so
 * malformed inputs can be tested on the host without a PSP.
 */

#ifndef OL_PBP_H
#define OL_PBP_H

#include <stddef.h>

#define PBP_HEADER_SIZE   40
#define PBP_SECTION_COUNT 8

/* Section indices in a PBP header. */
enum {
    PBP_SECTION_SFO = 0,
    PBP_SECTION_ICON0,
    PBP_SECTION_ICON1,
    PBP_SECTION_PIC0,
    PBP_SECTION_PIC1,
    PBP_SECTION_SND0,
    PBP_SECTION_DATA_PSP,
    PBP_SECTION_DATA_PSAR
};

typedef struct {
    unsigned int offset[PBP_SECTION_COUNT];
    unsigned int size[PBP_SECTION_COUNT];
} OL_Pbp;

/* Parse the first PBP_HEADER_SIZE bytes.  `file_size` is the real file size
 * (from a stat/seek) and is used to bound every section.  Returns 1 on
 * success, 0 for a bad magic/truncated header/out-of-range or unsorted
 * offsets.  Absent sections have offset == 0 and size == 0. */
int pbp_parse(const unsigned char *header, size_t header_len,
              unsigned int file_size, OL_Pbp *out);

/* Return a section range.  Returns 1 when present, 0 otherwise. */
int pbp_section(const OL_Pbp *pbp, int index,
                unsigned int *offset, unsigned int *size);

/* ---- PARAM.SFO ---------------------------------------------------------- */

#define SFO_HEADER_SIZE   20
#define SFO_ENTRY_SIZE    16
#define SFO_MAX_ENTRIES   256

typedef struct {
    const unsigned char *data;
    size_t size;
    unsigned int key_start;
    unsigned int data_start;
    unsigned int count;
} OL_Sfo;

/* Parse a complete PARAM.SFO buffer.  Returns 1 on success. */
int sfo_parse(const unsigned char *data, size_t size, OL_Sfo *out);

/* Fetch entry `index`.  On success returns 1 and fills key/fmt/value/len.
 * Strings point into the SFO buffer and are not necessarily terminated at
 * `len`; use strnlen.  Unknown formats, bad offsets and truncated values
 * return 0. */
int sfo_entry(const OL_Sfo *sfo, unsigned int index,
              const char **key, unsigned int *fmt,
              const unsigned char **value, unsigned int *len);

#endif
