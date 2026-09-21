#!/usr/bin/env python3
"""Draws the launcher's own icons in src/resources/evoimg - line glyphs on nothing, white, drawn at 4x and
scaled down, the style of tools/make_theme_images.py:

  python tools/make_evoimg_icons.py

  tab_playstation.png  64x64  a disc (the PlayStation tab of the set picker - no Sony mark)
  tab_retroarch.png    64x64  an arcade stick
  tab_apps.png         64x64  a grid of four tiles
"""
import os
import sys

from PIL import Image, ImageDraw

OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'src', 'resources', 'evoimg')
SIZE = 64
K = 4
WHITE = (245, 245, 245, 255)
CLEAR = (0, 0, 0, 0)


def canvas():
    return Image.new('RGBA', (SIZE * K, SIZE * K), CLEAR)


def down(im):
    return im.resize((SIZE, SIZE), Image.LANCZOS)


def disc():
    im = canvas()
    d = ImageDraw.Draw(im)
    c = SIZE * K // 2
    d.ellipse((6 * K, 6 * K, 58 * K, 58 * K), outline=WHITE, width=4 * K)
    d.ellipse((24 * K, 24 * K, 40 * K, 40 * K), outline=WHITE, width=4 * K)
    # a data ring
    d.arc((14 * K, 14 * K, 50 * K, 50 * K), start=200, end=330, fill=WHITE, width=2 * K)
    return down(im)


def arcade_stick():
    im = canvas()
    d = ImageDraw.Draw(im)
    # the base, the stick, the ball, two buttons
    d.rounded_rectangle((8 * K, 40 * K, 56 * K, 56 * K), radius=5 * K, outline=WHITE, width=4 * K)
    d.line((22 * K, 40 * K, 22 * K, 20 * K), fill=WHITE, width=4 * K)
    d.ellipse((13 * K, 6 * K, 31 * K, 24 * K), outline=WHITE, width=4 * K)
    d.ellipse((36 * K, 26 * K, 44 * K, 34 * K), fill=WHITE)
    d.ellipse((46 * K, 22 * K, 54 * K, 30 * K), fill=WHITE)
    return down(im)


def app_grid():
    im = canvas()
    d = ImageDraw.Draw(im)
    for x, y in ((8, 8), (34, 8), (8, 34), (34, 34)):
        d.rounded_rectangle((x * K, y * K, (x + 22) * K, (y + 22) * K), radius=4 * K, outline=WHITE, width=4 * K)
    return down(im)


def main():
    for name, make in (('tab_playstation.png', disc), ('tab_retroarch.png', arcade_stick), ('tab_apps.png', app_grid)):
        path = os.path.join(OUT, name)
        make().save(path, optimize=True)
        print(path)
    return 0


if __name__ == '__main__':
    sys.exit(main())
