#!/usr/bin/env python3
"""Draws src/resources/evoimg/bigbox.png: the edge of a printed cardboard game box ("big box"), as a
9-slice frame the carousel lays over a RetroArch game's or an App's cover art (PsCarouselGame::loadTex).

The picture is 226x226 with a transparent middle; only the outer BORDER pixels carry anything: a 1 px
near-black outline, a 1 px highlight just inside it along the top and left (the lit edge of the box),
and a soft inner shadow fading in over the rest, so the art looks printed on a box rather than floating.
Being drawn as a 9-slice, the corners are used as they are and the edges are stretched, which is why
nothing here varies along an edge. Replace the file with real artwork of the same layout any time.
"""
from PIL import Image

SIZE = 226
BORDER = 7
OUTLINE = (18, 16, 14, 255)
HIGHLIGHT = (255, 255, 255, 70)
SHADOW_ALPHA = 105  # at the outline, fading to 0 at BORDER

im = Image.new("RGBA", (SIZE, SIZE), (0, 0, 0, 0))
px = im.load()
for y in range(SIZE):
    for x in range(SIZE):
        d = min(x, y, SIZE - 1 - x, SIZE - 1 - y)  # distance from the outer edge
        if d >= BORDER:
            continue
        if d == 0:
            px[x, y] = OUTLINE
        elif d == 1 and (x <= y and x < SIZE - 1 - y or y <= x and y < SIZE - 1 - x) and (x == 1 or y == 1):
            px[x, y] = HIGHLIGHT
        else:
            a = int(SHADOW_ALPHA * (1 - (d - 1) / (BORDER - 1)))
            px[x, y] = (0, 0, 0, a)
im.save("src/resources/evoimg/bigbox.png")
print("wrote src/resources/evoimg/bigbox.png", im.size, "border", BORDER)
