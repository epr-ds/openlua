/* OpenLua - OneLua-compatible 3D model module.
 *
 * OBJ/MTL parsing is portable and host-tested.  The only PSP-specific work is
 * the final triangle submission through gfx3d.c; on the host the parse and
 * transform API remains fully usable.
 */

#include "model3d.h"
#include "texture3d.h"
#include "engine/core/olreg.h"

#include <lauxlib.h>
#include <lualib.h>

#include <stdlib.h>
#include <string.h>

static OL_Model3D g_models[MODEL3D_MAX];

static int claim_model(void)
{
    for (int i = 0; i < MODEL3D_MAX; i++) {
        if (!g_models[i].used)
            return i + 1;
    }
    return 0;
}

static OL_Model3D *to_model(lua_State *L, int idx)
{
    int h = (int)luaL_checkinteger(L, idx);
    if (h < 1 || h > MODEL3D_MAX || !g_models[h - 1].used)
        luaL_error(L, "invalid model3d handle");
    return &g_models[h - 1];
}

static int object_index(lua_State *L, int idx, OL_Model3D *m)
{
    int o = (int)luaL_checkinteger(L, idx);
    if (o < 0 || o >= m->object_count)
        luaL_error(L, "model3d object %d out of range", o);
    return o;
}

static float table_number(lua_State *L, int idx, const char *key,
                          int array_index, float fallback)
{
    lua_getfield(L, idx, key);
    if (lua_isnumber(L, -1)) {
        float v = (float)lua_tonumber(L, -1);
        lua_pop(L, 1);
        return v;
    }
    lua_pop(L, 1);

    lua_rawgeti(L, idx, array_index);
    if (lua_isnumber(L, -1)) {
        float v = (float)lua_tonumber(L, -1);
        lua_pop(L, 1);
        return v;
    }
    lua_pop(L, 1);
    return fallback;
}

static void table_vec3(lua_State *L, int idx, float *x, float *y, float *z,
                       float fx, float fy, float fz)
{
    luaL_checktype(L, idx, LUA_TTABLE);
    *x = table_number(L, idx, "x", 1, fx);
    *y = table_number(L, idx, "y", 2, fy);
    *z = table_number(L, idx, "z", 3, fz);
}

static void push_vec3(lua_State *L, float x, float y, float z)
{
    lua_createtable(L, 3, 3);
    lua_pushnumber(L, x); lua_setfield(L, -2, "x");
    lua_pushnumber(L, y); lua_setfield(L, -2, "y");
    lua_pushnumber(L, z); lua_setfield(L, -2, "z");
    lua_pushnumber(L, x); lua_rawseti(L, -2, 1);
    lua_pushnumber(L, y); lua_rawseti(L, -2, 2);
    lua_pushnumber(L, z); lua_rawseti(L, -2, 3);
}

static unsigned int material_color(const OL_ObjMaterial *mat)
{
    if (!mat)
        return 0xFFFFFFFFu;
    int r = (int)(mat->kd[0] * 255.0f + 0.5f);
    int g = (int)(mat->kd[1] * 255.0f + 0.5f);
    int b = (int)(mat->kd[2] * 255.0f + 0.5f);
    int a = (int)(mat->alpha * 255.0f + 0.5f);
    if (r < 0) r = 0;
    if (r > 255) r = 255;
    if (g < 0) g = 0;
    if (g > 255) g = 255;
    if (b < 0) b = 0;
    if (b > 255) b = 255;
    if (a < 0) a = 0;
    if (a > 255) a = 255;
    return ((unsigned int)a << 24) | ((unsigned int)b << 16) |
           ((unsigned int)g << 8) | (unsigned int)r;
}

static int batch_append(OL_Model3DBatch *b, const OL_3DVertex *v)
{
    if (b->count >= b->capacity) {
        int cap = b->capacity ? b->capacity * 2 : 64;
        OL_3DVertex *p = (OL_3DVertex *)realloc(
            b->vertices, (size_t)cap * sizeof(*p));
        if (!p)
            return -1;
        b->vertices = p;
        b->capacity = cap;
    }
    b->vertices[b->count++] = *v;
    return 0;
}

static OL_Model3DBatch *get_batch(OL_Model3D *m, int object, int material,
                                  int first_batch, int texture)
{
    for (int i = first_batch; i < m->batch_count; i++) {
        if (m->batches[i].material == material) {
            m->batches[i].texture = texture;
            return &m->batches[i];
        }
    }

    OL_Model3DBatch *p = (OL_Model3DBatch *)realloc(
        m->batches, (size_t)(m->batch_count + 1) * sizeof(*p));
    if (!p)
        return NULL;
    m->batches = p;
    OL_Model3DBatch *b = &m->batches[m->batch_count++];
    memset(b, 0, sizeof(*b));
    b->material = material;
    b->texture = texture;
    b->filter = -1;
    (void)object;
    return b;
}

