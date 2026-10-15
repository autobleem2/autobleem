#include "evoui_ra_manager.h"
#include "../../app.h"
#include "gui/gui.h"
#include "gui/screens/gui_confirm.h"

#include <ab_gui/panel.h>
#include <ab_gui/text_page.h>

#include <algorithm>

using namespace std;

namespace {
const int PanelWidth = 800;                     // at least - wider for a long line
const int TextInset = PanelStyle::RowInset + 8; // a line's x in the panel, and its right margin
const int RowHeight = PanelStyle::RowHeight;
const int RowInset = PanelStyle::RowInset;
const int LineHeight = 30;

// as wide as the widest of `texts` needs, from PanelWidth up to the screen's margins
int panelWidthFor(Gui &gui, const vector<pair<const ableem::Font *, string>> &texts) {
    int widest = 0;
    for (const auto &t : texts)
        widest = max(widest, gui.text().textWidth(*t.first, t.second));
    const int maxPanelWidth = gui.renderer().width() - 2 * PanelStyle::Margin;
    return min(maxPanelWidth, max(PanelWidth, widest + 2 * TextInset));
}

// the dimmed launcher's snapshot under the panel; returns the panel's rect
ableem::Rect drawPanel(Gui &gui, const PanelStyle &style, int width, int height) {
    gui.renderBackground();
    style.dim(gui.uiContext());
    ableem::Rect panel{(gui.renderer().width() - width) / 2, (gui.renderer().height() - height) / 2, width, height};
    style.sheet(gui.uiContext(), panel);
    return panel;
}

// the title of a phase: a literal _() per key, so tools/lang_tools.py extracts them; the key itself is never shown
string phaseTitle(const string &key) {
    if (key == "check-space")
        return _("Checking free space...");
    if (key == "download-retroarch")
        return _("Downloading RetroArch");
    if (key == "download-cores")
        return _("Downloading cores");
    if (key == "download-libs")
        return _("Downloading libraries");
    if (key == "download-apps")
        return _("Downloading apps");
    if (key == "download-assets")
        return _("Downloading assets");
    if (key == "download-bios")
        return _("Downloading BIOS files");
    if (key == "unpack")
        return _("Unpacking");
    if (key == "merge-config")
        return _("Merging the settings");
    if (key == "install-binary")
        return _("Installing RetroArch");
    if (key == "remove-files")
        return _("Removing files");
    return _("Loading ... Please Wait ..."); // a key this launcher does not know (a newer runner)
}
} // namespace

//*******************************
// GuiRaMessage
//*******************************
// abgui::Confirm::draw with the one hint
void GuiRaMessage::draw() {
    ctx.drawBackdrop();
    const abgui::Style style = ctx.style();
    const ableem::Font &font = ctx.font(abgui::FontRole::Row);
    const vector<abgui::HintItem> hints = {{{"X"}, _("OK")}};
    const int panelWidth = abgui::Panel::compactWidth(ctx, hints, "");
    const int width = min(textWidth(style), panelWidth - 2 * (style.rowInset + 8));
    const vector<string> rows = abgui::wrapText(label, width, [&font](const string &s) { return font.width(s); });
    const int textHeight = static_cast<int>(rows.size()) * font.lineHeight();
    const abgui::Panel panel(panelRect(style, textHeight, ctx.renderer().width(), ctx.renderer().height(), panelWidth),
                             style);
    panel.sheet(ctx);

    const bool programShadow = ctx.setTextShadow(style.textShadow);
    int y = panel.header(ctx, title.empty() ? _("RetroArch...") : title) + TextGapTop;
    const int x = panel.rect().x + style.rowInset + 8;
    for (const string &row : rows) {
        if (!row.empty())
            ctx.drawText(font, row, x, y, style.text);
        y += font.lineHeight();
    }
    style.footer(ctx, panel.footer(), hints, "", false);
    ctx.setTextShadow(programShadow);
}

// Cross and Circle, Enter and Escape: all say OK
void GuiRaMessage::onAction(const abgui::ActionEvent &action) {
    const Event &e = action.event;
    if (e.type == Event::Type::ButtonDown &&
        (action.action == abgui::Action::Confirm || action.action == abgui::Action::Back))
        answer(true);
    else if (e.type == Event::Type::KeyDown && (e.key == Key::Return || e.key == Key::Escape))
        answer(true);
}

void GuiRaMessage::onUnmapped(const Event &event) {
    if (event.type == Event::Type::KeyDown && (event.key == Key::Return || event.key == Key::Escape))
        answer(true);
}

//*******************************
// GuiRaManager
//*******************************
string GuiRaManager::summary(bool installed, const string &version, uint64_t bytes) {
    if (!installed)
        return _("Not installed");
    string text = _("Installed") + ": ";
    text += version.empty() ? "?" : version;
    return text + ", " + rajob::humanBytes(bytes);
}

string GuiRaManager::titleOf(rajob::RowAction action) const {
    switch (action) {
    case rajob::RowAction::Install:
        return _("Install");
    case rajob::RowAction::Update:
        return _("Update");
    case rajob::RowAction::Remove:
        return _("Remove");
    case rajob::RowAction::DeleteBios:
        return _("Also delete the BIOS files");
    }
    return "";
}

