/* OpenLua - OneLua-compatible UMD module (read-only subset).
 *
 * See umd.h for the Lua surface.  Only the probing/mounting calls are exposed;
 * the destructive OneLua dumpiso/launch calls are intentionally left out.
 */

#include "engine/sys/umd.h"
#include "engine/sys/game.h"
#include "engine/fs/olfs.h"
#include "engine/core/olreg.h"

#include <lauxlib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define UMD_SFO_MAX       (128u * 1024u)
#define UMD_WALK_BATCH    32
#define UMD_WALK_MAX      20000
#define UMD_WALK_DEPTH    12

static int g_inited;

const char *umd_type_name(unsigned int mask)
{
    if (mask & 0x10u)   /* PSP_UMD_TYPE_GAME */
        return "GAME";
    if (mask & 0x20u)   /* PSP_UMD_TYPE_VIDEO */
        return "VIDEO";
    if (mask & 0x40u)   /* PSP_UMD_TYPE_AUDIO */
        return "AUDIO";
    return "-";
}

void umd_format_size(unsigned long long bytes, char *buf, size_t cap)
{
    double gb;

    if (!buf || cap == 0)
        return;
    gb = (double)bytes / (1024.0 * 1024.0 * 1024.0);
    snprintf(buf, cap, "%.4f", gb);
}

/* Read a whole file with a hard cap.  Caller frees. */
static unsigned char *read_all(const char *path, unsigned int max,
                               unsigned int *out_len)
{
    long size = olfs_size(path);
    FILE *f;
    unsigned char *buf;

    if (size <= 0 || (unsigned long)size > max)
        return NULL;
    f = fopen(path, "rb");
    if (!f)
        return NULL;
    buf = (unsigned char *)malloc((size_t)size);
    if (!buf) {
        fclose(f);
        return NULL;
    }
    if (fread(buf, 1, (size_t)size, f) != (size_t)size) {
        free(buf);
        fclose(f);
        return NULL;
    }
    fclose(f);
    *out_len = (unsigned int)size;
    return buf;
}

static unsigned char *read_sfo(unsigned int *len)
{
    return read_all("disc0:/PSP_GAME/PARAM.SFO", UMD_SFO_MAX, len);
}

/* Recursive size estimate over the mounted ISO9660 view.  Every level is
 * bounded so a corrupt tree cannot loop or exhaust memory. */
static unsigned long long walk_size(const char *dir, int depth, int *budget)
{
    OLEntry *entries;
    int count = 0, trunc = 0;
    unsigned long long total = 0;

    if (depth > UMD_WALK_DEPTH || *budget <= 0)
        return 0;

    entries = (OLEntry *)malloc(sizeof(OLEntry) * UMD_WALK_BATCH);
    if (!entries)
        return 0;
    if (olfs_list(dir, entries, UMD_WALK_BATCH, &count, &trunc) != 0) {
        free(entries);
        return 0;
    }

    for (int i = 0; i < count && *budget > 0; i++) {
        (*budget)--;
        if (entries[i].is_dir)
            total += walk_size(entries[i].path, depth + 1, budget);
        else if (entries[i].size > 0)
            total += (unsigned long long)entries[i].size;
    }

    free(entries);
    return total;
}

#if defined(__PSP__)

#include <pspumd.h>

static int umd_present_hw(void)
{
    return sceUmdCheckMedium() != 0;
}

static int umd_activate_hw(void)
{
    if (sceUmdActivate(1, "disc0:") < 0)
        return 0;
    if (sceUmdWaitDriveStatWithTimer(PSP_UMD_READY, 5000000) < 0) {
        sceUmdDeactivate(1, "disc0:");
        return 0;
    }
    return 1;
}

static void umd_deactivate_hw(void)
{
    sceUmdDeactivate(1, "disc0:");
}

static int umd_type_hw(void)
{
    pspUmdInfo info;

    memset(&info, 0, sizeof(info));
    info.size = (unsigned int)sizeof(info);
    if (sceUmdGetDiscInfo(&info) < 0)
        return -1;
    return (int)info.type;
}

#else /* host: no UMD drive */

static int umd_present_hw(void) { return 0; }
static int umd_activate_hw(void) { return 0; }
static void umd_deactivate_hw(void) { }
static int umd_type_hw(void) { return -1; }

#endif

static int l_init(lua_State *L)
{
    if (!g_inited) {
        if (!umd_present_hw() || !umd_activate_hw()) {
            lua_pushboolean(L, 0);
            return 1;
        }
        g_inited = 1;
    }
    lua_pushboolean(L, 1);
    return 1;
}

static int l_term(lua_State *L)
{
    (void)L;
    if (g_inited) {
        umd_deactivate_hw();
        g_inited = 0;
    }
    return 0;
}

static int l_present(lua_State *L)
{
    lua_pushboolean(L, umd_present_hw());
    return 1;
}

static int l_type(lua_State *L)
{
    int mask = umd_type_hw();
    lua_pushstring(L, mask < 0 ? "-" : umd_type_name((unsigned int)mask));
    return 1;
}

static int l_gameid(lua_State *L)
{
    unsigned int len = 0;
    unsigned char *buf = read_sfo(&len);

    if (!buf) {
        lua_pushstring(L, "");
        return 1;
    }
    if (!game_push_sfo_table(L, buf, len)) {
        free(buf);
        lua_pushstring(L, "");
        return 1;
    }
    free(buf);

    lua_getfield(L, -1, "DISC_ID");
    if (!lua_isstring(L, -1)) {
        lua_pop(L, 2);
        lua_pushstring(L, "");
        return 1;
    }
    lua_remove(L, -2);
    return 1;
}

static int l_info(lua_State *L)
{
    unsigned int len = 0;
    unsigned char *buf = read_sfo(&len);

    if (!buf) {
        lua_pushnil(L);
        return 1;
    }
    if (!game_push_sfo_table(L, buf, len)) {
        free(buf);
        lua_pushnil(L);
        return 1;
    }
    free(buf);
    return 1;
}

static int l_size(lua_State *L)
{
    char text[32];
    int budget = UMD_WALK_MAX;
    unsigned long long bytes = 0;

    if (umd_present_hw())
        bytes = walk_size("disc0:/", 0, &budget);
    umd_format_size(bytes, text, sizeof(text));
    lua_pushstring(L, text);
    return 1;
}

static const luaL_Reg umd_funcs[] = {
    { "init",    l_init    },
    { "present", l_present },
    { "size",    l_size    },
    { "type",    l_type    },
    { "gameid",  l_gameid  },
    { "info",    l_info    },
    { "term",    l_term    },
    { NULL, NULL }
};

void umd_api_register(lua_State *L)
{
    lua_newtable(L);
    ol_setfuncs(L, umd_funcs);
    lua_setglobal(L, "umd");
}
