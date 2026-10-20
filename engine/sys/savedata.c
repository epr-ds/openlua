/* OpenLua - savedata module (OneLua-compatible API surface).
 *
 *    data.save(data, title, gameid, subtitle, description, mode)
 *        data   - payload, up to 256 chars
 *        mode 0 - dialog save  : written to the first free slot SLOT1..SLOT5
 *        mode 1 - autosave     : written to AUTOSAVE
 *        -> true on success, false otherwise
 *
 *    data.load(gameid, mode)
 *        mode 1 - autosave slot (AUTOSAVE)
 *        mode 0 - dialog load : most recently used slot, else AUTOSAVE
 *        -> payload string, or nil if no save exists
 *
 *    data.delete(gameid)   -> true on success
 *
 * Storage layout (PSP convention, VSH-visible directory):
 *    <root>/PSP/SAVEDATA/<GAMEID>/META          title/subtitle/description
 *    <root>/PSP/SAVEDATA/<GAMEID>/AUTOSAVE      mode-1 payload
 *    <root>/PSP/SAVEDATA/<GAMEID>/SLOT1..SLOT5  mode-0 payloads
 *
 * root is the first writable of: ms0:, flash0:, <cwd> (host/dev fallback).
 * The VSH save dialog (pspsavdat / pspUtility) is a follow-up (board #20);
 * the payload format is plain text so a later dialog backend can reuse it.
 */

#include "engine/sys/savedata.h"
#include "engine/fs/olfs.h"
#include "engine/core/olreg.h"

#include <lauxlib.h>
#include <lualib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if !defined(__PSP__)
#include <errno.h>
#endif

#define SAVE_BODY_MAX   256
#define SAVE_TITLE_MAX  128
#define SAVE_DES_MAX    1024
#define SLOTS           5

// Root directories to probe for save data on PSP (ms0:, flash0:) or host (.).
#if defined(__PSP__)
static const char *ROOTS[] = { "ms0:", "flash0:", NULL };
#else
static const char *ROOTS[] = { ".", "data", NULL };
#endif

/*
 * Create parent directories (mkdir -a equivalent).  Returns 0 on success.
 */
static __attribute__((unused)) int mkdir_p(const char *path, unsigned len)
{
    if (len == 0) return 0;
    /* Find last / to get parent */
    for (unsigned i = 1; i < len; i++) {
        if (path[i] == '/' || path[i] == ':') {
            char tmp[256];
            unsigned clen = i;
            memcpy(tmp, path, clen);
            if (!olfs_exists(tmp)) {
                if (mkdir_p(tmp, clen) < 0) return -1;
            }
            if (olfs_mkdir(tmp) != 0) return -1;
        }
    }
    return 0;
}


/* ---- storage core ------------------------------------------------------ */

static char cached_root[32] = {0};
static int cached_valid = 0;

static const char *save_root(void)
{
    if (cached_valid) return cached_root;
    cached_root[0] = 0;  /* clear */
    cached_valid = 0;
#if defined(__PSP__)
    for (int i = 0; ROOTS[i]; i++) {
        char test[256];
        snprintf(test, sizeof(test), "%sPSP/SAVEDATA/.olprobe", ROOTS[i]);
        FILE *f = fopen(test, "a+");
        if (f) { fclose(f); olfs_delete(test);
            strncpy(cached_root, ROOTS[i], sizeof(cached_root)-1);
            cached_valid = 1;
            return cached_root; }
    }
#else
    for (int i = 0; ROOTS[i]; i++) {
        char dirp[256];
        snprintf(dirp, sizeof(dirp), "%s/PSP/SAVEDATA", ROOTS[i]);
        if (olfs_mkdir_p(dirp) != 0) continue;
        char test[256];
        snprintf(test, sizeof(test), "%s/PSP/SAVEDATA/.olprobe", ROOTS[i]);
        FILE *f = fopen(test, "a+");
        if (f) { fclose(f); olfs_delete(test);
            strncpy(cached_root, ROOTS[i], sizeof(cached_root)-1);
            cached_valid = 1;
            return cached_root; }
    }
#endif
    return NULL;
}

static int game_dir(const char *gameid, char *out, unsigned int cap)
{
    const char *root = save_root();
    if (!root) return -1;
    /* Check if root already ends with a separator like ms0: or flash0: */
    size_t rlen = strlen(root);
    if (rlen > 0 && root[rlen-1] != '/' && root[rlen-1] != ':') {
        snprintf(out, cap, "%s/PSP/SAVEDATA/%s", root, gameid);
    } else {
        snprintf(out, cap, "%sPSP/SAVEDATA/%s", root, gameid);
    }
    return 0;
}

