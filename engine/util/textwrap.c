#include "textwrap.h"

#include <string.h>

int ol_text_wrap(const char *src, int width, char *out, int out_size)
{
    int written = 0;
    const char *p = src ? src : "";

    if (!out || out_size <= 0)
        return 0;
    if (width < 1)
        width = 1;

    while (*p) {
        const char *nl = strchr(p, '\n');
        int remaining = nl ? (int)(nl - p) : (int)strlen(p);

        if (remaining == 0) {
            /* Preserve an explicit blank line. */
            if (written < out_size - 1)
                out[written++] = '\n';
            p = nl + 1;
            continue;
        }

        while (remaining > 0) {
            int take = remaining > width ? width : remaining;

            /* Prefer the last space/tab inside the window so words stay
             * intact; fall back to a hard cut for long unbroken tokens
             * such as asset paths. */
            if (take < remaining) {
                int cut = take;
                while (cut > 0 && p[cut] != ' ' && p[cut] != '\t')
                    cut--;
                if (cut > 0)
                    take = cut;
            }

            int n = take;
            while (n > 0 && (p[n - 1] == ' ' || p[n - 1] == '\t'))
                n--;

            for (int i = 0; i < n && written < out_size - 1; i++)
                out[written++] = p[i];
            if (written < out_size - 1)
                out[written++] = '\n';

            p += take;
            remaining -= take;

            /* Continuation lines drop the whitespace at the break. */
            while (remaining > 0 && (*p == ' ' || *p == '\t')) {
                p++;
                remaining--;
            }
        }

        if (nl)
            p = nl + 1;
    }

    out[written] = '\0';
    return written;
}
