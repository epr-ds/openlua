/* Host checks for the camera module's pure OneLua <-> PSPSDK converters.
 *
 * The camera itself is absent on the host, so these tests cover the value
 * mapping and clamping that the PSP path uses.  Absent-hardware Lua behaviour
 * is covered by test_cam.lua; the active path is PPSSPP-verified.
 */
#include <stdio.h>

#include "engine/sys/cam.h"

static int checks;
static int failures;

static void expect(const char *what, int got, int want)
{
    checks++;
    if (got != want) {
        failures++;
        printf("cam: %s: got %d, want %d\n", what, got, want);
    }
}

int main(void)
{
    expect("effect normal", cam_effect_to_psp(1), 0);
    expect("effect green", cam_effect_to_psp(7), 6);
    expect("effect low clamp", cam_effect_to_psp(-4), 0);
    expect("effect high clamp", cam_effect_to_psp(99), 6);
    expect("effect back", cam_effect_from_psp(0), 1);
    expect("effect back last", cam_effect_from_psp(6), 7);
    expect("effect back bad low", cam_effect_from_psp(-1), -1);
    expect("effect back bad high", cam_effect_from_psp(7), -1);

    expect("ev +2", cam_ev_to_psp(1), 0);
    expect("ev 0", cam_ev_to_psp(8), 7);
    expect("ev -2", cam_ev_to_psp(17), 16);
    expect("ev low clamp", cam_ev_to_psp(0), 0);
    expect("ev high clamp", cam_ev_to_psp(40), 16);
    expect("ev back", cam_ev_from_psp(7), 8);
    expect("ev back bad", cam_ev_from_psp(17), -1);

    expect("zoom 1x", cam_zoom_to_psp(1), 10);
    expect("zoom 5x", cam_zoom_to_psp(5), 50);
    expect("zoom low clamp", cam_zoom_to_psp(0), 10);
    expect("zoom high clamp", cam_zoom_to_psp(9), 50);
    expect("zoom back", cam_zoom_from_psp(30), 3);
    expect("zoom back 15", cam_zoom_from_psp(15), 1);
    expect("zoom back bad low", cam_zoom_from_psp(9), -1);
    expect("zoom back bad high", cam_zoom_from_psp(51), -1);

    printf("cam: %d checks, %d failure(s)\n", checks, failures);
    return failures ? 1 : 0;
}
