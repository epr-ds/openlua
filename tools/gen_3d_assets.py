#!/usr/bin/env python3
"""Generate the small original 3D demo assets.

Outputs:
  assets/3d/cube.obj
  assets/3d/cube.mtl
  assets/3d/checker.png

The cube is centred at the origin and uses one checker material.  The texture
is generated here so the repository does not depend on a third-party model or
image licence.
"""
import os
import struct
import zlib

OUT = "assets/3d"
W = H = 128


def chunk(tag, data):
    body = tag + data
    return struct.pack(">I", len(data)) + body + struct.pack(">I", zlib.crc32(body) & 0xFFFFFFFF)


def make_checker(path):
    stride = W * 4 + 1
    img = bytearray(stride * H)
    square = 16
    for y in range(H):
        row = y * stride + 1
        for x in range(W):
            cx, cy = x // square, y // square
            if (cx + cy) & 1:
                r, g, b = 30, 150, 160
            else:
                r, g, b = 230, 120, 40
            # White border makes the cube edges readable against the background.
            if x < 4 or y < 4 or x >= W - 4 or y >= H - 4:
                r = g = b = 235
            # Two coloured corner marks make rotation obvious.
            if 10 <= x < 26 and 10 <= y < 26:
                r, g, b = 245, 60, 60
            if W - 26 <= x < W - 10 and H - 26 <= y < H - 10:
                r, g, b = 60, 90, 245
            o = row + x * 4
            img[o:o + 4] = bytes((r, g, b, 255))

    png = (b"\x89PNG\r\n\x1a\n" +
           chunk(b"IHDR", struct.pack(">IIBBBBB", W, H, 8, 6, 0, 0, 0)) +
           chunk(b"IDAT", zlib.compress(bytes(img), 9)) +
           chunk(b"IEND", b""))
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "wb") as f:
        f.write(png)
    print(f"{path}: {W}x{H}, {len(png)} bytes")


def main():
    os.makedirs(OUT, exist_ok=True)

    obj = """# OpenLua original demo cube (axis-aligned, centred at the origin).
mtllib cube.mtl
o Cube

v -1 -1  1
v  1 -1  1
v  1  1  1
v -1  1  1
v  1 -1 -1
v -1 -1 -1
v -1  1 -1
v  1  1 -1

vt 0 0
vt 1 0
vt 1 1
vt 0 1

usemtl checker
f 1/1 2/2 3/3 4/4
f 2/1 5/2 8/3 3/4
f 5/1 6/2 7/3 8/4
f 6/1 1/2 4/3 7/4
f 4/1 3/2 8/3 7/4
f 6/1 5/2 2/3 1/4
"""
    with open(os.path.join(OUT, "cube.obj"), "w", encoding="ascii") as f:
        f.write(obj)

    mtl = """# OpenLua original demo material.
newmtl checker
Ka 0.20 0.20 0.20
Kd 1.00 1.00 1.00
Ks 0.00 0.00 0.00
Ns 0
d 1.0
map_Kd checker.png
"""
    with open(os.path.join(OUT, "cube.mtl"), "w", encoding="ascii") as f:
        f.write(mtl)

    make_checker(os.path.join(OUT, "checker.png"))
    print(f"{OUT}/cube.obj, {OUT}/cube.mtl")


if __name__ == "__main__":
    main()
