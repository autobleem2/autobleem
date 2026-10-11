#!/usr/bin/env python3
"""Draws the resume slots' emulator badges in src/resources/evoimg: emu_ab.png (the AutoBleem 2 square logo -
the slot resumes in pcsx) and emu_ra.png (the RetroArch logo - it resumes in RetroArch), each with its @2x file.
Both are the same dark square tile, so they read as a pair in the corner of a slot's picture.

  python tools/make_emu_badges.py

Sources (tools/art): ab2-square.png - our square AB2 logo; retroarch-monochrome.svg - the RetroArch logo from
https://github.com/libretro/retroarch-assets (src/xmb/monochrome/retroarch.svg, CC-BY-4.0). The SVG is one path
of M/L/C/z commands; it is filled even-odd here (no SVG library needed): the white invader of the logo, its eyes
cut out. Needs Pillow.
"""
import os
import re

from PIL import Image, ImageChops, ImageDraw

HERE = os.path.dirname(os.path.abspath(__file__))
ART = os.path.join(HERE, 'art')
OUT = os.path.join(HERE, '..', 'src', 'resources', 'evoimg')

SIZE = 32          # the 1x badge, logical pixels; @2x is twice that
SUPER = 16         # drawn this many times larger, then scaled down
TILE = (23, 27, 34)  # the AB2 square's own ground
EDGE = (70, 80, 96)
TILE_ALPHA = 235


def bezier(p0, p1, p2, p3, steps=24):
    pts = []
    for i in range(1, steps + 1):
        t = i / steps
        u = 1 - t
        pts.append((u * u * u * p0[0] + 3 * u * u * t * p1[0] + 3 * u * t * t * p2[0] + t * t * t * p3[0],
                    u * u * u * p0[1] + 3 * u * u * t * p1[1] + 3 * u * t * t * p2[1] + t * t * t * p3[1]))
    return pts


def svg_subpaths(path):
    """the path's closed outlines as point lists - absolute M, L, C and z only (what the logo uses)"""
    tokens = re.findall(r'[MLCZmlcz]|-?[0-9.]+(?:e-?[0-9]+)?', path)
    subpaths, cur, pos, cmd, i = [], [], (0.0, 0.0), None, 0
    while i < len(tokens):
        tok = tokens[i]
        if tok in 'MLCZmlcz':
            cmd = tok
            i += 1
            if cmd in 'Zz':
                if cur:
                    subpaths.append(cur)
                cur = []
            continue
        if cmd in 'mlc':
            raise ValueError('relative commands are not supported')
        if cmd == 'M':
            if cur:
                subpaths.append(cur)
            pos = (float(tokens[i]), float(tokens[i + 1]))
            cur = [pos]
            i += 2
            cmd = 'L'  # further pairs after M are line-tos
        elif cmd == 'L':
            pos = (float(tokens[i]), float(tokens[i + 1]))
            cur.append(pos)
            i += 2
        elif cmd == 'C':
            c = [float(t) for t in tokens[i:i + 6]]
            cur += bezier(pos, (c[0], c[1]), (c[2], c[3]), (c[4], c[5]))
            pos = (c[4], c[5])
            i += 6
        else:
            raise ValueError('unsupported command ' + cmd)
    if cur:
        subpaths.append(cur)
    return subpaths


def ra_invader(px):
    """the RetroArch invader, white on nothing, px x px: the logo's rounded square minus the even-odd fill"""
    with open(os.path.join(ART, 'retroarch-monochrome.svg'), encoding='utf-8') as f:
        svg = f.read()
    d = re.search(r'\sd="([^"]+)"', svg).group(1)
    subs = svg_subpaths(d)
    big = 64 * SUPER
    masks = []
    for sp in subs:
        m = Image.new('L', (big, big), 0)
        ImageDraw.Draw(m).polygon([(x * SUPER, y * SUPER) for x, y in sp], fill=255)
        masks.append(m)
    evenodd = masks[0]
    for m in masks[1:]:
        evenodd = ImageChops.logical_xor(evenodd.convert('1'), m.convert('1')).convert('L')
    invader = ImageChops.subtract(masks[0], evenodd)  # inside the rounded square, not filled: the invader
    box = invader.getbbox()
    return invader.crop(box)


def tile(px):
    img = Image.new('RGBA', (px * SUPER, px * SUPER), TILE + (TILE_ALPHA,))
    d = ImageDraw.Draw(img)
    w = max(1, px * SUPER // 32)
    d.rectangle([0, 0, px * SUPER - 1, px * SUPER - 1], outline=EDGE + (255,), width=w)
    return img


def badge_ra(px):
    img = tile(px)
    inv = ra_invader(px)
    side = px * SUPER
    width = int(side * 0.78)
    height = int(inv.height * width / inv.width)
    inv = inv.resize((width, height), Image.LANCZOS)
    white = Image.new('RGBA', inv.size, (255, 255, 255, 255))
    white.putalpha(inv)
    img.alpha_composite(white, ((side - width) // 2, (side - height) // 2))
    return img.resize((px, px), Image.LANCZOS)


def badge_ab(px):
    src = Image.open(os.path.join(ART, 'ab2-square.png')).convert('RGB')
    # the logo's extent: whatever differs from the square's ground
    diff = ImageChops.difference(src, Image.new('RGB', src.size, src.getpixel((5, 5)))).convert('L')
    diff = diff.point(lambda v: 255 if v > 24 else 0)
    x0, y0, x1, y1 = diff.getbbox()
    cx, cy = (x0 + x1) / 2, (y0 + y1) / 2
    half = max(x1 - x0, y1 - y0) / 2 / 0.84  # the logo fills 84% of the tile
    logo = src.crop((int(cx - half), int(cy - half), int(cx + half), int(cy + half)))
    side = px * SUPER
    img = tile(px)
    inner = side - 2 * max(1, px * SUPER // 32)
    logo = logo.resize((inner, inner), Image.LANCZOS).convert('RGBA')
    logo.putalpha(TILE_ALPHA)
    img.alpha_composite(logo, ((side - inner) // 2, (side - inner) // 2))
    return img.resize((px, px), Image.LANCZOS)


def main():
    for name, make in (('emu_ab', badge_ab), ('emu_ra', badge_ra)):
        for scale, suffix in ((1, ''), (2, '@2x')):
            path = os.path.join(OUT, name + suffix + '.png')
            make(SIZE * scale).save(path, optimize=True)
            print('wrote', os.path.relpath(path), SIZE * scale)


if __name__ == '__main__':
    main()
