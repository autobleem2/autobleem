#!/usr/bin/env python3
"""Stage the download repository's page assets from the ab2 theme.

    repo_assets.py <outdir>

Writes into <outdir> (what tools/repo_publish.sh assets uploads to <repo>/assets/):

    hero.jpg            the theme's background - the AutoBleem 2 logo is painted into it
    icon.png            the emblem cut out of it, 128x128 - the page's favicon and Raspberry Pi Imager's icon
                        (copied to <repo>/rpi-imager/icon.png by the publish script)
    selawik-light.ttf   the theme's font (SIL OFL) and its OFL.txt

Nothing is duplicated in git: the sources stay in the autobleem-themes submodule's Themes/ab2. icon.png is
cut out with Pillow, or copied from tools/repo_icon.png (the same cut, checked in for a python without
Pillow - MSYS2's).
"""

import os
import shutil
import sys

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
THEME = os.path.join(REPO, "autobleem-themes", "Themes", "ab2")
# where the emblem sits in abback2.jpg (1280x720): the "A" with the pad, above the wordmark
EMBLEM_BOX = (430, 90, 850, 440)


def main():
    if len(sys.argv) != 2:
        sys.exit(__doc__)
    out = sys.argv[1]
    os.makedirs(out, exist_ok=True)
    for src, dst in (("abback2.jpg", "hero.jpg"), ("selawik-light.ttf", "selawik-light.ttf"), ("OFL.txt", "OFL.txt")):
        shutil.copyfile(os.path.join(THEME, src), os.path.join(out, dst))
    try:
        from PIL import Image
    except ImportError:
        shutil.copyfile(os.path.join(REPO, "tools", "repo_icon.png"), os.path.join(out, "icon.png"))
        print("staged %s (icon.png from tools/repo_icon.png - no Pillow here)" % ", ".join(sorted(os.listdir(out))))
        return
    with Image.open(os.path.join(THEME, "abback2.jpg")) as im:
        emblem = im.crop(EMBLEM_BOX)
        side = max(emblem.size)
        square = Image.new("RGB", (side, side), (7, 34, 74))
        square.paste(emblem, ((side - emblem.width) // 2, (side - emblem.height) // 2))
        square.resize((128, 128), Image.LANCZOS).save(os.path.join(out, "icon.png"), optimize=True)
    print("staged %s" % ", ".join(sorted(os.listdir(out))))


if __name__ == "__main__":
    main()
