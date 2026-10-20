#include "transform.h"

#include <math.h>

#define OL_PI 3.14159265358979f

void ol_blit_quad(const OL_BlitParams *p, OL_BlitVertex out[4])
{
    float u0 = (float)p->srcX / (float)p->texW;
    float u1 = (float)(p->srcX + p->srcW) / (float)p->texW;
    float v0 = (float)p->srcY / (float)p->texH;
    float v1 = (float)(p->srcY + p->srcH) / (float)p->texH;
    float px = p->pivotX;
    float py = p->pivotY;
    float dw = (float)p->srcW * p->scaleX;
    float dh = (float)p->srcH * p->scaleY;
    float cx = p->x + px * p->scaleX;
    float cy = p->y + py * p->scaleY;
    float lx[4] = {
        -px * p->scaleX,
        dw - px * p->scaleX,
        dw - px * p->scaleX,
        -px * p->scaleX
    };
    float ly[4] = {
        -py * p->scaleY,
        -py * p->scaleY,
        dh - py * p->scaleY,
        dh - py * p->scaleY
    };

    if (p->flipX) {
        float t = u0;
        u0 = u1;
        u1 = t;
    }
    if (p->flipY) {
        float t = v0;
        v0 = v1;
        v1 = t;
    }

    out[0].u = u0; out[0].v = v0;
    out[1].u = u1; out[1].v = v0;
    out[2].u = u1; out[2].v = v1;
    out[3].u = u0; out[3].v = v1;

    if (p->angle == 0.0f) {
        for (int i = 0; i < 4; i++) {
            out[i].x = cx + lx[i];
            out[i].y = cy + ly[i];
        }
        return;
    }

    float rad = p->angle * OL_PI / 180.0f;
    float c = cosf(rad);
    float s = sinf(rad);

    for (int i = 0; i < 4; i++) {
        out[i].x = cx + lx[i] * c - ly[i] * s;
        out[i].y = cy + lx[i] * s + ly[i] * c;
    }
}
