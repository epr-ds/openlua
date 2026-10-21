/* OpenLua - portable PSMF / PMF header parser. */

#include "engine/data/psmf.h"

#include <string.h>

static uint32_t be32(const unsigned char *p)
{
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] << 8) | (uint32_t)p[3];
}

static uint32_t be16(const unsigned char *p)
{
    return ((uint32_t)p[0] << 8) | (uint32_t)p[1];
}

static uint64_t be_ts(const unsigned char *p)
{
    return ((uint64_t)be16(p) << 32) | (uint64_t)be32(p + 2);
}

int psmf_parse_header(const unsigned char *data, size_t len,
                      uint64_t file_size, OL_PsmfInfo *out)
{
    OL_PsmfInfo info;
    uint64_t stream_end;

    if (!data || !out || len < PSMF_HEADER_SIZE)
        return -1;
    if (memcmp(data, "PSMF", 4) != 0)
        return -1;

    for (int i = 4; i < 8; i++) {
        if (data[i] < '0' || data[i] > '9')
            return -1;
    }

    memset(&info, 0, sizeof(info));
    info.version = be32(data + 4);
    info.stream_offset = be32(data + 8);
    info.stream_size = be32(data + 12);

    if (info.stream_offset < PSMF_HEADER_SIZE)
        return -1;
    if ((info.stream_offset & (PSMF_HEADER_SIZE - 1)) != 0)
        return -1;
    if (info.stream_size == 0)
        return -1;

    stream_end = (uint64_t)info.stream_offset + info.stream_size;
    if (stream_end > file_size)
        return -1;

    info.first_ts = be_ts(data + 0x54);
    info.last_ts = be_ts(data + 0x5A);
    if (info.last_ts >= info.first_ts) {
        uint64_t ticks = info.last_ts - info.first_ts;
        info.duration_90k = ticks > UINT32_MAX ? UINT32_MAX : (uint32_t)ticks;
    }

    info.width = (int)data[142] * 16;
    info.height = (int)data[143] * 16;

    *out = info;
    return 0;
}
