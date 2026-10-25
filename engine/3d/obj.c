/* OpenLua - small original OBJ/MTL parser.
 *
 * Supported OBJ records: v, vt, vn, f (triangles and convex polygons),
 * o, g, usemtl, mtllib.  Supported MTL records: newmtl, Ka, Kd, Ks, Ns, d,
 * Tr and map_Kd.  Unknown records are ignored, which keeps ordinary exported
 * meshes usable without pretending to implement the whole OBJ format.
 */

#include "obj.h"

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define OBJ_LINE_MAX 4096

static char *ol_strdup(const char *s)
{
    if (!s)
        return NULL;
    size_t n = strlen(s) + 1;
    char *p = (char *)malloc(n);
    if (p)
        memcpy(p, s, n);
    return p;
}

static char *ol_dirname(const char *path)
{
    if (!path)
        return ol_strdup(".");
    const char *slash = strrchr(path, '/');
    const char *colon = strrchr(path, ':');
    const char *cut = slash;
    if (!cut || (colon && colon > cut))
        cut = colon;

    if (!cut || cut == path) {
        if (cut == path && *cut == '/')
            return ol_strdup("/");
        if (cut == path && *cut == ':')
            return ol_strdup(".");
        return ol_strdup(".");
    }

    size_t n = (size_t)(cut - path);
    char *d = (char *)malloc(n + 1);
    if (!d)
        return NULL;
    memcpy(d, path, n);
    d[n] = '\0';
    return d;
}

static int ol_path_is_absolute(const char *p)
{
    return p && (p[0] == '/' || strchr(p, ':') != NULL);
}

static char *ol_join_path(const char *dir, const char *name)
{
    if (!name)
        return NULL;
    if (ol_path_is_absolute(name) || !dir || !dir[0])
        return ol_strdup(name);

    size_t dlen = strlen(dir);
    size_t nlen = strlen(name);
    int need_sep = (dir[dlen - 1] != '/');
    char *p = (char *)malloc(dlen + (size_t)need_sep + nlen + 1);
    if (!p)
        return NULL;
    memcpy(p, dir, dlen);
    size_t at = dlen;
    if (need_sep)
        p[at++] = '/';
    memcpy(p + at, name, nlen + 1);
    return p;
}

static int grow_array(void **data, int *capacity, int need, size_t elem)
{
    if (need <= *capacity)
        return 0;
    int cap = *capacity ? *capacity * 2 : 16;
    while (cap < need)
        cap *= 2;
    void *p = realloc(*data, (size_t)cap * elem);
    if (!p)
        return -1;
    *data = p;
    *capacity = cap;
    return 0;
}

static int parse_float(const char *s, float *out)
{
    if (!s || !*s)
        return -1;
    char *end = NULL;
    errno = 0;
    double v = strtod(s, &end);
    if (errno || end == s)
        return -1;
    *out = (float)v;
    return 0;
}

static int parse_int(const char *s, int *out)
{
    if (!s || !*s)
        return -1;
    char *end = NULL;
    errno = 0;
    long v = strtol(s, &end, 10);
    if (errno || end == s)
        return -1;
    *out = (int)v;
    return 0;
}

/* Convert a one-based or negative OBJ index to a zero-based array index. */
static int resolve_index(int idx, int count, int *out)
{
    if (idx == 0)
        return -1;
    if (idx > 0)
        idx -= 1;
    else
        idx = count + idx;
    if (idx < 0 || idx >= count)
        return -1;
    *out = idx;
    return 0;
}

static int append_vec3(OL_ObjVec3 **arr, int *count, int *cap,
                       float x, float y, float z)
{
    if (grow_array((void **)arr, cap, *count + 1, sizeof(**arr)) != 0)
        return -1;
    (*arr)[*count].x = x;
    (*arr)[*count].y = y;
    (*arr)[*count].z = z;
    (*count)++;
    return 0;
}

static int append_vec2(OL_ObjVec2 **arr, int *count, int *cap, float u, float v)
{
    if (grow_array((void **)arr, cap, *count + 1, sizeof(**arr)) != 0)
        return -1;
    (*arr)[*count].u = u;
    (*arr)[*count].v = v;
    (*count)++;
    return 0;
}

