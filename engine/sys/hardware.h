#ifndef OL_HARDWARE_H
#define OL_HARDWARE_H

#include <stddef.h>

#include <lua.h>

void hardware_api_register(lua_State *L);

/* Pure formatting and validation helpers.  They are shared by the PSP and
 * host builds and are covered directly by test/host/test_hardware.c. */
void hw_format_hex(unsigned int value, char *out, size_t cap);
void hw_format_firmware(unsigned int version, char *out, size_t cap);
void hw_format_nand_mb(unsigned int megabytes, char *out, size_t cap);
const char *hw_model_name(int model);
const char *hw_region_name(int region);
int hw_clamp_volume(int level);
int hw_clamp_mute(int mode);
int hw_clamp_audioout(int mode);
int hw_valid_led(int led);

#endif
