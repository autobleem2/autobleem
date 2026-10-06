#include "evoui_extensions.h"
#include "core/services/environment.h"
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
const int RowHeight = 72; // a 56 px icon with room around it
const int RowInset = PanelStyle::RowInset;
const int IconSize = 56;
const int EmptyHeight = 110;  // the panel's body when nothing is installed
const int HeadingHeight = 44; // the heading between ours and the third-party ones
// ours: what the AutoBleem team ships (the Store, PSC-Bios, the SDK's sample), by folder name - listed
// first. A fixed list, not the Author line, which any extension could claim.
const char *const OurExtensions[] = {"store", "pscbios", "hello"};
// the console's hardware tool (WiFi, time zone, pads): on the console it cannot be switched off here
const char *const PscBiosExtension = "pscbios";

// An extension's Description= is its own English line. The two the team ships are translated with the launcher's
// language files (BUG-47) - listed here as literals so lang_tools finds their keys; any other extension's line is
// shown as written.
string descriptionOf(const ExtensionInfo &extension) {
    if (extension.description == "Wi-Fi, time zone, Bluetooth and gamepads")
        return _("Wi-Fi, time zone, Bluetooth and gamepads");
    if (extension.description == "Download apps and games")
        return _("Download apps and games");
    return extension.description;
}
} // namespace

const int GuiExtensions::HeadingRow; // odr-used by push_back (C++14)

//*******************************
// GuiExtensions::lockedOn
//*******************************
// PSC-Bios on the console is where the WiFi, the pads and the time are set up - disabling it would leave
// no way back to them, so Triangle is refused (and its hint not shown). One the crash guard disabled can
// still be switched back on.
bool GuiExtensions::lockedOn(const ExtensionInfo &extension) {
    return extension.name == PscBiosExtension && string(Env::platformName()) == "psc" && !extension.disabled;
}

//*******************************
// GuiExtensions::isOurs
//*******************************
bool GuiExtensions::isOurs(const ExtensionInfo &extension) {
    for (const char *name : OurExtensions)
        if (extension.name == name)
            return true;
    return false;
}

//*******************************
// GuiExtensions::reasonFor
//*******************************
string GuiExtensions::reasonFor(const ExtensionInfo &extension, bool networkUp) {
    if (!extension.builtForThisSystem())
        return _("Not available for this system");
    if (extension.disabled)
        return _("Disabled");
    if (extension.loadProblem == "built for a different AutoBleem")
        return _("Built for a different AutoBleem");
    if (!extension.loadProblem.empty())
        return _("It could not be loaded");
    if (extension.network == ExtensionNetwork::Required && !networkUp)
        return _("Needs a network connection");
    return "";
}

//*******************************
// GuiExtensions::init
//*******************************
void GuiExtensions::init() {
    style = gui->panelStyle();
    icons.clear();
    for (const ExtensionInfo &e : catalog.extensions())
        icons.push_back(e.icon.empty() ? ableem::Texture() : ableem::Texture::loadFile(renderer, e.icon));
    // ours first, then the third-party ones under a heading (only when there are both), each in the
    // catalog's order (by title)
    rows.clear();
    vector<int> theirs;
    const auto &list = catalog.extensions();
    for (int i = 0; i < static_cast<int>(list.size()); i++)
        (isOurs(list[i]) ? rows : theirs).push_back(i);
    if (!rows.empty() && !theirs.empty())
        rows.push_back(HeadingRow);
    rows.insert(rows.end(), theirs.begin(), theirs.end());
    selected = 0;
    firstVisible = 0;
    chosen.clear();
    // opened at one (Network & Controllers greyed: the extension providing it): the cursor on its row,
    // scrolled into view
    for (int i = 0; i < count(); i++) {
        if (rows[i] != HeadingRow && extensionAt(i).name == select) {
            selected = i;
            while (selected >= firstVisible + visibleRows())
                firstVisible++;
            break;
        }
    }
}

//*******************************
// GuiExtensions::rowHeight / visibleRows / bodyHeight
//*******************************
int GuiExtensions::rowHeight(int row) const {
    return rows[row] == HeadingRow ? HeadingHeight : RowHeight;
}

