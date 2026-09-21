//
// GuiActionMenu: a compact panel of actions. See the header.
//
#include "gui_action_menu.h"
#include "../gui.h"

#include <algorithm>

using namespace std;

namespace {
// the panel: as tall as its rows need, up to the screen less a margin; more rows than fit scroll
const int PanelWidth = 800;
const int PanelMargin = PanelStyle::Margin;
const int HeaderHeight = PanelStyle::HeaderHeight;
const int FooterHeight = PanelStyle::FooterHeight;
const int RowHeight = PanelStyle::RowHeight;
const int RowInset = PanelStyle::RowInset;
} // namespace

//*******************************
// GuiActionMenu::init
//*******************************
void GuiActionMenu::init() {
    selected = max(0, min(selected, static_cast<int>(items.size()) - 1));
    firstVisible = 0;
    result = -1;
    style = gui->panelStyle();
    moveSelection(0); // scrolls the kept selection into view
}

//*******************************
// GuiActionMenu::visibleRows
//*******************************
int GuiActionMenu::visibleRows() const {
    int roomForRows = SCREEN_HEIGHT - 2 * PanelMargin - HeaderHeight - FooterHeight;
    return max(1, min(static_cast<int>(items.size()), roomForRows / RowHeight));
}

//*******************************
// GuiActionMenu::render
//*******************************
void GuiActionMenu::render() {
    renderer.clear();
    gui->renderBackground();
    style.dim(renderer);
    const int rows = visibleRows();
    const int panelHeight = HeaderHeight + rows * RowHeight + FooterHeight;
    ableem::Rect panel{(SCREEN_WIDTH - PanelWidth) / 2, (SCREEN_HEIGHT - panelHeight) / 2, PanelWidth, panelHeight};
    style.sheet(renderer, panel);

    Fonts &fonts = gui->assets().themeFonts;
    int rowY = style.header(*gui, panel, title);
    if (!subtitle.empty()) {
        const ableem::Font &font = fonts[FONT_22_MED];
        const int y = panel.y + 18 + (fonts[FONT_28_BOLD].lineHeight() - font.lineHeight()) / 2;
        gui->text().renderText_WithColor(font, subtitle,
                                         panel.x + panel.w - RowInset - gui->text().textWidth(font, subtitle), y,
                                         style.secondary, XALIGN_LEFT);
    }
    for (int i = firstVisible; i < firstVisible + rows && i < static_cast<int>(items.size()); i++) {
        if (i == selected)
            style.selection(renderer, ableem::Rect(panel.x + 1, rowY, panel.w - 2, RowHeight));
        gui->text().renderText_WithColor(fonts[FONT_22_MED], items[i].title, panel.x + RowInset + 8, rowY + 7,
                                         i == selected ? style.text : style.secondary, XALIGN_LEFT);
        gui->text().renderText_WithColor(fonts[FONT_15_BOLD], items[i].description, panel.x + RowInset + 8, rowY + 35,
                                         style.secondary, XALIGN_LEFT);
        rowY += RowHeight;
    }
    // scroll markers: a small triangle at the top or bottom edge of the rows when more are that way
    const int markerX = panel.x + panel.w - RowInset;
    if (firstVisible > 0)
        style.scrollMarker(renderer, markerX, panel.y + HeaderHeight - 4, -1);
    if (firstVisible + rows < static_cast<int>(items.size()))
        style.scrollMarker(renderer, markerX, panel.y + HeaderHeight + rows * RowHeight + 2, 1);

    style.footer(*gui, ableem::Rect(panel.x, panel.y + panel.h - FooterHeight, panel.w, FooterHeight),
                 {{{"X"}, crossLabel.empty() ? _("Select") : crossLabel},
                  {{"O"}, circleLabel.empty() ? _("Back") : circleLabel}},
                 "", false);
    renderer.present();
}

//*******************************
// GuiActionMenu::moveSelection
//*******************************
void GuiActionMenu::moveSelection(int step) {
    const int count = static_cast<int>(items.size());
    if (count == 0)
        return;
    selected = (selected + step + count) % count;
    const int rows = visibleRows();
    if (selected < firstVisible)
        firstVisible = selected;
    else if (selected >= firstVisible + rows)
        firstVisible = selected - rows + 1;
}

//*******************************
// GuiActionMenu::loop
//*******************************
void GuiActionMenu::loop() {
    menuVisible = true;
    while (menuVisible) {
        render();
        Event e;
        while (gui->input().poll(e)) {
            if (e.type == Event::Type::Quit) {
                result = -1;
                menuVisible = false;
            }
            switch (e.type) {
            case Event::Type::DpadDown:
            case Event::Type::DpadUp:
                if (gui->input().dpadUp()) {
                    app.audio().cursor.play();
                    moveSelection(-1);
                } else if (gui->input().dpadDown()) {
                    app.audio().cursor.play();
                    moveSelection(1);
                }
                break;
            case Event::Type::ButtonDown:
                if (e.button == Button::Cross && !items.empty()) {
                    app.audio().cursor.play();
                    result = selected;
                    menuVisible = false;
                } else if (e.button == Button::Circle) {
                    app.audio().cancel.play();
                    result = -1;
                    menuVisible = false;
                }
                break;
            default:
                break;
            }
        }
    }
}
