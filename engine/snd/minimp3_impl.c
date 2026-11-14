/* minimp3 single-header decoder implementation.
 *
 * minimp3 is CC0 (public domain): https://github.com/lieff/minimp3
 * The decoder is used by engine/snd/sound.c to stream MP3 menu music.
 */
#define MINIMP3_IMPLEMENTATION
#define MINIMP3_NO_SIMD
#include "third_party/minimp3/minimp3.h"
