#!/usr/bin/env python3
"""Draws the launcher images the themes used to take from the PlayStation Classic's firmware
(/usr/sony/share/data - see docs/theme-format.md), so that every file under payload/Themes is our own.

  python tools/make_theme_images.py [theme ...]      (default: every theme that has a plan below)

Per theme it writes only the files listed in PLANS - the art a theme drew itself (every play_text.png,
evolution's footer and settings panel, autobleem's footer and play button, ab2's icons, stylus and
background) is left alone. Everything is drawn at 4x and scaled down, with the standard library, Pillow and the glyphs
of tools/make_ab2_icons.py (the gear, gamepad and memory card outlines).

The files and where the launcher shows them:
  meta_panel.png      30x30    the controller glyph next to "n Players" on the meta panel
  hint_cross/circle/triangle.png  30x30   the footer's button hints
  arrow.png           24x24    the arrow that points at the selected cover
  menu_settings/guide/memcard/resume.png  118x118  the game menu's icons; the resume icon's window is
                               where PsMenu pastes the save state's picture (theme.json's
                               launcher.menuIcons.resumePicture, (25, 33) 68x52 when unset)
  memcard_grid.png    256x420  the memory card manager's dot matrix (4 x 6 dots, the slots between them)
  memcard_pencil.png  42x42    the manager's cursor, its tip at the top-left corner
  launcher_footer.png 1280x88, settings_panel.png 1282x229, play_button.png 200x68
                               transparent in the themes that have no art for them (the originals were
                               empty too) - written so no byte of the firmware's file is left
"""
import os
import sys

from PIL import Image, ImageChops, ImageDraw, ImageFilter

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from make_ab2_icons import gamepad, gear, memcard  # noqa: E402

REPO = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..')
THEMES = os.path.join(REPO, 'payload', 'Themes')

K = 4  # supersampling
WHITE = (245, 245, 245)
LIGHT = (218, 218, 222)
DARK = (74, 74, 78)
DISC = (58, 58, 62)
DISC_RIM = (96, 96, 100)
BLUE = (112, 128, 255)   # the hints' glyph colours - the usual hues, drawn fresh
RED = (240, 80, 120)
GREEN = (64, 200, 128)

# the resume icon's picture window (theme.json launcher.menuIcons.resumePicture when unset)
RESUME_X, RESUME_Y, RESUME_W, RESUME_H = 25, 33, 68, 52


class Style:
    """what differs between the themes: the glyph colour, and ab2's glow behind it"""

    def __init__(self, glyph=WHITE, glow=None):
        self.glyph = glyph
        self.glow = glow  # ab2: a soft glow behind the light glyphs


def canvas(w, h):
    return Image.new('RGBA', (w * K, h * K), (0, 0, 0, 0))


def down(im, w, h):
    return im.resize((w, h), Image.LANCZOS)


