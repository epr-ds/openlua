#define _GNU_SOURCE
#include "engine/fs/olfs.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

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

static int touch(const char *path)
{
    FILE *f = fopen(path, "w");
    if (!f)
        return -1;
    fputs("x", f);
    fclose(f);
    return 0;
}

int main(void)
{
    char original[1024];
    char tmp[] = "/tmp/openlua-olfs-XXXXXX";
    char path[1024];
    char out[64];

    if (!getcwd(original, sizeof(original)) || !mkdtemp(tmp)) {
        fprintf(stderr, "olfs setup failed\n");
        return 1;
    }

    check(olfs_path_dir("/a/b/c.lua", out, sizeof(out)) == 0 &&
          strcmp(out, "/a/b") == 0, "path_dir nested");
    check(olfs_path_dir("file.lua", out, sizeof(out)) == 0 &&
          out[0] == 0, "path_dir no slash");
    check(olfs_path_dir("/file", out, sizeof(out)) == 0 &&
          strcmp(out, "/") == 0, "path_dir root child");
    check(olfs_path_dir("a/b", out, 1) == -1, "path_dir cap too small");

    check(olfs_path_base("/a/b.lua", out, sizeof(out)) == 0 &&
          strcmp(out, "b.lua") == 0, "path_base nested");
    check(olfs_path_base("plain", out, sizeof(out)) == 0 &&
          strcmp(out, "plain") == 0, "path_base plain");
    check(olfs_path_base("a", out, 1) == -1, "path_base cap too small");

    check(olfs_path_ext("a.b.c", out, sizeof(out)) == 0 &&
          strcmp(out, "c") == 0, "path_ext last dot");
    check(olfs_path_ext(".hidden", out, sizeof(out)) == 0 &&
          out[0] == 0, "path_ext hidden");
    check(olfs_path_ext("file", out, sizeof(out)) == 0 &&
          out[0] == 0, "path_ext none");
    check(olfs_path_ext("a.c", out, 1) == -1, "path_ext cap too small");

    size_t need = strlen(original) + 1;
    char *exact = (char *)malloc(need);
    check(exact != NULL, "cwd exact allocation");
    check(olfs_cwd(out, 0) == -1, "cwd cap zero");
    check(olfs_cwd(NULL, sizeof(out)) == -1, "cwd null buffer");
    check(olfs_cwd(path, sizeof(path)) == 0 &&
          strcmp(path, original) == 0, "cwd full copy");
    if (exact) {
        check(olfs_cwd(exact, (unsigned int)need) == 0 &&
              strcmp(exact, original) == 0, "cwd exact cap");
        check(olfs_cwd(exact, (unsigned int)need - 1) == -1,
              "cwd one byte short");
        free(exact);
    }

    check(olfs_chdir(tmp) == 0, "chdir absolute");
    check(strcmp(olfs_cwd_current(), tmp) == 0, "chdir cache absolute");
    check(olfs_chdir(".") == 0, "chdir relative");
    check(olfs_cwd(path, sizeof(path)) == 0 &&
          strcmp(path, tmp) == 0, "cwd after relative chdir");

    snprintf(path, sizeof(path), "%s/a/b", tmp);
    check(olfs_mkdir_p(path) == 0, "mkdir_p absolute chain");
    check(olfs_isdir(path) == 1, "mkdir_p created leaf");
    check(olfs_mkdir_p(path) == 0, "mkdir_p existing dir succeeds");
    check(olfs_mkdir_p("") == -1, "mkdir_p empty fails");

    char longpath[520];
    memset(longpath, 'x', sizeof(longpath));
    longpath[sizeof(longpath) - 1] = 0;
    check(olfs_mkdir_p(longpath) == -1, "mkdir_p long path fails");

    snprintf(path, sizeof(path), "%s/notadir", tmp);
    check(touch(path) == 0, "create file for mkdir conflict");
    check(olfs_mkdir_p(path) == -1, "mkdir_p file conflict fails");

    snprintf(path, sizeof(path), "%s/one.txt", tmp);
    check(touch(path) == 0, "create one.txt");
    snprintf(path, sizeof(path), "%s/two.txt", tmp);
    check(touch(path) == 0, "create two.txt");

    char longname[256];
    memset(longname, 'L', 200);
    strcpy(longname + 200, ".txt");
    snprintf(path, sizeof(path), "%s/%s", tmp, longname);
    check(touch(path) == 0, "create long name");

    OLEntry entries[8];
    int outn = -1, trunc = -1;
    check(olfs_list(tmp, entries, 0, &outn, &trunc) == 0 &&
          outn == 0 && trunc == 1, "list zero truncates");
    check(olfs_list(tmp, entries, 2, &outn, &trunc) == 0 &&
          outn == 2 && trunc == 1, "list cap truncates");
    check(olfs_list(tmp, entries, 8, &outn, &trunc) == 0 &&
          outn == 4 && trunc == 1, "list skips long name");
    int long_seen = 0;
    for (int i = 0; i < outn; i++)
        if (strstr(entries[i].name, "LLLL"))
            long_seen = 1;
    check(long_seen == 0, "long name not exposed");

    check(olfs_chdir(original) == 0, "restore cwd");
    check(olfs_delete(tmp) == 0, "remove test tree");

    printf("olfs: %d checks, %d failed\n", checks, fails);
    return fails ? 1 : 0;
}
