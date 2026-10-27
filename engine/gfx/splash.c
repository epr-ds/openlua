/* OpenLua - OneLua-compatible splash screens.
 *
 * `splash.show()` / `splash.old()` display an image (or a table of images)
 * for about two seconds each.  `splash.oldschool()` shows an original
 * tribute screen in the spirit of the OldSchool Library intro.  The splash
 * loop presents frames through the normal 2D GE path, so it can be called
 * from a game before (or after) any 2D/3D drawing.
 *
 * Missing or oversized images are skipped instead of raising.  If no image
 * can be loaded, a built-in geometric fallback is shown.  On the host the
 * images are still decoded and validated, but no window or timing loop is
 * entered; test/host/test_splash.lua checks the statistics.
 */

#include "splash.h"
#include "engine/gfx/gfx.h"
#include "engine/core/olreg.h"

#include "third_party/stb/stb_image.h"

#include <lauxlib.h>
#include <string.h>

#if defined(__PSP__)
#include <pspctrl.h>
#include <sys/time.h>

#include "engine/gfx/gfx3d.h"
#include "engine/snd/sound.h"
#endif

#define SPLASH_DEFAULT_MS   2000
#define SPLASH_FADE_IN_MS    250
#define SPLASH_FADE_OUT_MS   300
#define SPLASH_MAX_SIDE      512

static const char *DEFAULT_NEW       = "assets/splash.png";
static const char *DEFAULT_OLD       = "assets/splash_old.png";
static const char *DEFAULT_OLDSCHOOL = "assets/splash_oldschool.png";

static OL_SplashStats g_stats;

typedef struct {
    char paths[SPLASH_MAX_IMAGES][SPLASH_PATH_MAX];
    int count;
} SplashRequest;

const OL_SplashStats *splash_last_stats(void)
{
    return &g_stats;
}

/* ---------------------------------------------------------------------- */
/* Argument handling                                                       */
/* ---------------------------------------------------------------------- */

static void add_path(lua_State *L, SplashRequest *req, const char *path)
{
    if (req->count >= SPLASH_MAX_IMAGES)
        luaL_error(L, "splash: too many images (max %d)", SPLASH_MAX_IMAGES);

    size_t len = strlen(path);
    if (len >= SPLASH_PATH_MAX)
        luaL_error(L, "splash: image path is too long");

    memcpy(req->paths[req->count], path, len + 1);
    req->count++;
}

static void collect_paths(lua_State *L, SplashRequest *req, int idx,
                          const char *fallback)
{
    memset(req, 0, sizeof(*req));

    if (lua_gettop(L) < idx || lua_isnil(L, idx)) {
        add_path(L, req, fallback);
        return;
    }

    int type = lua_type(L, idx);
    if (type == LUA_TSTRING) {
        add_path(L, req, lua_tostring(L, idx));
        return;
    }

    if (type != LUA_TTABLE)
        luaL_error(L, "splash: expected an image path or a table of paths");

    int n = (int)lua_objlen(L, idx);
    for (int i = 1; i <= n; i++) {
        lua_rawgeti(L, idx, i);
        if (lua_type(L, -1) != LUA_TSTRING) {
            lua_pop(L, 1);
            luaL_error(L, "splash: table entry %d is not a string", i);
        }
        add_path(L, req, lua_tostring(L, -1));
        lua_pop(L, 1);
    }
}

/* ---------------------------------------------------------------------- */
/* Image loading                                                           */
/* ---------------------------------------------------------------------- */

static OL_Surface *load_surface(const char *path)
{
    if (!path || !*path)
        return NULL;

    int w = 0, h = 0, comp = 0;
    stbi_uc *data = stbi_load(path, &w, &h, &comp, STBI_rgb_alpha);
    if (!data)
        return NULL;

    if (w <= 0 || h <= 0 || w > SPLASH_MAX_SIDE || h > SPLASH_MAX_SIDE) {
        stbi_image_free(data);
        return NULL;
    }

    OL_Surface *s = gfx_surface_new(w, h);
    if (!s) {
        stbi_image_free(data);
        return NULL;
    }

    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            unsigned int c = ((const unsigned int *)data)[y * w + x];
            s->pixels[y * s->realW + x] = c;
        }
    }
    stbi_image_free(data);
    return s;
}

