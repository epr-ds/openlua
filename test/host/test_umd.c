/* Host checks for the UMD module's pure helpers.
 *
 * Actual mount/probing is PSP-only; the Lua surface and the safe absent-drive
 * contract are covered by test_peripherals.lua.
 */
#include <stdio.h>
#include <string.h>

#include "engine/sys/umd.h"

static int checks;
static int failures;

static void expect_str(const char *what, const char *got, const char *want)
{
    checks++;
    if (strcmp(got, want) != 0) {
        failures++;
        printf("umd: %s: got \"%s\", want \"%s\"\n", what, got, want);
    }
}

static void expect_size(const char *what, unsigned long long bytes,
                        const char *want)
{
    char buf[32];
    umd_format_size(bytes, buf, sizeof(buf));
    expect_str(what, buf, want);
}

int main(void)
{
    expect_str("type game", umd_type_name(0x10), "GAME");
    expect_str("type video", umd_type_name(0x20), "VIDEO");
    expect_str("type audio", umd_type_name(0x40), "AUDIO");
    expect_str("type none", umd_type_name(0), "-");
    expect_str("type unknown", umd_type_name(0x80), "-");
    expect_str("type priority", umd_type_name(0x30), "GAME");

    expect_size("zero", 0, "0.0000");
    expect_size("one GiB", 1024ull * 1024ull * 1024ull, "1.0000");
    expect_size("1.5 GiB", 1536ull * 1024ull * 1024ull, "1.5000");
    expect_size("1.1 GiB", 1181116006ull, "1.1000");
    expect_size("4.7 GiB", 5046586572ull, "4.7000");
    expect_size("small", 1024ull * 1024ull, "0.0010");

    printf("umd: %d checks, %d failure(s)\n", checks, failures);
    return failures ? 1 : 0;
}