int GuiExtensions::visibleRows() const {
    const int roomForRows = gui->renderer().height() - 2 * PanelMargin - HeaderHeight - FooterHeight;
    int used = 0, shown = 0;
    for (int i = firstVisible; i < count() && used + rowHeight(i) <= roomForRows; i++, shown++)
        used += rowHeight(i);
    return max(1, shown);
}

// the whole list when it fits, else as much room as there is - the panel keeps its height while it scrolls
int GuiExtensions::bodyHeight() const {
    const int roomForRows = gui->renderer().height() - 2 * PanelMargin - HeaderHeight - FooterHeight;
    int all = 0;
    for (int i = 0; i < count(); i++)
        all += rowHeight(i);
    return min(all, roomForRows);
}

//*******************************
// GuiExtensions::publishItems
//*******************************
// the DebugDriver's `items`/`selected`: the rows as drawn, the heading with a leading '#'; from the frame and from
// every move, so the driver's cursor never lags the real one
void GuiExtensions::publishItems() const {
    if (!menuVisible || !ableem::DebugDriver::active())
        return;
    vector<string> names;
    for (int i = 0; i < count(); i++)
        names.push_back(rows[i] == HeadingRow ? "#" + _("Third-party extensions") : extensionAt(i).title);
    ableem::DebugDriver::publish(typeid(*this).name(), names, count() == 0 ? -1 : selected);
}

//*******************************
// GuiExtensions::draw
//*******************************
void GuiExtensions::draw() {
    publishItems();
    gui->renderBackground();
    style.dim(gui->uiContext());

    const bool empty = count() == 0;
    const int shown = empty ? 0 : visibleRows();
    const int body = empty ? EmptyHeight : bodyHeight();
    const int panelHeight = HeaderHeight + body + FooterHeight;
    const int panelWidth = min(PanelWidth, gui->renderer().width() - 2 * PanelMargin); // a 4:3 canvas is narrower
    ableem::Rect panel{(gui->renderer().width() - panelWidth) / 2, (gui->renderer().height() - panelHeight) / 2,
                       panelWidth, panelHeight};
    style.sheet(gui->uiContext(), panel);

    const TextRenderer::Shadow classicShadow = gui->text().shadow();
    TextRenderer::Shadow shadow;
    shadow.enabled = style.textShadow;
    gui->text().setShadow(shadow);

    Fonts &fonts = gui->assets().themeFonts;
    int rowY = style.header(*gui, panel, _("Extensions"));
    if (empty) {
        gui->text().renderText_WithColor(fonts[FONT_22_MED], _("No extensions installed"), panel.x + RowInset + 8,
                                         rowY + 20, style.text, XALIGN_LEFT);
        gui->text().renderText_WithColor(fonts[FONT_15_BOLD], _("Unpack an extension into the Extensions folder"),
                                         panel.x + RowInset + 8, rowY + 56, style.secondary, XALIGN_LEFT);
    }
    for (int i = firstVisible; i < firstVisible + shown && i < count(); i++) {
        if (rows[i] == HeadingRow) {
            const ableem::Rect band(panel.x + 1, rowY, panel.w - 2, HeadingHeight);
            style.label(gui->uiContext(), band);
            gui->text().renderText_WithColor(fonts[FONT_15_BOLD], _("Third-party extensions"), panel.x + RowInset + 8,
                                             rowY + (HeadingHeight - fonts[FONT_15_BOLD].lineHeight()) / 2,
                                             style.heading, XALIGN_LEFT);
            rowY += HeadingHeight;
            continue;
        }
        const ExtensionInfo &e = extensionAt(i);
        const ableem::Rect row(panel.x + 1, rowY, panel.w - 2, RowHeight);
        if (i == selected)
            style.selection(gui->uiContext(), row);
        ableem::Texture icon = icons[rows[i]];
        // an extension that ships none gets the theme's "extension" icon when it has one (no built-in), asked at
        // draw time; with neither, the title starts where the icon would be (no empty column)
        if (!icon.valid() && e.icon.empty())
            icon = gui->uiContext().icon("extension");
        const int textX = panel.x + RowInset + 8 + (icon.valid() ? IconSize + 16 : 0);
        if (icon.valid()) {
            const ableem::Size iconSize = icon.size();
            int w = IconSize, h = IconSize;
            if (iconSize.w > 0 && iconSize.h > 0) {
                if (iconSize.w >= iconSize.h) {
                    w = IconSize;
                    h = max(1, IconSize * iconSize.h / iconSize.w);
                } else {
                    h = IconSize;
                    w = max(1, IconSize * iconSize.w / iconSize.h);
                }
            }
            ableem::Rect dst(panel.x + RowInset + 8 + (IconSize - w) / 2, rowY + (RowHeight - h) / 2, w, h);
            renderer.copy(icon, nullptr, &dst);
        }
        const string reason = reasonFor(e, networkUp);
        const string title = e.version.empty() ? e.title : e.title + "  " + e.version;
        // a row that cannot run is under the theme's `disabled` role (G5t: its veil, its text in `description`)
        gui->text().renderText_WithColor(fonts[FONT_22_MED], title, textX, rowY + 11,
                                         reason.empty()
                                             ? style.rowColor(i == selected)
                                             : style.disabledColor(gui->uiContext(), style.rowColor(i == selected)),
                                         XALIGN_LEFT);
        gui->text().renderText_WithColor(fonts[FONT_15_BOLD], reason.empty() ? descriptionOf(e) : reason, textX,
                                         rowY + 41, style.description, XALIGN_LEFT);
        if (!reason.empty())
            style.disabled(gui->uiContext(), row);
        rowY += RowHeight;
    }

    const int markerX = panel.x + panel.w - RowInset;
    if (firstVisible > 0)
        style.scrollMarker(gui->uiContext(), markerX, panel.y + HeaderHeight - 4, -1);
    if (!empty && firstVisible + shown < count())
        style.scrollMarker(gui->uiContext(), markerX, panel.y + HeaderHeight + body + 2, 1);

    vector<PanelStyle::HintItem> hints;
    if (!empty) {
        hints.push_back({{"X"}, _("Run")});
        hints.push_back({{"O"}, _("Back")});
        if (!lockedOn(extensionAt(selected)))
            hints.push_back({{"T"}, extensionAt(selected).disabled ? _("Enable") : _("Disable")});
        hints.push_back({{"L1", "R1"}, _("First/last")});
        hints.push_back({{"L2", "R2"}, _("Page")});
    } else {
        hints.push_back({{"O"}, _("Back")});
    }
    style.footer(*gui, ableem::Rect(panel.x, panel.y + panel.h - FooterHeight, panel.w, FooterHeight), hints, "",
                 false);

    gui->text().setShadow(classicShadow);
}

