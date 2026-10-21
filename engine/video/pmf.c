/* OpenLua - PMF / PSMF playback through the PSP sceMpeg libraries.
 *
 * pmf.run() is deliberately blocking, matching OneLua's contract: while a
 * movie is playing the script sits inside the call and onPmfPlay(state) is
 * invoked every displayed frame.  A callback can therefore call pmf.pause(),
 * pmf.play() or pmf.stop() to control the movie.
 *
 * Video is decoded to a 512-wide 8888 buffer and composited into the inactive
 * GE framebuffer at the requested x/y.  This works for both full-screen
 * movies and the small gameboot ICON1.PMF clips typically used by games.
 */

#include "engine/video/pmf.h"
#include "engine/data/psmf.h"
#include "engine/core/olreg.h"
#include "engine/gfx/gfx.h"

#include <pspkernel.h>
#include <pspdisplay.h>
#include <pspaudio.h>
#include <pspgu.h>
#include <pspmpeg.h>
#include <psputility.h>
#include <pspiofilemgr.h>
#include <psputils.h>

#include <lauxlib.h>
#include <lualib.h>

#include <malloc.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PMF_BUF_WIDTH 512
#define PMF_SCR_W     480
#define PMF_SCR_H     272
#define PMF_RING_PKTS 512

/* PSP headers describe only the public prefix of the ringbuffer; the real
 * structure is larger (ppsspp and pspautotests both use this layout). */
typedef struct {
    SceInt32 packetsTotal;
    SceInt32 packetsRead;
    SceInt32 packetsWritten;
    SceInt32 packetsAvail;
    SceInt32 packetSize;
    void *data;
    sceMpegRingbufferCB callback;
    void *callbackArg;
    void *dataEnd;
    int unknownValue;
    SceMpeg mpeg;
    SceUInt32 gp;
} OlMpegRingbuffer;

typedef struct {
    SceUID fd;
    SceInt32 remaining;
} OlPmfIo;

typedef struct {
    SceMpeg mpeg;
    OlMpegRingbuffer rb;
    OlPmfIo io;

    void *rb_data;
    void *mpeg_data;
    void *header;

    SceMpegStream *avc_stream;
    SceMpegStream *atrac_stream;
    void *avc_es;
    void *atrac_es;
    SceMpegAu avc_au;
    SceMpegAu atrac_au;

    SceInt32 atrac_out_size;
    void *atrac_buf;
    int audio_ch;

    unsigned char *video_buf;
    void *fb[2];
    int fb_index;

    void *old_fb;
    int old_width;
    int old_format;
    int display_saved;

    int width;
    int height;
    int x;
    int y;

    uint64_t first_ts;
    uint64_t last_ts;
    int stream_offset;
    int stream_size;
} PmfPlayer;

static volatile int g_state; /* 0 idle, 1 play, 2 pause, 3 stop */
static int g_mpeg_inited;

static uint64_t au_pts(const SceMpegAu *au)
{
    return ((uint64_t)au->iPtsMSB << 32) | (uint64_t)au->iPts;
}

static void pmf_call_callback(lua_State *L, int playing)
{
    int top = lua_gettop(L);
    lua_getglobal(L, "onPmfPlay");
    if (lua_isfunction(L, -1)) {
        lua_pushboolean(L, playing);
        if (lua_pcall(L, 1, 0, 0) != 0)
            lua_pop(L, 1);
    } else {
        lua_pop(L, 1);
    }
    lua_settop(L, top);
}

static SceInt32 pmf_ring_callback(void *data, SceInt32 packets, void *param)
{
    OlPmfIo *io = (OlPmfIo *)param;
    SceInt32 bytes = packets * 2048;
    SceInt32 got;

    if (!io || io->remaining <= 0)
        return 0;
    if (bytes > io->remaining)
        bytes = io->remaining;

    got = sceIoRead(io->fd, data, (SceSize)bytes);
    if (got <= 0) {
        io->remaining = 0;
        return 0;
    }
    io->remaining -= got;
    return got / 2048;
}

