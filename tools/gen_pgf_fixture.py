#!/usr/bin/env python3
"""Generate the synthetic PGF fixtures used by test/host/test_pgf.c.

The fixture is a revision 2 PGF that exercises the software decoder without
shipping a real (copyrighted) OneLua game font:

  glyph  char  shape  flags                purpose
  -----  ----  -----  -------------------  ---------------------------------
    0     ' '    -    H rows               empty glyph with its own advance
   33     'A'   6x8   H rows               row-major RLE
   34     'B'   6x8   H rows               atlas packing
   35     'C'   4x5   V rows               column-major RLE
   36     'D'   6x8   H rows + index bits  metric table lookup
   'Z'    -      -    -                    char map points outside the font

Pixels of 'A'/'B'/'D' are (x + y) & 15, pixels of 'C' are (y * 4 + x) & 15.
Advances are 6px except space (4px) and 'D' (5px, indexed table).

Usage:  python3 tools/gen_pgf_fixture.py assets
"""

import os
import struct
import sys

FIRST = 32
LAST = 126
COUNT = LAST - FIRST + 1

HDR_SIZE = 392

# Glyphs that carry a bitmap.  Every other code point is empty.
BITMAP_CHARS = {ord('A'), ord('B'), ord('C'), ord('D')}
INDEXED_CHARS = {ord('D')}
VERTICAL_CHARS = {ord('C')}


class BitWriter:
    def __init__(self):
        self.bits = []

    def put(self, value, n):
        for i in range(n):
            self.bits.append((value >> i) & 1)

    def put_signed(self, value, n):
        if value < 0:
            value += 1 << n
        self.put(value, n)

    def align(self):
        while len(self.bits) % 8:
            self.bits.append(0)

    def bytes(self):
        self.align()
        out = bytearray(len(self.bits) // 8)
        for i, bit in enumerate(self.bits):
            out[i >> 3] |= bit << (i & 7)
        return bytes(out)


def glyph_record(code):
    xadj, yadj, adv = 64, 448, 384        # 1px, 7px, 6px
    if code == ord(' '):
        adv = 256                         # 4px
    if code == ord('D'):
        adv = 320                         # 5px, taken from the table

    if code in BITMAP_CHARS:
        if code == ord('C'):
            w, h = 4, 5
            pixels = [(y * 4 + x) & 15 for x in range(w) for y in range(h)]
        else:
            w, h = 6, 8
            pixels = [(x + y) & 15 for y in range(h) for x in range(w)]
    else:
        w = h = 0
        pixels = None

    indexed = code in INDEXED_CHARS
    flags = 0x02 if code in VERTICAL_CHARS else 0x01
    if indexed:
        flags |= 0x04 | 0x08 | 0x10 | 0x20

    bw = BitWriter()
    bw.put(0, 14)            # uncompressed bitmap size (unused)
    bw.put(w, 7)
    bw.put(h, 7)
    bw.put_signed(1, 7)      # left bearing
    bw.put_signed(7, 7)      # top bearing
    bw.put(flags, 6)
    bw.put(0, 2)             # shadow flags
    bw.put(0, 2)
    bw.put(0, 3)
    bw.put(0, 9)             # shadow glyph id

    if indexed:
        # Four 8-bit indexes into the metric tables.
        for _ in range(4):
            bw.put(0, 8)
    else:
        # Raw 26.6 metrics: dimension, x bearing, y bearing, advance. Each is
        # a (horizontal, vertical) pair of 32-bit signed values.
        for hval, vval in ((0, 0), (xadj, xadj), (yadj, yadj), (adv, adv)):
            bw.put_signed(hval, 32)
            bw.put_signed(vval, 32)

    if pixels:
        for value in pixels:
            bw.put(0xF, 4)   # one-pixel run
            bw.put(value, 4)
    return bw.bytes()


def header():
    h = bytearray(HDR_SIZE)
    struct.pack_into('<HH', h, 0, 0, HDR_SIZE)
    h[4:8] = b'PGF0'
    struct.pack_into('<ii', h, 8, 2, 6)           # revision, version
    struct.pack_into('<iiii', h, 16, COUNT, COUNT, 8, 16)
    h[32:36] = bytes((4, 4, 4, 0))                # pad, bpp, pad
    struct.pack_into('<iiii', h, 36, 648, 648, 8192, 8192)
    h[53:53 + 12] = b'OpenLua Test'
    h[117:117 + 7] = b'Regular'
    struct.pack_into('<HH', h, 182, FIRST, LAST)
    struct.pack_into('<iiiiii', h, 212, 1024, -128, 512, 1024, -256, 960)
    struct.pack_into('<ii', h, 236, 384, 384)     # max advance
    struct.pack_into('<ii', h, 244, 1024, 1024)   # max size
    struct.pack_into('<HH', h, 252, 6, 8)         # max glyph w/h
    h[258:262] = bytes((1, 1, 1, 1))              # metric table lengths
    struct.pack_into('<ii', h, 364, 0, 0)         # shadow map len/bpe
    return h


def metric_tables():
    out = bytearray()
    for hval, vval in ((0, 0), (64, 64), (448, 448), (320, 320)):
        out += struct.pack('<ii', hval, vval)
    return out


def build():
    records = []
    pointers = []
    offset = 0

    for code in range(FIRST, LAST + 1):
        rec = glyph_record(code)
        pointers.append(offset // 4)
        records.append(rec)
        offset += len(rec)

    assert offset % 4 == 0

    # Char map: glyph index == code - FIRST, except 'Z' which points past the
    # glyph table so the decoder must treat it as absent.
    charmap = bytearray(range(COUNT))
    charmap[ord('Z') - FIRST] = 0xFF

    data = bytearray()
    data += header()
    data += metric_tables()
    data += charmap
    while len(data) % 4:
        data += b'\0'
    for ptr in pointers:
        data += struct.pack('<H', ptr)            # pointer in 4-byte units
    while len(data) % 4:
        data += b'\0'
    data += b''.join(records)
    return bytes(data)


def main():
    outdir = sys.argv[1] if len(sys.argv) > 1 else 'assets'
    data = build()

    full = os.path.join(outdir, 'test.pgf')
    with open(full, 'wb') as fp:
        fp.write(data)

    # Revision 3 must be rejected (the decoder only supports revision 2).
    rev3 = bytearray(data[:HDR_SIZE])
    struct.pack_into('<i', rev3, 8, 3)
    with open(os.path.join(outdir, 'test-rev3.pgf'), 'wb') as fp:
        fp.write(rev3)

    # Truncated header must be rejected too.
    with open(os.path.join(outdir, 'test-truncated.pgf'), 'wb') as fp:
        fp.write(data[:100])

    print('wrote %s (%d bytes)' % (full, len(data)))


if __name__ == '__main__':
    main()
