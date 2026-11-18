#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <lua.h>
#include <lauxlib.h>

#include "ecs.h"
#include "../core/olreg.h"

/*
 * Entity handles are userdata holding (world id, slot, generation).  A slot is
 * reused only after its generation has been bumped, so an old handle can never
 * resolve to a newer entity.  Nothing in a handle dereferences the world, so a
 * handle that outlives its world is simply dead instead of dangling.
 *
 * Per-slot bookkeeping lives in C (generations, free list); the component and
 * system tables live in the world userdata's environment and are owned by Lua.
 * That keeps table values alive naturally and lets the world's __gc just free
 * the C arrays.
 */

#define OL_ECS_WORLD_MT "OL.ECS.World"
#define OL_ECS_ENTITY_MT "OL.ECS.Entity"

typedef struct EcsWorld {
    unsigned int id;
    unsigned int *gen;
    unsigned char *alive;
    unsigned int *free_list;
    unsigned int capacity;
    unsigned int slot_next;
    unsigned int free_top;
    unsigned int count;
} EcsWorld;

typedef struct EcsEntity {
    unsigned int world_id;
    unsigned int slot;
    unsigned int gen;
} EcsEntity;

static unsigned int next_world_id = 0;

/* ---- helpers ---------------------------------------------------------- */

static EcsWorld *check_world(lua_State *L, int idx)
{
    return (EcsWorld *)luaL_checkudata(L, idx, OL_ECS_WORLD_MT);
}

static EcsEntity *check_entity(lua_State *L, int idx)
{
    return (EcsEntity *)luaL_checkudata(L, idx, OL_ECS_ENTITY_MT);
}

static const char *check_name(lua_State *L, int idx)
{
    luaL_checktype(L, idx, LUA_TSTRING);
    return lua_tostring(L, idx);
}

static void push_env(lua_State *L, int world_idx)
{
    lua_getfenv(L, world_idx);
}

static void push_components(lua_State *L, int world_idx)
{
    lua_getfenv(L, world_idx);
    lua_getfield(L, -1, "components");
    lua_remove(L, -2);
}

static void push_entities(lua_State *L, int world_idx)
{
    lua_getfenv(L, world_idx);
    lua_getfield(L, -1, "entities");
    lua_remove(L, -2);
}

static void push_canonical(lua_State *L, int world_idx, unsigned int slot)
{
    push_entities(L, world_idx);
    lua_rawgeti(L, -1, (int)slot + 1);
    lua_remove(L, -2);
}

static int entity_alive(const EcsWorld *w, const EcsEntity *e)
{
    if (e->world_id != w->id)
        return 0;
    if (e->slot >= w->slot_next)
        return 0;
    return w->alive[e->slot] && w->gen[e->slot] == e->gen;
}

static void bump_generation(EcsWorld *w, unsigned int slot)
{
    w->gen[slot]++;
    if (w->gen[slot] == 0)
        w->gen[slot] = 1;
}

static int ensure_capacity(lua_State *L, EcsWorld *w, unsigned int need)
{
    unsigned int cap;
    unsigned int *gen;
    unsigned char *alive;
    unsigned int *free_list;

    if (need <= w->capacity)
        return 1;

    cap = w->capacity ? w->capacity : 16;
    while (cap < need) {
        if (cap > 0x7FFFFFFFu)
            return luaL_error(L, "ecs: too many entities");
        cap *= 2;
    }

    gen = (unsigned int *)realloc(w->gen, (size_t)cap * sizeof(*gen));
    if (!gen)
        return luaL_error(L, "ecs: out of memory");
    w->gen = gen;

    alive = (unsigned char *)realloc(w->alive, (size_t)cap * sizeof(*alive));
    if (!alive)
        return luaL_error(L, "ecs: out of memory");
    w->alive = alive;

    free_list = (unsigned int *)realloc(w->free_list,
                                        (size_t)cap * sizeof(*free_list));
    if (!free_list)
        return luaL_error(L, "ecs: out of memory");
    w->free_list = free_list;

    memset(w->gen + w->capacity, 0,
           (size_t)(cap - w->capacity) * sizeof(*w->gen));
    memset(w->alive + w->capacity, 0,
           (size_t)(cap - w->capacity) * sizeof(*w->alive));
    w->capacity = cap;
    return 1;
}

