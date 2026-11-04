/* OpenLua - OneLua-compatible USB camera (GO! CAM) module.
 *
 * See cam.h for the Lua surface.  The driver path uses the PSPSDK
 * sceUsb/sceUsbCam APIs.  Video frames arrive as JPEG data; they are decoded
 * in memory with stb_image and either blitted directly (cam.render) or handed
 * to the normal image ownership path (cam.toimage).
 *
 * Host builds deliberately behave as if no camera is attached.  That keeps the
 * normal host suite from touching camera hardware while still covering the
 * documented absent-hardware contract.  The active path is built against the
 * real PSPSDK headers and verified in PPSSPP (dummy camera frames).
 */

#include "engine/sys/cam.h"
#include "engine/core/olreg.h"
#include "engine/gfx/gfx.h"
#include "engine/img/image.h"

#include <lauxlib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "stb_image_write.h"

#define CAM_WIDTH       480
#define CAM_HEIGHT      272
#define CAM_FRAME_MAX   (512u * 1024u)
#define CAM_WORK_MAX    (1024u * 1024u)
#define CAM_REVERSE_MIRROR 0x100

typedef enum {
    CAM_BRIGHTNESS,
    CAM_CONTRAST,
    CAM_SHARPNESS,
    CAM_SATURATION,
    CAM_EFFECT,
    CAM_EV,
    CAM_ZOOM,
    CAM_MIRROR
} CamControl;

static int g_inited;
static int g_video;
static unsigned char *g_work;
static unsigned char *g_frame;

static int clamp_int(int value, int lo, int hi)
{
    if (value < lo)
        return lo;
    if (value > hi)
        return hi;
    return value;
}

int cam_effect_to_psp(int value)
{
    return clamp_int(value, 1, 7) - 1;
}

int cam_ev_to_psp(int value)
{
    return clamp_int(value, 1, 17) - 1;
}

int cam_zoom_to_psp(int value)
{
    return clamp_int(value, 1, 5) * 10;
}

int cam_effect_from_psp(int value)
{
    if (value < 0 || value > 6)
        return -1;
    return value + 1;
}

int cam_ev_from_psp(int value)
{
    if (value < 0 || value > 16)
        return -1;
    return value + 1;
}

int cam_zoom_from_psp(int value)
{
    if (value < 10 || value > 50)
        return -1;
    return value / 10;
}

#if defined(__PSP__)

#include <pspusb.h>
#include <pspusbcam.h>

static int cam_hw_init(void)
{
    return sceUsbStart(PSP_USBCAM_DRIVERNAME, 0, NULL);
}

static int cam_hw_term(void)
{
    return sceUsbStop(PSP_USBCAM_DRIVERNAME, 0, NULL);
}

static int cam_hw_available(void)
{
    int state = sceUsbGetState();
    if (state < 0)
        return 0;
    return (state & PSP_USB_CABLE_CONNECTED) != 0;
}

static int cam_hw_begin(void)
{
    PspUsbCamSetupVideoParam param;

    memset(&param, 0, sizeof(param));
    param.size       = (int)sizeof(param);
    param.resolution = PSP_USBCAM_RESOLUTION_480_272;
    param.framerate  = PSP_USBCAM_FRAMERATE_30_FPS;
    param.wb         = PSP_USBCAM_WB_AUTO;
    param.saturation = 125;
    param.brightness = 128;
    param.contrast   = 64;
    param.sharpness  = 0;
    param.effectmode = PSP_USBCAM_EFFECTMODE_NORMAL;
    param.framesize  = (int)CAM_FRAME_MAX;
    param.evlevel    = PSP_USBCAM_EVLEVEL_0_0;

    if (sceUsbCamSetupVideo(&param, g_work, (int)CAM_WORK_MAX) < 0)
        return -1;
    if (sceUsbCamStartVideo() < 0)
        return -1;
    return 0;
}

static int cam_hw_end(void)
{
    return sceUsbCamStopVideo();
}

static int cam_hw_read_frame(unsigned char *buf, int cap)
{
    return sceUsbCamReadVideoFrameBlocking(buf, (SceSize)cap);
}

