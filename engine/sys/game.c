/* OpenLua - OneLua-compatible PBP metadata and image extraction.
 *
 *    game.info(path)      -> table of PARAM.SFO fields, or nil
 *    game.geticon0(path)  -> image handle, or nil
 *    game.getpic0(path)   -> image handle, or nil
 *    game.getpic1(path)   -> image handle, or nil
 *
 * Only the read-only subset is exposed.  The OneLua pack/add/remove and
 * launch operations are intentionally not registered; they rewrite or start
 * other applications and need a separate, explicit safety review.
 *
 * ISO/CSO images are not parsed yet.  A path must point at a PBP file.
 */

#include "engine/sys/game.h"
#include "engine/data/pbp.h"
#include "engine/img/image.h"
#include "engine/core/olreg.h"

#include <lauxlib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define GAME_SFO_MAX   (128u * 1024u)
#define GAME_IMAGE_MAX (4u * 1024u * 1024u)

static unsigned int rd32(const unsigned char *p)
{
    return (unsigned int)p[0] |
           ((unsigned int)p[1] << 8) |
           ((unsigned int)p[2] << 16) |
           ((unsigned int)p[3] << 24);
}

/* Read one PBP section.  Returns a malloc'd buffer (caller frees) or NULL for
 * a missing file, malformed header, absent section or oversized section. */
static unsigned char *read_section(const char *path, int index,
                                   unsigned int max_size,
                                   unsigned int *out_len)
{
    FILE *f = fopen(path, "rb");
    if (!f)
        return NULL;

    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        return NULL;
    }
    long end = ftell(f);
    if (end < PBP_HEADER_SIZE || end > 0x7FFFFFFFL) {
        fclose(f);
        return NULL;
    }
    if (fseek(f, 0, SEEK_SET) != 0) {
        fclose(f);
        return NULL;
    }

    unsigned char header[PBP_HEADER_SIZE];
    if (fread(header, 1, sizeof(header), f) != sizeof(header)) {
        fclose(f);
        return NULL;
    }

    OL_Pbp pbp;
    if (!pbp_parse(header, sizeof(header), (unsigned int)end, &pbp)) {
        fclose(f);
        return NULL;
    }

    unsigned int off, len;
    if (!pbp_section(&pbp, index, &off, &len) || len == 0 ||
        len > max_size) {
        fclose(f);
        return NULL;
    }

    unsigned char *buf = (unsigned char *)malloc(len);
    if (!buf) {
        fclose(f);
        return NULL;
    }
    if (fseek(f, (long)off, SEEK_SET) != 0 ||
        fread(buf, 1, len, f) != len) {
        free(buf);
        fclose(f);
        return NULL;
    }

    fclose(f);
    *out_len = len;
    return buf;
}

static int l_info(lua_State *L)
{
    const char *path = luaL_checkstring(L, 1);
    unsigned int len = 0;
    unsigned char *buf = read_section(path, PBP_SECTION_SFO,
                                      GAME_SFO_MAX, &len);
    if (!buf) {
        lua_pushnil(L);
        return 1;
    }

    OL_Sfo sfo;
    if (!sfo_parse(buf, len, &sfo)) {
        free(buf);
        lua_pushnil(L);
        return 1;
    }

    lua_newtable(L);
    for (unsigned int i = 0; i < sfo.count; i++) {
        const char *key;
        unsigned int fmt, vlen;
        const unsigned char *value;
        if (!sfo_entry(&sfo, i, &key, &fmt, &value, &vlen))
            continue;

        if (fmt == 0x0404) {
            lua_pushnumber(L, (lua_Number)rd32(value));
        } else {
            size_t n = strnlen((const char *)value, vlen);
            lua_pushlstring(L, (const char *)value, n);
        }
        lua_setfield(L, -2, key);
    }

    free(buf);
    return 1;
}

static int push_image(lua_State *L, const char *path, int section)
{
    unsigned int len = 0;
    unsigned char *buf = read_section(path, section, GAME_IMAGE_MAX, &len);
    if (!buf) {
        lua_pushnil(L);
        return 1;
    }

    int ok = image_push_memory(L, buf, len);
    free(buf);
    if (!ok)
        lua_pushnil(L);
    return 1;
}

static int l_geticon0(lua_State *L)
{
    return push_image(L, luaL_checkstring(L, 1), PBP_SECTION_ICON0);
}

static int l_getpic0(lua_State *L)
{
    return push_image(L, luaL_checkstring(L, 1), PBP_SECTION_PIC0);
}

static int l_getpic1(lua_State *L)
{
    return push_image(L, luaL_checkstring(L, 1), PBP_SECTION_PIC1);
}

static const luaL_Reg game_funcs[] = {
    { "info",     l_info },
    { "geticon0", l_geticon0 },
    { "getpic0",  l_getpic0 },
    { "getpic1",  l_getpic1 },
    { NULL, NULL }
};

void game_api_register(lua_State *L)
{
    lua_newtable(L);
    ol_setfuncs(L, game_funcs);
    lua_setglobal(L, "game");
}