static void pmf_restore_display(PmfPlayer *p)
{
    if (p->display_saved) {
        sceDisplaySetFrameBuf(p->old_fb, p->old_width, p->old_format,
                              PSP_DISPLAY_SETBUF_NEXTFRAME);
        sceDisplayWaitVblankStart();
        p->display_saved = 0;
    }
}

static void pmf_cleanup(PmfPlayer *p)
{
    if (p->audio_ch >= 0) {
        sceAudioChRelease(p->audio_ch);
        p->audio_ch = -1;
    }

    if (p->mpeg) {
        SceInt32 status = 0;
        if (p->video_buf)
            sceMpegAvcDecodeStop(&p->mpeg, PMF_BUF_WIDTH, &p->video_buf, &status);
        sceMpegFlushAllStream(&p->mpeg);

        if (p->avc_stream)
            sceMpegUnRegistStream(p->mpeg, p->avc_stream);
        if (p->atrac_stream)
            sceMpegUnRegistStream(p->mpeg, p->atrac_stream);
        if (p->avc_es)
            sceMpegFreeAvcEsBuf(&p->mpeg, p->avc_es);

        sceMpegDelete(&p->mpeg);
        p->mpeg = NULL;
    }

    if (p->rb_data && p->rb.packetsTotal > 0)
        sceMpegRingbufferDestruct((SceMpegRingbuffer *)&p->rb);
    if (g_mpeg_inited) {
        sceMpegFinish();
        g_mpeg_inited = 0;
    }

    pmf_restore_display(p);

    if (p->io.fd >= 0)
        sceIoClose(p->io.fd);

    free(p->video_buf);
    free(p->atrac_buf);
    free(p->atrac_es);
    free(p->rb_data);
    free(p->mpeg_data);
    free(p->header);

    p->video_buf = NULL;
    p->atrac_buf = NULL;
    p->atrac_es = NULL;
    p->avc_es = NULL;
    p->rb_data = NULL;
    p->mpeg_data = NULL;
    p->header = NULL;
    p->io.fd = -1;
}

static int pmf_present(PmfPlayer *p)
{
    void *back = p->fb[p->fb_index];

    {
        unsigned int *dst = (unsigned int *)back;
        sceKernelDcacheInvalidateRange(p->video_buf,
                                       PMF_BUF_WIDTH * PMF_SCR_H * 4);
        for (int row = 0; row < p->height; row++) {
            memcpy(dst + (p->y + row) * PMF_BUF_WIDTH + p->x,
                   p->video_buf + row * PMF_BUF_WIDTH,
                   (size_t)p->width * 4);
        }
    }

    sceDisplaySetFrameBuf(back, PMF_BUF_WIDTH, PSP_DISPLAY_PIXEL_FORMAT_8888,
                          PSP_DISPLAY_SETBUF_NEXTFRAME);
    sceDisplayWaitVblankStart();
    p->fb_index ^= 1;
    return 0;
}

/* Decode one ATRAC AU if available.  Returns 1 when a buffer was produced. */
static int pmf_decode_audio(PmfPlayer *p, SceInt32 *init)
{
    SceInt32 unknown = 0;

    if (!p->atrac_stream || !p->atrac_buf)
        return 0;
    if (sceMpegGetAtracAu(&p->mpeg, p->atrac_stream, &p->atrac_au,
                          &unknown) != 0)
        return 0;
    if (sceMpegAtracDecode(&p->mpeg, &p->atrac_au, p->atrac_buf, *init) != 0)
        return 0;

    if (*init == 0) {
        *init = 0;
        return 1;
    }
    *init = 0;
    return 0;
}

