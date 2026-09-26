#include "evoui_extensions.h"
#include "core/services/environment.h"
#include "gui/gui.h"

#include <algorithm>

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
}

//*******************************
// GuiExtensions::rowHeight / visibleRows / bodyHeight
//*******************************
int GuiExtensions::rowHeight(int row) const {
    return rows[row] == HeadingRow ? HeadingHeight : RowHeight;
}

int GuiExtensions::visibleRows() const {
    const int roomForRows = SCREEN_HEIGHT - 2 * PanelMargin - HeaderHeight - FooterHeight;
    int used = 0, shown = 0;
    for (int i = firstVisible; i < count() && used + rowHeight(i) <= roomForRows; i++, shown++)
        used += rowHeight(i);
    return max(1, shown);
}

// the whole list when it fits, else as much room as there is - the panel keeps its height while it scrolls
int GuiExtensions::bodyHeight() const {
    const int roomForRows = SCREEN_HEIGHT - 2 * PanelMargin - HeaderHeight - FooterHeight;
    int all = 0;
    for (int i = 0; i < count(); i++)
        all += rowHeight(i);
    return min(all, roomForRows);
}

//*******************************
// GuiExtensions::render
//*******************************
void GuiExtensions::render() {
    if (background.valid())
        renderer.copy(background, nullptr, nullptr);
    else
        gui->renderBackground();
    style.dim(renderer);

    const bool empty = count() == 0;
    const int shown = empty ? 0 : visibleRows();
    const int body = empty ? EmptyHeight : bodyHeight();
    const int panelHeight = HeaderHeight + body + FooterHeight;
    ableem::Rect panel{(SCREEN_WIDTH - PanelWidth) / 2, (SCREEN_HEIGHT - panelHeight) / 2, PanelWidth, panelHeight};
    style.sheet(renderer, panel);

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
            style.label(renderer, band);
            gui->text().renderText_WithColor(fonts[FONT_15_BOLD], _("Third-party extensions"), panel.x + RowInset + 8,
                                             rowY + (HeadingHeight - fonts[FONT_15_BOLD].lineHeight()) / 2,
                                             style.secondary, XALIGN_LEFT);
            rowY += HeadingHeight;
            continue;
        }
        const ExtensionInfo &e = extensionAt(i);
        const ableem::Rect row(panel.x + 1, rowY, panel.w - 2, RowHeight);
        if (i == selected)
            style.selection(renderer, row);
        const int textX = panel.x + RowInset + 8 + IconSize + 16;
        const ableem::Texture &icon = icons[rows[i]];
        if (icon.valid()) {
            ableem::Rect dst(panel.x + RowInset + 8, rowY + (RowHeight - IconSize) / 2, IconSize, IconSize);
            renderer.copy(icon, nullptr, &dst);
        }
        const string reason = reasonFor(e, networkUp);
        const string title = e.version.empty() ? e.title : e.title + "  " + e.version;
        gui->text().renderText_WithColor(fonts[FONT_22_MED], title, textX, rowY + 11,
                                         i == selected ? style.text : style.secondary, XALIGN_LEFT);
        gui->text().renderText_WithColor(fonts[FONT_15_BOLD], reason.empty() ? e.description : reason, textX, rowY + 41,
                                         style.secondary, XALIGN_LEFT);
        if (!reason.empty())
            style.disabled(renderer, row);
        rowY += RowHeight;
    }

    const int markerX = panel.x + panel.w - RowInset;
    if (firstVisible > 0)
        style.scrollMarker(renderer, markerX, panel.y + HeaderHeight - 4, -1);
    if (!empty && firstVisible + shown < count())
        style.scrollMarker(renderer, markerX, panel.y + HeaderHeight + body + 2, 1);

    vector<PanelStyle::HintItem> hints;
    if (!empty) {
        hints.push_back({{"X"}, _("Run")});
        hints.push_back({{"O"}, _("Back")});
        if (!lockedOn(extensionAt(selected)))
            hints.push_back({{"T"}, extensionAt(selected).disabled ? _("Enable") : _("Disable")});
    } else {
        hints.push_back({{"O"}, _("Back")});
    }
    style.footer(*gui, ableem::Rect(panel.x, panel.y + panel.h - FooterHeight, panel.w, FooterHeight), hints, "",
                 false);

    gui->text().setShadow(classicShadow);
    renderer.present();
}

//*******************************
// GuiExtensions::moveSelection
//*******************************
void GuiExtensions::moveSelection(int step) {
    if (count() == 0)
        return;
    if (step == 1 || step == -1)
        selected = (selected + step + count()) % count(); // a row at a time wraps
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
}

//*******************************
// GuiExtensions::loop
//*******************************
void GuiExtensions::loop() {
    menuVisible = true;
    while (menuVisible) {
        render();
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
                break;
            case Event::Type::ButtonDown:
                if (e.button == Button::Cross && count() > 0) {
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
