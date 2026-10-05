//
// The launcher's layout profile (evoui/evoui_layout.h): the 1280x720 numbers the code always had, and the 4:3 canvas's
// from a theme's layout4x3 block with the built-in 4:3 default for whatever the theme leaves out.
//
#include "doctest/doctest.h"

#include "evoui_layout.h"

#include <ableem/engine/theme_spec.h>

namespace {
ableem::ThemeLayout4x3 themeWith(std::initializer_list<std::pair<const std::string, double>> values) {
    ableem::ThemeLayout4x3 theme;
    theme.set = true;
    theme.values = values;
    return theme;
}
} // namespace

TEST_CASE("EvoLayout::wide: the 1280x720 launcher's numbers, unchanged") {
    const EvoLayout l = EvoLayout::wide();
    CHECK_FALSE(l.fourByThree);
    CHECK(l.canvasW == 1280);
    CHECK(l.canvasH == 720);
    CHECK_FALSE(l.logoSet);
    // the selected cover at 527, 180 (90 raised), full size; the shelf through y 156, 0.5, 190 out, 50 apart
    CHECK(l.carousel.mainX() == 640 - 113);
    CHECK(l.carousel.mainY(false) == 180);
    CHECK(l.carousel.mainY(true) == 90);
    CHECK(l.carousel.mainScale == 1.0f);
    CHECK(l.carousel.shelfMiddleY == 100 + static_cast<int>(226 * 0.5f) / 2);
    CHECK(l.carousel.sideScale == 0.5f);
    CHECK(l.carousel.sideOffset == 190);
    CHECK(l.carousel.sideStep == 50);
    CHECK(l.carousel.lightRange == 150.0f);
    CHECK(l.carousel.isWide()); // the carousel places this shelf with its own constants
    // Play's box and image places, the details, the arrow
    CHECK(l.play.box.x == 540);
    CHECK(l.play.box.y == 428);
    CHECK(l.play.box.w == 200);
    CHECK(l.play.box.h == 68);
    CHECK(l.play.textX == 640 - 262 / 2);
    CHECK(l.play.iconSize == 28);
    CHECK(l.play.fontMax == 28);
    CHECK(l.play.fontMin == 14);
    CHECK(l.meta.x == 785);
    CHECK(l.meta.y == 285);
    CHECK(l.meta.yRaised == 215);
    CHECK(l.meta.metrics.ruleWidth == MetaLayout::RuleWidth);
    CHECK(l.meta.metrics.iconRowY == MetaLayout::IconRowY);
    CHECK(l.meta.metrics.valueWidth() == MetaLayout::ValueWidth);
    CHECK(l.meta.metrics.descriptionLines() == MetaLayout::DescriptionLines);
    CHECK(l.meta.metrics.badgeX(3, 1) == MetaLayout::badgeX(3, 1));
    CHECK(l.arrowX == 628);
    CHECK(l.arrowY == 360);
    CHECK(l.arrowSize == 0);
    // the menu row, its caption, the band, the hint bar's fallback and the bubbles
    CHECK(l.menu.x == 581);
    CHECK(l.menu.yClosed == 520);
    CHECK(l.menu.yOpen == 440);
    CHECK(l.menu.icon == 118);
    CHECK(l.menu.pitch == 130.0f);
    CHECK(l.menu.headY == 545);
    CHECK(l.menu.textY == 585);
    CHECK(l.band.bottom == 632);
    CHECK(l.band.closed == 100);
    CHECK(l.band.open == 280);
    CHECK_FALSE(l.hintBarFromLayout);
    CHECK(l.hintBar.x == 560);
    CHECK(l.hintBar.y == 624);
    CHECK(l.hintBar.w == 680);
    CHECK(l.hintBar.h == 72);
    CHECK(l.emptyTextY == 412);
    CHECK(l.bubbleRight == 1264);
    CHECK(l.bubbleWidth == 440);
    CHECK(l.messageWidth == 840);
    CHECK(l.cornerTop == 0);
}