static const char *pmf_play(lua_State *L, const char *path, int x, int y)
{
    PmfPlayer p;
    OL_PsmfInfo info;
    SceInt32 stream_offset = 0;
    SceInt32 stream_size = 0;
    SceOff file_size;
    int ret;
    SceInt32 unknown = 0;
    SceInt32 video_status = 0;
    SceInt32 audio_init = 1;
    int stall = 0;
    uint64_t start_us;
    uint64_t last_target_us = 0;
    int was_paused = 0;
    const char *err = NULL;

    memset(&p, 0, sizeof(p));
    p.io.fd = -1;
    p.audio_ch = -1;

    p.io.fd = sceIoOpen(path, PSP_O_RDONLY, 0);
    if (p.io.fd < 0)
        return "cannot open file";

    file_size = sceIoLseek(p.io.fd, 0, PSP_SEEK_END);
    if (file_size < 0) {
        err = "cannot read file size";
        goto out;
    }
    sceIoLseek(p.io.fd, 0, PSP_SEEK_SET);

    p.header = memalign(64, PSMF_HEADER_SIZE);
    if (!p.header) {
        err = "out of memory";
        goto out;
    }
    if (sceIoRead(p.io.fd, p.header, PSMF_HEADER_SIZE) !=
        (SceSSize)PSMF_HEADER_SIZE) {
        err = "short PMF header";
        goto out;
    }
    if (psmf_parse_header((const unsigned char *)p.header, PSMF_HEADER_SIZE,
                          (uint64_t)file_size, &info) != 0) {
        err = "invalid PMF header";
        goto out;
    }

    p.width = info.width > 0 ? info.width : PMF_SCR_W;
    p.height = info.height > 0 ? info.height : PMF_SCR_H;
    if (p.width > PMF_SCR_W)
        p.width = PMF_SCR_W;
    if (p.height > PMF_SCR_H)
        p.height = PMF_SCR_H;
    p.x = x < 0 ? 0 : x;
    p.y = y < 0 ? 0 : y;
    if (p.x + p.width > PMF_SCR_W)
        p.x = PMF_SCR_W - p.width;
    if (p.y + p.height > PMF_SCR_H)
        p.y = PMF_SCR_H - p.height;
    p.first_ts = info.first_ts;
    p.last_ts = info.last_ts;

    p.fb[0] = gfx_vram_fb(0);
    p.fb[1] = gfx_vram_fb(1);
    if (!p.fb[0] || !p.fb[1]) {
        err = "graphics not initialized";
        goto out;
    }
    p.video_buf = memalign(64, PMF_BUF_WIDTH * PMF_SCR_H * 4);
    if (!p.video_buf) {
        err = "out of memory";
        goto out;
    }

    sceUtilityLoadModule(PSP_MODULE_AV_AVCODEC);
    sceUtilityLoadModule(PSP_MODULE_AV_ATRAC3PLUS);
    sceUtilityLoadModule(PSP_MODULE_AV_MP3);
    sceUtilityLoadModule(PSP_MODULE_AV_MPEGBASE);
    sceUtilityLoadModule(PSP_MODULE_AV_VAUDIO);

    if (sceMpegInit() != 0) {
        err = "sceMpegInit failed";
        goto out;
    }
    g_mpeg_inited = 1;

    {
        SceInt32 rb_size = sceMpegRingbufferQueryMemSize(PMF_RING_PKTS);
        SceInt32 mem_size = sceMpegQueryMemSize(0);
        SceMpegAvcMode mode;

        if (rb_size <= 0 || mem_size <= 0) {
            err = "sceMpeg memory query failed";
            goto out;
        }
        p.rb_data = malloc((size_t)rb_size);
        p.mpeg_data = malloc((size_t)mem_size);
        if (!p.rb_data || !p.mpeg_data) {
            err = "out of memory";
            goto out;
        }

        memset(&p.rb, 0, sizeof(p.rb));
        if (sceMpegRingbufferConstruct((SceMpegRingbuffer *)&p.rb,
                                       PMF_RING_PKTS, p.rb_data, rb_size,
                                       pmf_ring_callback, &p.io) != 0) {
            err = "sceMpegRingbufferConstruct failed";
            goto out;
        }

        if (sceMpegCreate(&p.mpeg, p.mpeg_data, mem_size,
                          (SceMpegRingbuffer *)&p.rb, PMF_BUF_WIDTH,
                          0, 0) != 0) {
            err = "sceMpegCreate failed";
            goto out;
        }

        mode.iUnk0 = -1;
        mode.iPixelFormat = GU_PSM_8888;
        if (sceMpegAvcDecodeMode(&p.mpeg, &mode) != 0) {
            err = "sceMpegAvcDecodeMode failed";
            goto out;
        }
    }

    ret = sceMpegQueryStreamOffset(&p.mpeg, p.header, &stream_offset);
    if (ret != 0) {
        stream_offset = (SceInt32)info.stream_offset;
    }
    ret = sceMpegQueryStreamSize(p.header, &stream_size);
    if (ret != 0 || stream_size <= 0) {
        stream_size = (SceInt32)info.stream_size;
    }
    if (stream_offset <= 0 || stream_size <= 0 ||
        (uint64_t)stream_offset + (uint64_t)stream_size > (uint64_t)file_size) {
        err = "invalid PMF stream range";
        goto out;
    }
    p.stream_offset = stream_offset;
    p.stream_size = stream_size;
    p.io.remaining = stream_size;

    if (sceIoLseek(p.io.fd, stream_offset, PSP_SEEK_SET) < 0) {
        err = "cannot seek to PMF stream";
        goto out;
    }

    p.avc_stream = sceMpegRegistStream(&p.mpeg, 0, 0);
    if (!p.avc_stream) {
        err = "sceMpegRegistStream(video) failed";
        goto out;
    }
    p.avc_es = sceMpegMallocAvcEsBuf(&p.mpeg);
    if (!p.avc_es) {
        err = "sceMpegMallocAvcEsBuf failed";
        goto out;
    }
    if (sceMpegInitAu(&p.mpeg, p.avc_es, &p.avc_au) != 0) {
        err = "sceMpegInitAu(video) failed";
        goto out;
    }

    p.atrac_stream = sceMpegRegistStream(&p.mpeg, 1, 0);
    if (p.atrac_stream) {
        SceInt32 es_size = 0;
        if (sceMpegQueryAtracEsSize(&p.mpeg, &es_size,
                                    &p.atrac_out_size) == 0 &&
            es_size > 0 && p.atrac_out_size > 0) {
            p.atrac_es = memalign(64, (size_t)es_size);
            p.atrac_buf = memalign(64, (size_t)p.atrac_out_size);
            if (!p.atrac_es || !p.atrac_buf) {
                err = "out of memory";
                goto out;
            }
            if (sceMpegInitAu(&p.mpeg, p.atrac_es, &p.atrac_au) != 0) {
                err = "sceMpegInitAu(audio) failed";
                goto out;
            }
            p.audio_ch = sceAudioChReserve(PSP_AUDIO_NEXT_CHANNEL, 512,
                                            PSP_AUDIO_FORMAT_STEREO);
            if (p.audio_ch < 0) {
                err = "sceAudioChReserve failed";
                goto out;
            }
            sceAudioSetChannelDataLen(p.audio_ch, p.atrac_out_size / 4);
        } else {
            p.atrac_stream = NULL;
        }
    }

    if (sceDisplayGetFrameBuf(&p.old_fb, &p.old_width, &p.old_format,
                              PSP_DISPLAY_SETBUF_IMMEDIATE) == 0)
        p.display_saved = 1;

    memset(p.fb[0], 0, PMF_BUF_WIDTH * PMF_SCR_H * 4);
    memset(p.fb[1], 0, PMF_BUF_WIDTH * PMF_SCR_H * 4);
    p.fb_index = 0;
    sceDisplaySetFrameBuf(p.fb[0], PMF_BUF_WIDTH,
                          PSP_DISPLAY_PIXEL_FORMAT_8888,
                          PSP_DISPLAY_SETBUF_NEXTFRAME);
    sceDisplayWaitVblankStart();

    g_state = 1;
    start_us = sceKernelGetSystemTimeWide();

    for (;;) {
        SceInt32 avail;
        int got_frame = 0;
        int audio_ready = 0;
        uint64_t pts = 0;
        uint64_t target_us = 0;

        if (g_state == 3)
            break;

        if (g_state != 1) {
            pmf_call_callback(L, 0);
            if (g_state == 3)
                break;
            was_paused = 1;
            sceKernelDelayThread(10000);
            continue;
        }
        if (was_paused) {
            start_us = sceKernelGetSystemTimeWide() - last_target_us;
            was_paused = 0;
        }

        avail = sceMpegRingbufferAvailableSize((SceMpegRingbuffer *)&p.rb);
        if (avail < 0) {
            err = "sceMpegRingbufferAvailableSize failed";
            break;
        }
        if (avail > 0 && p.io.remaining > 0) {
            SceInt32 want = avail;
            if (want > p.io.remaining / 2048)
                want = p.io.remaining / 2048;
            if (want > 0) {
                ret = sceMpegRingbufferPut((SceMpegRingbuffer *)&p.rb,
                                           want, avail);
                if (ret < 0) {
                    err = "sceMpegRingbufferPut failed";
                    break;
                }
            }
        }

        audio_ready = pmf_decode_audio(&p, &audio_init);

        ret = sceMpegGetAvcAu(&p.mpeg, p.avc_stream, &p.avc_au, &unknown);
        if (ret == 0) {
            pts = au_pts(&p.avc_au);
            if (p.last_ts > p.first_ts && pts > p.last_ts) {
                if (audio_ready && p.audio_ch >= 0)
                    sceAudioOutputBlocking(p.audio_ch, PSP_AUDIO_VOLUME_MAX,
                                           p.atrac_buf);
                break;
            }

            {
                void *dst = p.video_buf;
                ret = sceMpegAvcDecode(&p.mpeg, &p.avc_au, PMF_BUF_WIDTH,
                                       &dst, &video_status);
                if (ret != 0) {
                    err = "sceMpegAvcDecode failed";
                    break;
                }
                if (video_status == 1)
                    got_frame = 1;
            }
        } else if (p.io.remaining == 0 &&
                   avail == PMF_RING_PKTS) {
            break;
        } else if (++stall > 3000) {
            err = "PMF decoder stalled";
            break;
        }

        if (ret == 0)
            stall = 0;

        if (audio_ready && p.audio_ch >= 0)
            sceAudioOutputBlocking(p.audio_ch, PSP_AUDIO_VOLUME_MAX,
                                   p.atrac_buf);

        if (got_frame) {
            uint64_t now;

            pmf_present(&p);

            if (p.last_ts > p.first_ts && pts >= p.first_ts)
                target_us = ((pts - p.first_ts) * 1000000ull) / 90000ull;
            last_target_us = target_us;

            if (!p.atrac_stream) {
                now = sceKernelGetSystemTimeWide();
                if (now < start_us + target_us) {
                    uint64_t wait = start_us + target_us - now;
                    if (wait > 1000000ull)
                        wait = 1000000ull;
                    sceKernelDelayThread((SceUInt32)wait);
                }
            }

            pmf_call_callback(L, 1);
            if (g_state == 3)
                break;
        }

        sceKernelDelayThread(1000);
    }

    pmf_call_callback(L, 0);
    g_state = 0;

out:
    g_state = 0;
    pmf_cleanup(&p);
    return err;
}