static int load_file(const char *path, char *out, unsigned int cap)
{
    FILE *f = fopen(path, "rb");
    if (!f) return -1;
    unsigned int n = (unsigned int)fread(out, 1, cap - 1, f);
    fclose(f);
    out[n] = 0;
    return (n > 0) ? 0 : -1;
}

static int store_file(const char *path, const char *data, unsigned int len)
{
    FILE *f = fopen(path, "wb");
    if (!f) return -1;
    int ok = (fwrite(data, 1, len, f) == len);
    fclose(f);
    return ok ? 0 : -1;
}

/* ---- OneLua API --------------------------------------------------------- */

static int l_save(lua_State *L)
{
    const char *data   = luaL_checkstring(L, 1);
    const char *title  = luaL_optstring(L, 2, "");
    const char *gameid = luaL_checkstring(L, 3);
    const char *sub    = luaL_optstring(L, 4, "");
    const char *desc   = luaL_optstring(L, 5, "");
    int mode           = (int)luaL_optinteger(L, 6, 0);

    if (strlen(gameid) > 13 || strlen(title) > SAVE_TITLE_MAX ||
        strlen(desc) > SAVE_DES_MAX)
    { lua_pushboolean(L, 0); return 1; }

    char body[SAVE_BODY_MAX + 1];
    unsigned int n = strlen(data);
    if (n > SAVE_BODY_MAX) n = SAVE_BODY_MAX;
    memcpy(body, data, n);
    body[n] = 0;

    char dir[300];
    if (game_dir(gameid, dir, sizeof(dir)) != 0) { lua_pushboolean(L, 0); return 1; }
    if (olfs_mkdir_p(dir) != 0) { lua_pushboolean(L, 0); return 1; }

    char meta[2048];
    int  mlen = snprintf(meta, sizeof(meta),
                         "title=%s\nsubtitle=%s\ndescription=%s\n",
                         title, sub, desc);
    char mpath[400];
    snprintf(mpath, sizeof(mpath), "%s/META", dir);
    if (store_file(mpath, meta, (unsigned int)mlen) != 0) {}   /* non-fatal */
    /* meta failure is non-fatal: the payload matters most */

    char slot[400];
    int  written = 0;
    if (mode == 1) {
        snprintf(slot, sizeof(slot), "%s/AUTOSAVE", dir);
        written = (store_file(slot, body, n) == 0);
    } else {
        for (int i = 1; i <= SLOTS && !written; i++) {
            snprintf(slot, sizeof(slot), "%s/SLOT%d", dir, i);
            FILE *probe = fopen(slot, "r");
            int   empty = (!probe);
            if (probe) fclose(probe);
            if (empty && store_file(slot, body, n) == 0)
                written = 1;
        }
    }

    lua_pushboolean(L, written ? 1 : 0);
    return 1;
}

static int l_load(lua_State *L)
{
    const char *gameid = luaL_checkstring(L, 1);
    int mode           = (int)luaL_optinteger(L, 2, 0);

    char dir[300];
    if (game_dir(gameid, dir, sizeof(dir)) != 0) { lua_pushnil(L); return 1; }

    char body[SAVE_BODY_MAX + 1];
    char slot[400];

    if (mode == 1) {
        snprintf(slot, sizeof(slot), "%s/AUTOSAVE", dir);
        if (load_file(slot, body, sizeof(body)) == 0)
        { lua_pushstring(L, body); return 1; }
        lua_pushnil(L);
        return 1;
    }

    /* dialog mode: most recently used slot, scanning 5..1 then autosave */
    for (int i = SLOTS; i >= 1; i--) {
        snprintf(slot, sizeof(slot), "%s/SLOT%d", dir, i);
        if (load_file(slot, body, sizeof(body)) == 0)
        { lua_pushstring(L, body); return 1; }
    }
    snprintf(slot, sizeof(slot), "%s/AUTOSAVE", dir);
    if (load_file(slot, body, sizeof(body)) == 0)
    { lua_pushstring(L, body); return 1; }
    lua_pushnil(L);
    return 1;
}

static int l_delete(lua_State *L)
{
    const char *gameid = luaL_checkstring(L, 1);
    char dir[300];
    int gd_rc = game_dir(gameid, dir, sizeof(dir));
    if (gd_rc != 0) { lua_pushboolean(L, 0); return 1; }
    lua_pushboolean(L, olfs_delete(dir) == 0 ? 1 : 0);
    return 1;
}

static const luaL_Reg data_funcs[] = {
    { "save",   l_save },
    { "load",   l_load },
    { "delete", l_delete },
    { NULL,     NULL }
};

void savedata_api_register(lua_State *L)
{
    lua_newtable(L);
    ol_setfuncs(L, data_funcs);
    lua_setglobal(L, "data");
    lua_getglobal(L, "data");        /* re-fetch the same table for the alias */
    lua_setglobal(L, "savedata");
}
