#include "engine/util/textwrap.h"

#include <stdio.h>
#include <string.h>

static int checks;
static int fails;

static void check(const char *name, const char *src, int width,
                  const char *want)
{
    char out[512];
    int n = ol_text_wrap(src, width, out, (int)sizeof(out));

    checks++;
    if (strcmp(out, want) != 0) {
        fails++;
        printf("FAIL %s: got \"%s\" want \"%s\"\n", name, out, want);
    }

    checks++;
    if (n != (int)strlen(out)) {
        fails++;
        printf("FAIL %s: return %d != strlen %d\n",
               name, n, (int)strlen(out));
    }
}

int main(void)
{
    char small[5];

    /* Short lines pass through with a terminating newline. */
    check("short", "hello", 10, "hello\n");

    /* Word wrap prefers spaces and drops the break-point space. */
    check("words", "the quick brown fox", 10, "the quick\nbrown fox\n");

    /* An unbroken token wider than the window is cut hard. */
    check("long-token", "abcdefghij", 4, "abcd\nefgh\nij\n");

    /* Explicit newlines are preserved, including blank lines. */
    check("newline", "a\nb", 10, "a\nb\n");
    check("blank-line", "a\n\nb", 10, "a\n\nb\n");

    /* Leading indentation survives; trailing spaces are stripped. */
    check("indent", "  two  spaces  ", 10, "  two\nspaces\n");

    /* Empty input yields empty output. */
    check("empty", "", 10, "");

    /* A non-positive width clamps to one column. */
    check("width-clamp", "abc", 0, "a\nb\nc\n");

    /* Long asset paths wrap like a real loader error. */
    check("path",
          "assets/image/sprites/spartan/body_left_missing.png: No such file",
          20,
          "assets/image/sprites\n/spartan/body_left_m\n"
          "issing.png: No such\nfile\n");

    /* Truncation stays NUL-terminated and reports the bytes written. */
    memset(small, 'x', sizeof(small));
    {
        int n = ol_text_wrap("abcdef", 10, small, (int)sizeof(small));
        checks++;
        if (strcmp(small, "abcd") != 0) {
            fails++;
            printf("FAIL truncate: got \"%s\" want \"abcd\"\n", small);
        }
        checks++;
        if (n != 4) {
            fails++;
            printf("FAIL truncate: return %d want 4\n", n);
        }
    }

    /* Bad destinations are rejected instead of dereferenced. */
    checks++;
    if (ol_text_wrap("x", 10, NULL, 10) != 0) {
        fails++;
        printf("FAIL null-out: expected 0\n");
    }
    small[0] = 'x';
    checks++;
    if (ol_text_wrap("x", 10, small, 0) != 0) {
        fails++;
        printf("FAIL zero-size: expected 0\n");
    }
    checks++;
    if (small[0] != 'x') {
        fails++;
        printf("FAIL zero-size: output buffer was touched\n");
    }

    /* NULL source is treated as empty text. */
    check("null-src", NULL, 10, "");

    printf("textwrap: %d checks, %d failed\n", checks, fails);
    return fails ? 1 : 0;
}
