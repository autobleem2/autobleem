//
// GuiRaCores: the Quick menu's "RetroArch cores" window - see the header.
//
#include "gui/menus/gui_ra_cores_menu.h"
#include "gui/gui.h"
#include "../../app.h"

#include <ableem/ui/debug_driver.h>

#include <algorithm>
#include <typeinfo>

using namespace std;

//*******************************
// GuiRaCores::valueText
//*******************************
string GuiRaCores::valueText(const RACorePlatform &row, int choice) {
    const string name = row.cores[choice]->shortName();
    return name + " " + (choice == 0 ? _("(default)") : _("(changed)"));
}

//*******************************
// GuiRaCores::init
//*******************************
void GuiRaCores::init() {
    rows_ = app.retroArch().corePlatforms();
    start_.clear();
    for (const RACorePlatform &row : rows_)
        start_.push_back(row.current);
    selected_ = 0;
    firstVisible_ = 0;
}

//*******************************
// GuiRaCores::visibleRows
//*******************************
int GuiRaCores::visibleRows() const {
    shared_ptr<Gui> gui(Gui::getInstance());
    return max(1, gui->classicRowsThatFit(gui->assets().themeFont) - 1); // the heading takes a line
}

//*******************************
// GuiRaCores::publish
//*******************************
// the DebugDriver's rows: the heading band, then one row per system; the cursor's index among them
void GuiRaCores::publish() const {
    if (!menuVisible || !ableem::DebugDriver::active())
        return;
    vector<string> names{"#" + _("RetroArch cores")};
    for (const RACorePlatform &row : rows_)
        names.push_back(row.database);
    ableem::DebugDriver::publish(typeid(*this).name(), names, rows_.empty() ? -1 : selected_ + 1);
}

//*******************************
// GuiRaCores::draw
//*******************************
void GuiRaCores::draw() {
    shared_ptr<Gui> gui(Gui::getInstance());
    gui->renderBackground();
    gui->renderTextBar();
    int yoffset = gui->renderHeader(_("RetroArch cores"));
    publish();

    const ableem::Rect content = gui->classicContent();
    const int right = content.x + content.w - PanelStyle::RowInset;
    const ableem::Font &font = gui->assets().themeFont;
    const int fit = visibleRows();
    const int total = static_cast<int>(rows_.size());
    if (selected_ < firstVisible_)
        firstVisible_ = selected_;
    if (selected_ >= firstVisible_ + fit)
        firstVisible_ = selected_ - fit + 1;
    firstVisible_ = max(0, min(firstVisible_, max(0, total - fit)));

    // a theme's selection frame goes under the heading and the rows' text, so it comes first (G4d)
    const bool framed = gui->text().selectionFramed(gui->uiContext());
    const int selectedLine = selected_ - firstVisible_ + 1;
    if (framed && total > 0)
        gui->text().renderSelectionBox(gui->uiContext(), selectedLine, yoffset, 0, ableem::Font(), right);
    gui->text().renderLabelBox(gui->uiContext(), 0, yoffset, right);
    {
        TextRenderer::RowRoleScope role(gui->text(), TextRenderer::RowRole::Heading);
        gui->text().renderTextLine(_("Core that plays each system"), 0, yoffset, XALIGN_LEFT);
    }
    if (!framed && total > 0)
        gui->text().renderSelectionBox(gui->uiContext(), selectedLine, yoffset, 0, ableem::Font(), right);

    for (int i = firstVisible_, line = 1; i < total && line <= fit; i++, line++) {
        const RACorePlatform &row = rows_[i];
        TextRenderer::RowRoleScope role(gui->text(),
                                        i == selected_ ? TextRenderer::RowRole::Selected : TextRenderer::RowRole::Row);
        const string label = row.database;
        const string value = valueText(row, row.current);
        // the value sits at the right edge: what is left of the row after the label and a gap is its room
        const int room = right - content.x - gui->text().textWidth(font, label) - 3 * PanelStyle::RowInset;
        gui->text().renderTextLine(label, line, yoffset, XALIGN_LEFT);
        gui->text().renderRowValue(gui->text().elide(font, value, max(room, 0)), line, yoffset, right);
    }
    gui->renderScrollMarkers(firstVisible_ > 0, firstVisible_ + fit < total);

    gui->renderStatus("|@L1/R1| " + _("First/last") + "   |@L2/R2| " + _("Page") + "   |@Left+Right| " + _("Choose") +
                      "   |@O| " + _("Back") + "|");
}

//*******************************
// GuiRaCores::change / select
//*******************************
void GuiRaCores::change(int step) {
    if (rows_.empty())
        return;
    RACorePlatform &row = rows_[selected_];
    const int count = static_cast<int>(row.cores.size());
    row.current = (row.current + step + count) % count;
}

void GuiRaCores::select(int row) {
    if (rows_.empty())
        return;
    selected_ = max(0, min(row, static_cast<int>(rows_.size()) - 1));
}

//*******************************
// GuiRaCores::save
//*******************************
// the systems whose core is not the one the window opened with: the user's file, the playlists, the games
void GuiRaCores::save() {
    vector<pair<string, ableem::CoreInfoPtr>> choices;
    for (size_t i = 0; i < rows_.size(); i++) {
        if (rows_[i].current != start_[i])
            choices.emplace_back(rows_[i].database, rows_[i].cores[rows_[i].current]);
    }
    if (choices.empty())
        return;
    changed = app.retroArch().saveCorePicks(choices) > 0;
}

//*******************************
// GuiRaCores::loop
//*******************************
void GuiRaCores::loop() {
    shared_ptr<Gui> gui(Gui::getInstance());
    menuVisible = true;
    while (menuVisible) {
        // nothing animates here: sleep until a press, and redraw 4 times a second meanwhile
        if (!gui->input().waitForEvent(250))
            render();
        Event e;
        while (gui->input().poll(e)) {
            if (e.type == Event::Type::Quit)
                menuVisible = false;
            switch (e.type) {
            case Event::Type::DpadDown:
            case Event::Type::DpadUp:
                if (gui->input().dpadDown() || gui->input().dpadUp()) {
                    app.audio().cursor.play();
                    select(selected_ + (gui->input().dpadDown() ? 1 : -1));
                    render();
                } else if (gui->input().dpadRight() || gui->input().dpadLeft()) {
                    app.audio().cursor.play();
                    change(gui->input().dpadRight() ? 1 : -1);
                    render();
                }
                break;
            case Event::Type::ButtonDown:
                if (e.button == Button::L1 || e.button == Button::R1) {
                    app.audio().cursor.play();
                    select(e.button == Button::R1 ? static_cast<int>(rows_.size()) - 1 : 0);
                    render();
                } else if (e.button == Button::L2 || e.button == Button::R2) {
                    app.audio().cursor.play();
                    select(selected_ + (e.button == Button::R2 ? 1 : -1) * visibleRows());
                    render();
                } else if (e.button == Button::Circle) {
                    app.audio().cancel.play();
                    menuVisible = false;
                }
                break;
            default:
                break;
            }
        }
    }
    save();
}