TEST_CASE("shelfScale/shelfStep: the 1280x720 shelf's steps in float maths, on every target") {
    // 50 at each cover's size: 48.25, 46.5, ... - 43, 36 and 29 are whole, and stay whole (x87 would give 42, 35, 28)
    const int steps[] = {48, 46, 44, 43, 41, 39, 37, 36, 34, 32, 30, 29, 27};
    for (int d = 2; d <= 14; d++)
        CHECK_MESSAGE(shelfStep(50, shelfScale(0.5f, d), 0.5f) == steps[d - 2], "cover ", d);
}

TEST_CASE("EvoLayout::fromLayout4x3: an empty layout4x3 gives the designer's 4:3 defaults on 640x480") {
    const EvoLayout l = EvoLayout::fromLayout4x3(themeWith({}));
    CHECK(l.fourByThree);
    CHECK(l.canvasW == 640);
    CHECK(l.canvasH == 480);
    CHECK(l.logoSet);
    CHECK(l.logo.x == 14);
    CHECK(l.logo.w == 158);
    // the cover 146 px centred on (196, 214); raised 58 (0.4 of it, as 90 of 226)
    CHECK(l.carousel.centreX == 196);
    CHECK(l.carousel.mainScale == doctest::Approx(146 / 226.0));
    CHECK(l.carousel.mainX() == 196 - 73);
    CHECK(l.carousel.mainY(false) == 214 - 73);
    CHECK(l.carousel.raise == 58);
    CHECK(l.carousel.shelfMiddleY == 98);
    CHECK(l.carousel.sideScale == doctest::Approx(0.3));
    CHECK(l.carousel.sideOffset == 114);
    CHECK(l.carousel.sideStep == 30);
    CHECK_FALSE(l.carousel.isWide());
    // Play under the cover, centred on it; its content scaled with the box
    CHECK(l.play.box.x == 130);
    CHECK(l.play.box.y == 300);
    CHECK(l.play.box.x + l.play.box.w / 2 == l.carousel.centreX);
    CHECK(l.play.iconSize == 19);
    CHECK(l.play.fontMax == 19);
    // the details on the right half, inside the canvas
    CHECK(l.meta.x == 340);
    CHECK(l.meta.x + l.meta.metrics.ruleWidth <= l.canvasW);
    CHECK(l.meta.metrics.titleSize == 21);
    CHECK(l.meta.metrics.iconRowY + l.meta.metrics.iconSize == l.meta.metrics.height);
    CHECK(l.meta.metrics.gridY + 3 * l.meta.metrics.rowPitch <= l.meta.metrics.iconRowY); // three facts fit
    // the menu row: 65 px icons 89 apart, the selected one under the cover; it opens upwards
    CHECK(l.menu.icon == 65);
    CHECK(l.menu.pitch == 89.0f);
    CHECK(l.menu.x + l.menu.icon / 2 == l.carousel.centreX);
    CHECK(l.menu.yClosed == 330);
    CHECK(l.menu.yOpen < l.menu.yClosed);
    CHECK(l.menu.captionX == l.carousel.centreX);
    // the arrow between the raised cover and the open row
    CHECK(l.arrowY > l.carousel.mainY(true) + 146);
    CHECK(l.arrowY + l.arrowSize < l.menu.yOpen);
    // the hint bar across the bottom, the band ending on it, the corner under the logo
    CHECK(l.hintBarFromLayout);
    CHECK(l.hintBar.x == 8);
    CHECK(l.hintBar.y == 418);
    CHECK(l.hintBar.w == 624);
    CHECK(l.hintBar.y + l.hintBar.h <= l.canvasH);
    CHECK(l.band.bottom == l.hintBar.y);
    CHECK(l.menu.textY + l.menu.textSize <= l.hintBar.y);
    CHECK(l.cornerTop == 12 + 38 + 4);
    CHECK(l.bubbleRight <= l.canvasW);
}

namespace {
struct Box {
    float x, y, w, h;
};
bool intersects(const Box &a, const Box &b) {
    return a.x < b.x + b.w && b.x < a.x + a.w && a.y < b.y + b.h && b.y < a.y + a.h;
}
// a cover's box at scale `scale` with its top-left at (x, y), grown by 10 % about its middle: a turned box's near edge
// stands a little taller and wider than its unturned box (Carousel's perspective)
Box coverBox(float x, float y, float scale) {
    const float size = 226 * scale, grow = size * 0.1f;
    return {x - grow / 2, y - grow / 2, size + grow, size + grow};
}
} // namespace