static int cam_hw_get(CamControl control, int *out)
{
    switch (control) {
    case CAM_BRIGHTNESS: return sceUsbCamGetBrightness(out);
    case CAM_CONTRAST:   return sceUsbCamGetContrast(out);
    case CAM_SHARPNESS:  return sceUsbCamGetSharpness(out);
    case CAM_SATURATION: return sceUsbCamGetSaturation(out);
    case CAM_EFFECT:     return sceUsbCamGetImageEffectMode(out);
    case CAM_EV:         return sceUsbCamGetEvLevel(out);
    case CAM_ZOOM:       return sceUsbCamGetZoom(out);
    case CAM_MIRROR:     return sceUsbCamGetReverseMode(out);
    default:             return -1;
    }
}

static int cam_hw_set(CamControl control, int value)
{
    switch (control) {
    case CAM_BRIGHTNESS: return sceUsbCamSetBrightness(value);
    case CAM_CONTRAST:   return sceUsbCamSetContrast(value);
    case CAM_SHARPNESS:  return sceUsbCamSetSharpness(value);
    case CAM_SATURATION: return sceUsbCamSetSaturation(value);
    case CAM_EFFECT:     return sceUsbCamSetImageEffectMode(value);
    case CAM_EV:         return sceUsbCamSetEvLevel(value);
    case CAM_ZOOM:       return sceUsbCamSetZoom(value);
    case CAM_MIRROR:
        return sceUsbCamSetReverseMode(value ? PSP_USBCAM_MIRROR : 0);
    default:             return -1;
    }
}

static int cam_hw_direction(void)
{
    return sceUsbCamGetLensDirection();
}

static int cam_hw_autoreverse_get(void)
{
    return sceUsbCamGetAutoImageReverseState();
}

static int cam_hw_autoreverse_set(int value)
{
    return sceUsbCamAutoImageReverseSW(value ? 1 : 0);
}

#else /* host: no camera, exercise the safe failure path */

static int cam_hw_init(void) { return -1; }
static int cam_hw_term(void) { return -1; }
static int cam_hw_available(void) { return 0; }
static int cam_hw_begin(void) { return -1; }
static int cam_hw_end(void) { return -1; }
static int cam_hw_read_frame(unsigned char *buf, int cap)
{
    (void)buf;
    (void)cap;
    return -1;
}
static int cam_hw_get(CamControl control, int *out)
{
    (void)control;
    (void)out;
    return -1;
}
static int cam_hw_set(CamControl control, int value)
{
    (void)control;
    (void)value;
    return -1;
}
static int cam_hw_direction(void) { return -1; }
static int cam_hw_autoreverse_get(void) { return -1; }
static int cam_hw_autoreverse_set(int value)
{
    (void)value;
    return -1;
}

#endif

static int read_frame(void)
{
    if (!g_video || !g_frame)
        return 0;
    int len = cam_hw_read_frame(g_frame, (int)CAM_FRAME_MAX);
    if (len <= 0 || (unsigned int)len > CAM_FRAME_MAX)
        return 0;
    return len;
}

static int save_png(const char *path, OL_Surface *s)
{
    unsigned char *flat;
    int ok;

    if (!path || !s || s->w <= 0 || s->h <= 0)
        return 0;

    flat = (unsigned char *)malloc((size_t)s->w * (size_t)s->h * 4u);
    if (!flat)
        return 0;
    for (int y = 0; y < s->h; y++) {
        memcpy(flat + (size_t)y * (size_t)s->w * 4u,
               &s->pixels[(size_t)y * (size_t)s->realW],
               (size_t)s->w * 4u);
    }
    ok = stbi_write_png(path, s->w, s->h, 4, flat, s->w * 4);
    free(flat);
    return ok ? 1 : 0;
}

static int l_init(lua_State *L)
{
    (void)L;
    if (g_inited) {
        lua_pushinteger(L, 1);
        return 1;
    }
    if (cam_hw_init() < 0) {
        lua_pushinteger(L, 0);
        return 1;
    }
    g_inited = 1;
    lua_pushinteger(L, 1);
    return 1;
}

