#include <pspctrl.h>
#include <pspkernel.h>

#include "buttons.h"
#include "engine/core/olreg.h"

#include <lauxlib.h>
#include <string.h>

/* OneLua's `buttons.read()` fills boolean fields on the global `buttons`
 * table (`buttons.start`, `buttons.cross`, ...), plus the `held` and
 * `released` sub-tables and the `analogx`/`analogy` numbers. OpenLua used to
 * only return a bitmask; both forms are supported here. */

typedef struct BtnField {
    const char  *name;
    unsigned int bit;
} BtnField;

static const BtnField k_fields[] = {
    { "select",   PSP_CTRL_SELECT   },
    { "start",    PSP_CTRL_START    },
    { "up",       PSP_CTRL_UP       },
    { "down",     PSP_CTRL_DOWN     },
    { "left",     PSP_CTRL_LEFT     },
    { "right",    PSP_CTRL_RIGHT    },
    { "l",        PSP_CTRL_LTRIGGER },
    { "r",        PSP_CTRL_RTRIGGER },
    /* Legacy OpenLua/harness aliases. */
    { "L",        PSP_CTRL_LTRIGGER },
    { "R",        PSP_CTRL_RTRIGGER },
    { "triangle", PSP_CTRL_TRIANGLE },
    { "circle",   PSP_CTRL_CIRCLE   },
    { "cross",    PSP_CTRL_CROSS    },
    { "square",   PSP_CTRL_SQUARE   },
    { "hold",     PSP_CTRL_HOLD     },
    { "note",     PSP_CTRL_NOTE     },
    { NULL, 0 }
};

static unsigned int s_prev;          /* buttons held during the last read */
static int          s_deadzone;      /* analog -> dpad conversion */
static int          s_rep_delay;     /* frames before autorepeat starts */
static int          s_rep_interval;  /* frames between repeats */
static int          s_rep_count[16]; /* per repeatable-button counters */

/* Repeatable buttons: d-pad + shoulders. The index is local to this table. */
static const unsigned int k_repeat_bits[] = {
    PSP_CTRL_UP, PSP_CTRL_DOWN, PSP_CTRL_LEFT, PSP_CTRL_RIGHT,
    PSP_CTRL_LTRIGGER, PSP_CTRL_RTRIGGER
};
#define REPEAT_N (int)(sizeof(k_repeat_bits) / sizeof(k_repeat_bits[0]))

static void set_bool_field(lua_State *L, const char *name, int value)
{
    lua_pushboolean(L, value);
    lua_setfield(L, -2, name);
}

static void set_mask_table(lua_State *L, const char *name, unsigned int bits)
{
    lua_newtable(L);
    for (int i = 0; k_fields[i].name; i++)
        set_bool_field(L, k_fields[i].name, (bits & k_fields[i].bit) != 0);
    lua_setfield(L, -2, name);
}

static unsigned int add_autorepeat(unsigned int held)
{
    unsigned int pressed = 0;

    for (int i = 0; i < REPEAT_N; i++) {
        unsigned int bit = k_repeat_bits[i];
        if (!(held & bit)) {
            s_rep_count[i] = 0;
            continue;
        }
        s_rep_count[i]++;
        if (s_rep_count[i] == 1)
            pressed |= bit;                       /* initial press */
        else if (s_rep_interval > 0 &&
                 s_rep_count[i] > s_rep_delay &&
                 ((s_rep_count[i] - s_rep_delay) % s_rep_interval) == 0)
            pressed |= bit;                       /* repeat pulse */
    }
    return pressed;
}

