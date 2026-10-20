#include <sys/time.h>
#include <stdlib.h>

#include "timers.h"
#include "../core/olreg.h"

#include <lauxlib.h>

#define OL_TIMER "OL.Timer"

typedef struct OL_Timer {
    struct timeval started;
    long long base_ms;
    long long elapsed_ms;
    int stopped;
} OL_Timer;

static struct timeval now_time(void)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv;
}

static long long ms_since(const struct timeval *a, const struct timeval *b)
{
    long long us = ((long long)b->tv_sec - (long long)a->tv_sec) * 1000000LL
                 + ((long long)b->tv_usec - (long long)a->tv_usec);
    if (us < 0)
        return 0;
    return us / 1000;
}

static long long timer_elapsed(OL_Timer *t)
{
    if (t->stopped)
        return t->elapsed_ms;

    struct timeval now = now_time();
    long long candidate = t->base_ms + ms_since(&t->started, &now);
    if (candidate > t->elapsed_ms)
        t->elapsed_ms = candidate;
    return t->elapsed_ms;
}

static long long timer_restart(OL_Timer *t, long long base_ms)
{
    if (base_ms < 0)
        base_ms = 0;
    t->started = now_time();
    t->base_ms = base_ms;
    t->elapsed_ms = base_ms;
    t->stopped = 0;
    return t->elapsed_ms;
}

static OL_Timer **test_timer(lua_State *L, int idx)
{
    OL_Timer **p = (OL_Timer **)lua_touserdata(L, idx);
    if (!p || !lua_getmetatable(L, idx))
        return NULL;
    luaL_getmetatable(L, OL_TIMER);
    int ok = lua_rawequal(L, -1, -2);
    lua_pop(L, 2);
    return ok ? p : NULL;
}

static OL_Timer *to_timer(lua_State *L, int idx)
{
    OL_Timer **p = (OL_Timer **)luaL_checkudata(L, idx, OL_TIMER);
    if (!p || !*p)
        luaL_error(L, "bad timer");
    return *p;
}

static void push_timer(lua_State *L, OL_Timer *t)
{
    OL_Timer **p = (OL_Timer **)lua_newuserdata(L, sizeof(OL_Timer *));
    *p = t;
    luaL_getmetatable(L, OL_TIMER);
    lua_setmetatable(L, -2);
}

static int l_new(lua_State *L)
{
    OL_Timer *t = (OL_Timer *)calloc(1, sizeof(OL_Timer));
    if (!t)
        return luaL_error(L, "timers.new: OOM");

    long long initial_ms = 0;
    if (!lua_isnoneornil(L, 1))
        initial_ms = (long long)luaL_checkinteger(L, 1);
    timer_restart(t, initial_ms);
    push_timer(L, t);
    return 1;
}

static int l_start(lua_State *L)
{
    OL_Timer *t = to_timer(L, 1);
    lua_pushinteger(L, (lua_Integer)timer_restart(t, t->base_ms));
    return 1;
}

static int l_stop(lua_State *L)
{
    OL_Timer *t = to_timer(L, 1);
    if (!t->stopped) {
        timer_elapsed(t);
        t->stopped = 1;
    }
    lua_pushinteger(L, (lua_Integer)t->elapsed_ms);
    return 1;
}

static int l_gettime(lua_State *L)
{
    OL_Timer *t = to_timer(L, 1);
    lua_pushinteger(L, (lua_Integer)timer_elapsed(t));
    return 1;
}

static int l_reset(lua_State *L)
{
    OL_Timer *t = to_timer(L, 1);
    long long base_ms = 0;
    if (!lua_isnoneornil(L, 2))
        base_ms = (long long)luaL_checkinteger(L, 2);
    lua_pushinteger(L, (lua_Integer)timer_restart(t, base_ms));
    return 1;
}

static int gc_timer(lua_State *L)
{
    OL_Timer **p = test_timer(L, 1);
    if (p && *p) {
        free(*p);
        *p = NULL;
    }
    return 0;
}

static const luaL_Reg timer_meta[] = {
    { "start",      l_start },
    { "stoptime",   l_stop },
    { "time",       l_gettime },
    { "gettime",    l_gettime },
    { "startreset", l_reset },
    { "reset",      l_reset },
    { "__gc",       gc_timer },
    { NULL, NULL }
};

static const luaL_Reg timers_funcs[] = {
    { "new", l_new },
    { NULL, NULL }
};

void timers_api_register(lua_State *L)
{
    luaL_newmetatable(L, OL_TIMER);
    ol_setfuncs(L, timer_meta);
    lua_pushvalue(L, -1);
    lua_setfield(L, -2, "__index");
    lua_pop(L, 1);

    lua_createtable(L, 0, 1);
    ol_setfuncs(L, timers_funcs);
    lua_setglobal(L, "timers");
}
