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
        return _("Checking free space");
    if (key == "download-retroarch")
        return _("Downloading RetroArch");
    if (key == "download-cores")
        return _("Downloading emulator cores");
    if (key == "download-libs")
        return _("Downloading libraries");
    if (key == "download-apps")
        return _("Downloading apps");
    if (key == "download-assets")
        return _("Downloading menu assets");
    if (key == "download-bios")
        return _("Downloading BIOS files");
    if (key == "unpack")
        return _("Unpacking");
    if (key == "merge-config")
        return _("Merging your settings");
    if (key == "install-binary")
        return _("Installing RetroArch");
    if (key == "remove-files")
        return _("Removing files");
    return _("Loading ... Please Wait ..."); // a key this launcher does not know (a newer runner)
}

// the one-line "why" under a phase's package
string phaseWhy(const string &key) {
    if (key == "check-space")
        return _("Making sure the stick has room for everything.");
    if (key == "download-retroarch")
        return _("The RetroArch program itself.");
    if (key == "download-cores")
        return _("The emulators that play each system's games.");
    if (key == "download-libs")
        return _("Helper libraries RetroArch needs to run.");
    if (key == "download-apps")
        return _("Extra apps that come with RetroArch.");
    if (key == "download-assets")
        return _("Pictures, fonts and icons for RetroArch's menu.");
    if (key == "download-bios")
        return _("System files some emulators need to start games.");
    if (key == "unpack")
        return _("Opening the downloaded packages onto the stick.");
    if (key == "merge-config")
        return _("We keep your retroarch.cfg, add this version's keys and the AutoBleem theme.");
    if (key == "install-binary")
        return _("Putting RetroArch in place so it can start.");
    if (key == "remove-files")
        return _("Deleting RetroArch's files. Your games, saves and settings stay.");
    return "";
}