static int build_render_data(OL_Model3D *m)
{
    int ocount = m->obj.object_count;
    if (ocount < 0)
        return -1;

    m->objects = (OL_Model3DObject *)calloc((size_t)ocount,
                                            sizeof(*m->objects));
    if (ocount > 0 && !m->objects)
        return -1;
    m->object_count = ocount;

    for (int i = 0; i < ocount; i++) {
        OL_Model3DObject *o = &m->objects[i];
        o->sx = o->sy = o->sz = 1.0f;
        o->texture_override = 0;
        o->filter = -1;
    }

    m->material_count = m->obj.material_count;
    if (m->material_count > 0) {
        m->material_textures = (int *)calloc((size_t)m->material_count,
                                             sizeof(*m->material_textures));
        if (!m->material_textures)
            return -1;
        for (int i = 0; i < m->material_count; i++) {
            const char *map = m->obj.materials[i].map_kd;
            if (map && *map)
                m->material_textures[i] = texture3d_load_internal(map, 0);
        }
    }

    for (int i = 0; i < ocount; i++) {
        OL_ObjObject *src = &m->obj.objects[i];
        OL_Model3DObject *dst = &m->objects[i];
        dst->first_batch = m->batch_count;

        int end = src->first_tri + src->tri_count;
        if (end > m->obj.tri_count)
            end = m->obj.tri_count;

        for (int t = src->first_tri; t < end; t++) {
            OL_ObjTri *tri = &m->obj.tris[t];
            int mat = tri->material;
            if (mat < 0 || mat >= m->material_count)
                mat = -1;
            int tex = (mat >= 0) ? m->material_textures[mat] : 0;
            unsigned int color = material_color(
                (mat >= 0) ? &m->obj.materials[mat] : NULL);

            OL_Model3DBatch *b = get_batch(m, i, mat,
                                           dst->first_batch, tex);
            if (!b)
                return -1;

            for (int k = 0; k < 3; k++) {
                int vi = tri->v[k];
                if (vi < 0 || vi >= m->obj.position_count)
                    return -1;
                OL_3DVertex v;
                memset(&v, 0, sizeof(v));
                v.x = m->obj.positions[vi].x;
                v.y = m->obj.positions[vi].y;
                v.z = m->obj.positions[vi].z;
                if (tri->t[k] >= 0 && tri->t[k] < m->obj.uv_count) {
                    v.u = m->obj.uvs[tri->t[k]].u;
                    /* OBJ V is bottom-up; PSP textures are sampled top-down. */
                    v.v = 1.0f - m->obj.uvs[tri->t[k]].v;
                } else {
                    v.u = 0.5f;
                    v.v = 0.5f;
                }
                v.color = color;
                if (batch_append(b, &v) != 0)
                    return -1;
            }
        }

        dst->batch_count = m->batch_count - dst->first_batch;
    }

    return 0;
}

static void free_model(OL_Model3D *m)
{
    if (!m)
        return;
    for (int i = 0; i < m->batch_count; i++)
        free(m->batches[i].vertices);
    free(m->batches);
    free(m->objects);
    free(m->material_textures);
    ol_obj_free(&m->obj);
    memset(m, 0, sizeof(*m));
}

static int l_load(lua_State *L)
{
    const char *path = luaL_checkstring(L, 1);
    int css = (int)luaL_optinteger(L, 2, 0);

    OL_ObjModel obj;
    char err[256];
    if (ol_obj_load(path, &obj, err, sizeof(err)) != 0)
        return luaL_error(L, "model3d.load: %s", err);

    int handle = claim_model();
    if (!handle) {
        ol_obj_free(&obj);
        return luaL_error(L, "model3d.load: model slots full");
    }

    OL_Model3D *m = &g_models[handle - 1];
    memset(m, 0, sizeof(*m));
    m->used = 1;
    m->obj = obj;
    m->css = css;
    if (build_render_data(m) != 0) {
        free_model(m);
        return luaL_error(L, "model3d.load: out of memory or invalid indices");
    }

    lua_pushinteger(L, handle);
    return 1;
}

static int l_countobj(lua_State *L)
{
    OL_Model3D *m = to_model(L, 1);
    lua_pushinteger(L, m->object_count);
    return 1;
}