static int l_term(lua_State *L)
{
    int ok = 1;
    (void)L;

    if (g_video) {
        if (cam_hw_end() < 0)
            ok = 0;
        g_video = 0;
    }
    if (g_inited) {
        if (cam_hw_term() < 0)
            ok = 0;
        g_inited = 0;
    }

    free(g_frame);
    free(g_work);
    g_frame = NULL;
    g_work = NULL;
    lua_pushinteger(L, ok);
    return 1;
}

static int l_state(lua_State *L)
{
    lua_pushboolean(L, g_inited);
    return 1;
}

static int l_available(lua_State *L)
{
    lua_pushboolean(L, g_inited && cam_hw_available() > 0);
    return 1;
}

static int l_begin(lua_State *L)
{
    (void)L;
    if (!g_inited) {
        lua_pushinteger(L, 0);
        return 1;
    }
    if (g_video) {
        lua_pushinteger(L, 1);
        return 1;
    }

    g_work = (unsigned char *)malloc(CAM_WORK_MAX);
    g_frame = (unsigned char *)malloc(CAM_FRAME_MAX);
    if (!g_work || !g_frame) {
        free(g_frame);
        free(g_work);
        g_frame = NULL;
        g_work = NULL;
        lua_pushinteger(L, 0);
        return 1;
    }

    if (cam_hw_begin() < 0) {
        free(g_frame);
        free(g_work);
        g_frame = NULL;
        g_work = NULL;
        lua_pushinteger(L, 0);
        return 1;
    }

    g_video = 1;
    lua_pushinteger(L, 1);
    return 1;
}

static int l_render(lua_State *L)
{
    int x = (int)luaL_checkinteger(L, 1);
    int y = (int)luaL_checkinteger(L, 2);
    int target_w = (lua_gettop(L) >= 4) ? (int)luaL_checkinteger(L, 3) : 0;
    int target_h = (lua_gettop(L) >= 4) ? (int)luaL_checkinteger(L, 4) : 0;
    int len = read_frame();
    if (len <= 0)
        return 0;

    OL_Surface *s = image_surface_from_memory(g_frame, (size_t)len);
    if (!s)
        return 0;

    float sx = 1.0f;
    float sy = 1.0f;
    if (target_w > 0 && target_h > 0) {
        sx = (float)target_w / (float)s->w;
        sy = (float)target_h / (float)s->h;
    }

    gfx_blit(s, x, y, 0, 0, s->w, s->h, 255,
             0, 0, 0.0f, sx, sy, 0, 0xFFFFFFFFu);
    gfx_surface_free(s);
    return 0;
}

static int l_shot(lua_State *L)
{
    const char *path = luaL_checkstring(L, 1);
    int len = read_frame();
    if (len <= 0) {
        lua_pushinteger(L, 0);
        return 1;
    }

    OL_Surface *s = image_surface_from_memory(g_frame, (size_t)len);
    if (!s) {
        lua_pushinteger(L, 0);
        return 1;
    }

    int ok = save_png(path, s);
    gfx_surface_free(s);
    lua_pushinteger(L, ok);
    return 1;
}

static int l_toimage(lua_State *L)
{
    int len = read_frame();
    if (len <= 0) {
        lua_pushnil(L);
        return 1;
    }
    if (!image_push_memory(L, g_frame, (size_t)len)) {
        lua_pushnil(L);
        return 1;
    }
    return 1;
}

static int l_direction(lua_State *L)
{
    int value = g_video ? cam_hw_direction() : 0;
    lua_pushinteger(L, value >= 0 ? value : 0);
    return 1;
}

static int read_control(lua_State *L, CamControl control)
{
    int raw = -1;
    if (g_video && cam_hw_get(control, &raw) >= 0) {
        switch (control) {
        case CAM_EFFECT:
            lua_pushinteger(L, cam_effect_from_psp(raw));
            break;
        case CAM_EV:
            lua_pushinteger(L, cam_ev_from_psp(raw));
            break;
        case CAM_ZOOM:
            lua_pushinteger(L, cam_zoom_from_psp(raw));
            break;
        case CAM_MIRROR:
            lua_pushinteger(L, (raw & CAM_REVERSE_MIRROR) ? 1 : 0);
            break;
        default:
            lua_pushinteger(L, raw);
            break;
        }
    } else {
        lua_pushinteger(L, -1);
    }
    return 1;
}

