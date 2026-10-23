#!/usr/bin/env python3
"""Generate font.png — a readable 8x16 bitmap font sheet (one row of 96 glyphs).

Glyphs map ASCII 0x20 (space) .. 0x7F, left to right, one row each.
Each cell is 8x16; the 5x7 glyph is placed at (x=1, y=2) inside the cell.
White pixels on transparent background, RGBA 8-bit.
"""
import os, struct, zlib

CELL_W, CELL_H = 8, 16
FIRST, LAST = 32, 126
W = CELL_W * (LAST - FIRST + 1)   # 95 * 8 = 760 px
H = CELL_H                        # 16 px

# 5 wide x 7 tall ASCII art; '.'=off  '#'=on.  Seven rows, top to bottom.
A = {}
def add(code, r0, r1, r2, r3, r4, r5, r6):
    for r in (r0, r1, r2, r3, r4, r5, r6):
        assert len(r) == 5, (code, r)
    A[code] = (r0, r1, r2, r3, r4, r5, r6)

# ---------------------------------------------------------------- space
add( 32, ".....",".....",".....",".....",".....",".....",".....")
add( 33, ".#...",".#...",".#...",".#...",".....",".#...",".....")            # !
add( 34, ".#.#.",".#.#.",".....",".....",".....",".....",".....")            # "
add( 35, ".#.#.","#####",".#.#.","#####",".....",".....",".....")            # #
add( 36, "##...",".#...",".##..","##...",".#...","..##.",".....")            # $
add( 37, ".#...",".##..","...#.","..#..",".#...","...#.","..#..")            # %
add( 38, ".##..",".#.#.",".#...",".##..",".#.#.","..##.",".....")            # &
add( 39, ".#...",".#...",".....",".....",".....",".....",".....")            # '
add( 40, "..#..",".#...",".#...",".#...",".#...","..#..",".....")            # (
add( 41, "..#..","..#..","...#.","...#.","...#.","..#..",".....")            # )
add( 42, ".#.#.","..#..","#####","..#..",".##..",".....",".....")            # *
add( 43, ".....","..#..","..#..","#####","..#..","..#..",".....")            # +
add( 44, ".....",".....",".....",".....",".#...",".#...","..#..")            # ,
add( 45, ".....",".....",".....","#####",".....",".....",".....")            # -
add( 46, ".....",".....",".....",".....",".....","..##.","..##.")            # .
add( 47, "....#","...#.","..#..",".#...",".#...","#....",".....")            # /
add( 48, ".###.","#...#","#..##","#.#..","##...","#...#",".###.")            # 0
add( 49, "..#..",".##..","..#..","..#..","..#..","..#..",".###.")            # 1
add( 50, ".###.","#...#","....#","...#.","..#..",".#...","#####")            # 2
add( 51, "#####","...#.","..#..","...#.","....#","#...#",".###.")            # 3
add( 52, "...#.","..##.",".#.#.","#..#.","#####","...#.","...#.")            # 4
add( 53, "#####","#....","#....","####.","....#","#...#",".###.")            # 5
add( 54, ".###.","#....","#....","####.","#...#","#...#",".###.")            # 6
add( 55, "#####","....#","...#.","..#..","..#..","..#..","..#..")            # 7
add( 56, ".###.","#...#","#...#",".###.","#...#","#...#",".###.")            # 8
add( 57, ".###.","#...#","#...#",".####","....#","....#",".###.")            # 9
add( 58, ".....",".##..",".##..",".....",".##..",".##..",".....")            # :
add( 59, ".....",".##..",".##..",".....",".##..",".##..","..#..")            # ;
add( 60, "..#..",".#...","..#..",".#...",".#...","..#..",".....")            # <
add( 61, ".....",".....","#####",".....","#####",".....",".....")            # =
add( 62, "..#..","...#.","..#..","...#.","...#.","..#..",".....")            # >
add( 63, ".###.","#...#","....#","...#.","..#..",".....","..#..")            # ?
add( 64, ".###.","#...#","#.##.","#.##.","#.###","#...#",".###.")            # @
add( 65, ".###.","#...#","#...#","#####","#...#","#...#","#...#")            # A
add( 66, "####.","#...#","#...#","####.","#...#","#...#","####.")            # B
add( 67, ".###.","#...#","#....","#....","#....","#...#",".###.")            # C
add( 68, "####.","#...#","#...#","#...#","#...#","#...#","####.")            # D
add( 69, "#####","#....","#....","####.","#....","#....","#####")            # E
add( 70, "#####","#....","#....","####.","#....","#....","#....")            # F
add( 71, ".###.","#...#","#....","#.###","#...#","#...#",".###.")            # G
add( 72, "#...#","#...#","#...#","#####","#...#","#...#","#...#")            # H
add( 73, ".###.","..#..","..#..","..#..","..#..","..#..",".###.")            # I
add( 74, "..###","...#.","...#.","...#.","...#.","#..#.",".##..")            # J
add( 75, "#..#.","#.#..","##...","#....","##...","#.#..","#..#.")            # K
add( 76, "#....","#....","#....","#....","#....","#....","#####")            # L
add( 77, "#...#","##.##","#.#.#","#.#.#","#...#","#...#","#...#")            # M
add( 78, "#...#","#..##","#.#.#","##..#","#...#","#...#","#...#")            # N
add( 79, ".###.","#...#","#...#","#...#","#...#","#...#",".###.")            # O
add( 80, "####.","#...#","#...#","####.","#....","#....","#....")            # P
add( 81, ".###.","#...#","#...#","#...#","#.#.#","#..#.",".##.#")            # Q
add( 82, "####.","#...#","#...#","####.","##...","#.#..","#..#.")            # R
add( 83, ".####","#....","#....",".###.","....#","....#","####.")            # S
add( 84, "#####","..#..","..#..","..#..","..#..","..#..","..#..")            # T
add( 85, "#...#","#...#","#...#","#...#","#...#","#...#",".###.")            # U
add( 86, "#...#","#...#","#...#","#...#","#...#",".#.#.","..#..")            # V
add( 87, "#...#","#...#","#...#","#.#.#","#.#.#","##.##","#...#")            # W
add( 88, "#...#","#...#",".#.#.","..#..",".#.#.","#...#","#...#")            # X
add( 89, "#...#","#...#",".#.#.","..#..","..#..","..#..","..#..")            # Y
add( 90, "#####","....#","...#.","..#..",".#...","#....","#####")            # Z
add( 91, ".###.",".#...","#....","#....","#....",".#...",".###.")            # [
add( 92, "#....",".#...",".#...","..#..","...#.","...#.","....#")            # \
add( 93, ".###.","....#","#....","#....","#....","....#",".###.")            # ]
add( 94, "..#..",".#.#.","#...#",".....",".....",".....",".....")            # ^
add( 95, ".....",".....",".....",".....",".....",".....","#####")            # _
add( 96, "#.#..",".#...",".....",".....",".....",".....",".....")            # `
# lowercase a..z -> mirror uppercase (common for simple spritesheet fonts)
for off, up in enumerate("ABCDEFGHIJKLMNOPQRSTUVWXYZ"):
    lo = ord('a') + off
    A[lo] = A[ord(up)]