static int l_render(lua_State *L)
{
    OL_Model3D *m = to_model(L, 1);
    int draw_type = (int)luaL_optinteger(L, 2, 2);
    (void)draw_type;   /* FRONT/TRANS/ALL are accepted; depth-test all faces */

    int only = -1;
    int skip = -1;
    if (lua_gettop(L) >= 3) {
        int o = (int)luaL_checkinteger(L, 3);
        if (o >= 0) {
            if (o >= m->object_count)
                return luaL_error(L, "model3d.render: object out of range");
            only = o;
        } else {
            skip = -o - 1;
        }
    }

    for (int i = 0; i < m->object_count; i++) {
        if (only >= 0 && i != only)
            continue;
        if (skip >= 0 && i == skip)
            continue;

        OL_Model3DObject *o = &m->objects[i];
        OL_3DTransform xf;
        xf.px = o->px; xf.py = o->py; xf.pz = o->pz;
        xf.rx = o->rx; xf.ry = o->ry; xf.rz = o->rz;
        xf.sx = o->sx; xf.sy = o->sy; xf.sz = o->sz;

        for (int b = 0; b < o->batch_count; b++) {
            OL_Model3DBatch *batch = &m->batches[o->first_batch + b];
            if (batch->count <= 0)
                continue;
            int tex_handle = o->texture_override ? o->texture_override
                                                 : batch->texture;
            int filter = o->filter >= 0 ? o->filter : batch->filter;
            if (filter < 0)
                filter = 0;
            OL_Texture3D *tex = texture3d_get(tex_handle);
#if defined(__PSP__)
            gfx3d_draw_batch(batch->vertices, batch->count,
                             tex ? tex->surface : NULL, filter, &xf);
#else
            (void)tex;
            (void)filter;
            (void)xf;
#endif
        }
    }
    return 0;
}

static int l_rotation(lua_State *L)
{
    OL_Model3D *m = to_model(L, 1);
    int o = object_index(L, 2, m);
    float x, y, z;
    table_vec3(L, 3, &x, &y, &z, 0.0f, 0.0f, 0.0f);
    m->objects[o].rx = x;
    m->objects[o].ry = y;
    m->objects[o].rz = z;
    return 0;
}

static int l_getrotation(lua_State *L)
{
    OL_Model3D *m = to_model(L, 1);
    int o = object_index(L, 2, m);
    push_vec3(L, m->objects[o].rx, m->objects[o].ry, m->objects[o].rz);
    return 1;
}

static int l_position(lua_State *L)
{
    OL_Model3D *m = to_model(L, 1);
    int o = object_index(L, 2, m);
    float x, y, z;
    table_vec3(L, 3, &x, &y, &z, 0.0f, 0.0f, 0.0f);
    m->objects[o].px = x;
    m->objects[o].py = y;
    m->objects[o].pz = z;
    return 0;
}

static int l_getposition(lua_State *L)
{
    OL_Model3D *m = to_model(L, 1);
    int o = object_index(L, 2, m);
    push_vec3(L, m->objects[o].px, m->objects[o].py, m->objects[o].pz);
    return 1;
}

static int l_scaling(lua_State *L)
{
    OL_Model3D *m = to_model(L, 1);
    int o = object_index(L, 2, m);
    float x, y, z;
    table_vec3(L, 3, &x, &y, &z, 1.0f, 1.0f, 1.0f);
    m->objects[o].sx = x;
    m->objects[o].sy = y;
    m->objects[o].sz = z;
    return 0;
}

static int l_getscaling(lua_State *L)
{
    OL_Model3D *m = to_model(L, 1);
    int o = object_index(L, 2, m);
    push_vec3(L, m->objects[o].sx, m->objects[o].sy, m->objects[o].sz);
    return 1;
}

static int l_free(lua_State *L)
{
    OL_Model3D *m = to_model(L, 1);
    free_model(m);
    return 0;
}

int model3d_set_texture(int model, int object, int slot, int texture)
{
    (void)slot;
    if (model < 1 || model > MODEL3D_MAX || !g_models[model - 1].used)
        return -1;
    OL_Model3D *m = &g_models[model - 1];
    if (object < 0 || object >= m->object_count)
        return -1;
    m->objects[object].texture_override = texture;
    return 0;
}

int model3d_set_filter(int model, int object, int filter)
{
    if (model < 1 || model > MODEL3D_MAX || !g_models[model - 1].used)
        return -1;
    OL_Model3D *m = &g_models[model - 1];
    if (filter < 0)
        filter = 0;
    if (filter > 1)
        filter = 1;

    if (object < 0) {
        for (int i = 0; i < m->object_count; i++)
            m->objects[i].filter = filter;
        return 0;
    }
    if (object >= m->object_count)
        return -1;
    m->objects[object].filter = filter;
    return 0;
}

static const luaL_Reg model3d_funcs[] = {
    { "load",        l_load },
    { "countobj",    l_countobj },
    { "render",      l_render },
    { "rotation",    l_rotation },
    { "getrotation", l_getrotation },
    { "position",    l_position },
    { "getposition", l_getposition },
    { "scaling",     l_scaling },
    { "getscaling",  l_getscaling },
    { "free",        l_free },
    { NULL, NULL }
};

void model3d_api_register(lua_State *L)
{
    lua_newtable(L);
    ol_setfuncs(L, model3d_funcs);
    lua_setglobal(L, "model3d");
}
