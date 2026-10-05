//
// EvoLayout: where the EvolutionUI launcher puts its parts - the layout profile. wide() is the 1280x720 launcher, every
// number the one the code always had; fromLayout4x3() is the 640x480 canvas of a 4:3 output (CRT 480p, the Renderer's
// CanvasMapping), from the theme's `layout4x3` block (ableem::ThemeLayout4x3) with this file's own 4:3 default for
// any number the theme leaves out. The defaults are the designer's ab2.0.0 4:3 mockup (autobleem-design, CRT 480p,
// 2026-10-05): the logo top left, the carousel centred at x 196 with the details on the right half, Play under the
// cover, the menu row under Play, a full-width hint bar of two lines. Pure, so tests/screens/test_evoui_layout holds
// it without a Gui.
//
#pragma once

#include "controls/evoui_meta_layout.h"

#include <ableem/engine/theme_spec.h>
#include <ableem/ui/canvas.h>
#include <ableem/ui/types.h>

#include <cmath>
#include <algorithm>
#include <string>

// The shelf's rule in float maths, every step rounded to a float as SSE/ARM do: the 32-bit PC stick's x87 keeps 80
// bits until a value is stored, and `step * scale / nearest` then falls a hair under a whole number (50 * 0.29 at the
// thirteenth cover) - an outer cover would move a pixel. The cover's scale `distance` covers out (1 = the nearest)
// from the nearest's, 3.5 % of it smaller each; and the step to it, at its own size.
inline float storedFloat(float v) {
    volatile float stored = v;
    return stored;
}
inline float shelfScale(float nearest, int distance) {
    const float shrink = storedFloat(0.035f * static_cast<float>(distance - 1));
    return storedFloat(nearest * storedFloat(1.0f - shrink));
}
inline int shelfStep(int step, float scale, float nearest) {
    return static_cast<int>(storedFloat(storedFloat(static_cast<float>(step) * scale) / nearest));
}

struct EvoLayout {
    // a side cover's place: the top-left of its (unturned) box, its width and its scale of the 226 px cover
    struct ShelfSlot {
        int x = 0, y = 0, size = 0;
        float scale = 0.0f;
    };

    bool fourByThree = false;
    int canvasW = 1280, canvasH = 720;

    // the 4:3 pictures (absolute paths; "" = the theme's 16:9 one: a background is then cut to its middle, a footer is
    // not drawn - the hint bar's frame stands)
    std::string background, footer, settingsPanel;

    // the launcher logo's box; unset = the theme's launcher.logo as it is
    ableem::Rect logo;
    bool logoSet = false;

    // the carousel: the selected cover's centre and size (scale of the 226 px cover), raised by `raise` for the game
    // menu; the shelf of side covers on the line `shelfMiddleY`, the nearest at `sideScale`, `sideOffset` from the
    // centre and each next one `sideStep` (at the nearest's size) further out
    struct Carousel {
        int centreX = 640;
        int centreY = 180 + 113;
        float mainScale = 1.0f;
        int raise = 90;
        int shelfMiddleY = 100 + 113 / 2; // 156: the shelf's covers are 226 * 0.5 high, their top at 100
        float sideScale = 0.5f;
        int sideOffset = 190;
        int sideStep = 50;
        int sideCovers = 14; // covers a side (the row is 2 * this + 1 slots)
        // the selected cover's light reaches this far from the centre (it hands over as the row scrolls)
        float lightRange = 150.0f;
        // where the selected cover's box stands (its top-left), in the Games row or raised over the open menu
        int half() const { return static_cast<int>(std::lround(113 * mainScale)); }
        int mainX() const { return centreX - half(); }
        int mainY(bool raised) const { return centreY - half() - (raised ? raise : 0); }
        // the `distance`th cover out (1..sideCovers) on the left (side 0) or the right: each next one a little smaller
        // (3.5 % of the nearest's size) and `sideStep` at its own size further out - PsCarousel::createCoverPoint's
        // shelf. (The 1280x720 one is placed with its own constants there, the same rule.)
        ShelfSlot shelfSlot(int distance, int side) const {
            float scale = sideScale;
            int offset = sideOffset;
            for (int d = 2; d <= distance; d++) {
                scale = shelfScale(sideScale, d);
                offset += shelfStep(sideStep, scale, sideScale);
            }
            ShelfSlot slot;
            slot.scale = scale;
            slot.size = static_cast<int>(226 * scale);
            slot.y = shelfMiddleY - slot.size / 2;
            slot.x = (side == 0 ? centreX - offset : centreX + offset) - slot.size / 2;
            return slot;
        }
        // the 1280x720 shelf (the carousel then places it with its own constants, carousel_game.cpp)
        bool isWide() const {
            const Carousel wide;
            return centreX == wide.centreX && shelfMiddleY == wide.shelfMiddleY && sideScale == wide.sideScale &&
                   sideOffset == wide.sideOffset && sideStep == wide.sideStep && sideCovers == wide.sideCovers;
        }
    } carousel;