//*******************************
// GuiExtensions::moveSelection
//*******************************
void GuiExtensions::moveSelection(int step, bool repeat) {
    if (count() == 0)
        return;
    if (step == 1 || step == -1) // a row at a time: a press wraps, a held key's repeat stops at the end
        selected = abgui::stepIndex(selected, step, count(), repeat, [&](int i) { return rows[i] == HeadingRow; });
    else
        selected = max(0, min(count() - 1, selected + step)); // a page stops at the ends
    if (rows[selected] == HeadingRow) // never on the heading: on past it (it is never first or last)
        selected += step > 0 ? 1 : -1;
    if (selected < firstVisible)
        firstVisible = selected;
    while (selected >= firstVisible + visibleRows())
        firstVisible++;
    if (firstVisible == selected && selected > 0 && rows[selected - 1] == HeadingRow)
        firstVisible--; // the first third-party row shows its heading above it
    publishItems();
}

//*******************************
// GuiExtensions::loop
//*******************************
void GuiExtensions::loop() {
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
                chosen.clear();
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
                    const ExtensionInfo &picked = extensionAt(selected);
                    if (reasonFor(picked, networkUp).empty()) {
                        app.audio().cursor.play();
                        chosen = picked.name;
                        menuVisible = false;
                    } else {
                        app.audio().cancel.play(); // greyed: its reason is on the row
                    }
                } else if (e.button == Button::Triangle && count() > 0) {
                    const ExtensionInfo &picked = extensionAt(selected);
                    if (lockedOn(picked)) {
                        app.audio().cancel.play();
                    } else {
                        app.audio().cursor.play();
                        catalog.setDisabled(picked.name, !picked.disabled);
                    }
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
                    chosen.clear();
                    menuVisible = false;
                }
                break;
            default:
                break;
            }
        }
    }
}
