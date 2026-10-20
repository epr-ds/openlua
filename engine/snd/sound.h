/* OpenLua - audio system (public API)
 *
 * OneLua-compatible sound module built on the native PSP audio path:
 *
 *   local s = sound.load("assets/beep.wav") -- load 16-bit PCM (WAV / VMA)
 *   sound.play(s)                          -- play (auto channel)
 *   sound.play(s, 2)                       -- play on channel group 2
 *   sound.loop(s)                          -- toggle loop, returns state
 *   sound.pause(s)                         -- pause (mode: -1 toggle, 0 resume, 1 pause)
 *   sound.stop(s)                          -- stop
 *   sound.vol(s, 50)                       -- set volume 0..100
 *   sound.vol(s)                           -- get volume
 *   sound.playing(s)                       -- bool
 *   sound.endstream(s)                     -- bool: finished/stopped?
 *   sound.rate(s)                          -- sample rate Hz (22050 after resample)
 *
 * The audio pump runs automatically once per screen.flip() — match OneLua's
 * contract, where the game loop just needs to keep flipping.  Scripts do
 * not call a sound pump manually.
 *
 * Supported formats: 16-bit PCM WAV (mono/stereo, any sample rate; resampled
 * to 22050 Hz mono on load) and PSP VMA (22050 Hz mono, raw PCM16 after a
 * 4-byte sample-count header).  MP3 / S3M decoding is a future extension
 * (see issue #3).
 */

#ifndef OL_SOUND_H
#define OL_SOUND_H

#include <lua.h>
#include <lauxlib.h>

#define OL_SOUND_MAX 4

/* ---- C API ---- */

int  sound_alloc(const char *path);          /* 1-based handle, 0 on error  */
void sound_free(int h);

int  sound_play(int h);                       /* start/resume                */
int  sound_play_channel(int h, int ch);       /* start/resume on PSP ch 0..7 */
int  sound_pause(int h, int mode);            /* -1 toggle, 0 resume, 1 pause */
int  sound_stop(int h);                       /* stop & mark finished        */
int  sound_loop(int h);                       /* toggle loop, returns state  */
int  sound_looping(int h);                    /* 0/1                         */
int  sound_vol(int h, int pct);               /* set(0..100) / get(pct<0)   */
int  sound_playing(int h);                    /* 0/1                         */
int  sound_endstream(int h);                  /* 0/1 finished                */
int  sound_rate(int h);                       /* Hz                          */
int  sound_channels(int h);                   /* 1 or 2                      */

/** Pump: feed one chunk to any playing sound whose channel buffer drained.
 *  Called once per screen.flip() from the core — scripts do not call it. */
void sound_pump(void);
void sound_update(void);                      /* pump: call once per frame   */

/* ---- Lua bindings ---- */

/** Register global `sound` table. Call after register_api core tables. */
void sound_api_register(lua_State *L);

#endif /* OL_SOUND_H */
