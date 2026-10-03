#!/usr/bin/env python3
"""Draws the launcher's own icons in src/resources/evoimg - line glyphs on nothing, white, drawn at 4x and
scaled down, the style of tools/make_theme_images.py:

  python tools/make_evoimg_icons.py

  tab_playstation.png  64x64  a disc (the PlayStation tab of the set picker - no Sony mark)
  tab_retroarch.png    64x64  an arcade stick
  tab_apps.png         64x64  a grid of four tiles
  dpad_up/down/left/right.png  28x28  a filled chevron, for the footer's d-pad hint chips (GuiLauncher's
                                hint lines - "|@Up|", "|@Down|", "|@Left|", "|@Right|" - drawn through
                                PanelStyle::faceIcon at their native size, same as the X/O/T hint images)
"""
import os
import sys

from PIL import Image, ImageDraw

OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'src', 'resources', 'evoimg')
SIZE = 64
K = 4
DPAD_SIZE = 28
WHITE = (245, 245, 245, 255)
CLEAR = (0, 0, 0, 0)


def canvas(size=SIZE):
    return Image.new('RGBA', (size * K, size * K), CLEAR)


def down(im, size=SIZE):
    return im.resize((size, size), Image.LANCZOS)


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


def dpad_arrow(direction):
    """A filled chevron pointing `direction` ('up', 'down', 'left' or 'right'), for a d-pad hint chip -
    always drawn 'up' then rotated, so the four stay identical apart from orientation."""
    im = canvas(DPAD_SIZE)
    d = ImageDraw.Draw(im)
    s = DPAD_SIZE * K
    # a triangle over a short tail, like an arrow - centred, pointing up
    d.polygon([(s * 0.5, s * 0.12), (s * 0.18, s * 0.52), (s * 0.36, s * 0.52), (s * 0.36, s * 0.88),
               (s * 0.64, s * 0.88), (s * 0.64, s * 0.52), (s * 0.82, s * 0.52)], fill=WHITE)
    im = down(im, DPAD_SIZE)
    rotation = {'up': 0, 'right': -90, 'down': 180, 'left': 90}[direction]
    return im.rotate(rotation, resample=Image.BICUBIC)


def main():
    for name, make in (('tab_playstation.png', disc), ('tab_retroarch.png', arcade_stick), ('tab_apps.png', app_grid)):
        path = os.path.join(OUT, name)
        make().save(path, optimize=True)
        print(path)
    for direction in ('up', 'down', 'left', 'right'):
        path = os.path.join(OUT, 'dpad_%s.png' % direction)
        dpad_arrow(direction).save(path, optimize=True)
        print(path)
    return 0


if __name__ == '__main__':
    sys.exit(main())
