#ifndef OL_TEXTWRAP_H
#define OL_TEXTWRAP_H

/*
 * Soft-wraps `src` into lines of at most `width` columns.
 *
 * - Explicit '\n' characters start a new line and are preserved.
 * - Breaks prefer the last space or tab inside the window so words and
 *   paths stay readable; unbroken tokens longer than `width` are cut.
 * - Trailing spaces/tabs on a wrapped line are dropped, and the space at
 *   a break point is not repeated at the start of the next line.
 *
 * The result is written to `out` as NUL-terminated lines separated by
 * '\n' (a trailing '\n' is emitted after the last line). At most
 * `out_size - 1` bytes are written. Returns the number of bytes written,
 * or 0 when `out` is NULL or `out_size` is not positive.
 */
int ol_text_wrap(const char *src, int width, char *out, int out_size);

#endif
