#include <pspctrl.h>

#include "buttons.h"

#include <lauxlib.h>

static int l_read(lua_State *L)
{
    static SceCtrlData pad;
    sceCtrlSetSamplingMode(PSP_CTRL_MODE_ANALOG);
    sceCtrlPeekBufferPositive(&pad, 1);
    lua_pushinteger(L, (lua_Integer)pad.Buttons);
    lua_pushinteger(L, (lua_Integer)pad.Lx);
    lua_pushinteger(L, (lua_Integer)pad.Ly);
    return 3;
}

static void push_bit(lua_State *L, const char *name, unsigned int bit)
{
    lua_pushinteger(L, (lua_Integer)bit);
    lua_setglobal(L, name);
}

void buttons_api_register(lua_State *L)
{
    lua_createtable(L, 0, 2);

    lua_pushcfunction(L, l_read);
    lua_setfield(L, -2, "read");

    lua_setglobal(L, "buttons");

    push_bit(L, "__SELECT",   PSP_CTRL_SELECT);
    push_bit(L, "__START",    PSP_CTRL_START);
    push_bit(L, "__UP",       PSP_CTRL_UP);
    push_bit(L, "__DOWN",     PSP_CTRL_DOWN);
    push_bit(L, "__LEFT",     PSP_CTRL_LEFT);
    push_bit(L, "__RIGHT",    PSP_CTRL_RIGHT);
    push_bit(L, "__L",        PSP_CTRL_LTRIGGER);
    push_bit(L, "__R",        PSP_CTRL_RTRIGGER);
    push_bit(L, "__TRIANGLE", PSP_CTRL_TRIANGLE);
    push_bit(L, "__CIRCLE",   PSP_CTRL_CIRCLE);
    push_bit(L, "__CROSS",    PSP_CTRL_CROSS);
    push_bit(L, "__SQUARE",   PSP_CTRL_SQUARE);
    push_bit(L, "__NOTE",     PSP_CTRL_NOTE);
    push_bit(L, "__HOLD",     PSP_CTRL_HOLD);
}
