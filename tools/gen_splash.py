#!/usr/bin/env python3
"""Generate the original splash demo images.

Outputs:
  assets/splash.png             -- current OpenLua introduction
  assets/splash_old.png         -- retro "old splash" variant
  assets/splash_oldschool.png   -- original OldSchool-Library tribute

Everything is drawn from code; the 5x7 glyphs come from tools/gen_font.py so
the repository does not depend on third-party artwork or a system font.
"""

import os
import struct
import zlib

from gen_font import A

W, H = 480, 272


def chunk(tag, data):
    body = tag + data
    return struct.pack(">I", len(data)) + body + struct.pack(">I", zlib.crc32(body) & 0xFFFFFFFF)


def write_png(path, img):
    stride = W * 4 + 1
    raw = bytearray(stride * H)
    for y in range(H):
        raw[y * stride] = 0
        raw[y * stride + 1:(y + 1) * stride] = img[y * W * 4:(y + 1) * W * 4]
    png = (b"\x89PNG\r\n\x1a\n" +
           chunk(b"IHDR", struct.pack(">IIBBBBB", W, H, 8, 6, 0, 0, 0)) +
           chunk(b"IDAT", zlib.compress(bytes(raw), 9)) +
           chunk(b"IEND", b""))
    os.makedirs(os.path.dirname(path) or ".", exist_ok=True)
    with open(path, "wb") as f:
        f.write(png)
    print(f"{path}: {W}x{H}, {len(png)} bytes")


def put(img, x, y, color, alpha=255):
    if 0 <= x < W and 0 <= y < H:
        o = (y * W + x) * 4
        img[o:o + 4] = bytes((color[0], color[1], color[2], alpha))


def blend(img, x, y, color, amount):
    if not (0 <= x < W and 0 <= y < H):
        return
    o = (y * W + x) * 4
    for i in range(3):
        img[o + i] = (img[o + i] * (255 - amount) + color[i] * amount) // 255


def fill_rect(img, x0, y0, w, h, color):
    for y in range(y0, y0 + h):
        for x in range(x0, x0 + w):
            put(img, x, y, color)


def text(img, x, y, s, scale, color):
    advance = 6 * scale
    for i, ch in enumerate(s.upper()):
        rows = A.get(ord(ch))
        if not rows:
            continue
        gx = x + i * advance
        for ry, row in enumerate(rows):
            for rx, bit in enumerate(row):
                if bit == "#":
                    fill_rect(img, gx + rx * scale, y + ry * scale, scale, scale, color)


def text_width(s, scale):
    return len(s) * 6 * scale - scale


def centered_text(img, y, s, scale, color):
    text(img, (W - text_width(s, scale)) // 2, y, s, scale, color)


def ring(img, cx, cy, r_out, r_in, color):
    r2o, r2i = r_out * r_out, r_in * r_in
    for y in range(cy - r_out, cy + r_out + 1):
        for x in range(cx - r_out, cx + r_out + 1):
            d2 = (x - cx) ** 2 + (y - cy) ** 2
            if r2i <= d2 <= r2o:
                put(img, x, y, color)


def checker_band(img, y0, h, a, b, cell=16):
    for y in range(y0, y0 + h):
        for x in range(W):
            put(img, x, y, a if ((x // cell + y // cell) & 1) else b)


def make_new():
    img = bytearray(W * H * 4)
    for y in range(H):
        t = y / (H - 1)
        base = (int(8 + 10 * t), int(14 + 18 * t), int(32 + 36 * t))
        for x in range(W):
            # A faint diagonal weave keeps the flat background from banding.
            shade = 5 if ((x + y) // 32) & 1 else 0
            put(img, x, y, (base[0] + shade, base[1] + shade, base[2] + shade))

    ring(img, 240, 86, 60, 56, (40, 80, 150))
    ring(img, 240, 86, 50, 40, (235, 112, 42))
    ring(img, 240, 86, 29, 22, (48, 180, 160))
    # A short orange rule separates the mark from the wordmark.
    fill_rect(img, 200, 148, 80, 3, (235, 112, 42))

    centered_text(img, 162, "OPENLUA", 4, (238, 240, 246))
    centered_text(img, 201, "PSP HOMEBREW RUNTIME", 2, (120, 190, 230))
    centered_text(img, 243, "ONELUA API COMPATIBLE", 1, (120, 132, 154))
    return img


def make_old():
    img = bytearray(W * H * 4)
    for y in range(H):
        t = y / (H - 1)
        base = (int(20 + 20 * t), int(17 + 17 * t), int(22 + 10 * t))
        for x in range(W):
            put(img, x, y, base)

    checker_band(img, 0, 16, (214, 190, 122), (34, 30, 26))
    checker_band(img, H - 16, 16, (214, 190, 122), (34, 30, 26))

    centered_text(img, 70, "OPENLUA", 4, (238, 214, 146))
    centered_text(img, 126, "OLD SPLASH", 3, (240, 128, 46))
    fill_rect(img, 150, 174, 180, 2, (120, 96, 52))
    centered_text(img, 194, "THE CLASSIC INTRODUCTION", 1, (198, 176, 132))

    # Subtle interlacing for the retro look.
    for y in range(0, H, 3):
        for x in range(W):
            blend(img, x, y, (0, 0, 0), 18)
    return img


def make_oldschool():
    img = bytearray(W * H * 4)
    for y in range(H):
        t = y / (H - 1)
        base = (int(8 + 8 * t), int(22 + 20 * t), int(42 + 34 * t))
        for x in range(W):
            put(img, x, y, base)

    # Checkerboard corners, an OSLib-era visual signature.
    for y in range(0, H):
        for x in range(0, W):
            if x < 48 or x >= W - 48 or y < 32 or y >= H - 32:
                if ((x // 16 + y // 16) & 1):
                    blend(img, x, y, (30, 90, 130), 90)

    ring(img, 240, 52, 32, 28, (60, 150, 200))
    centered_text(img, 98, "OSLIB", 6, (235, 242, 250))
    centered_text(img, 158, "OLDSCHOOL LIBRARY", 2, (98, 200, 225))
    fill_rect(img, 150, 188, 180, 2, (60, 130, 170))
    centered_text(img, 204, "OPENLUA TRIBUTE SCREEN", 1, (190, 206, 226))
    centered_text(img, 226, "ORIGINAL ARTWORK", 1, (120, 150, 180))
    return img


def main():
    write_png("assets/splash.png", make_new())
    write_png("assets/splash_old.png", make_old())
    write_png("assets/splash_oldschool.png", make_oldschool())


if __name__ == "__main__":
    main()
