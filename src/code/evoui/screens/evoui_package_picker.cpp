#include "evoui_package_picker.h"
#include "package_picker_logic.h"
#include "gui/gui.h"

#include <ableem/ui/debug_driver.h>

#include <algorithm>
#include <typeinfo>

using namespace std;

namespace {
const int PanelWidth = 860;
const int PanelMargin = PanelStyle::Margin;
const int HeaderHeight = PanelStyle::HeaderHeight;
const int FooterHeight = PanelStyle::FooterHeight;
const int RowInset = PanelStyle::RowInset;
const int RowHeight = 68;     // the title and the smaller line under it
const int HeadingHeight = 40; // "Choose game data", pinned under the header
const int TextInset = 8;      // the text from the row's edge, past RowInset
} // namespace

//*******************************
// packageKindLabel / packageSourceLabel / packageLicenceLabel
//*******************************
// literal _() calls at each branch so tools/lang_tools.py extract finds the keys (it reads literals only)
string packageKindLabel(const string &kind) {
    if (kind == "doom-iwad")
        return _("Doom data");
    if (kind == "heretic-iwad")
        return _("Heretic data");
    if (kind == "hexen-iwad")
        return _("Hexen data");
    if (kind == "strife-iwad")
        return _("Strife data");
    if (kind == "quake-id1")
        return _("Quake data");
    if (kind == "q3-openarena")
        return _("OpenArena data");
    if (kind == "q3-baseq3")
        return _("Quake III Arena data");
    if (kind == "theme-hospital")
        return _("Theme Hospital data");
    if (kind == "dos-game")
        return _("DOS game");
    if (kind == "duke3d-grp")
        return _("Duke Nukem 3D data");
    if (kind == "sw-grp")
        return _("Shadow Warrior data");
    return kind; // a kind of the player's own table: its id
}

string packageSourceLabel(const string &source, bool inApp) {
    if (inApp)
        return _("In this App");
    if (source == "store")
        return _("Store");
    if (source == "mod")
        return _("Mod");
    return _("Your files");
}

// a table's licence is shown as written, but the two words every table uses are translated
string packageLicenceLabel(const string &licence) {
    if (licence == "Shareware")
        return _("Shareware");
    if (licence == "Free")
        return _("Free");
    return licence;
}

//*******************************
// GuiPackagePicker::init
//*******************************
void GuiPackagePicker::init() {
    style = gui->panelStyle();
    chosen = -1;
    selected = packagepicker::startRow(entries, lastId);
    firstVisible = 0;
    while (selected >= firstVisible + visibleRows())
        firstVisible++;
}

//*******************************
// GuiPackagePicker::visibleRows / bodyHeight
//*******************************
int GuiPackagePicker::visibleRows() const {
    const int room = gui->renderer().height() - 2 * PanelMargin - HeaderHeight - HeadingHeight - FooterHeight;
    return max(1, room / RowHeight);
}

// the whole list when it fits, else as many whole rows as there are room for
int GuiPackagePicker::bodyHeight() const {
    return min(count(), visibleRows()) * RowHeight;
}

//*******************************
// GuiPackagePicker::publishItems
//*******************************
// the DebugDriver's `items`/`selected`: the rows as drawn (the first line), from the frame and from every move
void GuiPackagePicker::publishItems() const {
    if (!menuVisible || !ableem::DebugDriver::active())
        return;
    vector<string> names;
    for (const PackageEntry &entry : entries)
        names.push_back(packagepicker::firstLine(entry));
    ableem::DebugDriver::publish(typeid(*this).name(), names, count() == 0 ? -1 : selected);
}

