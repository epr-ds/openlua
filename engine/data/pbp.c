/* OpenLua - bounds-checked PBP and PARAM.SFO parsing. */

#include "engine/data/pbp.h"

#include <string.h>

static unsigned int rd32(const unsigned char *p)
{
    return (unsigned int)p[0] |
           ((unsigned int)p[1] << 8) |
           ((unsigned int)p[2] << 16) |
           ((unsigned int)p[3] << 24);
}

static unsigned int rd16(const unsigned char *p)
{
    return (unsigned int)p[0] | ((unsigned int)p[1] << 8);
}

int pbp_parse(const unsigned char *header, size_t header_len,
              unsigned int file_size, OL_Pbp *out)
{
    if (!header || !out || header_len < PBP_HEADER_SIZE ||
        file_size < PBP_HEADER_SIZE)
        return 0;

    if (memcmp(header, "\0PBP", 4) != 0)
        return 0;

    memset(out, 0, sizeof(*out));

    unsigned int prev = PBP_HEADER_SIZE;
    for (int i = 0; i < PBP_SECTION_COUNT; i++) {
        unsigned int off = rd32(header + 8 + i * 4);
        out->offset[i] = off;
        if (off == 0)
            continue;
        if (off < PBP_HEADER_SIZE || off > file_size || off < prev)
            return 0;
        prev = off;
    }

    /* Size runs to the next present section, or to EOF. */
    for (int i = 0; i < PBP_SECTION_COUNT; i++) {
        unsigned int off = out->offset[i];
        if (off == 0)
            continue;
        unsigned int end = file_size;
        for (int j = i + 1; j < PBP_SECTION_COUNT; j++) {
            if (out->offset[j] != 0) {
                end = out->offset[j];
                break;
            }
        }
        if (end < off)
            return 0;
        out->size[i] = end - off;
    }

    return 1;
}

int pbp_section(const OL_Pbp *pbp, int index,
                unsigned int *offset, unsigned int *size)
{
    if (!pbp || index < 0 || index >= PBP_SECTION_COUNT ||
        pbp->offset[index] == 0)
        return 0;
    if (offset)
        *offset = pbp->offset[index];
    if (size)
        *size = pbp->size[index];
    return 1;
}

int sfo_parse(const unsigned char *data, size_t size, OL_Sfo *out)
{
    if (!data || !out || size < SFO_HEADER_SIZE)
        return 0;

    if (memcmp(data, "\0PSF", 4) != 0)
        return 0;

    unsigned int key_start = rd32(data + 8);
    unsigned int data_start = rd32(data + 12);
    unsigned int count = rd32(data + 16);

    if (count > SFO_MAX_ENTRIES)
        return 0;
    if ((size_t)SFO_HEADER_SIZE + (size_t)count * SFO_ENTRY_SIZE > size)
        return 0;
    if (key_start < SFO_HEADER_SIZE || key_start > size)
        return 0;
    if (data_start < key_start || data_start > size)
        return 0;

    out->data = data;
    out->size = size;
    out->key_start = key_start;
    out->data_start = data_start;
    out->count = count;
    return 1;
}

int sfo_entry(const OL_Sfo *sfo, unsigned int index,
              const char **key, unsigned int *fmt,
              const unsigned char **value, unsigned int *len)
{
    if (!sfo || !sfo->data || index >= sfo->count)
        return 0;

    const unsigned char *e =
        sfo->data + SFO_HEADER_SIZE + (size_t)index * SFO_ENTRY_SIZE;
    unsigned int key_off = rd16(e);
    unsigned int format = rd16(e + 2);
    unsigned int value_len = rd32(e + 4);
    unsigned int max_len = rd32(e + 8);
    unsigned int data_off = rd32(e + 12);

    size_t key_abs = (size_t)sfo->key_start + key_off;
    if (key_abs >= sfo->data_start || key_abs >= sfo->size)
        return 0;

    const unsigned char *k = sfo->data + key_abs;
    size_t key_cap = sfo->data_start - key_abs;
    if (memchr(k, 0, key_cap) == NULL)
        return 0;

    size_t value_abs = (size_t)sfo->data_start + data_off;
    if (value_abs > sfo->size)
        return 0;

    if (format == 0x0404) {
        if (value_len < 4 || value_abs + 4 > sfo->size)
            return 0;
        if (len)
            *len = 4;
    } else if (format == 0x0004 || format == 0x0204) {
        unsigned int avail = (unsigned int)(sfo->size - value_abs);
        unsigned int n = value_len;
        if (format == 0x0204 && max_len != 0)
            n = max_len;
        if (n > avail)
            n = avail;
        if (len)
            *len = n;
    } else {
        return 0;
    }

    if (key)
        *key = (const char *)k;
    if (fmt)
        *fmt = format;
    if (value)
        *value = sfo->data + value_abs;
    return 1;
}
