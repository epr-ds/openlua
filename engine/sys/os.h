#ifndef OL_OS_H
#define OL_OS_H

#include <stddef.h>

#include <lua.h>

void os_api_register(lua_State *L);

/* Pure formatting helpers shared by the PSP and host builds. */
void os_format_date(int year, int month, int day,
                    int hour, int minute, int second,
                    char *out, size_t cap);
void os_format_mac(const unsigned char mac[6], char *out, size_t cap);
const char *os_language_name(int language);

#endif