/* True when every name in the filter table has a non-nil component value. */
static int filter_matches(lua_State *L, int comps_idx, int ent_idx,
                          int filter_idx)
{
    size_t i;
    size_t n = lua_objlen(L, filter_idx);

    for (i = 1; i <= n; i++) {
        int present;

        lua_pushvalue(L, comps_idx);
        lua_rawgeti(L, filter_idx, (int)i);
        lua_gettable(L, -2);
        if (!lua_istable(L, -1)) {
            lua_pop(L, 2);
            return 0;
        }
        lua_pushvalue(L, ent_idx);
        lua_rawget(L, -2);
        present = !lua_isnil(L, -1);
        lua_pop(L, 3);
        if (!present)
            return 0;
    }
    return 1;
}

/* ---- entity metamethods ----------------------------------------------- */

static int ecs_entity_tostring(lua_State *L)
{
    EcsEntity *e = check_entity(L, 1);
    char buf[64];

    snprintf(buf, sizeof(buf), "ecs.entity(%u:%u)", e->slot, e->gen);
    lua_pushstring(L, buf);
    return 1;
}

static int ecs_entity_eq(lua_State *L)
{
    EcsEntity *a = check_entity(L, 1);
    EcsEntity *b = check_entity(L, 2);

    lua_pushboolean(L, a->world_id == b->world_id &&
                       a->slot == b->slot && a->gen == b->gen);
    return 1;
}

/* ---- world lifecycle -------------------------------------------------- */

static int ecs_world_new(lua_State *L)
{
    EcsWorld *w;

    lua_settop(L, 0);
    w = (EcsWorld *)lua_newuserdata(L, sizeof(*w));
    memset(w, 0, sizeof(*w));

    w->id = ++next_world_id;
    if (w->id == 0)
        w->id = ++next_world_id;

    luaL_getmetatable(L, OL_ECS_WORLD_MT);
    lua_setmetatable(L, -2);

    lua_newtable(L);
    lua_newtable(L);
    lua_setfield(L, -2, "components");
    lua_newtable(L);
    lua_setfield(L, -2, "systems");
    lua_newtable(L);
    lua_setfield(L, -2, "entities");
    lua_setfenv(L, -2);

    if (!ensure_capacity(L, w, 16))
        return 0;
    return 1;
}

static int ecs_world_gc(lua_State *L)
{
    EcsWorld *w = (EcsWorld *)lua_touserdata(L, 1);

    if (!w)
        return 0;
    free(w->gen);
    free(w->alive);
    free(w->free_list);
    w->gen = NULL;
    w->alive = NULL;
    w->free_list = NULL;
    w->capacity = 0;
    w->slot_next = 0;
    w->free_top = 0;
    w->count = 0;
    return 0;
}

/* ---- entities --------------------------------------------------------- */

static int ecs_world_spawn(lua_State *L)
{
    EcsWorld *w = check_world(L, 1);
    EcsEntity *e;
    unsigned int slot;
    int udata;

    if (w->free_top > 0) {
        slot = w->free_list[--w->free_top];
    } else {
        if (!ensure_capacity(L, w, w->slot_next + 1))
            return 0;
        slot = w->slot_next++;
    }

    e = (EcsEntity *)lua_newuserdata(L, sizeof(*e));
    e->world_id = w->id;
    e->slot = slot;
    e->gen = w->gen[slot];

    luaL_getmetatable(L, OL_ECS_ENTITY_MT);
    lua_setmetatable(L, -2);

    udata = lua_gettop(L);
    push_entities(L, 1);
    lua_pushvalue(L, udata);
    lua_rawseti(L, -2, (int)slot + 1);
    lua_pop(L, 1);

    w->alive[slot] = 1;
    w->count++;
    return 1;
}

static int ecs_world_destroy(lua_State *L)
{
    EcsWorld *w = check_world(L, 1);
    EcsEntity *e = check_entity(L, 2);
    unsigned int slot;
    int ent;

    if (!entity_alive(w, e)) {
        lua_pushboolean(L, 0);
        return 1;
    }

    slot = e->slot;
    push_canonical(L, 1, slot);
    ent = lua_gettop(L);

    push_components(L, 1);
    lua_pushnil(L);
    while (lua_next(L, -2) != 0) {
        if (lua_istable(L, -1)) {
            lua_pushvalue(L, ent);
            lua_pushnil(L);
            lua_rawset(L, -3);
        }
        lua_pop(L, 1);
    }
    lua_pop(L, 1);

    push_entities(L, 1);
    lua_pushnil(L);
    lua_rawseti(L, -2, (int)slot + 1);
    lua_pop(L, 1);

    w->alive[slot] = 0;
    bump_generation(w, slot);
    w->free_list[w->free_top++] = slot;
    w->count--;

    lua_pushboolean(L, 1);
    return 1;
}

