#include "evoui_system_menu.h"
#include "gui/gui.h"

#include <algorithm>

using namespace std;

namespace {
// the panel: as tall as its rows need, up to the screen less a margin; more rows than fit scroll
const int PanelWidth = 800;
const int PanelMargin = PanelStyle::Margin;
const int HeaderHeight = PanelStyle::HeaderHeight;
const int FooterHeight = PanelStyle::FooterHeight;
// a row: the title over its description, smaller than a compact panel's usual (PanelStyle::RowHeight, 60), so
// every item fits on the screen at once without scrolling (the owner, 2026-09-25)
const int RowHeight = 46;
const int TitleSize = 19;       // the theme's medium font
const int DescriptionSize = 13; // its bold one
const int RowInset = PanelStyle::RowInset; // the rows' text from the panel's edge
} // namespace

//*******************************
// GuiSystemMenu::init
//*******************************
void GuiSystemMenu::init() {
    items.clear();
    items.push_back(
        {SystemMenuAction::RescanGames, _("Re-Scan Games"),
         scanInProgress ? _("A scan is already in progress") : _("Look for new, changed or removed games")});
    // second, right after the rescan (the owner, 2026-09-25): the Store is an extension
    items.push_back({SystemMenuAction::Extensions, _("Extensions"), _("Run an installed extension")});
    items.push_back({SystemMenuAction::RetroArch, retroArchLabel, _("Exit to") + " " + retroArchLabel});
    items.push_back({SystemMenuAction::MemoryCards, _("Memory Cards"), _("Create, rename or manage memory card sets")});
    items.push_back({SystemMenuAction::GameManager, _("Game Manager"), _("Delete games, flush covers")});
    items.push_back(
        {SystemMenuAction::HardwareInfo, _("Hardware Information"), _("Controller and system information")});
    items.push_back({SystemMenuAction::Options, _("Options"), _("Customize AutoBleem settings")});
    items.push_back({SystemMenuAction::Processors, _("Scanner processors"),
                     _("Put the scan's processors in order, switch them on or off")});
#ifdef AB_ONLINE_UPDATE
    items.push_back({SystemMenuAction::SoftwareUpdate, _("Software Update"),
                     updateAvailable ? _("An update is available") : _("Check the download site for a newer version")});
#endif
    items.push_back({SystemMenuAction::About, _("About"), _("About AutoBleem")});
    items.push_back({SystemMenuAction::PowerOff, _("Power Off"), _("Safely power off the console")});
    selected = 0;
    firstVisible = 0;
    result = SystemMenuAction::None;

    style = gui->panelStyle();
}

//*******************************
// GuiSystemMenu::visibleRows
//*******************************
int GuiSystemMenu::visibleRows() const {
    int roomForRows = SCREEN_HEIGHT - 2 * PanelMargin - HeaderHeight - FooterHeight;
    return max(1, min(static_cast<int>(items.size()), roomForRows / RowHeight));
}

//*******************************
// GuiSystemMenu::render
//*******************************
void GuiSystemMenu::render() {
    // the launcher's own background, dimmed, so the menu reads as an overlay on the screen it came from
    if (background.valid())
        renderer.copy(background, nullptr, nullptr);
    else
        gui->renderBackground();
    style.dim(renderer);

    const int rows = visibleRows();
    const int panelHeight = HeaderHeight + rows * RowHeight + FooterHeight;
    ableem::Rect panel{(SCREEN_WIDTH - PanelWidth) / 2, (SCREEN_HEIGHT - panelHeight) / 2, PanelWidth, panelHeight};
    style.sheet(renderer, panel);

    // every text on this screen gets the launcher's halo, like the launcher's own
    const TextRenderer::Shadow classicShadow = gui->text().shadow();
    TextRenderer::Shadow shadow;
    shadow.enabled = style.textShadow;
    gui->text().setShadow(shadow);

    Fonts &fonts = gui->assets().themeFonts;
    int rowY = style.header(*gui, panel, _("System"));
    for (int i = firstVisible; i < firstVisible + rows && i < static_cast<int>(items.size()); i++) {
        if (i == selected)
            style.selection(renderer, ableem::Rect(panel.x + 1, rowY, panel.w - 2, RowHeight));
        gui->text().renderText_WithColor(fonts.atSize(FONT_MED, TitleSize), items[i].title, panel.x + RowInset + 8,
                                         rowY + 4, i == selected ? style.text : style.secondary, XALIGN_LEFT);
        gui->text().renderText_WithColor(fonts.atSize(FONT_BOLD, DescriptionSize), items[i].description,
                                         panel.x + RowInset + 8, rowY + 27, style.secondary, XALIGN_LEFT);
        rowY += RowHeight;
    }

    // scroll markers: a small triangle at the top or bottom edge of the rows when more are that way
    const int markerX = panel.x + panel.w - RowInset;
    if (firstVisible > 0)
        style.scrollMarker(renderer, markerX, panel.y + HeaderHeight - 4, -1);
    if (firstVisible + rows < static_cast<int>(items.size()))
        style.scrollMarker(renderer, markerX, panel.y + HeaderHeight + rows * RowHeight + 2, 1);

    // the footer: the launcher's own button hints
    style.footer(*gui, ableem::Rect(panel.x, panel.y + panel.h - FooterHeight, panel.w, FooterHeight),
                 {{{"X"}, _("Select")}, {{"O"}, _("Back")}}, "", false);

    gui->text().setShadow(classicShadow);
    renderer.present();
}

//*******************************
// GuiSystemMenu::moveSelection
//*******************************
void GuiSystemMenu::moveSelection(int step) {
    const int count = static_cast<int>(items.size());
    selected = (selected + step + count) % count;
    const int rows = visibleRows();
    if (selected < firstVisible)
        firstVisible = selected;
    else if (selected >= firstVisible + rows)
        firstVisible = selected - rows + 1;
}

//*******************************
// GuiSystemMenu::loop
//*******************************
void GuiSystemMenu::loop() {
    menuVisible = true;
    while (menuVisible) {
        render();
        Event e;
        while (gui->input().poll(e)) {
            if (e.type == Event::Type::Quit) {
                result = SystemMenuAction::None;
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
                if (e.button == Button::Cross) {
                    app.audio().cursor.play();
                    result = items[selected].action;
                    menuVisible = false;
                } else if (e.button == Button::Circle) {
                    app.audio().cancel.play();
                    result = SystemMenuAction::None;
                    menuVisible = false;
                }
                break;
            default:
                break;
            }
        }
    }
}