static int find_material(const OL_ObjModel *m, const char *name)
{
    if (!name)
        return -1;
    for (int i = 0; i < m->material_count; i++) {
        if (m->materials[i].name && strcmp(m->materials[i].name, name) == 0)
            return i;
    }
    return -1;
}

static int append_material(OL_ObjModel *m, const char *name)
{
    /* Material arrays are small for demo meshes.  Reallocating every time is
     * simpler and still linear in practice; models with many materials are
     * uncommon in the PSP homebrew this module targets. */
    OL_ObjMaterial *p = (OL_ObjMaterial *)realloc(
        m->materials, (size_t)(m->material_count + 1) * sizeof(*p));
    if (!p)
        return -1;
    m->materials = p;
    OL_ObjMaterial *mat = &m->materials[m->material_count];
    memset(mat, 0, sizeof(*mat));
    mat->name = ol_strdup(name ? name : "material");
    if (!mat->name)
        return -1;
    mat->kd[0] = mat->kd[1] = mat->kd[2] = 1.0f;
    mat->ka[0] = mat->ka[1] = mat->ka[2] = 0.2f;
    mat->ks[0] = mat->ks[1] = mat->ks[2] = 0.0f;
    mat->ns = 0.0f;
    mat->alpha = 1.0f;
    m->material_count++;
    return m->material_count - 1;
}

static int load_mtl(OL_ObjModel *m, const char *mtl_path)
{
    FILE *f = fopen(mtl_path, "rb");
    if (!f)
        return -1;

    char *dir = ol_dirname(mtl_path);
    int current = -1;
    char line[OBJ_LINE_MAX];

    while (fgets(line, sizeof(line), f)) {
        char *hash = strchr(line, '#');
        if (hash)
            *hash = '\0';

        char *cmd = strtok(line, " \t\r\n");
        if (!cmd)
            continue;

        if (strcmp(cmd, "newmtl") == 0) {
            char *name = strtok(NULL, " \t\r\n");
            current = append_material(m, name ? name : "material");
            if (current < 0)
                goto fail;
        } else if (strcmp(cmd, "Kd") == 0 || strcmp(cmd, "Ka") == 0 ||
                   strcmp(cmd, "Ks") == 0) {
            char *a = strtok(NULL, " \t\r\n");
            char *b = strtok(NULL, " \t\r\n");
            char *c = strtok(NULL, " \t\r\n");
            float r, g, bl;
            if (parse_float(a, &r) != 0 || parse_float(b, &g) != 0 ||
                parse_float(c, &bl) != 0)
                continue;
            if (current >= 0) {
                float *dst = strcmp(cmd, "Kd") == 0 ? m->materials[current].kd
                           : strcmp(cmd, "Ka") == 0 ? m->materials[current].ka
                                                    : m->materials[current].ks;
                dst[0] = r;
                dst[1] = g;
                dst[2] = bl;
            }
        } else if (strcmp(cmd, "Ns") == 0) {
            float v;
            if (current >= 0 && parse_float(strtok(NULL, " \t\r\n"), &v) == 0)
                m->materials[current].ns = v;
        } else if (strcmp(cmd, "d") == 0) {
            float v;
            if (current >= 0 && parse_float(strtok(NULL, " \t\r\n"), &v) == 0)
                m->materials[current].alpha = v;
        } else if (strcmp(cmd, "Tr") == 0) {
            float v;
            if (current >= 0 && parse_float(strtok(NULL, " \t\r\n"), &v) == 0)
                m->materials[current].alpha = 1.0f - v;
        } else if (strcmp(cmd, "map_Kd") == 0) {
            /* MTL map options (-s, -o, ...) are ignored; the filename is the
             * last token.  Filenames containing spaces are not supported. */
            char *tok = strtok(NULL, " \t\r\n");
            char *last = NULL;
            while (tok) {
                last = tok;
                tok = strtok(NULL, " \t\r\n");
            }
            if (current >= 0 && last) {
                free(m->materials[current].map_kd);
                m->materials[current].map_kd = ol_join_path(dir, last);
                if (!m->materials[current].map_kd)
                    goto fail;
            }
        }
    }

    free(dir);
    fclose(f);
    return 0;

fail:
    free(dir);
    fclose(f);
    return -1;
}