static int l_pmf_run(lua_State *L)
{
    const char *path = luaL_checkstring(L, 1);
    int x = (int)luaL_optinteger(L, 2, 0);
    int y = (int)luaL_optinteger(L, 3, 0);
    const char *err;

    if (g_state != 0)
        return luaL_error(L, "pmf.run: playback already active");

    err = pmf_play(L, path, x, y);
    if (err)
        return luaL_error(L, "pmf.run: %s", err);
    return 0;
}

static int l_pmf_play(lua_State *L)
{
    (void)L;
    if (g_state == 2)
        g_state = 1;
    return 0;
}

static int l_pmf_pause(lua_State *L)
{
    (void)L;
    if (g_state == 1)
        g_state = 2;
    return 0;
}

static int l_pmf_stop(lua_State *L)
{
    (void)L;
    if (g_state != 0)
        g_state = 3;
    return 0;
}

static const luaL_Reg pmf_funcs[] = {
    { "run",   l_pmf_run },
    { "play",  l_pmf_play },
    { "pause", l_pmf_pause },
    { "stop",  l_pmf_stop },
    { NULL, NULL }
};

void pmf_api_register(lua_State *L)
{
    lua_newtable(L);
    ol_setfuncs(L, pmf_funcs);
    lua_setglobal(L, "pmf");
}