def with_glow(im, w, h, colour, radius=3, alpha=170):
    """a blurred copy of the image's alpha in `colour` under the image"""
    mask = im.getchannel('A').filter(ImageFilter.GaussianBlur(radius * K))
    glow = Image.new('RGBA', im.size, colour + (0,))
    glow.putalpha(mask.point(lambda v: min(255, v * alpha // 255)))
    glow.alpha_composite(im)
    return glow


# ---------------------------------------------------------------- the small ones
def meta_panel(style):
    # a controller silhouette: the body, two grips below it, filled
    im = canvas(30, 30)
    d = ImageDraw.Draw(im)
    c = style.glyph + (255,)
    d.rounded_rectangle((4 * K, 9 * K, 26 * K, 19 * K), radius=5 * K, fill=c)
    d.rounded_rectangle((4 * K, 12 * K, 11 * K, 22 * K), radius=3 * K, fill=c)
    d.rounded_rectangle((19 * K, 12 * K, 26 * K, 22 * K), radius=3 * K, fill=c)
    # a notch between the grips and the cutouts for the sticks read the shape
    d.ellipse((13 * K, 16 * K, 17 * K, 20 * K), fill=(0, 0, 0, 0))
    out = down(im, 30, 30)
    return with_glow(im, 30, 30, style.glow).resize((30, 30), Image.LANCZOS) if style.glow else out


def hint(kind, style):
    # a dark disc with a lighter rim and the button's shape in its colour
    im = canvas(30, 30)
    d = ImageDraw.Draw(im)
    d.ellipse((1 * K, 1 * K, 29 * K, 29 * K), fill=DISC + (255,), outline=DISC_RIM + (255,), width=K)
    w = int(2.4 * K)
    if kind == 'cross':
        c = BLUE + (255,)
        d.line((9 * K, 9 * K, 21 * K, 21 * K), fill=c, width=w)
        d.line((21 * K, 9 * K, 9 * K, 21 * K), fill=c, width=w)
    elif kind == 'circle':
        c = RED + (255,)
        d.ellipse((8 * K, 8 * K, 22 * K, 22 * K), outline=c, width=w)
    else:
        c = GREEN + (255,)
        d.polygon([(15 * K, 8 * K), (22.5 * K, 21.5 * K), (7.5 * K, 21.5 * K)], outline=c, width=w)
    return down(im, 30, 30)


def arrow(style):
    # a triangle pointing down at the cover, fading towards its base
    im = canvas(24, 24)
    d = ImageDraw.Draw(im)
    d.polygon([(2 * K, 3 * K), (22 * K, 3 * K), (12 * K, 22 * K)], fill=style.glyph + (255,))
    # the fade: 60% at the base, 100% at the tip
    grad = Image.linear_gradient('L').resize(im.size).point(lambda v: 150 + v * 105 // 255)
    im.putalpha(ImageChops.multiply(im.getchannel('A'), grad))
    return down(im, 24, 24)


# ---------------------------------------------------------------- the menu icons
def icon_tile(style, box=(26, 30, 92, 88)):
    im = canvas(118, 118)
    d = ImageDraw.Draw(im)
    x0, y0, x1, y1 = [v * K for v in box]
    d.rounded_rectangle((x0, y0, x1, y1), radius=7 * K, fill=LIGHT + (255,), outline=(150, 150, 155, 255), width=K)
    # a slightly darker lower half
    d.rounded_rectangle((x0 + K, (y0 + y1) // 2, x1 - K, y1 - K), radius=6 * K, fill=(200, 200, 205, 255))
    d.rectangle((x0 + K, (y0 + y1) // 2, x1 - K, (y0 + y1) // 2 + 5 * K), fill=(200, 200, 205, 255))
    return im


def icon_with(glyph_fn, style):
    # the glyph, drawn for the whole 118 slot by make_ab2_icons, shrunk about the tile's centre
    im = icon_tile(style)
    layer = canvas(118, 118)
    glyph_fn(ImageDraw.Draw(layer), DARK + (255,), K)
    scale = 0.72
    w = int(118 * K * scale)
    small = layer.resize((w, w), Image.LANCZOS)
    cx, cy = (26 + 92) * K // 2, (30 + 88) * K // 2
    im.alpha_composite(small, (cx - w // 2, cy - w // 2))
    return down(im, 118, 118)


def icon_resume(style):
    # the frame around the picture window, the window dark for when there is no picture
    im = icon_tile(style, box=(RESUME_X - 7, RESUME_Y - 7, RESUME_X + RESUME_W + 6, RESUME_Y + RESUME_H + 6))
    d = ImageDraw.Draw(im)
    d.rectangle((RESUME_X * K, RESUME_Y * K, (RESUME_X + RESUME_W) * K - 1, (RESUME_Y + RESUME_H) * K - 1),
                fill=(40, 42, 50, 255))
    # a small "screen" gloss at the top of the window
    d.rectangle((RESUME_X * K, RESUME_Y * K, (RESUME_X + RESUME_W) * K - 1, (RESUME_Y + 3) * K),
                fill=(70, 72, 82, 255))
    return down(im, 118, 118)


# ---------------------------------------------------------------- the memory card manager
def memcard_grid():
    # 4 x 6 dots at the original's positions, each a dark bead with a highlight
    im = canvas(256, 420)
    d = ImageDraw.Draw(im)
    for cx in (6.5, 87, 167.5, 247.5):
        for cy in (7.5, 87.5, 167.5, 247.5, 327.5, 407.5):
            r = 5.8 * K
            d.ellipse((cx * K - r, cy * K - r, cx * K + r, cy * K + r), fill=(64, 64, 68, 255))
            d.ellipse((cx * K - r * 0.55, cy * K - r * 0.75, cx * K + r * 0.05, cy * K - r * 0.15),
                      fill=(120, 120, 126, 255))
    return down(im, 256, 420)


def memcard_pencil():
    # a pencil, its point at the top-left corner (GuiMcManager::renderPencil puts (0, 0) on the slot's corner)
    im = canvas(42, 42)
    d = ImageDraw.Draw(im)
    body = (250, 196, 60, 255)
    wood = (232, 190, 140, 255)
    lead = (60, 50, 45, 255)
    eraser = (240, 120, 130, 255)
    band = (170, 175, 185, 255)
    hw = 5 * K
    # the body along the diagonal, then the wood cone and the lead at the tip
    d.line((13 * K, 13 * K, 33 * K, 33 * K), fill=body, width=2 * hw)
    d.polygon([(3 * K, 3 * K), (13 * K + hw * 0.7, 13 * K - hw * 0.7), (13 * K - hw * 0.7, 13 * K + hw * 0.7)],
              fill=wood)
    d.polygon([(2 * K, 2 * K), (6.5 * K + hw * 0.3, 6.5 * K - hw * 0.3), (6.5 * K - hw * 0.3, 6.5 * K + hw * 0.3)],
              fill=lead)
    # the metal band and the eraser at the end
    d.line((31 * K + hw * 0.7, 31 * K - hw * 0.7, 31 * K - hw * 0.7, 31 * K + hw * 0.7), fill=band, width=3 * K)
    d.line((34 * K, 34 * K, 37 * K, 37 * K), fill=eraser, width=2 * hw)
    d.ellipse((37 * K - hw, 37 * K - hw, 37 * K + hw, 37 * K + hw), fill=eraser)
    # a darker edge line along the body
    d.line((13 * K + hw * 0.5, 13 * K - hw * 0.5, 33 * K + hw * 0.5, 33 * K - hw * 0.5), fill=(200, 150, 40, 255), width=K)
    return down(im, 42, 42)


def transparent(w, h):
    return Image.new('RGBA', (w, h), (0, 0, 0, 0))


# ---------------------------------------------------------------- the plans
STYLES = {
    'default': Style(),
    'aergb': Style(),
    'evolution': Style(),
    'autobleem': Style(),
    'ab2': Style(glyph=(235, 245, 255), glow=(70, 225, 255)),
}

COMMON = ['meta_panel', 'hint_cross', 'hint_circle', 'hint_triangle', 'arrow', 'memcard_grid']
MENU = ['menu_settings', 'menu_guide', 'menu_memcard', 'menu_resume']
EMPTY = ['launcher_footer', 'settings_panel', 'play_button']
PLANS = {
    'default': COMMON + MENU + EMPTY + ['memcard_pencil'],
    'aergb': COMMON + MENU + EMPTY + ['memcard_pencil'],
    'evolution': COMMON + MENU + ['memcard_pencil', 'play_button'],
    'autobleem': COMMON + ['memcard_pencil', 'menu_resume', 'settings_panel'],
    'ab2': COMMON + EMPTY,
}


def make(name, style):
    if name == 'meta_panel':
        return meta_panel(style)
    if name.startswith('hint_'):
        return hint(name[5:], style)
    if name == 'arrow':
        return arrow(style)
    if name == 'menu_settings':
        return icon_with(gear, style)
    if name == 'menu_guide':
        return icon_with(gamepad, style)
    if name == 'menu_memcard':
        return icon_with(memcard, style)
    if name == 'menu_resume':
        return icon_resume(style)
    if name == 'memcard_grid':
        return memcard_grid()
    if name == 'memcard_pencil':
        return memcard_pencil()
    if name == 'launcher_footer':
        return transparent(1280, 88)
    if name == 'settings_panel':
        return transparent(1282, 229)
    if name == 'play_button':
        return transparent(200, 68)
    raise KeyError(name)


def main(argv):
    themes = argv[1:] or list(PLANS)
    for theme in themes:
        out = os.path.join(THEMES, theme, 'images')
        os.makedirs(out, exist_ok=True)
        for name in PLANS[theme]:
            path = os.path.join(out, name + '.png')
            make(name, STYLES[theme]).save(path, optimize=True)
            print(path)
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv))
