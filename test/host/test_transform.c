#include "engine/gfx/transform.h"

#include <math.h>
#include <stdio.h>

static int checks;
static int fails;

static void near(const char *name, int vertex, const char *field,
                 float got, float want)
{
    checks++;
    if (fabsf(got - want) > 0.001f) {
        fails++;
        printf("FAIL %s v%d %s: got %.3f want %.3f\n",
               name, vertex, field, got, want);
    }
}

static void expect(const char *name, const OL_BlitParams *p,
                   const float xs[4], const float ys[4],
                   const float us[4], const float vs[4])
{
    OL_BlitVertex q[4];
    ol_blit_quad(p, q);
    for (int i = 0; i < 4; i++) {
        near(name, i, "x", q[i].x, xs[i]);
        near(name, i, "y", q[i].y, ys[i]);
        near(name, i, "u", q[i].u, us[i]);
        near(name, i, "v", q[i].v, vs[i]);
    }
}

int main(void)
{
    OL_BlitParams p = {
        .x = 10.0f, .y = 20.0f,
        .scaleX = 1.0f, .scaleY = 1.0f,
        .angle = 0.0f,
        .pivotX = 32.0f, .pivotY = 32.0f,
        .srcX = 64, .srcY = 0, .srcW = 64, .srcH = 64,
        .texW = 256, .texH = 64,
        .flipX = 0, .flipY = 0
    };

    {
        float xs[4] = {10, 74, 74, 10};
        float ys[4] = {20, 20, 84, 84};
        float us[4] = {0.25f, 0.5f, 0.5f, 0.25f};
        float vs[4] = {0, 0, 1, 1};
        expect("identity", &p, xs, ys, us, vs);
    }

    p.flipX = 1;
    {
        float xs[4] = {10, 74, 74, 10};
        float ys[4] = {20, 20, 84, 84};
        float us[4] = {0.5f, 0.25f, 0.25f, 0.5f};
        float vs[4] = {0, 0, 1, 1};
        expect("flipX", &p, xs, ys, us, vs);
    }
    p.flipX = 0;

    p.flipY = 1;
    {
        float xs[4] = {10, 74, 74, 10};
        float ys[4] = {20, 20, 84, 84};
        float us[4] = {0.25f, 0.5f, 0.5f, 0.25f};
        float vs[4] = {1, 1, 0, 0};
        expect("flipY", &p, xs, ys, us, vs);
    }
    p.flipY = 0;

    p.x = 100.0f;
    p.y = 50.0f;
    p.scaleX = 2.0f;
    p.scaleY = 2.0f;
    {
        float xs[4] = {100, 228, 228, 100};
        float ys[4] = {50, 50, 178, 178};
        float us[4] = {0.25f, 0.5f, 0.5f, 0.25f};
        float vs[4] = {0, 0, 1, 1};
        expect("scale", &p, xs, ys, us, vs);
    }

    p.x = 10.0f;
    p.y = 20.0f;
    p.scaleX = 1.0f;
    p.scaleY = 1.0f;
    p.angle = 90.0f;
    {
        float xs[4] = {74, 74, 10, 10};
        float ys[4] = {20, 84, 84, 20};
        float us[4] = {0.25f, 0.5f, 0.5f, 0.25f};
        float vs[4] = {0, 0, 1, 1};
        expect("rotate90", &p, xs, ys, us, vs);
    }

    p.pivotX = 0.0f;
    p.pivotY = 0.0f;
    {
        float xs[4] = {10, 10, -54, -54};
        float ys[4] = {20, 84, 84, 20};
        float us[4] = {0.25f, 0.5f, 0.5f, 0.25f};
        float vs[4] = {0, 0, 1, 1};
        expect("rotate90-pivot0", &p, xs, ys, us, vs);
    }

    p.angle = 180.0f;
    p.pivotX = 32.0f;
    p.pivotY = 32.0f;
    {
        float xs[4] = {74, 10, 10, 74};
        float ys[4] = {84, 84, 20, 20};
        float us[4] = {0.25f, 0.5f, 0.5f, 0.25f};
        float vs[4] = {0, 0, 1, 1};
        expect("rotate180", &p, xs, ys, us, vs);
    }

    p.angle = 0.0f;
    p.x = 0.0f;
    p.y = 0.0f;
    p.pivotX = 16.0f;
    p.pivotY = 16.0f;
    p.scaleX = 0.5f;
    p.scaleY = 0.5f;
    {
        float xs[4] = {0, 32, 32, 0};
        float ys[4] = {0, 0, 32, 32};
        float us[4] = {0.25f, 0.5f, 0.5f, 0.25f};
        float vs[4] = {0, 0, 1, 1};
        expect("scale-half", &p, xs, ys, us, vs);
    }

    p.x = 10.0f;
    p.y = 20.0f;
    p.pivotX = 32.0f;
    p.pivotY = 32.0f;
    p.scaleX = 1.0f;
    p.scaleY = 1.0f;
    p.srcX = 128;
    {
        float xs[4] = {10, 74, 74, 10};
        float ys[4] = {20, 20, 84, 84};
        float us[4] = {0.5f, 0.75f, 0.75f, 0.5f};
        float vs[4] = {0, 0, 1, 1};
        expect("src-offset", &p, xs, ys, us, vs);
    }

    printf("transform: %d checks, %d failed\n", checks, fails);
    return fails ? 1 : 0;
}
