#!/usr/bin/env python3
"""Draws the ab2 theme's launcher menu icons and its on/off switch in the theme's own style - a dark
circuit-board tile with a cyan neon outline and glow, a line glyph inside - instead of the grey icons it
inherited from the original theme.

  python tools/make_ab2_icons.py [out dir]      (default: payload/themes/ab2/images)

Files: menu_settings.png (a gear), menu_guide.png (a gamepad - the "Game" item, game parameters),
menu_memcard.png (a PS1 memory card), memcard_pencil.png (the memory card editor's cursor, a stylus with
its tip at the top-left corner like the pencil it replaces), menu_resume.png (a frame: PsMenu::render pastes the save
state's picture at (25, 33) 68x52 inside it, so the frame hugs that window and nothing else is drawn), all 118x118; and, one level
up in the theme folder, on.png / off.png (60x30): the default theme's switch with its green turned to
the theme's blue.
"""
import os
import sys
from PIL import Image, ImageDraw, ImageFilter

SIZE = 118
# the tile is smaller than the 118x118 slot the launcher gives it - a full-size one covered the option's
# label when the selected icon zooms, and crowded the row
TILE_LEFT, TILE_TOP, TILE_RIGHT, TILE_BOTTOM = 17, 6, 100, 90
# the tile sits high in its slot: in the launcher's Games state the row stands at y=520 with the footer
# bar at 620, and a tile reaching the slot's bottom went under the bar
GLYPH_SCALE = 0.8  # the glyphs, drawn for the full slot, shrink to the tile and move up to its middle
GLYPH_SHIFT_Y = (TILE_TOP + TILE_BOTTOM) // 2 - SIZE // 2
NAVY = (12, 22, 64)
NAVY_EDGE = (30, 52, 120)
CYAN = (70, 225, 255)
CYAN_DIM = (40, 140, 200)
WHITE = (235, 245, 255)
MAGENTA = (200, 110, 255)


def glow_layer(draw_fn, color, radius, alpha):
    layer = Image.new("RGBA", (SIZE * 2, SIZE * 2), (0, 0, 0, 0))
    d = ImageDraw.Draw(layer)
    draw_fn(d, color + (alpha,), 2)
    layer = layer.filter(ImageFilter.GaussianBlur(radius * 2))
    return layer.resize((SIZE, SIZE), Image.LANCZOS)


def crisp_layer(draw_fn, color):
    layer = Image.new("RGBA", (SIZE * 4, SIZE * 4), (0, 0, 0, 0))
    d = ImageDraw.Draw(layer)
    draw_fn(d, color + (255,), 4)
    return layer.resize((SIZE, SIZE), Image.LANCZOS)


def tile():
    """the rounded dark tile every icon sits on, with a thin cyan edge and a soft outer glow"""
    def edge(d, col, k):
        d.rounded_rectangle((TILE_LEFT * k, TILE_TOP * k, TILE_RIGHT * k, TILE_BOTTOM * k), radius=14 * k, outline=col,
                            width=2 * k)

    im = Image.new("RGBA", (SIZE, SIZE), (0, 0, 0, 0))
    im.alpha_composite(glow_layer(edge, CYAN, 4, 110))
    body = Image.new("RGBA", (SIZE * 4, SIZE * 4), (0, 0, 0, 0))
    d = ImageDraw.Draw(body)
    d.rounded_rectangle((TILE_LEFT * 4, TILE_TOP * 4, TILE_RIGHT * 4, TILE_BOTTOM * 4), radius=14 * 4, fill=NAVY + (150,))
    # faint circuit traces across the tile, like the background's
    for y in (24, 34, 76):
        d.line((22 * 4, y * 4, 40 * 4, y * 4), fill=NAVY_EDGE + (255,), width=4)
        d.ellipse((40 * 4 - 6, y * 4 - 6, 40 * 4 + 6, y * 4 + 6), fill=NAVY_EDGE + (255,))
    for x in (88, 94):
        d.line((x * 4, 50 * 4, x * 4, 82 * 4), fill=NAVY_EDGE + (255,), width=4)
    im.alpha_composite(body.resize((SIZE, SIZE), Image.LANCZOS))
    im.alpha_composite(crisp_layer(edge, CYAN))
    return im


