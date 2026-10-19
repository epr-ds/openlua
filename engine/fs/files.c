/* OpenLua - files module (OneLua-compatible).
 *
 *    files.exists(path)      -> true/false
 *    files.cdir()            -> current path string
 *    files.cdir(path)        -> change working path
 *    files.nofile(path)      -> path without the file name
 *    files.nopath(path)      -> file name only
 *    files.rename(old, new)  -> 1 on success, else error
 *    files.copy(src, dst)    -> 1 on success, else error
 *    files.move(src, dst)    -> 1 on success, else error
 *    files.delete(path)      -> true on success (file or whole tree)
 *    files.mkdir(path)       -> create directory (parents too)
 *    files.size(path)        -> bytes (-1 if unreadable)
 *    files.ext(path)         -> extension string ("lua", "" if none)
 *    files.list(path)        -> table of {name,path,size,ext,directory,mode}
 *    files.listfiles(path)   -> same, files only
 *    files.listdirs(path)    -> same, directories only
 *
 * Not implemented yet (tracked on board): files.scan / files.extract /
 * files.extractfile / thread* variants (zip+rar) - needs an archive backend.
 */

#include "engine/fs/files.h"
#include "engine/fs/olfs.h"
#include "engine/core/olreg.h"

#include <lauxlib.h>
#include <lualib.h>
#include <string.h>

#define MAX_LIST 128

/* ---- helpers ---------------------------------------------------------- */

static char *check_path(lua_State *L, int idx)
{
    return (char *)luaL_checkstring(L, idx);
}

/* Map platform permission bits to the OneLua "RWE"-style string.
 * Both PSP (FIO_S_*) and POSIX (st_mode) place owner R/W/X at 0x100/0x80/0x40. */
static void mode_str(lua_State *L, unsigned int attr, int is_dir)
{
    int r = (attr & 0x0100) ? 1 : 0;
    int w = (attr & 0x0080) ? 1 : 0;
    int x = (attr & 0x0040) ? 1 : 0;
    if (is_dir) { r = 1; x = 1; }           /* dirs are always listable   */
    lua_pushfstring(L, "%c%c%c", r ? 'R' : '-', w ? 'W' : '-', x ? 'E' : '-');
}

static void push_entry(lua_State *L, OLEntry *e)
{
    char ext[32] = "";
    olfs_path_ext(e->name, ext, sizeof(ext));

    lua_newtable(L);
    lua_pushstring(L, e->name);    lua_setfield(L, -2, "name");
    lua_pushstring(L, e->path);    lua_setfield(L, -2, "path");
    lua_pushinteger(L, e->size);   lua_setfield(L, -2, "size");
    lua_pushstring(L, ext);        lua_setfield(L, -2, "ext");
    lua_pushboolean(L, e->is_dir); lua_setfield(L, -2, "directory");
    mode_str(L, e->attr, e->is_dir); lua_setfield(L, -2, "mode");
}

/* ---- API -------------------------------------------------------------- */

static int l_exists(lua_State *L)
{
    lua_pushboolean(L, olfs_exists(check_path(L, 1)) ? 1 : 0);
    return 1;
}

static int l_cdir(lua_State *L)
{
    int nargs = lua_gettop(L);
    if (nargs >= 1 && lua_isstring(L, 1)) {
        int r = olfs_chdir(check_path(L, 1));
        if (r != 0)
            luaL_error(L, "files.cdir: cannot change to '%s'", check_path(L, 1));
        return 0;
    }
    char buf[512];
    if (olfs_cwd(buf, sizeof(buf)) != 0)
        buf[0] = 0;
    lua_pushstring(L, buf);
    return 1;
}

static int l_nofile(lua_State *L)
{
    char out[512];
    const char *p = check_path(L, 1);
    if (olfs_path_dir(p, out, sizeof(out)) != 0)
        out[0] = 0;
    lua_pushstring(L, out);
    return 1;
}

static int l_nopath(lua_State *L)
{
    char out[512];
    const char *p = check_path(L, 1);
    if (olfs_path_base(p, out, sizeof(out)) != 0)
        lua_pushnil(L);
    else
        lua_pushstring(L, out);
    return 1;
}

static int l_rename(lua_State *L)
{
    const char *oldp = check_path(L, 1);
    const char *newp = check_path(L, 2);
    if (olfs_move(oldp, newp) != 0)
        luaL_error(L, "files.rename: failed '%s' -> '%s'", oldp, newp);
    lua_pushinteger(L, 1);
    return 1;
}

static int l_copy(lua_State *L)
{
    const char *src = check_path(L, 1);
    const char *dst = check_path(L, 2);
    if (olfs_copyfile(src, dst) != 0)
        luaL_error(L, "files.copy: failed '%s' -> '%s'", src, dst);
    lua_pushinteger(L, 1);
    return 1;
}

static int l_move(lua_State *L)
{
    const char *src = check_path(L, 1);
    const char *dst = check_path(L, 2);
    if (olfs_move(src, dst) != 0)
        luaL_error(L, "files.move: failed '%s' -> '%s'", src, dst);
    lua_pushinteger(L, 1);
    return 1;
}

static int l_delete(lua_State *L)
{
    lua_pushboolean(L, olfs_delete(check_path(L, 1)) == 0 ? 1 : 0);
    return 1;
}

static int l_mkdir(lua_State *L)
{
    olfs_mkdir_p(check_path(L, 1));           /* best effort, parents too */
    return 0;
}

static int l_size(lua_State *L)
{
    lua_pushinteger(L, olfs_size(check_path(L, 1)));
    return 1;
}

static int l_ext(lua_State *L)
{
    char out[64];
    const char *p = check_path(L, 1);
    if (olfs_path_ext(p, out, sizeof(out)) != 0)
        out[0] = 0;
    lua_pushstring(L, out);
    return 1;
}

static int l_list_core(lua_State *L, int only_dir)
{
    const char *dir = check_path(L, 1);
    OLEntry e[MAX_LIST];
    int outn = 0, trunc = 0;

    if (olfs_list(dir, e, MAX_LIST, &outn, &trunc) != 0)
        lua_pushnil(L);
    else {
        lua_newtable(L);
        int k = 1;
        for (int i = 0; i < outn; i++) {
            if (only_dir >= 0 && only_dir != e[i].is_dir)
                continue;
            push_entry(L, &e[i]);
            lua_rawseti(L, -2, k++);
        }
    }
    return 1;
}

static int l_list(lua_State *L)      { return l_list_core(L, -1); }
static int l_listfiles(lua_State *L) { return l_list_core(L, 0); }
static int l_listdirs(lua_State *L)  { return l_list_core(L, 1); }

/* ---- registration ------------------------------------------------------ */

static const luaL_Reg files_funcs[] = {
    { "exists", l_exists },
    { "cdir",   l_cdir },
    { "nofile", l_nofile },
    { "nopath", l_nopath },
    { "rename", l_rename },
    { "copy",   l_copy },
    { "move",   l_move },
    { "delete", l_delete },
    { "mkdir",  l_mkdir },
    { "size",   l_size },
    { "ext",    l_ext },
    { "list",       l_list },
    { "listfiles",  l_listfiles },
    { "listdirs",   l_listdirs },
    { NULL, NULL }
};

void files_api_register(lua_State *L)
{
    lua_newtable(L);
    ol_setfuncs(L, files_funcs);
    lua_setglobal(L, "files");
}