static int write_control(lua_State *L, CamControl control, int value)
{
    (void)L;
    if (g_video)
        (void)cam_hw_set(control, value);
    return 0;
}

static int l_brightness(lua_State *L)
{
    if (lua_gettop(L) == 0)
        return read_control(L, CAM_BRIGHTNESS);
    return write_control(L, CAM_BRIGHTNESS,
                         clamp_int((int)luaL_checkinteger(L, 1), 0, 255));
}

static int l_contrast(lua_State *L)
{
    if (lua_gettop(L) == 0)
        return read_control(L, CAM_CONTRAST);
    return write_control(L, CAM_CONTRAST,
                         clamp_int((int)luaL_checkinteger(L, 1), 0, 255));
}

static int l_sharpness(lua_State *L)
{
    if (lua_gettop(L) == 0)
        return read_control(L, CAM_SHARPNESS);
    return write_control(L, CAM_SHARPNESS,
                         clamp_int((int)luaL_checkinteger(L, 1), 0, 255));
}

static int l_saturation(lua_State *L)
{
    if (lua_gettop(L) == 0)
        return read_control(L, CAM_SATURATION);
    return write_control(L, CAM_SATURATION,
                         clamp_int((int)luaL_checkinteger(L, 1), 0, 255));
}

static int l_effect(lua_State *L)
{
    if (lua_gettop(L) == 0)
        return read_control(L, CAM_EFFECT);
    return write_control(L, CAM_EFFECT,
                         cam_effect_to_psp((int)luaL_checkinteger(L, 1)));
}

static int l_evlevel(lua_State *L)
{
    if (lua_gettop(L) == 0)
        return read_control(L, CAM_EV);
    return write_control(L, CAM_EV,
                         cam_ev_to_psp((int)luaL_checkinteger(L, 1)));
}

static int l_zoom(lua_State *L)
{
    if (lua_gettop(L) == 0)
        return read_control(L, CAM_ZOOM);
    return write_control(L, CAM_ZOOM,
                         cam_zoom_to_psp((int)luaL_checkinteger(L, 1)));
}

static int l_mirror(lua_State *L)
{
    if (lua_gettop(L) == 0)
        return read_control(L, CAM_MIRROR);
    return write_control(L, CAM_MIRROR,
                         (int)luaL_checkinteger(L, 1) ? 1 : 0);
}

static int l_autoreverse(lua_State *L)
{
    if (lua_gettop(L) == 0) {
        int value = g_video ? cam_hw_autoreverse_get() : -1;
        lua_pushinteger(L, value >= 0 ? (value ? 1 : 0) : 0);
        return 1;
    }

    if (g_video)
        (void)cam_hw_autoreverse_set((int)luaL_checkinteger(L, 1));
    return 0;
}

static const luaL_Reg cam_funcs[] = {
    { "init",        l_init        },
    { "begin",       l_begin       },
    { "state",       l_state       },
    { "available",   l_available   },
    { "render",      l_render      },
    { "shot",        l_shot        },
    { "toimage",     l_toimage     },
    { "direction",   l_direction   },
    { "brightness",  l_brightness  },
    { "contrast",    l_contrast    },
    { "sharpness",   l_sharpness   },
    { "saturation",  l_saturation  },
    { "zoom",        l_zoom        },
    { "evlevel",     l_evlevel     },
    { "effect",      l_effect      },
    { "autoreverse", l_autoreverse },
    { "mirror",      l_mirror      },
    { "term",        l_term        },
    { NULL, NULL }
};

void cam_api_register(lua_State *L)
{
    lua_newtable(L);
    ol_setfuncs(L, cam_funcs);
    lua_setglobal(L, "cam");
}
