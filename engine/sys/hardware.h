#ifndef OL_HARDWARE_H
#define OL_HARDWARE_H

#include <stddef.h>

#include <lua.h>

void hardware_api_register(lua_State *L);

/* Pure formatting, validation and probe helpers.  They are shared by the PSP
 * and host builds and are covered directly by test/host/test_hardware.c. */
void hw_format_hex(unsigned int value, char *out, size_t cap);
void hw_format_firmware(unsigned int version, char *out, size_t cap);
void hw_format_nand_mb(unsigned int megabytes, char *out, size_t cap);
const char *hw_model_name(int model);
const char *hw_region_name(int region);

/* hw.getmodel() decision, kept pure for host tests.  ef0_blocks/ef0_frsize
 * come from statvfs("ef0:/"); zero capacity means the volume is absent or an
 * empty stub, not a PSP Go with usable internal storage. */
const char *hw_model_from_probe(int emulator_nickname,
                                unsigned long long ef0_blocks,
                                unsigned long long ef0_frsize);
int hw_clamp_volume(int level);
int hw_clamp_mute(int mode);
int hw_clamp_audioout(int mode);
int hw_valid_led(int led);

#endif