/* Original, asset-free fallback: a dark gradient with a simple ring logo. */
static OL_Surface *make_fallback_surface(void)
{
    OL_Surface *s = gfx_surface_new(480, 272);
    if (!s)
        return NULL;

    for (int y = 0; y < 272; y++) {
        for (int x = 0; x < 480; x++) {
            unsigned int r = 8u + (unsigned int)(x * 10 / 480);
            unsigned int g = 12u + (unsigned int)(y * 16 / 272);
            unsigned int b = 30u + (unsigned int)((x + y) * 28 / 752);
            s->pixels[y * s->realW + x] =
                0xFF000000u | (b << 16) | (g << 8) | r;
        }
    }

    /* A diagonal teal band behind the mark. */
    for (int y = 40; y < 240; y++) {
        int cx = 240 + (y - 136) / 2;
        for (int x = cx - 11; x <= cx + 11; x++) {
            if (x >= 0 && x < 480)
                s->pixels[y * s->realW + x] = 0xFF9C6E2Au;
        }
    }

    /* Orange outer ring and teal inner ring, centred. */
    for (int y = 0; y < 272; y++) {
        for (int x = 0; x < 480; x++) {
            int dx = x - 240, dy = y - 130;
            int d2 = dx * dx + dy * dy;
            int outer = 54 * 54, outer_in = 41 * 41;
            int inner = 29 * 29, inner_in = 21 * 21;

            if (d2 <= outer && d2 >= outer_in)
                s->pixels[y * s->realW + x] = 0xFF2E78E6u;
            else if (d2 <= inner && d2 >= inner_in)
                s->pixels[y * s->realW + x] = 0xFF3CB4A0u;
        }
    }

    /* Two thin frame lines keep the screen readable on black. */
    for (int x = 30; x < 450; x++) {
        s->pixels[24 * s->realW + x] = 0xFF303A50u;
        s->pixels[247 * s->realW + x] = 0xFF303A50u;
    }

    return s;
}

/* ---------------------------------------------------------------------- */
/* PSP presentation                                                        */
/* ---------------------------------------------------------------------- */

#if defined(__PSP__)

static int gfx_begun;
static unsigned int saved_clear;

static long long now_ms(void)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (long long)tv.tv_sec * 1000LL + (long long)tv.tv_usec / 1000LL;
}

static unsigned int read_buttons(void)
{
    SceCtrlData pad;
    sceCtrlSetSamplingMode(PSP_CTRL_MODE_ANALOG);
    if (sceCtrlPeekBufferPositive(&pad, 1) < 0)
        return 0;
    return pad.Buttons;
}

static void begin_gfx(void)
{
    if (gfx_begun || !gfx_gu_started())
        return;

    /* Leave any 3D scene in the documented 2D state first. */
    gfx3d_finish();
    saved_clear = gfx_clear_color();
    gfx_set_clear_color(0xFF000000u);
    gfx_clear_scene();
    gfx_begun = 1;
}

static void end_gfx(void)
{
    if (!gfx_begun)
        return;
    gfx_set_clear_color(saved_clear);
    gfx_begun = 0;
}

static void present(OL_Surface *s, int alpha)
{
    if (!s || !gfx_gu_started())
        return;

    float sx = 480.0f / (float)s->w;
    float sy = 272.0f / (float)s->h;
    float scale = sx < sy ? sx : sy;
    if (scale > 1.0f)
        scale = 1.0f;

    int dx = (int)((480.0f - (float)s->w * scale) * 0.5f + 0.5f);
    int dy = (int)((272.0f - (float)s->h * scale) * 0.5f + 0.5f);

    gfx_blit(s, dx, dy, 0, 0, s->w, s->h,
             alpha, 0, 0, 0.0f, scale, scale, 0, 0xFFFFFFFFu);
    sound_pump();
    gfx_flip_scene();
}

