#include "evoui_system_menu.h"
#include "gui/gui.h"

#include <ableem/ui/debug_driver.h>

#include <algorithm>

using namespace std;

namespace {
// the panel: as tall as its rows need, up to the screen less a margin; more rows than fit scroll
const int PanelWidth = 800;
const int PanelMargin = PanelStyle::Margin;
const int HeaderHeight = PanelStyle::HeaderHeight;
const int FooterHeight = PanelStyle::FooterHeight;
// the System menu: single-line items and thin headings, so its thirteen items and three headings fit on the
// screen with the description strip (13 x 32 + 3 x 24 + 24 = 512, all the room there is - the owner,
// 2026-09-26); the Quick menu's few rows are the usual single-line compact row
const int SystemItemHeight = 32;
const int QuickItemHeight = 44;
const int HeadingHeight = 24;
const int StripHeight = 24; // the selected item's description, above the footer
const int SystemTitleSize = 20;
const int QuickTitleSize = 22;
const int HeadingSize = 14;
const int NoteSize = 14;
const int StripSize = 15;
const int RowInset = PanelStyle::RowInset; // the rows' text from the panel's edge
const int TextX = RowInset + 8;            // the header's text x
} // namespace

//*******************************
// GuiSystemMenu::addItem / addHeading
//*******************************
void GuiSystemMenu::addItem(SystemMenuAction action, const string &key, const string &title, const string &description,
                            const string &note) {
    Row row;
    row.action = action;
    row.key = key;
    row.title = title;
    row.description = description;
    row.note = note;
    rows.push_back(row);
}

void GuiSystemMenu::addHeading(const string &title) {
    Row row;
    row.heading = true;
    row.title = title;
    rows.push_back(row);
}

//*******************************
// GuiSystemMenu::init
//*******************************
void GuiSystemMenu::init() {
    rows.clear();
    const string scanNote = scanInProgress ? _("Scan running") : "";
    const string rescanWhat = _("Look for new, changed or removed games");
    const string networkWhat = _("Wi-Fi, Bluetooth pairing and controller mapping");

    if (kind == Kind::Quick) {
        addItem(SystemMenuAction::RescanGames, "Re-Scan Games", _("Re-Scan Games"), rescanWhat, scanNote);
        addItem(SystemMenuAction::Store, "Store", _("Store"), _("Browse and install games, apps and extensions"));
        if (networkProvided)
            addItem(SystemMenuAction::Network, "Network & Controllers", _("Network & Controllers"), networkWhat);
        addItem(SystemMenuAction::SystemMenu, "System menu...", _("System menu..."),
                _("Everything else: Options, Game Manager, Power Off and more"));
    } else {
        // the two used most, on top with no heading (the owner, 2026-09-25/26): the Store is an extension
        addItem(SystemMenuAction::RescanGames, "Re-Scan Games", _("Re-Scan Games"), rescanWhat, scanNote);
        addItem(SystemMenuAction::Extensions, "Extensions", _("Extensions"), _("Run an installed extension"));

        addHeading(_("Library"));
        addItem(SystemMenuAction::GameManager, "Game Manager", _("Game Manager"), _("Delete games, flush covers"));
        addItem(SystemMenuAction::MemoryCards, "Memory Cards", _("Memory Cards"),
                _("Create, rename or manage memory card sets"));
        addItem(SystemMenuAction::Processors, "Scanner processors", _("Scanner processors"),
                _("Put the scan's processors in order, switch them on or off"));

        addHeading(_("System"));
        addItem(SystemMenuAction::Options, "Options", _("Options"), _("Customize AutoBleem settings"));
        if (networkProvided)
            addItem(SystemMenuAction::Network, "Network & Controllers", _("Network & Controllers"), networkWhat);
        addItem(SystemMenuAction::HardwareInfo, "Hardware Information", _("Hardware Information"),
                _("Controller and system information"));
#ifdef AB_ONLINE_UPDATE
        addItem(SystemMenuAction::SoftwareUpdate, "Software Update", _("Software Update"),
                _("Check the download site for a newer version"), updateAvailable ? _("Update available") : "");
#endif
        addItem(SystemMenuAction::About, "About", _("About"), _("About AutoBleem"));

        addHeading(_("Leave"));
        addItem(SystemMenuAction::RetroArch, "RetroArch", retroArchLabel, _("Exit to") + " " + retroArchLabel);
        addItem(SystemMenuAction::PowerOff, "Power Off", _("Power Off"), _("Safely power off the console"));
    }

    selected = 0;
    firstVisible = 0;
    result = SystemMenuAction::None;
    style = gui->panelStyle();
    publishItems();
}

//*******************************
// GuiSystemMenu::publishItems
//*******************************
void GuiSystemMenu::publishItems() const {
    vector<string> keys;
    for (const Row &row : rows)
        if (!row.heading)
            keys.push_back(row.key);
    ableem::DebugDriver::setItems(keys);
}

//*******************************
// GuiSystemMenu::rowHeight / roomForRows / visibleRowCount / visibleHeight
//*******************************
int GuiSystemMenu::rowHeight(const Row &row) const {
    if (row.heading)
        return HeadingHeight;
    return kind == Kind::Quick ? QuickItemHeight : SystemItemHeight;
}

int GuiSystemMenu::roomForRows() const {
    return SCREEN_HEIGHT - 2 * PanelMargin - HeaderHeight - StripHeight - FooterHeight;
}