static int begin_object(OL_ObjModel *m, const char *name)
{
    /* Exporters commonly emit a default object before the first "o" line.
     * Reuse it when it has no faces yet instead of counting an empty object. */
    if (m->object_count > 0) {
        OL_ObjObject *last = &m->objects[m->object_count - 1];
        if (last->tri_count == 0) {
            char *copy = ol_strdup(name && *name ? name : "object");
            if (!copy)
                return -1;
            free(last->name);
            last->name = copy;
            return m->object_count - 1;
        }
    }

    OL_ObjObject *p = (OL_ObjObject *)realloc(
        m->objects, (size_t)(m->object_count + 1) * sizeof(*p));
    if (!p)
        return -1;
    m->objects = p;
    OL_ObjObject *obj = &m->objects[m->object_count];
    memset(obj, 0, sizeof(*obj));
    obj->name = ol_strdup(name && *name ? name : "object");
    obj->first_tri = m->tri_count;
    if (!obj->name)
        return -1;
    m->object_count++;
    return m->object_count - 1;
}

static int add_triangle(OL_ObjModel *m, int object, int material,
                        const int *v, const int *t, const int *n)
{
    OL_ObjTri *p = (OL_ObjTri *)realloc(
        m->tris, (size_t)(m->tri_count + 1) * sizeof(*p));
    if (!p)
        return -1;
    m->tris = p;
    OL_ObjTri *tri = &m->tris[m->tri_count];
    for (int i = 0; i < 3; i++) {
        tri->v[i] = v[i];
        tri->t[i] = t[i];
        tri->n[i] = n[i];
    }
    tri->material = material;
    m->tri_count++;

    if (object >= 0 && object < m->object_count)
        m->objects[object].tri_count++;
    return 0;
}

static int parse_face_index(const char *token, const OL_ObjModel *m,
                            int *vi, int *ti, int *ni)
{
    char buf[128];
    size_t len = strlen(token);
    if (len == 0 || len >= sizeof(buf))
        return -1;
    memcpy(buf, token, len + 1);

    char *slash1 = strchr(buf, '/');
    char *slash2 = NULL;
    if (slash1) {
        *slash1 = '\0';
        slash2 = strchr(slash1 + 1, '/');
        if (slash2)
            *slash2 = '\0';
    }

    int one = 0;
    if (parse_int(buf, &one) != 0)
        return -1;
    if (resolve_index(one, m->position_count, vi) != 0)
        return -1;

    *ti = -1;
    *ni = -1;
    if (slash1 && slash1[1]) {
        int uv = 0;
        if (parse_int(slash1 + 1, &uv) != 0 ||
            resolve_index(uv, m->uv_count, ti) != 0)
            return -1;
    }
    if (slash2 && slash2[1]) {
        int normal = 0;
        if (parse_int(slash2 + 1, &normal) != 0 ||
            resolve_index(normal, m->normal_count, ni) != 0)
            return -1;
    }
    return 0;
}

static int parse_face(OL_ObjModel *m, int object, int material)
{
    int v[64], t[64], n[64];
    int count = 0;
    char *tok = strtok(NULL, " \t\r\n");
    while (tok && count < 64) {
        if (parse_face_index(tok, m, &v[count], &t[count], &n[count]) != 0)
            return -1;
        count++;
        tok = strtok(NULL, " \t\r\n");
    }
    if (count < 3)
        return -1;

    /* Convex polygons are triangulated as a fan.  OBJ faces are expected to
     * be planar; the parser does not attempt to repair concave n-gons. */
    for (int i = 1; i + 1 < count; i++) {
        int vv[3] = { v[0], v[i], v[i + 1] };
        int tt[3] = { t[0], t[i], t[i + 1] };
        int nn[3] = { n[0], n[i], n[i + 1] };
        if (add_triangle(m, object, material, vv, tt, nn) != 0)
            return -1;
    }
    return 0;
}