static int ecs_world_alive(lua_State *L)
{
    EcsWorld *w = check_world(L, 1);
    EcsEntity *e = check_entity(L, 2);

    lua_pushboolean(L, entity_alive(w, e));
    return 1;
}

static int ecs_world_count(lua_State *L)
{
    EcsWorld *w = check_world(L, 1);

    lua_pushinteger(L, (lua_Integer)w->count);
    return 1;
}

/* ---- components ------------------------------------------------------- */

static int ecs_world_set(lua_State *L)
{
    EcsWorld *w = check_world(L, 1);
    EcsEntity *e = check_entity(L, 2);
    const char *name = check_name(L, 3);
    int comps;
    int ent;

    if (!entity_alive(w, e)) {
        lua_pushboolean(L, 0);
        return 1;
    }

    push_components(L, 1);
    comps = lua_gettop(L);
    push_canonical(L, 1, e->slot);
    ent = lua_gettop(L);

    if (!lua_isnil(L, 4)) {
        lua_getfield(L, comps, name);
        if (!lua_istable(L, -1)) {
            lua_pop(L, 1);
            lua_newtable(L);
            lua_pushvalue(L, -1);
            lua_setfield(L, comps, name);
        }
        lua_pushvalue(L, ent);
        lua_pushvalue(L, 4);
        lua_rawset(L, -3);
        lua_pop(L, 1);
    } else {
        lua_getfield(L, comps, name);
        if (lua_istable(L, -1)) {
            lua_pushvalue(L, ent);
            lua_pushnil(L);
            lua_rawset(L, -3);
        }
        lua_pop(L, 1);
    }

    lua_pushboolean(L, 1);
    return 1;
}

static int ecs_world_get(lua_State *L)
{
    EcsWorld *w = check_world(L, 1);
    EcsEntity *e = check_entity(L, 2);
    const char *name = check_name(L, 3);

    if (!entity_alive(w, e)) {
        lua_pushnil(L);
        return 1;
    }

    push_components(L, 1);
    lua_getfield(L, -1, name);
    if (!lua_istable(L, -1)) {
        lua_pushnil(L);
        return 1;
    }
    push_canonical(L, 1, e->slot);
    lua_rawget(L, -2);
    return 1;
}

static int ecs_world_has(lua_State *L)
{
    EcsWorld *w = check_world(L, 1);
    EcsEntity *e = check_entity(L, 2);
    const char *name = check_name(L, 3);
    int present;

    if (!entity_alive(w, e)) {
        lua_pushboolean(L, 0);
        return 1;
    }

    push_components(L, 1);
    lua_getfield(L, -1, name);
    if (!lua_istable(L, -1)) {
        lua_pushboolean(L, 0);
        return 1;
    }
    push_canonical(L, 1, e->slot);
    lua_rawget(L, -2);
    present = !lua_isnil(L, -1);
    lua_pushboolean(L, present);
    return 1;
}

static int ecs_world_remove(lua_State *L)
{
    EcsWorld *w = check_world(L, 1);
    EcsEntity *e = check_entity(L, 2);
    const char *name = check_name(L, 3);
    int present;

    if (!entity_alive(w, e)) {
        lua_pushboolean(L, 0);
        return 1;
    }

    push_components(L, 1);
    lua_getfield(L, -1, name);
    if (!lua_istable(L, -1)) {
        lua_pushboolean(L, 0);
        return 1;
    }

    push_canonical(L, 1, e->slot);
    lua_rawget(L, -2);
    present = !lua_isnil(L, -1);
    if (present) {
        push_canonical(L, 1, e->slot);
        lua_pushnil(L);
        lua_rawset(L, -4);
    }
    lua_pushboolean(L, present);
    return 1;
}

/* ---- queries ---------------------------------------------------------- */

