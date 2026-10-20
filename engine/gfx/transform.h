#ifndef OL_TRANSFORM_H
#define OL_TRANSFORM_H

typedef struct {
    float x;
    float y;
    float u;
    float v;
} OL_BlitVertex;

typedef struct {
    float x;
    float y;
    float scaleX;
    float scaleY;
    float angle;
    float pivotX;
    float pivotY;
    int srcX;
    int srcY;
    int srcW;
    int srcH;
    int texW;
    int texH;
    int flipX;
    int flipY;
} OL_BlitParams;

void ol_blit_quad(const OL_BlitParams *p, OL_BlitVertex out[4]);

#endif