int ol_obj_load(const char *path, OL_ObjModel *out, char *err, size_t errcap)
{
    if (!path || !out)
        return -1;
    memset(out, 0, sizeof(*out));

    FILE *f = fopen(path, "rb");
    if (!f) {
        if (err && errcap)
            snprintf(err, errcap, "cannot open '%s'", path);
        return -1;
    }

    char *dir = ol_dirname(path);
    int current_object = -1;
    int current_material = -1;
    int pos_cap = 0, uv_cap = 0, normal_cap = 0;
    char line[OBJ_LINE_MAX];
    int ok = 1;

    while (fgets(line, sizeof(line), f)) {
        char *hash = strchr(line, '#');
        if (hash)
            *hash = '\0';

        char *cmd = strtok(line, " \t\r\n");
        if (!cmd)
            continue;

        if (strcmp(cmd, "v") == 0) {
            float x, y, z;
            if (parse_float(strtok(NULL, " \t\r\n"), &x) != 0 ||
                parse_float(strtok(NULL, " \t\r\n"), &y) != 0 ||
                parse_float(strtok(NULL, " \t\r\n"), &z) != 0 ||
                append_vec3(&out->positions, &out->position_count, &pos_cap,
                            x, y, z) != 0) {
                ok = 0;
                break;
            }
        } else if (strcmp(cmd, "vt") == 0) {
            float u, v;
            if (parse_float(strtok(NULL, " \t\r\n"), &u) != 0 ||
                parse_float(strtok(NULL, " \t\r\n"), &v) != 0 ||
                append_vec2(&out->uvs, &out->uv_count, &uv_cap, u, v) != 0) {
                ok = 0;
                break;
            }
        } else if (strcmp(cmd, "vn") == 0) {
            float x, y, z;
            if (parse_float(strtok(NULL, " \t\r\n"), &x) != 0 ||
                parse_float(strtok(NULL, " \t\r\n"), &y) != 0 ||
                parse_float(strtok(NULL, " \t\r\n"), &z) != 0 ||
                append_vec3(&out->normals, &out->normal_count, &normal_cap,
                            x, y, z) != 0) {
                ok = 0;
                break;
            }
        } else if (strcmp(cmd, "o") == 0 || strcmp(cmd, "g") == 0) {
            char *name = strtok(NULL, " \t\r\n");
            current_object = begin_object(out, name);
            if (current_object < 0) {
                ok = 0;
                break;
            }
        } else if (strcmp(cmd, "usemtl") == 0) {
            char *name = strtok(NULL, " \t\r\n");
            current_material = find_material(out, name);
        } else if (strcmp(cmd, "mtllib") == 0) {
            char *lib = strtok(NULL, " \t\r\n");
            if (lib) {
                char *resolved = ol_join_path(dir, lib);
                if (!resolved) {
                    ok = 0;
                    break;
                }
                /* A missing MTL is not fatal: the OBJ may still be usable as
                 * an untextured, material-coloured mesh. */
                (void)load_mtl(out, resolved);
                free(resolved);
            }
        } else if (strcmp(cmd, "f") == 0) {
            if (current_object < 0) {
                current_object = begin_object(out, "object0");
                if (current_object < 0) {
                    ok = 0;
                    break;
                }
            }
            if (parse_face(out, current_object, current_material) != 0) {
                ok = 0;
                break;
            }
        }
    }

    free(dir);
    fclose(f);

    if (!ok || out->tri_count == 0) {
        if (err && errcap)
            snprintf(err, errcap, "invalid or empty OBJ '%s'", path);
        ol_obj_free(out);
        return -1;
    }

    /* A model with vertices but no explicit object still gets one object. */
    if (out->object_count == 0 && out->tri_count > 0) {
        if (begin_object(out, "object0") < 0) {
            if (err && errcap)
                snprintf(err, errcap, "out of memory parsing '%s'", path);
            ol_obj_free(out);
            return -1;
        }
        out->objects[0].tri_count = out->tri_count;
    }

    if (err && errcap)
        err[0] = '\0';
    return 0;
}

void ol_obj_free(OL_ObjModel *m)
{
    if (!m)
        return;
    for (int i = 0; i < m->material_count; i++) {
        free(m->materials[i].name);
        free(m->materials[i].map_kd);
    }
    for (int i = 0; i < m->object_count; i++)
        free(m->objects[i].name);
    free(m->positions);
    free(m->uvs);
    free(m->normals);
    free(m->tris);
    free(m->objects);
    free(m->materials);
    memset(m, 0, sizeof(*m));
}