TEST_CASE(
    "EvoLayout::fromLayout4x3: the details never cover a cover - at rest, raised, or moving to and from the shelf") {
    const EvoLayout l = EvoLayout::fromLayout4x3(themeWith({}));
    const EvoLayout::Carousel &c = l.carousel;
    for (int raised = 0; raised < 2; raised++) {
        const Box meta{static_cast<float>(l.meta.x), static_cast<float>(raised ? l.meta.yRaised : l.meta.y),
                       static_cast<float>(l.meta.metrics.ruleWidth), static_cast<float>(l.meta.metrics.height)};
        // every shelf slot on both sides
        for (int side = 0; side < 2; side++)
            for (int d = 1; d <= c.sideCovers; d++) {
                const EvoLayout::ShelfSlot s = c.shelfSlot(d, side);
                CHECK_FALSE_MESSAGE(intersects(meta, coverBox(s.x, s.y, s.scale)), "slot ", d, " side ", side);
            }
        // the selected cover, and its way to the nearest slot on either side (a scroll moves place and size linearly)
        const float mx = static_cast<float>(c.mainX()), my = static_cast<float>(c.mainY(raised != 0));
        for (int side = 0; side < 2; side++) {
            const EvoLayout::ShelfSlot s = c.shelfSlot(1, side);
            for (int step = 0; step <= 20; step++) {
                const float t = step / 20.0f;
                const Box box =
                    coverBox(mx + (s.x - mx) * t, my + (s.y - my) * t, c.mainScale + (s.scale - c.mainScale) * t);
                CHECK_FALSE_MESSAGE(intersects(meta, box), "t ", t, " side ", side, " raised ", raised);
            }
        }
    }
}

TEST_CASE("EvoLayout::fromLayout4x3: the shelf reaches past the right edge (and past the left one)") {
    const EvoLayout l = EvoLayout::fromLayout4x3(themeWith({}));
    const EvoLayout::Carousel &c = l.carousel;
    const EvoLayout::ShelfSlot right = c.shelfSlot(c.sideCovers, 1);
    CHECK(right.x >= l.canvasW); // the outermost right one wholly off: a scroll brings covers in from the edge
    const EvoLayout::ShelfSlot left = c.shelfSlot(c.sideCovers, 0);
    CHECK(left.x + left.size < 0);
}

TEST_CASE("EvoLayout::fromLayout4x3: the theme's numbers win, the rest follow from them") {
    const EvoLayout l = EvoLayout::fromLayout4x3(themeWith({{"carousel.centreX", 220},
                                                            {"carousel.coverMax", 160},
                                                            {"playButton.y", 310},
                                                            {"menuRow.icon", 60},
                                                            {"menuRow.gap", 20},
                                                            {"meta.x", 330},
                                                            {"hintBar.y", 420},
                                                            {"logo.w", 0}}));
    CHECK(l.carousel.centreX == 220);
    CHECK(l.carousel.mainScale == doctest::Approx(160 / 226.0));
    CHECK(l.carousel.raise == 64); // 160 * 90 / 226
    CHECK(l.play.box.y == 310);
    CHECK(l.menu.icon == 60);
    CHECK(l.menu.pitch == 80.0f);
    CHECK(l.menu.x == 220 - 30); // still under the cover
    CHECK(l.meta.x == 330);
    CHECK(l.hintBar.y == 420);
    CHECK(l.band.bottom == 420);
    CHECK_FALSE(l.logoSet); // a zero-wide logo: the theme's own, and the corner stays free
    CHECK(l.cornerTop == 0);
}

TEST_CASE("EvoLayout::fromLayout4x3: the 4:3 pictures by name, none when the theme has none") {
    ableem::ThemeLayout4x3 theme = themeWith({});
    theme.images["background"] = "/t/images/bg_4x3.png";
    const EvoLayout l = EvoLayout::fromLayout4x3(theme);
    CHECK(l.background == "/t/images/bg_4x3.png");
    CHECK(l.footer.empty());
    CHECK(l.settingsPanel.empty());
}
