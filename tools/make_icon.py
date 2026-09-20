#!/usr/bin/env python3
"""Make src/win/autobleem.ico from tools/repo_icon.png - the Windows product's icon.

An .ico is a directory of images; Vista and later read PNG-compressed entries, so the file is the source
PNG as it is (128x128 - the large icon) plus 16, 32 and 48 px versions the shell wants for lists and the
taskbar. Only the stdlib: MSYS2's python has no Pillow, so the PNG is decoded here (8-bit RGB/RGBA, the
filters undone) and the small sizes are box-filtered from it, then re-encoded.

    tools/make_icon.py [--in tools/repo_icon.png] [--out src/win/autobleem.ico]
"""
import argparse
import os
import struct
import sys
import zlib


def read_png(path):
    """(width, height, rows of RGBA tuples) of an 8-bit RGB or RGBA non-interlaced PNG"""
    with open(path, "rb") as f:
        data = f.read()
    if data[:8] != b"\x89PNG\r\n\x1a\n":
        sys.exit("%s: not a PNG" % path)
    pos = 8
    width = height = 0
    color_type = 0
    idat = b""
    while pos < len(data):
        length, kind = struct.unpack(">I4s", data[pos:pos + 8])
        body = data[pos + 8:pos + 8 + length]
        pos += 12 + length
        if kind == b"IHDR":
            width, height, depth, color_type, _, _, interlace = struct.unpack(">IIBBBBB", body)
            if depth != 8 or color_type not in (2, 6) or interlace:
                sys.exit("%s: only 8-bit RGB/RGBA non-interlaced PNGs are read" % path)
        elif kind == b"IDAT":
            idat += body
        elif kind == b"IEND":
            break
    channels = 4 if color_type == 6 else 3
    raw = zlib.decompress(idat)
    stride = width * channels
    rows = []
    prev = bytearray(stride)
    p = 0
    for _ in range(height):
        filt = raw[p]
        line = bytearray(raw[p + 1:p + 1 + stride])
        p += 1 + stride
        for i in range(stride):
            a = line[i - channels] if i >= channels else 0
            b = prev[i]
            c = prev[i - channels] if i >= channels else 0
            if filt == 1:
                line[i] = (line[i] + a) & 255
            elif filt == 2:
                line[i] = (line[i] + b) & 255
            elif filt == 3:
                line[i] = (line[i] + (a + b) // 2) & 255
            elif filt == 4:
                pa, pb, pc = abs(b - c), abs(a - c), abs(a + b - 2 * c)
                pred = a if pa <= pb and pa <= pc else (b if pb <= pc else c)
                line[i] = (line[i] + pred) & 255
        px = []
        for x in range(width):
            o = x * channels
            px.append((line[o], line[o + 1], line[o + 2], line[o + 3] if channels == 4 else 255))
        rows.append(px)
        prev = line
    return width, height, rows


def write_png(width, height, rows):
    raw = b"".join(b"\x00" + bytes(v for px in row for v in px) for row in rows)

    def chunk(kind, body):
        return struct.pack(">I", len(body)) + kind + body + struct.pack(">I", zlib.crc32(kind + body) & 0xFFFFFFFF)

    return (b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 6, 0, 0, 0))
            + chunk(b"IDAT", zlib.compress(raw, 9)) + chunk(b"IEND", b""))


def resize(width, height, rows, size):
    """a box filter over the source pixels each destination pixel covers, alpha-weighted"""
    out = []
    for y in range(size):
        y0, y1 = y * height // size, max(y * height // size + 1, (y + 1) * height // size)
        row = []
        for x in range(size):
            x0, x1 = x * width // size, max(x * width // size + 1, (x + 1) * width // size)
            r = g = b = a = n = 0
            for sy in range(y0, y1):
                for sx in range(x0, x1):
                    pr, pg, pb, pa = rows[sy][sx]
                    r += pr * pa
                    g += pg * pa
                    b += pb * pa
                    a += pa
                    n += 1
            if a:
                row.append((r // a, g // a, b // a, a // n))
            else:
                row.append((0, 0, 0, 0))
        out.append(row)
    return out


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--in", dest="src", default="tools/repo_icon.png")
    ap.add_argument("--out", default="src/win/autobleem.ico")
    a = ap.parse_args()

    width, height, rows = read_png(a.src)
    if width != height:
        sys.exit("%s: the icon must be square" % a.src)
    sizes = [16, 32, 48]
    images = [(s, write_png(s, s, resize(width, height, rows, s))) for s in sizes if s < width]
    with open(a.src, "rb") as f:
        images.append((width, f.read()))

    header = struct.pack("<HHH", 0, 1, len(images))
    offset = len(header) + 16 * len(images)
    entries = b""
    for size, png in images:
        entries += struct.pack("<BBBBHHII", size if size < 256 else 0, size if size < 256 else 0, 0, 0, 1, 32,
                               len(png), offset)
        offset += len(png)
    os.makedirs(os.path.dirname(a.out) or ".", exist_ok=True)
    with open(a.out, "wb") as f:
        f.write(header + entries + b"".join(png for _, png in images))
    print("%s: %s (%d bytes)" % (a.out, ", ".join("%dpx" % s for s, _ in images), os.path.getsize(a.out)))


if __name__ == "__main__":
    main()