static int fade_alpha(long long elapsed)
{
    if (elapsed < SPLASH_FADE_IN_MS)
        return (int)(elapsed * 255 / SPLASH_FADE_IN_MS);

    if (elapsed > SPLASH_DEFAULT_MS - SPLASH_FADE_OUT_MS) {
        long long left = SPLASH_DEFAULT_MS - elapsed;
        if (left < 0)
            left = 0;
        return (int)(left * 255 / SPLASH_FADE_OUT_MS);
    }

    return 255;
}

static void display_surface(OL_Surface *s)
{
    if (!gfx_gu_started()) {
        /* No GE: never block a host-like or failed boot. */
        return;
    }

    begin_gfx();

    const unsigned int skip_mask =
        PSP_CTRL_CROSS | PSP_CTRL_CIRCLE | PSP_CTRL_START;
    unsigned int previous = read_buttons();
    long long start = now_ms();

    for (;;) {
        long long elapsed = now_ms() - start;
        present(s, fade_alpha(elapsed));

        unsigned int current = read_buttons();
        if ((current & ~previous) & skip_mask)
            break;
        previous = current;

        if (elapsed >= SPLASH_DEFAULT_MS)
            break;
    }
}

#else

static void display_surface(OL_Surface *s)
{
    (void)s;    /* Host tests validate loading only. */
}

#endif

/* ---------------------------------------------------------------------- */
/* Runner                                                                  */
/* ---------------------------------------------------------------------- */

static void run_request(const SplashRequest *req)
{
    memset(&g_stats, 0, sizeof(g_stats));
    g_stats.requested = req->count;

    for (int i = 0; i < req->count; i++) {
        OL_Surface *s = load_surface(req->paths[i]);
        if (!s) {
            g_stats.missing++;
            continue;
        }

        g_stats.loaded++;
        display_surface(s);
        gfx_surface_free(s);
    }

    if (g_stats.loaded == 0) {
        g_stats.fallback = 1;
        OL_Surface *s = make_fallback_surface();
        if (s) {
            display_surface(s);
            gfx_surface_free(s);
        }
    }

#if defined(__PSP__)
    end_gfx();
#endif
}

/* ---------------------------------------------------------------------- */
/* Lua bindings                                                            */
/* ---------------------------------------------------------------------- */

static int l_show(lua_State *L)
{
    SplashRequest req;
    collect_paths(L, &req, 1, DEFAULT_NEW);
    run_request(&req);
    return 0;
}

static int l_old(lua_State *L)
{
    SplashRequest req;
    collect_paths(L, &req, 1, DEFAULT_OLD);
    run_request(&req);
    return 0;
}

static int l_oldschool(lua_State *L)
{
    SplashRequest req;
    memset(&req, 0, sizeof(req));
    add_path(L, &req, DEFAULT_OLDSCHOOL);
    run_request(&req);
    return 0;
}

#if !defined(__PSP__)
static int l_stats(lua_State *L)
{
    lua_newtable(L);
    lua_pushinteger(L, g_stats.requested);
    lua_setfield(L, -2, "requested");
    lua_pushinteger(L, g_stats.loaded);
    lua_setfield(L, -2, "loaded");
    lua_pushinteger(L, g_stats.missing);
    lua_setfield(L, -2, "missing");
    lua_pushinteger(L, g_stats.fallback);
    lua_setfield(L, -2, "fallback");
    return 1;
}
#endif

static const luaL_Reg splash_funcs[] = {
    { "show",      l_show },
    { "old",       l_old },
    { "oldschool", l_oldschool },
#if !defined(__PSP__)
    { "_stats",    l_stats },   /* host test hook only */
#endif
    { NULL, NULL }
};

void splash_api_register(lua_State *L)
{
    lua_newtable(L);
    ol_setfuncs(L, splash_funcs);
    lua_setglobal(L, "splash");
}
