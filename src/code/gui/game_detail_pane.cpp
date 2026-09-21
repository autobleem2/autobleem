//
// GameDetailPane: the editors' and Game Manager's right-hand pane. See the header.
//
#include "game_detail_pane.h"
#include "gui.h"

using namespace std;

//*******************************
// GameDetailPane::rect / rowsRight
//*******************************
ableem::Rect GameDetailPane::rect(Gui &gui) {
    ableem::Rect content = gui.classicContent();
    return ableem::Rect(content.x + content.w - Width, content.y, Width, content.h);
}

int GameDetailPane::rowsRight(Gui &gui) {
    return rect(gui).x - PanelStyle::RowInset;
}

//*******************************
// GameDetailPane::render
//*******************************
void GameDetailPane::render(Gui &gui) const {
    PanelStyle style = gui.panelStyle();
    ableem::Renderer &renderer = gui.renderer();
    ableem::Rect pane = rect(gui);

    // the rule between the rows and the pane
    renderer.setBlendMode(ableem::BlendMode::Blend);
    renderer.setDrawColor(ableem::Color(style.secondary.r, style.secondary.g, style.secondary.b, 160));
    renderer.fillRect(ableem::Rect(pane.x, pane.y + 8, 1, pane.h - 16));

    const int innerX = pane.x + Inset;
    const int innerW = pane.w - 2 * Inset;
    int y = pane.y + 12;

    // the cover, aspect-fit into its box, centred, on a faint plate
    {
        ableem::Rect box(innerX + (innerW - CoverSize) / 2, y, CoverSize, CoverSize);
        renderer.setDrawColor(ableem::Color(255, 255, 255, 12));
        renderer.fillRect(box);
        if (cover.valid()) {
            ableem::Size s = cover.size();
            ableem::Rect dst = box;
            if (s.w > 0 && s.h > 0) {
                if (s.w >= s.h) {
                    dst.h = CoverSize * s.h / s.w;
                    dst.y = box.y + (CoverSize - dst.h) / 2;
                } else {
                    dst.w = CoverSize * s.w / s.h;
                    dst.x = box.x + (CoverSize - dst.w) / 2;
                }
            }
            renderer.copy(cover, nullptr, &dst);
        }
        y += CoverSize + 12;
    }

    // the screenshot, the pane's width, when there is one
    if (snap.valid()) {
        ableem::Size s = snap.size();
        ableem::Rect dst(innerX, y, innerW, s.w > 0 ? innerW * s.h / s.w : SnapHeight);
        if (dst.h > SnapHeight) {
            dst.h = SnapHeight;
            dst.w = s.h > 0 ? SnapHeight * s.w / s.h : innerW;
            dst.x = innerX + (innerW - dst.w) / 2;
        }
        renderer.copy(snap, nullptr, &dst);
        y += dst.h + 12;
    }

    // the facts: the label in the secondary colour, the value under it in the text colour
    Fonts &fonts = gui.assets().themeFonts;
    const ableem::Font &labelFont = fonts[FONT_15_BOLD];
    const ableem::Font &valueFont = fonts[FONT_20_BOLD];
    const int bottom = pane.y + pane.h - 8;
    for (const auto &fact : facts) {
        if (y + 42 > bottom)
            break;
        gui.text().renderText_WithColor(labelFont, fact.first, innerX, y, style.secondary, XALIGN_LEFT);
        y += 18;
        gui.text().renderText_WithColor(valueFont, gui.text().elide(valueFont, fact.second, innerW), innerX, y,
                                        style.text, XALIGN_LEFT);
        y += 30;
    }
}