# fill any holes in 32..126 with blank so glyph count is contiguous
BLANK = (".....",)*7
for c in range(FIRST, LAST + 1):
    A.setdefault(c, BLANK)

def _chunk(tag, data):
    c = tag + data
    return struct.pack(">I", len(data)) + c + struct.pack(">I", zlib.crc32(c) & 0xFFFFFFFF)

def make(path):
    os.makedirs(os.path.dirname(path) or ".", exist_ok=True)
    stride = W * 4 + 1                 # PNG filter byte + RGBA row
    img = bytearray(stride * H)        # transparent, filter type 0
    for i in range(LAST - FIRST + 1):
        code = FIRST + i
        ox, oy = i * CELL_W + 1, 2
        for ry, row in enumerate(A[code]):
            for rx in range(5):
                if row[rx] == '#':
                    o = (oy + ry) * stride + 1 + (ox + rx) * 4
                    img[o:o+4] = b'\xff\xff\xff\xff'
    out = (b'\x89PNG\r\n\x1a\n'
           + _chunk(b'IHDR', struct.pack(">IIBBBBB", W, H, 8, 6, 0, 0, 0))
           + _chunk(b'IDAT', zlib.compress(bytes(img), 9))
           + _chunk(b'IEND', b''))
    open(path, "wb").write(out)
    print(f"{path}: {W}x{H} px, {len(out)} bytes, glyphs 0x20..0x7E, cell {CELL_W}x{CELL_H}")

SFONT_H = 8          # 1 px separator row + 7 px glyph rows
SFONT_SEP = 2        # width of each magenta separator run

def make_sfont(path):
    """Generate a classic SFont PNG with variable-width glyphs.

    The top row contains magenta separator runs. Characters start at ASCII
    '!' (33); the glyph area follows each separator. Ink is trimmed and
    padded by one transparent column on each side, so glyph advances vary
    with the character (for example 'W' is wider than 'I').
    """
    os.makedirs(os.path.dirname(path) or ".", exist_ok=True)
    glyphs = []
    for code in range(33, 127):
        rows = A[code]
        ink_left, ink_right = 5, -1
        for row in rows:
            for x, ch in enumerate(row):
                if ch == '#':
                    ink_left = min(ink_left, x)
                    ink_right = max(ink_right, x)
        if ink_right < 0:
            left, width = 0, 3
        else:
            left = ink_left - 1
            width = (ink_right - ink_left + 1) + 2
        glyphs.append((rows, left, width))

    total_w = SFONT_SEP + sum(width + SFONT_SEP for _, _, width in glyphs)
    stride = total_w * 4 + 1                     # PNG filter byte + RGBA row
    img = bytearray(stride * SFONT_H)            # transparent, filter type 0

    def put(x, y, rgba):
        o = y * stride + 1 + x * 4
        img[o:o+4] = rgba

    x = 0
    for rows, left, width in glyphs:
        for sx in range(SFONT_SEP):
            put(x + sx, 0, b'\xff\x00\xff\xff')  # magenta separator
        x += SFONT_SEP
        for ry, row in enumerate(rows):
            for rx in range(width):
                src = left + rx
                if 0 <= src < len(row) and row[src] == '#':
                    put(x + rx, 1 + ry, b'\xff\xff\xff\xff')
        x += width
    for sx in range(SFONT_SEP):
        put(x + sx, 0, b'\xff\x00\xff\xff')

    out = (b'\x89PNG\r\n\x1a\n'
           + _chunk(b'IHDR', struct.pack(">IIBBBBB", total_w, SFONT_H, 8, 6, 0, 0, 0))
           + _chunk(b'IDAT', zlib.compress(bytes(img), 9))
           + _chunk(b'IEND', b''))
    open(path, "wb").write(out)
    print(f"{path}: {total_w}x{SFONT_H} px, {len(out)} bytes, "
          f"classic SFont glyphs 0x21..0x7E")

if __name__ == "__main__":
    make("assets/font.png")
    make_sfont("assets/sfont.png")