def shrink_about_centre(layer, scale):
    # the glyph, scaled and centred on the tile (the screen frame passes 1: it must stay on its window)
    if scale == 1:
        return layer
    w = int(round(SIZE * scale))
    small = layer.resize((w, w), Image.LANCZOS)
    out = Image.new("RGBA", (SIZE, SIZE), (0, 0, 0, 0))
    out.alpha_composite(small, ((SIZE - w) // 2, (SIZE - w) // 2 + GLYPH_SHIFT_Y))
    return out


def with_glyph(draw_fn, color=WHITE, glow=CYAN, scale=GLYPH_SCALE):
    im = tile()
    im.alpha_composite(shrink_about_centre(glow_layer(draw_fn, glow, 3, 230), scale))
    im.alpha_composite(shrink_about_centre(glow_layer(draw_fn, glow, 1, 160), scale))
    im.alpha_composite(shrink_about_centre(crisp_layer(draw_fn, color), scale))
    return im


# ---------------------------------------------------------------- the glyphs, drawn at k times 118
def gear(d, col, k):
    import math
    cx, cy = 59 * k, 59 * k
    d.ellipse((cx - 24 * k, cy - 24 * k, cx + 24 * k, cy + 24 * k), outline=col, width=4 * k)
    d.ellipse((cx - 10 * k, cy - 10 * k, cx + 10 * k, cy + 10 * k), outline=col, width=3 * k)
    # eight square teeth on the rim
    for i in range(8):
        a = math.pi * 2 * i / 8
        ca, sa = math.cos(a), math.sin(a)
        r0, r1, hw = 22 * k, 33 * k, 5 * k
        pts = [(cx + r0 * ca - hw * sa, cy + r0 * sa + hw * ca), (cx + r1 * ca - hw * sa, cy + r1 * sa + hw * ca),
               (cx + r1 * ca + hw * sa, cy + r1 * sa - hw * ca), (cx + r0 * ca + hw * sa, cy + r0 * sa - hw * ca)]
        d.polygon(pts, fill=col)


def gamepad(d, col, k):
    # a controller: two rounded grips joined by the body, d-pad left, four buttons right
    d.rounded_rectangle((22 * k, 44 * k, 96 * k, 78 * k), radius=17 * k, outline=col, width=3 * k)
    d.rounded_rectangle((22 * k, 50 * k, 44 * k, 88 * k), radius=10 * k, outline=col, width=3 * k)
    d.rounded_rectangle((74 * k, 50 * k, 96 * k, 88 * k), radius=10 * k, outline=col, width=3 * k)
    # cover the joins
    d.rectangle((26 * k, 48 * k, 40 * k, 76 * k), fill=(0, 0, 0, 0))
    d.rectangle((78 * k, 48 * k, 92 * k, 76 * k), fill=(0, 0, 0, 0))
    d.rounded_rectangle((22 * k, 44 * k, 96 * k, 78 * k), radius=17 * k, outline=col, width=3 * k)
    d.rounded_rectangle((22 * k, 50 * k, 44 * k, 88 * k), radius=10 * k, outline=col, width=3 * k)
    d.rounded_rectangle((74 * k, 50 * k, 96 * k, 88 * k), radius=10 * k, outline=col, width=3 * k)
    # d-pad
    d.line((33 * k, 55 * k, 33 * k, 69 * k), fill=col, width=3 * k)
    d.line((26 * k, 62 * k, 40 * k, 62 * k), fill=col, width=3 * k)
    # buttons
    for (x, y) in ((85, 55), (79, 62), (91, 62), (85, 69)):
        d.ellipse((x * k - 2 * k, y * k - 2 * k, x * k + 2 * k, y * k + 2 * k), fill=col)


def memcard(d, col, k):
    # a PS1 memory card from the front: the body with its raised top, the label, the two grip ridges
    d.rounded_rectangle((32 * k, 30 * k, 86 * k, 94 * k), radius=7 * k, outline=col, width=4 * k)
    d.rounded_rectangle((44 * k, 22 * k, 74 * k, 34 * k), radius=3 * k, outline=col, width=4 * k)
    d.rectangle((46 * k, 31 * k, 72 * k, 34 * k), fill=(0, 0, 0, 0))
    d.line((44 * k, 24 * k, 44 * k, 31 * k), fill=col, width=4 * k)
    d.line((74 * k, 24 * k, 74 * k, 31 * k), fill=col, width=4 * k)
    d.rounded_rectangle((40 * k, 52 * k, 78 * k, 86 * k), radius=4 * k, outline=col, width=3 * k)
    for x in (52, 59, 66):
        d.line((x * k, 40 * k, x * k, 46 * k), fill=col, width=3 * k)


def screen(d, col, k):
    # just a frame around the save state's picture, which the launcher draws at (25, 33) 68x52: the
    # outline hugs that window with a 2 px border, nothing else, so the picture fills it
    d.rounded_rectangle((22 * k, 30 * k, 95 * k, 87 * k), radius=4 * k, outline=col, width=3 * k)


def screen_window_clear(im):
    # the picture goes here; the tile shows through, darker, when there is none
    d = ImageDraw.Draw(im)
    d.rectangle((25, 33, 25 + 68 - 1, 33 + 52 - 1), fill=(6, 12, 36, 235))
    return im


# ---------------------------------------------------------------- the memory card editor's cursor
def stylus(d, col, k):
    # GuiMcManager::renderPencil draws this at the slot's top-left corner: the tip is at (0, 0) and the
    # body runs down-right, like the pixel pencil it replaces
    import math
    ax, ay = 2 * k, 2 * k        # the tip
    bx, by = 34 * k, 34 * k      # the end of the body
    hw = 5 * k                   # half the body's width
    # the body: a rounded stroke along the diagonal, with the tip as a triangle
    d.line((10 * k, 10 * k, bx, by), fill=col, width=2 * hw)
    d.ellipse((bx - hw, by - hw, bx + hw, by + hw), fill=col)
    d.polygon([(ax, ay), (10 * k + hw * 0.7, 10 * k - hw * 0.7), (10 * k - hw * 0.7, 10 * k + hw * 0.7)], fill=col)
    # a dark line across the body where the tip starts, and a cap at the end
    d.line((14 * k + hw * 0.7, 14 * k - hw * 0.7, 14 * k - hw * 0.7, 14 * k + hw * 0.7), fill=(8, 16, 48, 255), width=k)
    d.line((30 * k + hw * 0.7, 30 * k - hw * 0.7, 30 * k - hw * 0.7, 30 * k + hw * 0.7), fill=(8, 16, 48, 255), width=k)


def cursor():
    size = 42
    def layer(color, k, blur=0, alpha=255):
        im = Image.new("RGBA", (size * k, size * k), (0, 0, 0, 0))
        stylus(ImageDraw.Draw(im), color + (alpha,), k)
        if blur:
            im = im.filter(ImageFilter.GaussianBlur(blur * k))
        return im.resize((size, size), Image.LANCZOS)
    im = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    im.alpha_composite(layer(CYAN, 2, blur=3, alpha=220))
    im.alpha_composite(layer(CYAN, 2, blur=1, alpha=180))
    im.alpha_composite(layer(WHITE, 4))
    return im


# ---------------------------------------------------------------- the switch
def recolour_switch(src, dst, hue):
    """turns the default theme's green switch to the theme's blue: pixels in the green range get their hue
    moved, everything else (the grey knob, the shadows) is left alone"""
    im = Image.open(src).convert("RGBA")
    hsv = im.convert("RGB").convert("HSV")
    px, hp, ap = im.load(), hsv.load(), im.split()[3].load()
    out = Image.new("RGBA", im.size)
    op = out.load()
    for y in range(im.height):
        for x in range(im.width):
            h, s, v = hp[x, y]
            if 50 <= h <= 130 and s > 60:  # green: to the theme's blue, keeping the shading
                h = hue
                r, g, b = Image.new("HSV", (1, 1), (h, s, v)).convert("RGB").getpixel((0, 0))
                op[x, y] = (r, g, b, ap[x, y])
            else:
                op[x, y] = px[x, y]
    out.save(dst)


def main():
    out = sys.argv[1] if len(sys.argv) > 1 else os.path.join("payload", "themes", "ab2", "images")
    os.makedirs(out, exist_ok=True)
    with_glyph(gear).save(os.path.join(out, "menu_settings.png"))
    with_glyph(gamepad).save(os.path.join(out, "menu_guide.png"))
    with_glyph(memcard).save(os.path.join(out, "menu_memcard.png"))
    screen_window_clear(with_glyph(screen, scale=1)).save(os.path.join(out, "menu_resume.png"))
    cursor().save(os.path.join(out, "memcard_pencil.png"))
    theme_dir = os.path.dirname(out.rstrip("/\\")) if os.path.basename(out.rstrip("/\\")) == "images" else out
    default_dir = os.path.join("payload", "themes", "default")
    recolour_switch(os.path.join(default_dir, "on.png"), os.path.join(theme_dir, "on.png"), 150)
    Image.open(os.path.join(default_dir, "off.png")).save(os.path.join(theme_dir, "off.png"))
    print("wrote icons to", out, "and on.png/off.png to", theme_dir)


if __name__ == "__main__":
    main()
