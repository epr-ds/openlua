#include <sys/time.h>
#include <stdlib.h>

#include "timers.h"
#include "../core/olreg.h"

#include <lauxlib.h>

#define OL_TIMER "OL.Timer"

typedef struct OL_Timer {
    struct timeval created;
    struct timeval started;
    int stopped;
} OL_Timer;

static struct timeval now_time(void)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv;
}

static int ms_since(struct timeval *a, struct timeval *b)
{
    return (int)((b->tv_sec - a->tv_sec) * 1000 + (b->tv_usec - a->tv_usec) / 1000);
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
    t->created = now_time();
    t->started = t->created;
    push_timer(L, t);
    return 1;
}

static int l_start(lua_State *L)
{
    OL_Timer *t = to_timer(L, 1);
    t->started = now_time();
    t->stopped = 0;
    return 0;
}

static int l_stop(lua_State *L)
{
    OL_Timer *t = to_timer(L, 1);
    struct timeval now = now_time();
    int ms = t->stopped ? ms_since(&t->started, &t->created)
                        : ms_since(&now, &t->started);
    t->stopped = 1;
    lua_pushinteger(L, ms);
    return 1;
}

static int l_gettime(lua_State *L)
{
    OL_Timer *t = to_timer(L, 1);
    struct timeval now = now_time();
    lua_pushinteger(L, ms_since(&now, &t->started));
    return 1;
}

static int l_reset(lua_State *L)
{
    OL_Timer *t = to_timer(L, 1);
    t->created = now_time();
    t->started = t->created;
    t->stopped = 0;
    return 0;
}

static int gc_timer(lua_State *L)
{
    OL_Timer **p = (OL_Timer **)luaL_checkudata(L, 1, OL_TIMER);
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
    lua_pop(L, 1);

    lua_createtable(L, 0, 1);
    ol_setfuncs(L, timers_funcs);
    lua_setglobal(L, "timers");
}
