/* Host tests for the pure OneLua hardware formatting/clamping helpers. */
#include "engine/sys/hardware.h"

#include <stdio.h>
#include <string.h>

static int checks;
static int fails;

static void check(int cond, const char *msg)
{
    checks++;
    if (!cond) {
        fails++;
        printf("  FAIL: %s\n", msg);
    }
}

static void expect_hex(unsigned int value, const char *want, const char *msg)
{
    char buf[24];
    hw_format_hex(value, buf, sizeof(buf));
    check(strcmp(buf, want) == 0, msg);
}

static void expect_firmware(unsigned int value, const char *want,
                            const char *msg)
{
    char buf[24];
    hw_format_firmware(value, buf, sizeof(buf));
    check(strcmp(buf, want) == 0, msg);
}

static void expect_nand(unsigned int mb, const char *want, const char *msg)
{
    char buf[24];
    hw_format_nand_mb(mb, buf, sizeof(buf));
    check(strcmp(buf, want) == 0, msg);
}

int main(void)
{
    expect_hex(0x1234ABCDu, "0x1234ABCD", "hex uppercase and padded");
    expect_hex(0u, "0x00000000", "hex zero");
    expect_hex(0xFFFFFFFFu, "0xFFFFFFFF", "hex maximum");

    expect_firmware(0x06060010u, "6.60", "firmware 6.60");
    expect_firmware(0x05050010u, "5.50", "firmware 5.50");
    expect_firmware(0x06060110u, "6.61", "firmware 6.61");
    expect_firmware(0x00000010u, "UNK", "firmware zero major");
    expect_firmware(0u, "UNK", "firmware zero version");

    expect_nand(64u, "64MB", "NAND 64MB");
    expect_nand(128u, "128MB", "NAND 128MB");
    expect_nand(0u, "UNK", "NAND unknown");

    check(strcmp(hw_model_name(0), "1000") == 0, "model 1000");
    check(strcmp(hw_model_name(1), "2000") == 0, "model 2000");
    check(strcmp(hw_model_name(2), "3000") == 0, "model 3000");
    check(strcmp(hw_model_name(3), "Go") == 0, "model Go");
    check(strcmp(hw_model_name(4), "Street") == 0, "model Street");
    check(strcmp(hw_model_name(5), "Vita") == 0, "model Vita");
    check(strcmp(hw_model_name(6), "Emulator") == 0, "model Emulator");
    check(strcmp(hw_model_name(99), "UNK") == 0, "model unknown");
    check(strcmp(hw_model_name(-1), "UNK") == 0, "model negative unknown");

    check(strcmp(hw_region_name(0), "Japan") == 0, "region Japan");
    check(strcmp(hw_region_name(1), "America") == 0, "region America");
    check(strcmp(hw_region_name(2), "Australia") == 0, "region Australia");
    check(strcmp(hw_region_name(3), "United Kingdom") == 0,
          "region United Kingdom");
    check(strcmp(hw_region_name(4), "Europe") == 0, "region Europe");
    check(strcmp(hw_region_name(5), "Korea") == 0, "region Korea");
    check(strcmp(hw_region_name(6), "Hong-Kong") == 0, "region Hong-Kong");
    check(strcmp(hw_region_name(7), "Taiwan") == 0, "region Taiwan");
    check(strcmp(hw_region_name(8), "Russia") == 0, "region Russia");
    check(strcmp(hw_region_name(9), "China") == 0, "region China");
    check(strcmp(hw_region_name(10), "Mexico") == 0, "region Mexico");
    check(strcmp(hw_region_name(11), "--") == 0, "region unknown");
    check(strcmp(hw_region_name(-1), "--") == 0, "region negative unknown");

    check(hw_clamp_volume(-5) == 1, "volume negative clamps to 1");
    check(hw_clamp_volume(0) == 1, "volume zero clamps to 1");
    check(hw_clamp_volume(1) == 1, "volume lower bound accepted");
    check(hw_clamp_volume(15) == 15, "volume middle accepted");
    check(hw_clamp_volume(30) == 30, "volume upper bound accepted");
    check(hw_clamp_volume(31) == 30, "volume over maximum clamps to 30");
    check(hw_clamp_volume(999) == 30, "volume large clamps to 30");

    check(hw_clamp_mute(-2) == 1, "mute negative clamps to 1");
    check(hw_clamp_mute(0) == 0, "mute zero");
    check(hw_clamp_mute(1) == 1, "mute one");
    check(hw_clamp_mute(2) == 1, "mute over one clamps to 1");
    check(hw_clamp_audioout(-2) == 1, "audioout negative clamps to 1");
    check(hw_clamp_audioout(0) == 0, "audioout headphones");
    check(hw_clamp_audioout(1) == 1, "audioout speakers");
    check(hw_clamp_audioout(9) == 1, "audioout over one clamps to 1");

    check(hw_valid_led(0), "LED MS valid");
    check(hw_valid_led(1), "LED WLAN valid");
    check(hw_valid_led(2), "LED power valid");
    check(hw_valid_led(3), "LED BT valid");
    check(!hw_valid_led(-1), "LED negative invalid");
    check(!hw_valid_led(4), "LED over maximum invalid");

    printf("\nRESULT: %d passed, %d failed", checks, fails);
    if (fails > 0)
        printf(" (hw helper tests)\n");
    else
        printf("\n");
    return fails ? 1 : 0;
}