string GuiRaManager::descriptionOf(const rajob::Row &row) const {
    if (!row.enabled && (row.action == rajob::RowAction::Install || row.action == rajob::RowAction::Update)) {
        if (!networkUp)
            return _("Not connected");
        return row.action == rajob::RowAction::Install ? _("Already installed") : _("Not installed");
    }
    switch (row.action) {
    case rajob::RowAction::Install:
        return _("Download and install RetroArch");
    case rajob::RowAction::Update:
        return _("Get the newest RetroArch");
    case rajob::RowAction::Remove:
        return _("Delete RetroArch - games, saves and settings stay");
    case rajob::RowAction::DeleteBios:
        return _("Remove RetroArch and the BIOS files too");
    }
    return "";
}

void GuiRaManager::rebuild() {
    const rajob::RowAction chosen =
        selected < static_cast<int>(rows_.size()) ? rows_[selected].action : rajob::RowAction::Install;
    rows_ = rajob::panelRows(installed, networkUp, chosen);
    selected = min(selected, static_cast<int>(rows_.size()) - 1);
}

void GuiRaManager::init() {
    rows_.clear();
    selected = 0;
    rebuild();
    // the first row that can be used
    for (int i = 0; i < static_cast<int>(rows_.size()); i++)
        if (rows_[i].enabled) {
            selected = i;
            break;
        }
    rebuild();
    result = RaChoice::None;
    style = gui->panelStyle();
}

void GuiRaManager::draw() {
    const string head = summary(installed, version, bytes);
    const int headerHeight = PanelStyle::HeaderHeight + LineHeight + 12;
    const int panelHeight = headerHeight + 4 * RowHeight + PanelStyle::FooterHeight; // room for the BIOS row too
    Fonts &fonts = gui->assets().themeFonts;
    vector<pair<const ableem::Font *, string>> texts{{&fonts[FONT_22_MED], head}};
    for (const rajob::Row &row : rows_) {
        texts.emplace_back(&fonts[FONT_22_MED], titleOf(row.action));
        texts.emplace_back(&fonts[FONT_15_BOLD], descriptionOf(row));
    }
    ableem::Rect panel = drawPanel(*gui, style, panelWidthFor(*gui, texts), panelHeight);

    const TextRenderer::Shadow classicShadow = gui->text().shadow();
    TextRenderer::Shadow shadow;
    shadow.enabled = style.textShadow;
    gui->text().setShadow(shadow);

    gui->text().renderText_WithColor(fonts[FONT_28_BOLD], _("RetroArch..."), panel.x + RowInset, panel.y + 18,
                                     style.text, XALIGN_LEFT);
    gui->text().renderText_WithColor(fonts[FONT_22_MED],
                                     gui->text().elide(fonts[FONT_22_MED], head, panel.w - 2 * TextInset),
                                     panel.x + TextInset, panel.y + 66, style.description, XALIGN_LEFT);
    style.rule(gui->uiContext(), panel, panel.y + headerHeight - 8);

    int rowY = panel.y + headerHeight;
    for (int i = 0; i < static_cast<int>(rows_.size()); i++) {
        const rajob::Row &row = rows_[i];
        if (i == selected)
            style.selection(gui->uiContext(), ableem::Rect(panel.x + 1, rowY, panel.w - 2, RowHeight));
        const auto color = row.enabled ? style.rowColor(i == selected)
                                       : style.disabledColor(gui->uiContext(), style.rowColor(i == selected));
        gui->text().renderText_WithColor(fonts[FONT_22_MED], titleOf(row.action), panel.x + RowInset + 8, rowY + 7,
                                         color, XALIGN_LEFT);
        gui->text().renderText_WithColor(fonts[FONT_15_BOLD], descriptionOf(row), panel.x + RowInset + 8, rowY + 35,
                                         style.description, XALIGN_LEFT);
        if (!row.enabled)
            style.disabled(gui->uiContext(), ableem::Rect(panel.x + 1, rowY, panel.w - 2, RowHeight));
        rowY += RowHeight;
    }

    style.footer(*gui,
                 ableem::Rect(panel.x, panel.y + panel.h - PanelStyle::FooterHeight, panel.w, PanelStyle::FooterHeight),
                 {{{"X"}, _("Select")}, {{"O"}, _("Back")}}, "", false);
    gui->text().setShadow(classicShadow);
}

