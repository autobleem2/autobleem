#!/usr/bin/env python3
"""Draw the box art for the sample games (tools/samples/samples.json) into tools/samples/covers/.

Every sample is homebrew no cover database or thumbnail server knows, so each gets a cover made here: the
game's own title screen (a screenshot from its repository, under the game's licence) cropped to the box's
shape, the title in the ab2 theme's Selawik on a band at the bottom, and the system in a corner. A game
without a screenshot gets the title on a two-tone background. The PNGs are checked in - this needs Pillow,
which the MSYS2 python and the Docker image do not have; run it on the PC when a sample changes:

    python tools/samples/make_covers.py [--shots DIR]

--shots is where the screenshots are (default: tools/samples/shots/, the file each entry's "shot" names).
"""
import argparse
import json
import os
import sys

try:
    from PIL import Image, ImageDraw, ImageFilter, ImageFont
except ImportError:
    sys.exit("Pillow is needed: pip install pillow")

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))
FONT = os.path.join(ROOT, "payload", "themes", "ab2", "selawik-light.ttf")

# the box shape per system, as the carousel draws it: PS1 square in the jewel case, NES/Mega Drive tall,
# SNES wide (see evoui/carousel_game.* - a big box takes the art at its own aspect)
SHAPES = {
    "psx": (512, 512),
    "nes": (360, 512),
    "snes": (512, 360),
    "md": (360, 512),
}
SYSTEM_LABEL = {"psx": "PlayStation", "nes": "NES", "snes": "Super NES", "md": "Mega Drive"}
# the ab2 palette (navy/cyan) for the band and the fallback background
NAVY = (18, 32, 58)
CYAN = (58, 196, 232)
INK = (238, 244, 250)


def cover_crop(shot, size):
    """The screenshot scaled to fill the box, centre-cropped."""
    w, h = size
    sw, sh = shot.size
    scale = max(w / sw, h / sh)
    img = shot.convert("RGB").resize((round(sw * scale), round(sh * scale)), Image.NEAREST)
    x, y = (img.width - w) // 2, (img.height - h) // 2
    return img.crop((x, y, x + w, y + h))


def fallback_background(size, seed):
    """Two tones of navy split on a diagonal, for a game without a screenshot."""
    w, h = size
    img = Image.new("RGB", size, NAVY)
    d = ImageDraw.Draw(img)
    light = tuple(min(255, c + 22) for c in NAVY)
    d.polygon([(0, 0), (w, 0), (w, h * (seed % 3 + 2) // 6)], fill=light)
    d.polygon([(0, h), (w, h), (0, h * (seed % 2 + 3) // 6)], fill=light)
    return img


def fit_font(text, max_w, start, path=FONT):
    size = start
    while size > 14:
        f = ImageFont.truetype(path, size)
        if f.getlength(text) <= max_w:
            return f
        size -= 2
    return ImageFont.truetype(path, size)


def draw_cover(entry, shot, out):
    size = SHAPES[entry["system"]]
    w, h = size
    img = cover_crop(shot, size) if shot else fallback_background(size, len(entry["title"]))
    # the title band: the bottom fifth, blurred image under a translucent navy
    band_h = h // 4
    band = img.crop((0, h - band_h, w, h)).filter(ImageFilter.GaussianBlur(6))
    overlay = Image.new("RGBA", (w, band_h), NAVY + (196,))
    band = Image.alpha_composite(band.convert("RGBA"), overlay).convert("RGB")
    img.paste(band, (0, h - band_h))
    d = ImageDraw.Draw(img)
    d.line([(0, h - band_h), (w, h - band_h)], fill=CYAN, width=2)
    # title, centred on the band; the author under it
    title_font = fit_font(entry["title"], w - 32, band_h * 2 // 5)
    tw = title_font.getlength(entry["title"])
    ty = h - band_h + band_h // 8
    d.text(((w - tw) / 2, ty), entry["title"], font=title_font, fill=INK)
    by = ty + title_font.size + band_h // 12
    by_font = fit_font(entry["author"], w - 32, max(14, band_h // 5))
    bw = by_font.getlength(entry["author"])
    d.text(((w - bw) / 2, by), entry["author"], font=by_font, fill=CYAN)
    # the system in the top-right corner
    tag = SYSTEM_LABEL[entry["system"]]
    tag_font = ImageFont.truetype(FONT, max(16, h // 22))
    pad = 6
    tw = tag_font.getlength(tag)
    d.rectangle([w - tw - pad * 3, pad, w - pad, pad * 2 + tag_font.size + 2], fill=NAVY, outline=CYAN)
    d.text((w - tw - pad * 2, pad + 1), tag, font=tag_font, fill=INK)
    # a thin frame
    d.rectangle([0, 0, w - 1, h - 1], outline=NAVY, width=3)
    img.save(out, optimize=True)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--shots", default=os.path.join(HERE, "shots"))
    ap.add_argument("--out", default=os.path.join(HERE, "covers"))
    args = ap.parse_args()
    with open(os.path.join(HERE, "samples.json"), encoding="utf-8") as f:
        samples = json.load(f)["games"]
    os.makedirs(args.out, exist_ok=True)
    for entry in samples:
        shot = None
        if entry.get("shot"):
            path = os.path.join(args.shots, entry["shot"])
            if os.path.exists(path):
                shot = Image.open(path)
            else:
                print("  no screenshot %s for %s - text cover" % (path, entry["title"]))
        out = os.path.join(args.out, entry["cover"])
        draw_cover(entry, shot, out)
        print("  %s (%dx%d)" % (out, *SHAPES[entry["system"]]))


if __name__ == "__main__":
    main()