    // Play: the theme's `play` frame box (the label and icon pulse in it), or the two Play images at playButton /
    // playText scaled by `imageScale`
    struct Play {
        ableem::Rect box = ableem::Rect(540, 428, 200, 68);
        int textX = 640 - 262 / 2;
        float imageScale = 1.0f;
        int iconSize = 28, gap = 8, padding = 16;
        int fontMax = 28, fontMin = 14;
    } play;

    // the game's details: their place in the Games state and with the menu open, and the section's numbers
    struct Meta {
        int x = 785, y = 285, yRaised = 215;
        MetaLayout::Metrics metrics;
    } meta;

    // the arrow over the open menu
    int arrowX = 640 - 12, arrowY = 360, arrowSize = 0; // 0: the image's own size

    // the menu row: the selected icon's left, the row's y closed (Games) and open, the icon's size and the pitch from
    // one icon to the next; the caption (the selected icon's name and line) centred on captionX
    struct Menu {
        int x = 640 - 118 / 2;
        int yClosed = 520, yOpen = 440;
        int icon = 118;
        float pitch = 130.0f;
        int headY = 545, textY = 585;
        int headSize = 28, textSize = 22;
        int captionX = 640;
    } menu;

    // the settings band (launcher.settingsPanel) behind the menu row: it ends at `bottom`, `closed` / `open` tall
    struct Band {
        int bottom = 632, closed = 100, open = 280;
    } band;

    // the hint bar when the theme gives none (4:3: the layout's, always)
    ableem::Rect hintBar = ableem::Rect(560, 624, 680, 72);
    bool hintBarFromLayout = false;
    // the hint lines drawn at 1 / hintScale and scaled down into the bar (1: drawn as they are)
    float hintScale = 1.0f;

    // the rest: "No games here yet" under the shelf, the bubbles' right edge and widths, the corner's pad plate and
    // channel tag pushed under a logo in the corner
    int emptyTextY = 412, emptyTextSize = 22;
    int bubbleRight = 1280 - 16, bubbleWidth = 440, messageWidth = 840;
    int cornerTop = 0;

    static EvoLayout wide() { return EvoLayout(); }
    static EvoLayout fromLayout4x3(const ableem::ThemeLayout4x3 &theme);
};