void GuiRaManager::loop() {
    menuVisible = true;
    gui->input().setFrameNeed(ableem::Input::FrameNeed::Idle); // nothing moves between presses
    while (menuVisible) {
        if (gui->input().frameDue())
            render();
        Event e;
        while (gui->input().poll(e)) {
            if (e.type == Event::Type::Quit) {
                result = RaChoice::None;
                menuVisible = false;
            }
            switch (e.type) {
            case Event::Type::DpadDown:
            case Event::Type::DpadUp: {
                const int count = static_cast<int>(rows_.size());
                if (gui->input().dpadUp()) {
                    app.audio().cursor.play();
                    selected = (selected + count - 1) % count;
                } else if (gui->input().dpadDown()) {
                    app.audio().cursor.play();
                    selected = (selected + 1) % count;
                }
                rebuild(); // the BIOS row comes and goes with Remove
                break;
            }
            case Event::Type::ButtonDown:
                if (e.button == Button::Cross) {
                    const rajob::Row &row = rows_[selected];
                    if (!row.enabled) {
                        app.audio().cancel.play();
                        break;
                    }
                    app.audio().cursor.play();
                    switch (row.action) {
                    case rajob::RowAction::Install:
                        result = RaChoice::Install;
                        break;
                    case rajob::RowAction::Update:
                        result = RaChoice::Update;
                        break;
                    case rajob::RowAction::Remove:
                        result = RaChoice::Remove;
                        break;
                    case rajob::RowAction::DeleteBios:
                        result = RaChoice::RemoveWithBios;
                        break;
                    }
                    menuVisible = false;
                } else if (e.button == Button::Circle) {
                    app.audio().cancel.play();
                    result = RaChoice::None;
                    menuVisible = false;
                }
                break;
            default:
                break;
            }
        }
    }
}

//*******************************
// GuiRaProgress
//*******************************
void GuiRaProgress::init() {
    style = gui->panelStyle();
    stopping = false;
    barPhase = 0;
    barPhaseStartMs = 0;
}

void GuiRaProgress::draw() {
    Fonts &fonts = gui->assets().themeFonts;
    const rajob::Progress &p = shown.progress;
    const string title = p.phase > 0 ? phaseTitle(p.key) : _("Loading ... Please Wait ...");
    string detail = rajob::phaseCounter(p);
    if (p.phase > 0 && p.total > 0 && p.key.compare(0, 8, "download") == 0)
        detail += "   " + rajob::humanBytes(p.done) + " / " + rajob::humanBytes(p.total);
    else if (p.phase > 0 && p.total > 0)
        detail += "   " + to_string(p.done) + " / " + to_string(p.total);
    if (stopping)
        detail = _("Stopping...");
    // the bar belongs to one phase: a new phase starts it again; an unknown total sweeps to and fro so it is seen to
    // be alive, it never keeps the previous phase's fraction
    const unsigned int now = gui->platform().ticks();
    if (p.phase != barPhase) {
        barPhase = p.phase;
        barPhaseStartMs = now;
    }
    const double fraction = rajob::barFor(p, now - barPhaseStartMs).value;

    const int bodyHeight = 20 + LineHeight + 34 + 12;
    const int panelHeight = PanelStyle::HeaderHeight + bodyHeight + PanelStyle::FooterHeight;
    ableem::Rect panel = drawPanel(
        *gui, style, panelWidthFor(*gui, {{&fonts[FONT_28_BOLD], title}, {&fonts[FONT_22_MED], detail}}), panelHeight);

    const TextRenderer::Shadow classicShadow = gui->text().shadow();
    TextRenderer::Shadow shadow;
    shadow.enabled = style.textShadow;
    gui->text().setShadow(shadow);

    gui->text().renderText_WithColor(fonts[FONT_28_BOLD], title, panel.x + TextInset, panel.y + 18, style.text,
                                     XALIGN_LEFT);
    style.rule(gui->uiContext(), panel, panel.y + PanelStyle::HeaderHeight - 8);
    int y = panel.y + PanelStyle::HeaderHeight + 20;
    gui->text().renderText_WithColor(fonts[FONT_22_MED],
                                     gui->text().elide(fonts[FONT_22_MED], detail, panel.w - 2 * TextInset),
                                     panel.x + TextInset, y, style.description, XALIGN_LEFT);
    y += LineHeight;
    ableem::Rect bar(panel.x + TextInset, y + 4, panel.w - 2 * TextInset, 22);
    style.progressBox(gui->uiContext(), bar, min(1.0, fraction));

    style.footer(
        *gui, ableem::Rect(panel.x, panel.y + panel.h - PanelStyle::FooterHeight, panel.w, PanelStyle::FooterHeight),
        stopping ? vector<PanelStyle::HintItem>{} : vector<PanelStyle::HintItem>{{{"O"}, _("Stop")}}, "", true);
    gui->text().setShadow(classicShadow);
}

void GuiRaProgress::loop() {
    menuVisible = true;
    while (menuVisible && job != nullptr) {
        shown = job->poll();
        if (!job->running())
            break; // ok / stopped / error: the caller says what it was
        render();
        Event e;
        while (gui->input().poll(e)) {
            if (e.type == Event::Type::Quit) {
                job->stop();
                stopping = true;
            } else if (e.type == Event::Type::ButtonDown && e.button == Button::Circle && !stopping) {
                app.audio().cancel.play();
                GuiConfirm confirm(*gui);
                confirm.label = _("Stop?");
                confirm.show();
                if (confirm.result && job->running()) {
                    job->stop();
                    stopping = true;
                }
            }
        }
        gui->platform().delay(16);
    }
    menuVisible = false;
}
