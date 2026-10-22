#!/usr/bin/env python3
"""Generate sfont.png - a proportional SFont strip for the sfont demo/tests.

Classic SFont layout: row 0 holds magenta (255,0,255) markers and every glyph
is the non-magenta run between two markers. Glyphs cover ASCII '!' (0x21) ..
'~' (0x7E); space advances by the width of '!'.

The 5x7 art from gen_font.py is trimmed to its used columns, drawn at 2x with
a vertical yellow-to-orange gradient on a transparent background.
"""
import os, struct, sys, zlib

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from gen_font import A, _chunk  # noqa: E402

FIRST, LAST = 33, 126
SCALE = 2
PAD_L, PAD_R = 1, 2                 # transparent columns around each glyph
GLYPH_Y = 2                         # top margin inside the glyph rows
H = 1 + GLYPH_Y + 7 * SCALE + 2     # marker row + glyph rows = 19 px
MAGENTA = b'\xff\x00\xff\xff'


def trimmed(rows):
    cols = [c for c in range(5) if any(r[c] == '#' for r in rows)]
    if not cols:
        return 0, 1
    return cols[0], cols[-1] - cols[0] + 1


def color(y):
    t = (y - 1 - GLYPH_Y) / (7 * SCALE - 1)
    return bytes((255, int(230 - 120 * t), int(60 - 40 * t), 255))


def make(path):
    glyphs = []
    for code in range(FIRST, LAST + 1):
        c0, cw = trimmed(A[code])
        glyphs.append((code, c0, PAD_L + cw * SCALE + PAD_R))
    W = 1 + sum(w + 1 for _, _, w in glyphs)   # leading marker + run+marker

    img = bytearray(W * H * 4)
    x = 0
    img[0:4] = MAGENTA
    x = 1
    for code, c0, w in glyphs:
        for ry, row in enumerate(A[code]):
            for rx in range(5):
                if row[rx] != '#':
                    continue
                for sy in range(SCALE):
                    for sx in range(SCALE):
                        px = x + PAD_L + (rx - c0) * SCALE + sx
                        py = 1 + GLYPH_Y + ry * SCALE + sy
                        o = (py * W + px) * 4
                        img[o:o + 4] = color(py)
        x += w
        img[x * 4:x * 4 + 4] = MAGENTA
        x += 1
    assert x == W

    raw = b''.join(b'\x00' + bytes(img[y * W * 4:(y + 1) * W * 4])
                   for y in range(H))
    out = (b'\x89PNG\r\n\x1a\n'
           + _chunk(b'IHDR', struct.pack(">IIBBBBB", W, H, 8, 6, 0, 0, 0))
           + _chunk(b'IDAT', zlib.compress(raw, 9))
           + _chunk(b'IEND', b''))
    os.makedirs(os.path.dirname(path) or ".", exist_ok=True)
    open(path, "wb").write(out)
    print(f"{path}: {W}x{H} px, {len(out)} bytes, {len(glyphs)} glyphs "
          f"'!'..'~', height {H - 1}, space {glyphs[0][2]} px")


if __name__ == "__main__":
    make("assets/sfont.png")