static int l_read(lua_State *L)
{
    static SceCtrlData pad;
    sceCtrlSetSamplingMode(PSP_CTRL_MODE_ANALOG);
    sceCtrlPeekBufferPositive(&pad, 1);

    unsigned int held = pad.Buttons;

    /* Optional analog -> d-pad conversion. */
    if (s_deadzone > 0) {
        int dz = s_deadzone * 128 / 127;
        if (pad.Lx < 128 - dz) held |= PSP_CTRL_LEFT;
        if (pad.Lx > 128 + dz) held |= PSP_CTRL_RIGHT;
        if (pad.Ly < 128 - dz) held |= PSP_CTRL_UP;
        if (pad.Ly > 128 + dz) held |= PSP_CTRL_DOWN;
    }

    unsigned int pressed  = (held & ~s_prev) | add_autorepeat(held);
    unsigned int released = s_prev & ~held;
    s_prev = held;

    lua_getglobal(L, "buttons");
    if (lua_istable(L, -1)) {
        for (int i = 0; k_fields[i].name; i++)
            set_bool_field(L, k_fields[i].name,
                           (pressed & k_fields[i].bit) != 0);
        lua_pushinteger(L, (lua_Integer)pad.Lx - 128);
        lua_setfield(L, -2, "analogx");
        lua_pushinteger(L, (lua_Integer)pad.Ly - 128);
        lua_setfield(L, -2, "analogy");
        lua_pushboolean(L, (held & PSP_CTRL_HOLD) != 0);
        lua_setfield(L, -2, "hold");

        set_mask_table(L, "held", held);
        set_mask_table(L, "released", released);
    }
    lua_pop(L, 1);

    /* Legacy OpenLua return values. */
    lua_pushinteger(L, (lua_Integer)pad.Buttons);
    lua_pushinteger(L, (lua_Integer)pad.Lx);
    lua_pushinteger(L, (lua_Integer)pad.Ly);
    return 3;
}

static int l_interval(lua_State *L)
{
    if (lua_isnoneornil(L, 1)) {
        s_rep_delay = 0;
        s_rep_interval = 0;
    } else {
        s_rep_delay = (int)luaL_checkinteger(L, 1);
        s_rep_interval = (int)luaL_optinteger(L, 2, 0);
        if (s_rep_delay < 0) s_rep_delay = 0;
        if (s_rep_interval < 0) s_rep_interval = 0;
    }
    return 0;
}

static int l_analogtodpad(lua_State *L)
{
    s_deadzone = (int)luaL_optinteger(L, 1, 0);
    if (s_deadzone < 0) s_deadzone = 0;
    if (s_deadzone > 127) s_deadzone = 127;
    return 0;
}

static int l_homepopup(lua_State *L)
{
    (void)L; /* HOME popup control is not exposed by the PSP SDK here. */
    return 0;
}

static int l_assign(lua_State *L)
{
    lua_pushinteger(L, 1); /* X is the accept button. */
    return 1;
}

static int l_waitforkey(lua_State *L)
{
    unsigned int want = 0;
    if (!lua_isnoneornil(L, 1))
        want = (unsigned int)luaL_checkinteger(L, 1);

    SceCtrlData pad;
    for (;;) {
        memset(&pad, 0, sizeof(pad));
        sceCtrlReadBufferPositive(&pad, 1);
        unsigned int hit = want ? (pad.Buttons & want) : pad.Buttons;
        if (hit) {
            if (want)
                return 0;
            lua_pushinteger(L, (lua_Integer)hit);
            return 1;
        }
        sceKernelDelayThread(1000);
    }
}

static void push_bit(lua_State *L, const char *name, unsigned int bit)
{
    lua_pushinteger(L, (lua_Integer)bit);
    lua_setglobal(L, name);
}

static const luaL_Reg buttons_funcs[] = {
    { "read",         l_read         },
    { "interval",     l_interval     },
    { "waitforkey",   l_waitforkey   },
    { "analogtodpad", l_analogtodpad },
    { "homepopup",    l_homepopup    },
    { "assign",       l_assign       },
    { NULL,   NULL }
};

void buttons_api_register(lua_State *L)
{
    lua_newtable(L);
    ol_setfuncs(L, buttons_funcs);

    /* OneLua-style boolean fields start false so reads before the first
     * buttons.read() do not produce nil comparisons. */
    for (int i = 0; k_fields[i].name; i++)
        set_bool_field(L, k_fields[i].name, 0);
    lua_pushinteger(L, 0); lua_setfield(L, -2, "analogx");
    lua_pushinteger(L, 0); lua_setfield(L, -2, "analogy");
    lua_newtable(L); lua_setfield(L, -2, "held");
    lua_newtable(L); lua_setfield(L, -2, "released");

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
