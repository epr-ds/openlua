/* Host tests for the pure battery formatting helpers. */
#include "engine/sys/battery.h"

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

static void expect_lifetime(int minutes, const char *want, const char *msg)
{
    char buf[32];
    batt_format_lifetime(minutes, buf, sizeof(buf));
    check(strcmp(buf, want) == 0, msg);
}

static void expect_volt(int millivolts, const char *want, const char *msg)
{
    char buf[32];
    batt_format_volt(millivolts, buf, sizeof(buf));
    check(strcmp(buf, want) == 0, msg);
}

static void expect_capacity(int mah, const char *want, const char *msg)
{
    char buf[32];
    batt_format_capacity(mah, buf, sizeof(buf));
    check(strcmp(buf, want) == 0, msg);
}

int main(void)
{
    /* lifetime: decimal hours and zero-padded minutes */
    expect_lifetime(95,  "1:35",  "lifetime 95 minutes");
    expect_lifetime(0,   "0:00",  "lifetime zero");
    expect_lifetime(59,  "0:59",  "lifetime under an hour");
    expect_lifetime(60,  "1:00",  "lifetime exact hour");
    expect_lifetime(600, "10:00", "lifetime ten hours");
    expect_lifetime(-1,  "-",     "lifetime unavailable");

    /* voltage: millivolts to two-decimal volts, no float printf */
    expect_volt(4020, "4.02", "voltage 4020 mV");
    expect_volt(4150, "4.15", "voltage 4150 mV");
    expect_volt(950,  "0.95", "voltage below one volt");
    expect_volt(4000, "4.00", "voltage exact volts");
    expect_volt(0,    "-",    "voltage zero is unavailable");
    expect_volt(-1,   "-",    "voltage negative is unavailable");

    /* capacity: plain decimal mAh strings */
    expect_capacity(1200, "1200", "capacity 1200 mAh");
    expect_capacity(1,    "1",    "capacity one mAh");
    expect_capacity(0,    "-",    "capacity zero is unavailable");
    expect_capacity(-1,   "-",    "capacity negative is unavailable");

    printf("battery: %d checks, %d failed\n", checks, fails);
    return fails ? 1 : 0;
}
