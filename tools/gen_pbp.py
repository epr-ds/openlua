#!/usr/bin/env python3
"""Generate a small PBP fixture for the game module demo and host tests.

Outputs assets/sample.pbp with a PARAM.SFO, ICON0, PIC0 and PIC1.  Everything
is drawn from code so the repository does not ship third-party artwork.
"""

import os
import struct
import zlib

from gen_font import A

PBP_HEADER = 40


def chunk(tag, data):
    body = tag + data
    return (struct.pack(">I", len(data)) + body +
            struct.pack(">I", zlib.crc32(body) & 0xFFFFFFFF))


def write_png_bytes(w, h, img):
    stride = w * 4 + 1
    raw = bytearray(stride * h)
    for y in range(h):
        raw[y * stride] = 0
        raw[y * stride + 1:(y + 1) * stride] = img[y * w * 4:(y + 1) * w * 4]
    return (b"\x89PNG\r\n\x1a\n" +
            chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 6, 0, 0, 0)) +
            chunk(b"IDAT", zlib.compress(bytes(raw), 9)) +
            chunk(b"IEND", b""))


def put(img, w, h, x, y, color, alpha=255):
    if not (0 <= x < w and 0 <= y < h):
        return
    r, g, b = color
    i = (y * w + x) * 4
    img[i:i + 4] = bytes((r, g, b, alpha))


def fill_rect(img, w, h, x0, y0, rw, rh, color):
    for y in range(y0, y0 + rh):
        for x in range(x0, x0 + rw):
            put(img, w, h, x, y, color)


def text(img, w, h, x, y, s, scale, color):
    for i, ch in enumerate(s.upper()):
        rows = A.get(ord(ch))
        if not rows:
            continue
        gx = x + i * 6 * scale
        for ry, row in enumerate(rows):
            for rx, bit in enumerate(row):
                if bit == "#":
                    fill_rect(img, w, h, gx + rx * scale, y + ry * scale,
                              scale, scale, color)


def text_width(s, scale):
    return len(s) * 6 * scale - scale


def make_icon():
    w, h = 144, 80
    img = bytearray(w * h * 4)
    for y in range(h):
        t = y / (h - 1)
        color = (int(18 + 20 * t), int(34 + 44 * t), int(60 + 70 * t))
        fill_rect(img, w, h, 0, y, w, 1, color)
    fill_rect(img, w, h, 0, 0, w, 2, (92, 196, 236))
    fill_rect(img, w, h, 0, h - 2, w, 2, (46, 98, 130))
    label = "OPENLUA"
    text(img, w, h, (w - text_width(label, 2)) // 2, 26, label, 2,
         (238, 242, 250))
    tag = "PBP DEMO"
    text(img, w, h, (w - text_width(tag, 1)) // 2, 50, tag, 1,
         (128, 198, 230))
    return w, h, img


def make_pic0():
    w, h = 310, 180
    img = bytearray(w * h * 4)
    for y in range(h):
        for x in range(w):
            t = x / (w - 1)
            u = y / (h - 1)
            color = (int(14 + 52 * t), int(28 + 36 * u),
                     int(52 + 82 * (1 - t)))
            put(img, w, h, x, y, color)
    fill_rect(img, w, h, 0, 0, w, 3, (78, 186, 226))
    fill_rect(img, w, h, 0, h - 3, w, 3, (44, 92, 128))
    text(img, w, h, (w - text_width("OPENLUA", 3)) // 2, 54, "OPENLUA", 3,
         (238, 242, 250))
    tag = "PBP METADATA DEMO"
    text(img, w, h, (w - text_width(tag, 1)) // 2, 112, tag, 1,
         (126, 194, 228))
    return w, h, img


def make_pic1():
    w, h = 480, 272
    img = bytearray(w * h * 4)
    for y in range(h):
        for x in range(w):
            t = x / (w - 1)
            u = y / (h - 1)
            color = (int(11 + 34 * t), int(24 + 40 * u),
                     int(46 + 92 * (1 - t)))
            put(img, w, h, x, y, color)
    for y in range(h):
        v = abs(y - h / 2) / (h / 2)
        c = (int(90 - 40 * v), int(170 - 80 * v), int(210 - 90 * v))
        fill_rect(img, w, h, 40, y, 2, 1, c)
        fill_rect(img, w, h, w - 42, y, 2, 1, c)
    text(img, w, h, (w - text_width("OPENLUA", 5)) // 2, 94, "OPENLUA", 5,
         (240, 244, 252))
    text(img, w, h, (w - text_width("GAME.INFO / GETICON0 / GETPIC0 / GETPIC1", 1)) // 2,
         172, "GAME.INFO / GETICON0 / GETPIC0 / GETPIC1", 1, (150, 210, 238))
    text(img, w, h, (w - text_width("SYNTHETIC PBP FIXTURE", 1)) // 2, 196,
         "SYNTHETIC PBP FIXTURE", 1, (104, 154, 188))
    return w, h, img


def sfo(entries):
    count = len(entries)
    key_start = 20 + count * 16
    key_table = bytearray()
    data_table = bytearray()
    index = bytearray()
    for key, fmt, value in entries:
        key_off = len(key_table)
        key_table += key.encode("utf-8") + b"\0"
        data_off = len(data_table)
        if fmt == 0x0404:
            data_table += struct.pack("<I", value)
            value_len = 4
            max_len = 4
        else:
            raw = value.encode("utf-8") + b"\0"
            data_table += raw
            value_len = max_len = len(raw)
        index += struct.pack("<HHIII", key_off, fmt, value_len, max_len,
                             data_off)
        while len(data_table) % 4:
            data_table += b"\0"
    data_start = key_start + len(key_table)
    header = struct.pack("<IIIII", 0x46535000, 0x00000101, key_start,
                         data_start, count)
    return header + index + key_table + data_table


def pbp(sections):
    offsets = [0] * 8
    body = bytearray()
    pos = PBP_HEADER
    for index, payload in sections:
        offsets[index] = pos
        body += payload
        pos += len(payload)
    header = struct.pack("<4sI8I", b"\0PBP", 0x00010000, *offsets)
    return header + body


def main():
    entries = [
        ("TITLE", 0x0004, "OpenLua Sample"),
        ("TITLE_ID", 0x0004, "OPENLUA01"),
        ("CATEGORY", 0x0004, "MG"),
        ("APP_VER", 0x0004, "1.00"),
        ("PARENTAL_LEVEL", 0x0404, 3),
        ("BOOTABLE", 0x0404, 1),
    ]
    icon0 = write_png_bytes(*make_icon())
    pic0 = write_png_bytes(*make_pic0())
    pic1 = write_png_bytes(*make_pic1())
    data = pbp([
        (0, sfo(entries)),
        (1, icon0),
        (3, pic0),
        (4, pic1),
    ])
    os.makedirs("assets", exist_ok=True)
    with open("assets/sample.pbp", "wb") as f:
        f.write(data)
    print("assets/sample.pbp: %d bytes (SFO %d, ICON0 %d, PIC0 %d, PIC1 %d)" %
          (len(data), len(sfo(entries)), len(icon0), len(pic0), len(pic1)))


if __name__ == "__main__":
    main()
