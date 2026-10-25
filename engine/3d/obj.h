/* OpenLua - small OBJ/MTL loader for the 3D module family.
 *
 * This is intentionally a parser, not a renderer.  It keeps the original
 * indexed OBJ data and material references so the PSP-specific model code can
 * build flat triangle batches without pulling PSP headers into host tests.
 */

#ifndef OL_OBJ_H
#define OL_OBJ_H

#include <stddef.h>

typedef struct {
    float x, y, z;
} OL_ObjVec3;

typedef struct {
    float u, v;
} OL_ObjVec2;

typedef struct {
    int v[3];       /* 0-based position index */
    int t[3];       /* 0-based UV index, or -1 */
    int n[3];       /* 0-based normal index, or -1 */
    int material;   /* 0-based material index, or -1 */
} OL_ObjTri;

typedef struct {
    char *name;
    int first_tri;
    int tri_count;
} OL_ObjObject;

typedef struct {
    char *name;
    float ka[3];
    float kd[3];
    float ks[3];
    float ns;
    float alpha;    /* MTL d value */
    char *map_kd;   /* path resolved relative to the MTL file */
} OL_ObjMaterial;

typedef struct {
    OL_ObjVec3 *positions;
    int position_count;
    OL_ObjVec2 *uvs;
    int uv_count;
    OL_ObjVec3 *normals;
    int normal_count;
    OL_ObjTri *tris;
    int tri_count;
    OL_ObjObject *objects;
    int object_count;
    OL_ObjMaterial *materials;
    int material_count;
} OL_ObjModel;

/* Load an OBJ file (and any MTL files it references).  Returns 0 on success,
 * -1 on failure and fills err with a human-readable message. */
int ol_obj_load(const char *path, OL_ObjModel *out, char *err, size_t errcap);

/* Release every allocation owned by m.  Safe to call more than once. */
void ol_obj_free(OL_ObjModel *m);

#endif
