/* Host tests for the pure OneLua os formatting helpers. */
#include "engine/sys/os.h"

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

static void expect_date(int year, int month, int day,
                        int hour, int minute, int second,
                        const char *want, const char *msg)
{
    char buf[40];
    os_format_date(year, month, day, hour, minute, second, buf, sizeof(buf));
    check(strcmp(buf, want) == 0, msg);
}

static void expect_mac(const unsigned char mac[6], const char *want,
                       const char *msg)
{
    char buf[24];
    os_format_mac(mac, buf, sizeof(buf));
    check(strcmp(buf, want) == 0, msg);
}

int main(void)
{
    static const unsigned char host_mac[6] = {
        0x02, 0x4F, 0x4C, 0x55, 0x41, 0x01
    };
    static const unsigned char zero_mac[6] = { 0, 0, 0, 0, 0, 0 };

    expect_date(2026, 10, 21, 23, 4, 5, "21/10/2026 @ 23:04:05",
                "date zero-pads all fields");
    expect_date(2026, 1, 2, 3, 4, 5, "02/01/2026 @ 03:04:05",
                "date single-digit day/month");
    expect_date(999, 12, 31, 23, 59, 59, "31/12/0999 @ 23:59:59",
                "date four-digit year");
    expect_date(0, 0, 0, 0, 0, 0, "00/00/0000 @ 00:00:00",
                "date fallback values");

    expect_mac(host_mac, "02:4F:4C:55:41:01", "mac uppercase hex");
    expect_mac(zero_mac, "00:00:00:00:00:00", "mac zero");

    check(strcmp(os_language_name(1), "ENGLISH") == 0,
          "language English");
    check(strcmp(os_language_name(2), "FRENCH") == 0,
          "language French");
    check(strcmp(os_language_name(11), "UNKNOWN") == 0,
          "language unknown id");
    check(strcmp(os_language_name(-1), "UNKNOWN") == 0,
          "language negative id");

    printf("\nRESULT: %d passed, %d failed", checks, fails);
    if (fails > 0)
        printf(" (os helper tests)\n");
    else
        printf("\n");
    return fails ? 1 : 0;
}