static int ecs_query_iter(lua_State *L)
{
    EcsWorld *w = (EcsWorld *)lua_touserdata(L, lua_upvalueindex(1));
    unsigned int slot = (unsigned int)lua_tointeger(L, lua_upvalueindex(3));
    int world;
    int comps;
    int filter;

    lua_pushvalue(L, lua_upvalueindex(1));
    world = lua_gettop(L);
    push_components(L, world);
    comps = lua_gettop(L);
    lua_pushvalue(L, lua_upvalueindex(2));
    filter = lua_gettop(L);

    while (slot < w->slot_next) {
        int ent;

        if (!w->alive[slot]) {
            slot++;
            continue;
        }

        push_canonical(L, world, slot);
        ent = lua_gettop(L);
        if (filter_matches(L, comps, ent, filter)) {
            lua_pushinteger(L, (lua_Integer)(slot + 1));
            lua_replace(L, lua_upvalueindex(3));
            return 1;
        }
        lua_pop(L, 1);
        slot++;
    }

    lua_pushinteger(L, (lua_Integer)w->slot_next);
    lua_replace(L, lua_upvalueindex(3));
    return 0;
}

static int ecs_world_query(lua_State *L)
{
    int top = lua_gettop(L);
    int i;
    int filter;

    check_world(L, 1);

    lua_newtable(L);
    filter = lua_gettop(L);
    for (i = 2; i <= top; i++) {
        luaL_checktype(L, i, LUA_TSTRING);
        lua_pushvalue(L, i);
        lua_rawseti(L, filter, i - 1);
    }

    lua_pushvalue(L, 1);
    lua_pushvalue(L, filter);
    lua_pushinteger(L, 0);
    lua_pushcclosure(L, ecs_query_iter, 3);
    return 1;
}

/* ---- systems ---------------------------------------------------------- */

static int ecs_world_system(lua_State *L)
{
    int top = lua_gettop(L);
    int filter_idx;
    int fn_idx;
    const char *name = NULL;
    char auto_name[32];
    int sys;
    int env;
    int systems;
    int len;
    int i;
    int n;

    if (top >= 4 && lua_type(L, 2) == LUA_TSTRING) {
        name = lua_tostring(L, 2);
        filter_idx = 3;
        fn_idx = 4;
    } else {
        filter_idx = 2;
        fn_idx = 3;
    }

    if (top < fn_idx)
        return luaL_error(L, "ecs: system(name?, filter, fn) expected");
    luaL_checktype(L, filter_idx, LUA_TTABLE);
    luaL_checktype(L, fn_idx, LUA_TFUNCTION);

    lua_newtable(L);
    sys = lua_gettop(L);

    push_env(L, 1);
    env = lua_gettop(L);
    lua_getfield(L, env, "systems");
    systems = lua_gettop(L);
    len = (int)lua_objlen(L, systems);

    if (!name) {
        snprintf(auto_name, sizeof(auto_name), "system_%d", len + 1);
        name = auto_name;
    }
    lua_pushstring(L, name);
    lua_setfield(L, sys, "name");

    lua_newtable(L);
    n = (int)lua_objlen(L, filter_idx);
    for (i = 1; i <= n; i++) {
        lua_rawgeti(L, filter_idx, i);
        luaL_checktype(L, -1, LUA_TSTRING);
        lua_rawseti(L, -2, i);
    }
    lua_setfield(L, sys, "filter");

    lua_pushvalue(L, fn_idx);
    lua_setfield(L, sys, "fn");
    lua_pushboolean(L, 1);
    lua_setfield(L, sys, "enabled");

    lua_pushvalue(L, sys);
    lua_rawseti(L, systems, len + 1);

    lua_pop(L, 2);
    return 1;
}

static int ecs_world_remove_system(lua_State *L)
{
    const char *name = check_name(L, 2);
    int env;
    int systems;
    int len;
    int i;
    int found = 0;

    check_world(L, 1);
    push_env(L, 1);
    env = lua_gettop(L);
    lua_getfield(L, env, "systems");
    systems = lua_gettop(L);
    len = (int)lua_objlen(L, systems);

    for (i = 1; i <= len; i++) {
        lua_rawgeti(L, systems, i);
        lua_getfield(L, -1, "name");
        found = lua_type(L, -1) == LUA_TSTRING &&
                strcmp(lua_tostring(L, -1), name) == 0;
        lua_pop(L, 2);
        if (found)
            break;
    }

    if (found) {
        for (; i < len; i++) {
            lua_rawgeti(L, systems, i + 1);
            lua_rawseti(L, systems, i);
        }
        lua_pushnil(L);
        lua_rawseti(L, systems, len);
    }

    lua_pushboolean(L, found);
    return 1;
}

