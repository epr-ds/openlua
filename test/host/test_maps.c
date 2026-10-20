#include "engine/maps/maps.c"

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

OL_Surface *image_as_surface(lua_State *L, int idx)
{
    (void)L;
    (void)idx;
    return NULL;
}

void gfx_blit(OL_Surface *s, int x, int y, int sx, int sy, int sw, int sh,
              int alpha, int flipX, int flipY, float angle, float scaleX,
              float scaleY, int z, unsigned tint)
{
    (void)s; (void)x; (void)y; (void)sx; (void)sy; (void)sw; (void)sh;
    (void)alpha; (void)flipX; (void)flipY; (void)angle; (void)scaleX;
    (void)scaleY; (void)z; (void)tint;
}

int main(void)
{
    int data[6] = { 1, 2, 3, 4, 5, 6 };
    OL_Surface sheet;
    memset(&sheet, 0, sizeof(sheet));
    sheet.w = 64;
    sheet.h = 64;

    OLEmap m;
    memset(&m, 0, sizeof(m));
    m.used = 1;
    m.tileset = &sheet;
    m.rows = 2;
    m.cols = 3;
    m.tileW = 16;
    m.tileH = 16;
    m.tcols = 4;
    m.data = data;

    check(olmap_at(&m, 1, 1) == 1, "tile (1,1)");
    check(olmap_at(&m, 3, 2) == 6, "tile (3,2)");
    check(olmap_at(&m, 0, 1) == -1, "tile x below range");
    check(olmap_at(&m, 4, 1) == -1, "tile x above range");
    check(olmap_at(&m, 1, 3) == -1, "tile y above range");

    check(olmap_set(&m, 2, 1, 42) == 0 && olmap_at(&m, 2, 1) == 42,
          "tile set updates data");
    check(olmap_set(&m, 4, 1, 0) == -1, "tile set out of range");
    check(olmap_at(&m, 2, 1) == 42, "failed set leaves data intact");

    int sx = -1, sy = -1;
    check(olmap_tile_rect(&m, 0, &sx, &sy) == 0 && sx == 0 && sy == 0,
          "tile 0 rect");
    check(olmap_tile_rect(&m, 3, &sx, &sy) == 0 && sx == 48 && sy == 0,
          "tile 3 rect");
    check(olmap_tile_rect(&m, 4, &sx, &sy) == 0 && sx == 0 && sy == 16,
          "tile 4 wraps to next row");
    check(olmap_tile_rect(&m, 15, &sx, &sy) == 0 && sx == 48 && sy == 48,
          "tile 15 rect");
    check(olmap_tile_rect(&m, 16, &sx, &sy) == -1,
          "tile beyond sheet rejected");

    g_maps[0] = m;
    check(olmap_get(1) == &g_maps[0], "handle 1 resolves");
    check(olmap_get(0) == NULL, "handle 0 invalid");
    check(olmap_get(MAP_MAX + 1) == NULL, "handle above max invalid");

    printf("maps: %d checks, %d failed\n", checks, fails);
    return fails ? 1 : 0;
}
