//
// PanelStyle: the shared look of the menus and dialogs. See the header.
//
#include "panel_style.h"
#include "gui.h"
#include "theme_assets.h"

#include <ableem/engine/theme_spec.h>

using namespace std;
using ableem::Color;
using ableem::Rect;

//*******************************
// PanelStyle::fromTheme
//*******************************
PanelStyle PanelStyle::fromTheme(const ableem::LauncherTheme &theme, const ThemeAssets *assets) {
    PanelStyle s;
    if (theme.colors.text.set)
        s.text = TextRenderer::toColor(theme.colors.text, 255);
    if (theme.colors.secondary.set)
        s.secondary = TextRenderer::toColor(theme.colors.secondary, 255);
    s.hint = theme.colors.hint.set ? TextRenderer::toColor(theme.colors.hint, 255) : s.secondary;
    s.textShadow = !theme.textShadow.set || theme.textShadow;
    if (assets != nullptr) {
        s.crossIcon = assets->hintCross;
        s.circleIcon = assets->hintCircle;
        s.triangleIcon = assets->hintTriangle;
    }
    return s;
}

//*******************************
// PanelStyle::dim
//*******************************
void PanelStyle::dim(ableem::Renderer &renderer) const {
    renderer.setBlendMode(ableem::BlendMode::Blend);
    renderer.setDrawColor(Color(0, 0, 0, 110));
    renderer.fillRect();
}

//*******************************
// PanelStyle::sheet
//*******************************
void PanelStyle::sheet(ableem::Renderer &renderer, const Rect &panel) const {
    renderer.setBlendMode(ableem::BlendMode::Blend);
    renderer.setDrawColor(Color(0, 0, 0, 200));
    renderer.fillRect(panel);
    renderer.setDrawColor(Color(secondary.r, secondary.g, secondary.b, 160));
    renderer.drawRect(panel);
}

//*******************************
// PanelStyle::rule
//*******************************
void PanelStyle::rule(ableem::Renderer &renderer, const Rect &panel, int y) const {
    renderer.setBlendMode(ableem::BlendMode::Blend);
    renderer.setDrawColor(Color(secondary.r, secondary.g, secondary.b, 160));
    renderer.fillRect(Rect(panel.x + RowInset, y, panel.w - 2 * RowInset, 1));
}

//*******************************
// PanelStyle::header
//*******************************
int PanelStyle::header(Gui &gui, const Rect &panel, const string &title) const {
    gui.text().renderText_WithColor(gui.assets().themeFonts[FONT_28_BOLD], title, panel.x + RowInset, panel.y + 18,
                                    text, XALIGN_LEFT);
    rule(gui.renderer(), panel, panel.y + HeaderHeight - 8);
    return panel.y + HeaderHeight;
}

//*******************************
// PanelStyle::selection
//*******************************
void PanelStyle::selection(ableem::Renderer &renderer, const Rect &rect) const {
    renderer.setBlendMode(ableem::BlendMode::Blend);
    renderer.setDrawColor(Color(text.r, text.g, text.b, 38));
    renderer.fillRect(rect);
    renderer.setDrawColor(text);
    renderer.fillRect(Rect(rect.x, rect.y, SelectionBar, rect.h));
}

//*******************************
// PanelStyle::label
//*******************************
void PanelStyle::label(ableem::Renderer &renderer, const Rect &rect) const {
    renderer.setBlendMode(ableem::BlendMode::Blend);
    renderer.setDrawColor(Color(secondary.r, secondary.g, secondary.b, 70));
    renderer.fillRect(rect);
}

//*******************************
// PanelStyle::scrollMarker
//*******************************
void PanelStyle::scrollMarker(ableem::Renderer &renderer, int cx, int cy, int direction) const {
    renderer.setDrawColor(text);
    for (int i = 0; i < 5; i++)
        renderer.fillRect(Rect(cx - i, cy + direction * i, 2 * i + 1, 1));
}

//*******************************
// PanelStyle::hints
//*******************************
int PanelStyle::hints(Gui &gui, int x, int y, const vector<Hint> &list) const {
    const ableem::Font &font = gui.assets().themeFonts[FONT_22_MED];
    for (const Hint &h : list) {
        if (h.first != nullptr && h.first->valid()) {
            ableem::Size s = h.first->size();
            Rect dst(x, y + (28 - s.h) / 2, s.w, s.h);
            gui.renderer().copy(*h.first, nullptr, &dst);
            x += s.w + 8;
        }
        gui.text().renderText_WithColor(font, h.second, x, y, hint, XALIGN_LEFT);
        x += gui.text().textWidth(font, h.second) + 36;
    }
    return x;
}