static int ecs_world_update(lua_State *L)
{
    EcsWorld *w = check_world(L, 1);
    int nargs = lua_gettop(L) - 1;
    int total = 0;
    int env;
    int comps;
    int systems;
    int nsys;
    int i;

    push_env(L, 1);
    env = lua_gettop(L);
    lua_getfield(L, env, "components");
    comps = lua_gettop(L);
    lua_getfield(L, env, "systems");
    systems = lua_gettop(L);
    nsys = (int)lua_objlen(L, systems);

    for (i = 1; i <= nsys; i++) {
        int sys;
        int filter;
        int fn;
        int enabled;
        unsigned int slot = 0;

        lua_rawgeti(L, systems, i);
        sys = lua_gettop(L);
        lua_getfield(L, sys, "enabled");
        enabled = lua_toboolean(L, -1);
        lua_pop(L, 1);
        if (!enabled) {
            lua_pop(L, 1);
            continue;
        }

        lua_getfield(L, sys, "filter");
        filter = lua_gettop(L);
        lua_getfield(L, sys, "fn");
        fn = lua_gettop(L);

        while (slot < w->slot_next) {
            int ent;
            int a;

            if (!w->alive[slot]) {
                slot++;
                continue;
            }

            push_canonical(L, 1, slot);
            ent = lua_gettop(L);
            if (!filter_matches(L, comps, ent, filter)) {
                lua_pop(L, 1);
                slot++;
                continue;
            }

            lua_pushvalue(L, fn);
            lua_pushvalue(L, ent);
            for (a = 2; a <= 1 + nargs; a++)
                lua_pushvalue(L, a);
            lua_call(L, 1 + nargs, 0);
            total++;
            lua_pop(L, 1);
            slot++;
        }

        lua_pop(L, 2);
        lua_pop(L, 1);
    }

    lua_pushinteger(L, (lua_Integer)total);
    return 1;
}

static int ecs_world_clear(lua_State *L)
{
    EcsWorld *w = check_world(L, 1);
    unsigned int destroyed = w->count;
    unsigned int i;
    int env;

    for (i = 0; i < w->slot_next; i++) {
        w->alive[i] = 0;
        bump_generation(w, i);
        w->free_list[i] = i;
    }
    w->free_top = w->slot_next;
    w->count = 0;

    push_env(L, 1);
    env = lua_gettop(L);
    lua_newtable(L);
    lua_setfield(L, env, "components");
    lua_newtable(L);
    lua_setfield(L, env, "entities");
    lua_pop(L, 1);

    lua_pushinteger(L, (lua_Integer)destroyed);
    return 1;
}

/* ---- module registration ---------------------------------------------- */

void ecs_api_register(lua_State *L)
{
    const luaL_Reg methods[] = {
        { "spawn", ecs_world_spawn },
        { "create", ecs_world_spawn },
        { "destroy", ecs_world_destroy },
        { "alive", ecs_world_alive },
        { "count", ecs_world_count },
        { "set", ecs_world_set },
        { "get", ecs_world_get },
        { "has", ecs_world_has },
        { "remove", ecs_world_remove },
        { "query", ecs_world_query },
        { "system", ecs_world_system },
        { "remove_system", ecs_world_remove_system },
        { "update", ecs_world_update },
        { "clear", ecs_world_clear },
        { NULL, NULL }
    };

    luaL_newmetatable(L, OL_ECS_ENTITY_MT);
    lua_pushcfunction(L, ecs_entity_tostring);
    lua_setfield(L, -2, "__tostring");
    lua_pushcfunction(L, ecs_entity_eq);
    lua_setfield(L, -2, "__eq");
    lua_pop(L, 1);

    luaL_newmetatable(L, OL_ECS_WORLD_MT);
    lua_pushcfunction(L, ecs_world_gc);
    lua_setfield(L, -2, "__gc");
    lua_newtable(L);
    ol_setfuncs(L, methods);
    lua_setfield(L, -2, "__index");
    lua_pop(L, 1);

    lua_newtable(L);
    lua_pushcfunction(L, ecs_world_new);
    lua_setfield(L, -2, "world");
    lua_pushcfunction(L, ecs_world_new);
    lua_setfield(L, -2, "new");
    lua_setglobal(L, "ecs");
}