// "%s" in `format` filled in order by `args`
string fill(string format, const vector<string> &args) {
    for (const string &arg : args) {
        const size_t at = format.find("%s");
        if (at == string::npos)
            break;
        format.replace(at, 2, arg);
    }
    return format;
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
string GuiRaManager::summary(bool installed, const string &version) {
    if (!installed)
        return _("Not installed");
    return _("Installed") + ": " + (version.empty() ? "?" : version);
}

string GuiRaManager::spaceLine(uint64_t freeBytes, uint64_t totalBytes) {
    return fill(_("%s free of %s"), {rajob::humanSpace(freeBytes), rajob::humanSpace(totalBytes)});
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
    const string head = summary(installed, version);
    const string space = spaceKnown ? spaceLine(freeBytes, totalBytes) : "";
    const int spaceHeight = spaceKnown ? LineHeight + 10 : 0; // the line and the thin bar
    const int headerHeight = PanelStyle::HeaderHeight + LineHeight + 12 + spaceHeight;
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
    if (spaceKnown) {
        gui->text().renderText_WithColor(
            fonts[FONT_22_MED], gui->text().elide(fonts[FONT_22_MED], space, panel.w - 2 * TextInset),
            panel.x + TextInset, panel.y + 66 + LineHeight, style.description, XALIGN_LEFT);
        ableem::Rect bar(panel.x + TextInset, panel.y + 66 + 2 * LineHeight + 2, panel.w - 2 * TextInset, 8);
        style.progressBox(gui->uiContext(), bar, rajob::usedFraction(freeBytes, totalBytes));
    }
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
    meterItem = -1;
    meter.reset();
}

//*******************************
// GuiRaRemoveConfirm
//*******************************
void GuiRaRemoveConfirm::init() {
    style = gui->panelStyle();
    result = false;
}

void GuiRaRemoveConfirm::draw() {
    Fonts &fonts = gui->assets().themeFonts;
    vector<string> deleted = {_("RetroArch and its emulator cores"), _("Libraries, apps and menu assets")};
    vector<string> stays = {_("Your games"), _("Your saves and save states"), _("Your settings")};
    if (withBios)
        deleted.push_back(_("BIOS files"));
    else
        stays.push_back(_("BIOS files"));
    const string warning =
        withBios ? _("Without the BIOS files some games will not start until you add them again.") : "";
    const int rows = static_cast<int>(max(deleted.size(), stays.size()));
    const int bodyHeight = LineHeight + 12 + rows * LineHeight + (withBios ? LineHeight + 16 : 0) + 12;
    const int panelHeight = PanelStyle::HeaderHeight + bodyHeight + PanelStyle::FooterHeight;
    vector<pair<const ableem::Font *, string>> texts{{&fonts[FONT_28_BOLD], _("Remove RetroArch?")},
                                                     {&fonts[FONT_15_BOLD], warning}};
    ableem::Rect panel = drawPanel(*gui, style, panelWidthFor(*gui, texts), panelHeight);

    const TextRenderer::Shadow classicShadow = gui->text().shadow();
    TextRenderer::Shadow shadow;
    shadow.enabled = style.textShadow;
    gui->text().setShadow(shadow);

    gui->text().renderText_WithColor(fonts[FONT_28_BOLD], _("Remove RetroArch?"), panel.x + TextInset, panel.y + 18,
                                     style.text, XALIGN_LEFT);
    style.rule(gui->uiContext(), panel, panel.y + PanelStyle::HeaderHeight - 8);
    const int columnWidth = (panel.w - 2 * TextInset) / 2;
    const int top = panel.y + PanelStyle::HeaderHeight + 8;
    const struct {
        const string heading;
        const vector<string> &lines;
        int x;
    } columns[] = {{_("Will be deleted"), deleted, panel.x + TextInset},
                   {_("Stays on the stick"), stays, panel.x + TextInset + columnWidth}};
    for (const auto &column : columns) {
        gui->text().renderText_WithColor(fonts[FONT_22_MED], column.heading, column.x, top, style.text, XALIGN_LEFT);
        int y = top + LineHeight + 8;
        for (const string &line : column.lines) {
            gui->text().renderText_WithColor(fonts[FONT_15_BOLD],
                                             gui->text().elide(fonts[FONT_15_BOLD], line, columnWidth - 12), column.x,
                                             y, style.description, XALIGN_LEFT);
            y += LineHeight;
        }
    }
    if (withBios)
        gui->text().renderText_WithColor(
            fonts[FONT_15_BOLD], gui->text().elide(fonts[FONT_15_BOLD], warning, panel.w - 2 * TextInset),
            panel.x + TextInset, top + LineHeight + 8 + rows * LineHeight + 8, style.text, XALIGN_LEFT);
    style.footer(*gui,
                 ableem::Rect(panel.x, panel.y + panel.h - PanelStyle::FooterHeight, panel.w, PanelStyle::FooterHeight),
                 {{{"X"}, _("Remove")}, {{"O"}, _("Cancel")}}, "", false);
    gui->text().setShadow(classicShadow);
}

void GuiRaRemoveConfirm::loop() {
    menuVisible = true;
    gui->input().setFrameNeed(ableem::Input::FrameNeed::Idle);
    while (menuVisible) {
        if (gui->input().frameDue())
            render();
        Event e;
        while (gui->input().poll(e)) {
            if (e.type == Event::Type::Quit) {
                result = false;
                menuVisible = false;
            } else if (e.type == Event::Type::ButtonDown && e.button == Button::Cross) {
                result = true;
                menuVisible = false;
            } else if (e.type == Event::Type::ButtonDown && e.button == Button::Circle) {
                app.audio().cancel.play();
                result = false;
                menuVisible = false;
            }
        }
    }
}

void GuiRaProgress::draw() {
    Fonts &fonts = gui->assets().themeFonts;
    const rajob::Progress &p = shown.progress;
    const string phaseName = p.phase > 0 ? phaseTitle(p.key) : _("Loading ... Please Wait ...");
    const string title = p.phase > 0
                             ? fill(_("Step %s of %s"), {to_string(p.phase), to_string(p.phases)}) + " - " + phaseName
                             : phaseName;
    const int percent = static_cast<int>(rajob::jobFraction(p) * 100.0);
    const string percentText = to_string(percent) + "%";

    // the bar belongs to one phase: a new phase starts it again; an unknown total sweeps to and fro so it is seen to
    // be alive, it never keeps the previous phase's fraction
    const unsigned int now = gui->platform().ticks();
    if (p.phase != barPhase) {
        barPhase = p.phase;
        barPhaseStartMs = now;
    }
    // the speed follows one package of a download; a new package or phase starts the meter again
    const bool downloading = p.unit == "bytes" && p.key.compare(0, 8, "download") == 0;
    const int itemKey = p.phase * 1000 + p.itemIndex;
    if (itemKey != meterItem) {
        meterItem = itemKey;
        meter.reset();
    }
    if (downloading)
        meter.feed(now, p.done);
    const double fraction = rajob::barFor(p, now - barPhaseStartMs).value;

    // the package line: its name, "Package i of n"; the numbers line: MB or files (never raw bytes), speed, time left
    string package = p.item;
    if (p.itemCount > 0)
        package += (package.empty() ? "" : "   ") +
                   fill(_("Package %s of %s"), {to_string(p.itemIndex), to_string(p.itemCount)});
    string numbers;
    if (p.phase > 0 && p.total > 0) {
        numbers = rajob::packageAmount(p.done, p.total, p.unit);
        if (p.unit == "files")
            numbers += " " + _("files");
        if (downloading && meter.known())
            numbers += "   " + rajob::humanSpace(static_cast<uint64_t>(meter.bytesPerSecond())) + "/s   " +
                       fill(_("%s left"), {rajob::humanDuration(meter.secondsLeft(p.total - min(p.done, p.total)))});
    }
    string why = stopping ? _("Stopping...") : phaseWhy(p.key);

    const int bodyHeight = 16 + 3 * LineHeight + 2 * 34 + LineHeight + 12;
    const int panelHeight = PanelStyle::HeaderHeight + bodyHeight + PanelStyle::FooterHeight;
    ableem::Rect panel = drawPanel(
        *gui, style, panelWidthFor(*gui, {{&fonts[FONT_28_BOLD], title}, {&fonts[FONT_22_MED], numbers}}), panelHeight);

    const TextRenderer::Shadow classicShadow = gui->text().shadow();
    TextRenderer::Shadow shadow;
    shadow.enabled = style.textShadow;
    gui->text().setShadow(shadow);

    const int inner = panel.w - 2 * TextInset;
    const int percentWidth = gui->text().textWidth(fonts[FONT_28_BOLD], percentText);
    gui->text().renderText_WithColor(fonts[FONT_28_BOLD],
                                     gui->text().elide(fonts[FONT_28_BOLD], title, inner - percentWidth - 16),
                                     panel.x + TextInset, panel.y + 18, style.text, XALIGN_LEFT);
    gui->text().renderText_WithColor(fonts[FONT_28_BOLD], percentText, panel.x + panel.w - TextInset - percentWidth,
                                     panel.y + 18, style.text, XALIGN_LEFT);
    style.rule(gui->uiContext(), panel, panel.y + PanelStyle::HeaderHeight - 8);
    int y = panel.y + PanelStyle::HeaderHeight + 12;
    // the whole job
    style.progressBox(gui->uiContext(), ableem::Rect(panel.x + TextInset, y, inner, 22),
                      min(1.0, rajob::jobFraction(p)));
    y += 34 + 8;
    // the current package
    gui->text().renderText_WithColor(fonts[FONT_22_MED], gui->text().elide(fonts[FONT_22_MED], package, inner),
                                     panel.x + TextInset, y, style.text, XALIGN_LEFT);
    y += LineHeight;
    style.progressBox(gui->uiContext(), ableem::Rect(panel.x + TextInset, y + 4, inner, 22), min(1.0, fraction));
    y += 34;
    gui->text().renderText_WithColor(fonts[FONT_22_MED], gui->text().elide(fonts[FONT_22_MED], numbers, inner),
                                     panel.x + TextInset, y, style.description, XALIGN_LEFT);
    y += LineHeight + 4;
    gui->text().renderText_WithColor(fonts[FONT_15_BOLD], gui->text().elide(fonts[FONT_15_BOLD], why, inner),
                                     panel.x + TextInset, y, style.description, XALIGN_LEFT);

    const bool canStop = rajob::stopAllowed(op) && !stopping;
    style.footer(*gui,
                 ableem::Rect(panel.x, panel.y + panel.h - PanelStyle::FooterHeight, panel.w, PanelStyle::FooterHeight),
                 canStop ? vector<PanelStyle::HintItem>{{{"O"}, _("Stop")}} : vector<PanelStyle::HintItem>{}, "", true);
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
                if (rajob::stopAllowed(op)) {
                    job->stop();
                    stopping = true;
                }
            } else if (e.type == Event::Type::ButtonDown && e.button == Button::Circle && !stopping &&
                       rajob::stopAllowed(op)) { // a Remove has no Stop
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