//*******************************
// EvoLayout::fromLayout4x3
//*******************************
// The theme's numbers ("block.key", ThemeLayout4x3) over the 4:3 defaults. The keys: logo.{x,y,w,h};
// carousel.{centreX,centreY,coverMax,raise,shelfY,shelfH,sideScale}; playButton.{x,y,w,h}; meta.{x,y,yRaised,w,h,
// titleSize,labelSize,valueSize,infoSize}; arrow.{x,y,w}; menuRow.{x,y,yOpen,icon,gap}; menuCaption.{x,y,size,
// textY,textSize}; band.{bottom,closed,open}; hintBar.{x,y,w,h}; images background / footer / settingsPanel.
inline EvoLayout EvoLayout::fromLayout4x3(const ableem::ThemeLayout4x3 &theme) {
    auto num = [&theme](const char *key, double fallback) { return theme.value(key, fallback); };
    auto i = [&num](const char *key, int fallback) { return static_cast<int>(std::lround(num(key, fallback))); };
    EvoLayout l;
    l.fourByThree = true;
    l.canvasW = ableem::FourByThreeCanvasW;
    l.canvasH = ableem::FourByThreeCanvasH;
    l.background = theme.image("background");
    l.footer = theme.image("footer");
    l.settingsPanel = theme.image("settingsPanel");

    l.logo = ableem::Rect(i("logo.x", 14), i("logo.y", 12), i("logo.w", 158), i("logo.h", 38));
    l.logoSet = l.logo.w > 0 && l.logo.h > 0;

    // the selected cover 146 px (0.65 of 16:9) at (196, 214); the shelf of side covers along y 60..136, 0.6 of the
    // 16:9 shelf in size and spacing; raised by 0.4 of its size for the menu, as on 16:9 (90 of 226)
    Carousel &c = l.carousel;
    const int coverMax = std::max(16, i("carousel.coverMax", 146));
    c.centreX = i("carousel.centreX", 196);
    c.centreY = i("carousel.centreY", 214);
    c.mainScale = coverMax / 226.0f;
    c.raise = i("carousel.raise", static_cast<int>(std::lround(coverMax * 90 / 226.0)));
    const double shelfShare = num("carousel.sideScale", 0.6);
    c.shelfMiddleY = i("carousel.shelfY", 60) + i("carousel.shelfH", 76) / 2;
    c.sideScale = static_cast<float>(0.5 * shelfShare);
    c.sideOffset = static_cast<int>(std::lround(190 * shelfShare));
    c.sideStep = static_cast<int>(std::lround(50 * shelfShare));
    c.lightRange = 150.0f * c.mainScale;
    // the shelf starts left of the middle: 19 covers a side reach past the right edge (the left ones that fall
    // off the canvas are not drawn)
    c.sideCovers = std::max(1, i("carousel.sideCovers", 19));

    // Play in the box under the cover; the frame's content scaled with the box's height (68 on 16:9)
    Play &p = l.play;
    p.box = ableem::Rect(i("playButton.x", 130), i("playButton.y", 300), i("playButton.w", 132), i("playButton.h", 45));
    const float k = p.box.h / 68.0f;
    p.imageScale = k;
    p.textX = p.box.x + p.box.w / 2 - static_cast<int>(std::lround(262 * k / 2));
    p.iconSize = static_cast<int>(std::lround(28 * k));
    p.gap = std::max(3, static_cast<int>(std::lround(8 * k)));
    p.padding = static_cast<int>(std::lround(16 * k));
    p.fontMax = std::max(11, static_cast<int>(std::lround(28 * k)));
    p.fontMin = 11;

    // the details on the right half, below the shelf and clear of the selected cover on its way to and from the shelf
    // (it may cross only the reflections): the 16:9 section's grid in a 292 x 110 box, the designer's font sizes; it
    // stays put when the menu opens (raised, it would run into the shelf)
    Meta &m = l.meta;
    m.x = i("meta.x", 340);
    m.y = i("meta.y", 162);
    m.yRaised = i("meta.yRaised", m.y);
    MetaLayout::Metrics &g = m.metrics;
    g.height = i("meta.h", 110);
    g.ruleWidth = i("meta.w", 292);
    g.titleSize = i("meta.titleSize", 21);
    g.titleMinSize = 10;
    g.labelSize = i("meta.labelSize", 11);
    g.valueSize = i("meta.valueSize", 14);
    g.infoSize = i("meta.infoSize", 13);
    g.ruleY = g.titleSize + 8;
    g.gridY = g.ruleY + 7;
    g.rowPitch = g.valueSize + 3;
    g.labelDrop = 2;
    g.valueX = 84;
    g.iconSize = 22;
    g.iconRowY = g.height - g.iconSize;
    g.playersTextX = 28;
    g.discX = 100;
    g.discCountX = 127;
    g.badgePitch = 30;
    g.badgesRight = g.ruleWidth;

    // the menu row under Play: 65 px icons 89 apart, the selected one under the cover's centre; open, it rises as on
    // 16:9 (80 of 118) and its caption stands under it, over the hint bar
    Menu &n = l.menu;
    n.icon = i("menuRow.icon", 65);
    n.pitch = static_cast<float>(n.icon + i("menuRow.gap", 24));
    n.x = i("menuRow.x", c.centreX - n.icon / 2);
    n.yClosed = i("menuRow.y", 330);
    n.yOpen = i("menuRow.yOpen", n.yClosed - static_cast<int>(std::lround(80.0 * n.icon / 118)));
    n.captionX = i("menuCaption.x", n.x + n.icon / 2);
    n.headSize = i("menuCaption.size", 16);
    n.headY = i("menuCaption.y", 372);
    n.textSize = i("menuCaption.textSize", 13);
    n.textY = i("menuCaption.textY", n.headY + n.headSize + 6);

    // the arrow over the open row, under the raised cover
    l.arrowSize = i("arrow.w", 20);
    l.arrowX = i("arrow.x", c.centreX - l.arrowSize / 2);
    l.arrowY = i("arrow.y", c.mainY(true) + coverMax + 8);

    l.hintBar = ableem::Rect(i("hintBar.x", 8), i("hintBar.y", 418), i("hintBar.w", 624), i("hintBar.h", 58));
    l.hintBarFromLayout = true;
    l.hintScale = static_cast<float>(std::min(1.0, std::max(0.4, num("hintBar.scale", 0.75))));

    // the band ends on the hint bar, 0.6 of its 16:9 lengths
    l.band.bottom = i("band.bottom", l.hintBar.y);
    l.band.closed = i("band.closed", 60);
    l.band.open = i("band.open", 170);

    l.emptyTextY = p.box.y + 8;
    l.emptyTextSize = 15;
    l.bubbleRight = l.canvasW - 12;
    l.bubbleWidth = 300;
    l.messageWidth = 420;
    // the logo in the top-left corner: the pad plate and the channel tag go under it
    l.cornerTop = l.logoSet && l.logo.x < 100 && l.logo.y < 60 ? l.logo.y + l.logo.h + 4 : 0;
    return l;
}