int GuiSystemMenu::visibleRowCount() const {
    int used = 0;
    int count = 0;
    for (int i = firstVisible; i < static_cast<int>(rows.size()); i++) {
        if (used + rowHeight(rows[i]) > roomForRows())
            break;
        used += rowHeight(rows[i]);
        count++;
    }
    return max(1, count);
}

int GuiSystemMenu::visibleHeight() const {
    int used = 0;
    const int last = min(static_cast<int>(rows.size()), firstVisible + visibleRowCount());
    for (int i = firstVisible; i < last; i++)
        used += rowHeight(rows[i]);
    return used;
}

//*******************************
// GuiSystemMenu::keepSelectedVisible
//*******************************
// scrolled up to the selected item, its heading comes along; scrolled down, the rows above go one at a time
void GuiSystemMenu::keepSelectedVisible() {
    if (selected < firstVisible) {
        firstVisible = selected;
        if (firstVisible > 0 && rows[firstVisible - 1].heading)
            firstVisible--;
    }
    while (selected >= firstVisible + visibleRowCount() && firstVisible < selected)
        firstVisible++;
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

    const int rowsHeight = visibleHeight();
    const int panelHeight = HeaderHeight + rowsHeight + StripHeight + FooterHeight;
    ableem::Rect panel{(SCREEN_WIDTH - PanelWidth) / 2, (SCREEN_HEIGHT - panelHeight) / 2, PanelWidth, panelHeight};
    style.sheet(renderer, panel);

    // every text on this screen gets the launcher's halo, like the launcher's own
    const TextRenderer::Shadow classicShadow = gui->text().shadow();
    TextRenderer::Shadow shadow;
    shadow.enabled = style.textShadow;
    gui->text().setShadow(shadow);

    Fonts &fonts = gui->assets().themeFonts;
    ableem::Font &titleFont = fonts.atSize(FONT_MED, kind == Kind::Quick ? QuickTitleSize : SystemTitleSize);
    ableem::Font &headingFont = fonts.atSize(FONT_BOLD, HeadingSize);
    ableem::Font &noteFont = fonts.atSize(FONT_BOLD, NoteSize);
    const int rightEdge = panel.x + panel.w - TextX;

    int rowY = style.header(*gui, panel, kind == Kind::Quick ? _("Quick menu") : _("System"));
    const int rowsTop = rowY;
    const int last = min(static_cast<int>(rows.size()), firstVisible + visibleRowCount());
    for (int i = firstVisible; i < last; i++) {
        const Row &row = rows[i];
        const int h = rowHeight(row);
        if (row.heading) {
            style.label(renderer, ableem::Rect(panel.x + 1, rowY, panel.w - 2, h));
            gui->text().renderText_WithColor(headingFont, row.title, panel.x + TextX,
                                             rowY + (h - headingFont.lineHeight()) / 2, style.secondary, XALIGN_LEFT);
        } else {
            if (i == selected)
                style.selection(renderer, ableem::Rect(panel.x + 1, rowY, panel.w - 2, h));
            gui->text().renderText_WithColor(titleFont, row.title, panel.x + TextX,
                                             rowY + (h - titleFont.lineHeight()) / 2,
                                             i == selected ? style.text : style.secondary, XALIGN_LEFT);
            if (!row.note.empty()) // XALIGN_RIGHT takes the margin from the screen's right edge
                gui->text().renderText_WithColor(noteFont, row.note, SCREEN_WIDTH - rightEdge,
                                                 rowY + (h - noteFont.lineHeight()) / 2, style.hint, XALIGN_RIGHT);
        }
        rowY += h;
    }

    // scroll markers: a small triangle at the top or bottom edge of the rows when more are that way
    const int markerX = panel.x + panel.w - RowInset;
    if (firstVisible > 0)
        style.scrollMarker(renderer, markerX, rowsTop - 4, -1);
    if (last < static_cast<int>(rows.size()))
        style.scrollMarker(renderer, markerX, rowsTop + rowsHeight + 2, 1);

    // the strip: the selected item's description, over a rule
    const int stripY = rowsTop + rowsHeight;
    style.rule(renderer, panel, stripY);
    if (selected >= 0 && selected < static_cast<int>(rows.size())) {
        ableem::Font &stripFont = fonts.atSize(FONT_BOLD, StripSize);
        gui->text().renderText_WithColor(stripFont, rows[selected].description, panel.x + TextX,
                                         stripY + (StripHeight - stripFont.lineHeight()) / 2, style.secondary,
                                         XALIGN_LEFT);
    }

    // the footer: the launcher's own button hints
    style.footer(*gui, ableem::Rect(panel.x, panel.y + panel.h - FooterHeight, panel.w, FooterHeight),
                 {{{"X"}, _("Select")}, {{"O"}, _("Back")}}, "", false);

    gui->text().setShadow(classicShadow);
    renderer.present();
}

//*******************************
// GuiSystemMenu::moveSelection
//*******************************
// the next item that way, past the headings, wrapping round at either end
void GuiSystemMenu::moveSelection(int step) {
    const int count = static_cast<int>(rows.size());
    int next = selected;
    for (int tries = 0; tries < count; tries++) {
        next = (next + step + count) % count;
        if (!rows[next].heading)
            break;
    }
    selected = next;
    keepSelectedVisible();
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
                    result = rows[selected].action;
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
    ableem::DebugDriver::setItems({});
}