//*******************************
// GuiPackagePicker::draw
//*******************************
void GuiPackagePicker::draw() {
    publishItems();
    gui->renderBackground();
    style.dim(gui->uiContext());

    const int shown = min(count(), visibleRows());
    const int body = bodyHeight();
    const int panelHeight = HeaderHeight + HeadingHeight + body + FooterHeight;
    const int panelWidth = min(PanelWidth, gui->renderer().width() - 2 * PanelMargin); // a 4:3 canvas is narrower
    ableem::Rect panel{(gui->renderer().width() - panelWidth) / 2, (gui->renderer().height() - panelHeight) / 2,
                       panelWidth, panelHeight};
    style.sheet(gui->uiContext(), panel);

    const TextRenderer::Shadow classicShadow = gui->text().shadow();
    TextRenderer::Shadow shadow;
    shadow.enabled = style.textShadow;
    gui->text().setShadow(shadow);

    Fonts &fonts = gui->assets().themeFonts;
    int rowY = style.header(*gui, panel, appTitle);

    const ableem::Rect band(panel.x + 1, rowY, panel.w - 2, HeadingHeight);
    style.label(gui->uiContext(), band);
    gui->text().renderText_WithColor(fonts[FONT_15_BOLD], _("Choose game data"), panel.x + RowInset + TextInset,
                                     rowY + (HeadingHeight - fonts[FONT_15_BOLD].lineHeight()) / 2, style.heading,
                                     XALIGN_LEFT);
    rowY += HeadingHeight;

    const int textX = panel.x + RowInset + TextInset;
    const int textWidth = panel.w - 2 * (RowInset + TextInset);
    const packagepicker::Measure measure = [&](const string &text) {
        return gui->text().textWidth(fonts[FONT_15_BOLD], text);
    };
    for (int i = firstVisible; i < firstVisible + shown && i < count(); i++) {
        const PackageEntry &entry = entries[i];
        const ableem::Rect row(panel.x + 1, rowY, panel.w - 2, RowHeight);
        if (i == selected)
            style.selection(gui->uiContext(), row);
        const packagepicker::Measure titleMeasure = [&](const string &text) {
            return gui->text().textWidth(fonts[FONT_22_MED], text);
        };
        gui->text().renderText_WithColor(fonts[FONT_22_MED],
                                         packagepicker::elide(packagepicker::firstLine(entry), textWidth, titleMeasure),
                                         textX, rowY + 8, style.rowColor(i == selected), XALIGN_LEFT);
        gui->text().renderText_WithColor(
            fonts[FONT_15_BOLD],
            packagepicker::secondLine(entry.packageTitle, packageSourceLabel(entry.source, entry.inApp),
                                      packageKindLabel(entry.game.kind), textWidth, measure),
            textX, rowY + 40, style.description, XALIGN_LEFT);
        rowY += RowHeight;
    }

    const int markerX = panel.x + panel.w - RowInset;
    if (firstVisible > 0)
        style.scrollMarker(gui->uiContext(), markerX, panel.y + HeaderHeight + HeadingHeight - 4, -1);
    if (firstVisible + shown < count())
        style.scrollMarker(gui->uiContext(), markerX, panel.y + HeaderHeight + HeadingHeight + body + 2, 1);

    vector<PanelStyle::HintItem> hints;
    hints.push_back({{"X"}, _("Start")});
    hints.push_back({{"O"}, _("Back")});
    if (count() > shown) {
        hints.push_back({{"L1", "R1"}, _("First/last")});
        hints.push_back({{"L2", "R2"}, _("Page")});
    }
    style.footer(*gui, ableem::Rect(panel.x, panel.y + panel.h - FooterHeight, panel.w, FooterHeight), hints, "",
                 false);

    gui->text().setShadow(classicShadow);
}

//*******************************
// GuiPackagePicker::moveSelection
//*******************************
void GuiPackagePicker::moveSelection(int step, bool repeat) {
    if (count() == 0)
        return;
    if (step == 1 || step == -1) // a row at a time: a press wraps, a held key's repeat stops at the end
        selected = abgui::stepIndex(selected, step, count(), repeat);
    else
        selected = max(0, min(count() - 1, selected + step)); // a page stops at the ends
    if (selected < firstVisible)
        firstVisible = selected;
    while (selected >= firstVisible + visibleRows())
        firstVisible++;
    publishItems();
}

//*******************************
// GuiPackagePicker::loop
//*******************************
void GuiPackagePicker::loop() {
    menuVisible = true;
    gui->input().setFrameNeed(ableem::Input::FrameNeed::Idle); // nothing moves between presses
    while (menuVisible) {
        if (gui->input().frameDue())
            render();
        hold.tick(gui->input(), gui->platform().ticks(), [&](int dir, bool repeat) {
            const int before = selected;
            moveSelection(dir, repeat);
            if (selected != before) // a repeat at the end stays put, silently
                app.audio().cursor.play();
        });
        Event e;
        while (gui->input().poll(e)) {
            if (e.type == Event::Type::Quit) {
                chosen = -1;
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
                hold.track(gui->input(), gui->platform().ticks());
                break;
            case Event::Type::ButtonDown:
                if (e.button == Button::Cross && count() > 0) {
                    publishItems(); // the driver sees the row this press takes
                    app.audio().cursor.play();
                    chosen = selected;
                    menuVisible = false;
                } else if (e.button == Button::L1) {
                    app.audio().cursor.play();
                    moveSelection(-count());
                } else if (e.button == Button::R1) {
                    app.audio().cursor.play();
                    moveSelection(count());
                } else if (e.button == Button::L2) {
                    app.audio().cursor.play();
                    moveSelection(-visibleRows());
                } else if (e.button == Button::R2) {
                    app.audio().cursor.play();
                    moveSelection(visibleRows());
                } else if (e.button == Button::Circle) {
                    app.audio().cancel.play();
                    chosen = -1;
                    menuVisible = false;
                }
                break;
            default:
                break;
            }
        }
    }
}
