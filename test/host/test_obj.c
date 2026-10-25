/* Host tests for the portable OBJ/MTL parser. */

#include "engine/3d/obj.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

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

static int write_file(const char *path, const char *text)
{
    FILE *f = fopen(path, "wb");
    if (!f)
        return -1;
    fputs(text, f);
    fclose(f);
    return 0;
}

int main(void)
{
    if (mkdir("objwork", 0755) != 0 && errno != EEXIST) {
        perror("mkdir objwork");
        return 1;
    }

    const char *obj_path = "objwork/cube.obj";
    const char *mtl_path = "objwork/cube.mtl";
    const char *obj_text =
        "mtllib cube.mtl\n"
        "o One\n"
        "v -1 -1 1\n"
        "v 1 -1 1\n"
        "v 1 1 1\n"
        "v -1 1 1\n"
        "vt 0 0\n"
        "vt 1 0\n"
        "vt 1 1\n"
        "vt 0 1\n"
        "usemtl checker\n"
        "f 1/1 2/2 3/3 4/4\n"
        "o Two\n"
        "f 1/1 3/3 4/4\n";
    const char *mtl_text =
        "newmtl checker\n"
        "Kd 0.25 0.5 0.75\n"
        "d 0.5\n"
        "map_Kd checker.png\n";

    check(write_file(obj_path, obj_text) == 0, "write OBJ");
    check(write_file(mtl_path, mtl_text) == 0, "write MTL");

    OL_ObjModel m;
    char err[256];
    check(ol_obj_load(obj_path, &m, err, sizeof(err)) == 0, "OBJ loads");
    check(m.position_count == 4, "position count");
    check(m.uv_count == 4, "UV count");
    check(m.tri_count == 3, "quad + triangle triangulates");
    check(m.object_count == 2, "o records create objects");
    check(m.objects[0].first_tri == 0 && m.objects[0].tri_count == 2,
          "first object triangle range");
    check(m.objects[1].first_tri == 2 && m.objects[1].tri_count == 1,
          "second object triangle range");
    check(m.material_count == 1, "MTL material loaded");
    check(m.materials[0].kd[0] == 0.25f && m.materials[0].kd[2] == 0.75f,
          "Kd parsed");
    check(m.materials[0].alpha == 0.5f, "d parsed");
    check(m.materials[0].map_kd &&
          strstr(m.materials[0].map_kd, "objwork/checker.png") != NULL,
          "map_Kd resolved relative to MTL");
    check(m.tris[0].material == 0, "usemtl assigned to triangles");
    ol_obj_free(&m);
    check(m.positions == NULL && m.tri_count == 0, "ol_obj_free resets model");

    /* Negative indices are relative to the end of the vertex list. */
    check(write_file(obj_path,
        "v 0 0 0\nv 1 0 0\nv 0 1 0\n"
        "f -3 -2 -1\n") == 0, "write negative-index OBJ");
    check(ol_obj_load(obj_path, &m, err, sizeof(err)) == 0,
          "negative indices load");
    check(m.tri_count == 1 && m.tris[0].v[0] == 0 &&
          m.tris[0].v[2] == 2, "negative index mapping");
    ol_obj_free(&m);

    /* Missing MTL is allowed; texture and material lookups simply stay empty. */
    check(write_file(obj_path,
        "mtllib missing.mtl\nv 0 0 0\nv 1 0 0\nv 0 1 0\nf 1 2 3\n") == 0,
        "write OBJ with missing MTL");
    check(ol_obj_load(obj_path, &m, err, sizeof(err)) == 0,
          "missing MTL is non-fatal");
    check(m.material_count == 0 && m.tri_count == 1, "untextured OBJ still loads");
    ol_obj_free(&m);

    /* Invalid indices and empty meshes fail cleanly. */
    check(write_file(obj_path,
        "v 0 0 0\nv 1 0 0\nv 0 1 0\nf 1 2 9\n") == 0,
        "write bad-index OBJ");
    check(ol_obj_load(obj_path, &m, err, sizeof(err)) != 0,
          "out-of-range face index rejected");
    check(write_file(obj_path, "v 0 0 0\n") == 0, "write empty OBJ");
    check(ol_obj_load(obj_path, &m, err, sizeof(err)) != 0,
          "mesh without faces rejected");
    check(ol_obj_load("objwork/missing.obj", &m, err, sizeof(err)) != 0,
          "missing OBJ rejected");

    printf("obj: %d checks, %d failed\n", checks, fails);
    return fails ? 1 : 0;
}
