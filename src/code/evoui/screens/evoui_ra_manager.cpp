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

// the title of a phase in the job line ("Step 3 of 9 - Downloading cores"): the short form of phaseTitle
string stepTitle(const string &key) {
    if (key == "check-space")
        return _("Checking space");
    if (key == "download-cores")
        return _("Downloading cores");
    if (key == "download-assets")
        return _("Downloading assets");
    if (key == "merge-config")
        return _("Merging settings");
    if (key == "install-binary")
        return _("Installing");
    return phaseTitle(key);
}

// the one-line "why" under a phase's package; the unpack phase says it per package (`item`)
string phaseWhy(const string &key, const string &item) {
    if (key == "unpack") {
        switch (rajob::unpackKind(item)) {
        case rajob::UnpackKind::Cheats:
            return _("Cheat codes for thousands of games - many small files, so this part is slow.");
        case rajob::UnpackKind::Cores:
            return _("The cores are unpacked onto the stick, ready to run.");
        case rajob::UnpackKind::Other:
            break;
        }
    }
    if (key == "check-space")
        return _("Making sure the stick has room for everything.");
    if (key == "download-retroarch")
        return _("The RetroArch program itself.");
    if (key == "download-cores")
        return _("Cores are the emulators - one for every console RetroArch supports.");
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
        return _("Your games, saves and settings are not touched.");
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

// the language's decimal sign: the `decimal point` key is "," or "."
char decimalSign() {
    return _("decimal point") == "," ? ',' : '.';
}

// "128 of 289 MB" / "1200 of 5400 files" / "2 of 4"
string amountText(const rajob::Amount &amount) {
    const vector<string> numbers = {rajob::withDecimal(amount.done, decimalSign()),
                                    rajob::withDecimal(amount.total, decimalSign())};
    switch (amount.kind) {
    case rajob::Amount::Kind::MB:
        return fill(_("%s of %s MB"), numbers);
    case rajob::Amount::Kind::KB:
        return fill(_("%s of %s KB"), numbers);
    case rajob::Amount::Kind::Files:
        return fill(_("%s of %s files"), numbers);
    case rajob::Amount::Kind::Items:
        break;
    }
    return fill(_("%s of %s"), numbers);
}

// the words of the Remove confirm's lines
string removeItemText(rajob::RemoveItem item) {
    switch (item) {
    case rajob::RemoveItem::RetroArch:
        return _("RetroArch itself");
    case rajob::RemoveItem::Cores:
        return _("Emulator cores");
    case rajob::RemoveItem::MenuAssets:
        return _("Menu assets");
    case rajob::RemoveItem::Databases:
        return _("Databases");
    case rajob::RemoveItem::Cheats:
        return _("Cheats");
    case rajob::RemoveItem::Overlays:
        return _("Overlays");
    case rajob::RemoveItem::Shaders:
        return _("Shaders");
    case rajob::RemoveItem::Bios:
        return _("BIOS files");
    case rajob::RemoveItem::Games:
        return _("Your games");
    case rajob::RemoveItem::Playlists:
        return _("Playlists");
    case rajob::RemoveItem::Covers:
        return _("Covers");
    case rajob::RemoveItem::Saves:
        return _("Saves (memory cards)");
    case rajob::RemoveItem::SaveStates:
        return _("Save states");
    case rajob::RemoveItem::Settings:
        return _("Settings (stays)");
    }
    return "";
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
    const int tagHeight = tag.empty() ? 0 : font.lineHeight();
    const int textHeight = static_cast<int>(rows.size()) * font.lineHeight() + tagHeight;
    const abgui::Panel panel(panelRect(style, textHeight, ctx.renderer().width(), ctx.renderer().height(), panelWidth),
                             style);
    panel.sheet(ctx);

    const bool programShadow = ctx.setTextShadow(style.textShadow);
    int y = panel.header(ctx, title.empty() ? _("RetroArch...") : title) + TextGapTop;
    const int x = panel.rect().x + style.rowInset + 8;
    if (!tag.empty()) {
        ctx.drawText(font, tag, x, y, ableem::Color(230, 70, 70, 255)); // the ERROR tag
        y += tagHeight;
    }
    for (const string &row : rows) {
        if (!row.empty())
            ctx.drawText(font, row, x, y, style.text);
        y += font.lineHeight();
    }
    style.footer(ctx, panel.footer(), hints, "", false);
    ctx.setTextShadow(programShadow);
}

//*******************************
// GuiRaNotice
//*******************************
// abgui::Confirm::draw with the body, the bullets and the accent line
void GuiRaNotice::draw() {
    ctx.drawBackdrop();
    const abgui::Style style = ctx.style();
    const ableem::Font &font = ctx.font(abgui::FontRole::Row);
    const vector<abgui::HintItem> hints = {{{"X"}, confirmLabel.empty() ? _("Start") : confirmLabel},
                                           {{"O"}, cancelLabel.empty() ? _("Cancel") : cancelLabel}};
    const int panelWidth = abgui::Panel::compactWidth(ctx, hints, "");
    const int width = min(textWidth(style), panelWidth - 2 * (style.rowInset + 8));
    const auto measure = [&font](const string &s) { return font.width(s); };

    struct Row {
        string text;
        ableem::Color color;
    };
    vector<Row> rows;
    for (const string &row : abgui::wrapText(label, width, measure))
        rows.push_back({row, style.text});
    rows.push_back({"", style.text});
    const string dash = "- ";
    const string indent(dash.size(), ' ');
    const int bulletWidth = width - font.width(dash);
    for (const string &bullet : bullets) {
        bool first = true;
        for (const string &row : abgui::wrapText(bullet, bulletWidth, measure)) {
            rows.push_back({(first ? dash : indent) + row, style.text});
            first = false;
        }
    }
    if (!accent.empty()) {
        rows.push_back({"", style.text});
        for (const string &row : abgui::wrapText(accent, width, measure))
            rows.push_back({row, style.heading}); // the colour of the cyan headings
    }
    const int textHeight = static_cast<int>(rows.size()) * font.lineHeight();
    const abgui::Panel panel(panelRect(style, textHeight, ctx.renderer().width(), ctx.renderer().height(), panelWidth),
                             style);
    panel.sheet(ctx);

    const bool programShadow = ctx.setTextShadow(style.textShadow);
    int y = panel.header(ctx, title.empty() ? _("Before you start") : title) + TextGapTop;
    const int x = panel.rect().x + style.rowInset + 8;
    for (const Row &row : rows) {
        if (!row.text.empty())
            ctx.drawText(font, row.text, x, y, row.color);
        y += font.lineHeight();
    }
    style.footer(ctx, panel.footer(), hints, "", false);
    ctx.setTextShadow(programShadow);
}

void GuiRaNotice::setFor(rajob::Op op) {
    title = _("Before you start");
    if (op == rajob::Op::Update) {
        label = _("Updating RetroArch takes about 10-15 minutes on the console: about 400 MB to download and "
                  "tens of thousands of small files to write.");
        bullets = {_("Keep the console on and the stick in."),
                   _("Stop keeps everything downloaded so far - Update carries on later.")};
    } else {
        label = _("Installing RetroArch takes about 10-15 minutes on the console: about 400 MB to download and "
                  "tens of thousands of small files to write.");
        bullets = {_("Keep the console on and the stick in."),
                   _("Stop keeps everything downloaded so far - Install carries on later.")};
    }
    accent = _("A good moment to go and make a coffee.");
    confirmLabel = _("Start");
    cancelLabel = _("Cancel");
}

//*******************************
// GuiRaMessage (continued)
//*******************************
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
    return fill(_("%s free of %s"), {spaceText(freeBytes), spaceText(totalBytes)});
}

string GuiRaManager::spaceText(uint64_t bytes) {
    return rajob::withDecimal(rajob::humanSpace(bytes), decimalSign());
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
            return _("No network - connect in Network & Controllers");
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
    if (keepRow) {
        // the row the user came back from - the BIOS row exists only while Remove is the chosen one
        rows_ = rajob::panelRows(installed, networkUp, startRow);
        for (int i = 0; i < static_cast<int>(rows_.size()); i++)
            if (rows_[i].action == startRow)
                selected = i;
    }
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
                    chosenRow = row.action;
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
    stall.reset();
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
    const rajob::RemoveLists lists = rajob::removeLists(withBios);
    vector<string> deleted, stays;
    for (rajob::RemoveItem item : lists.deleted)
        deleted.push_back(removeItemText(item));
    for (rajob::RemoveItem item : lists.stays)
        stays.push_back(removeItemText(item));
    const string warning = withBios ? _("Cores that need these BIOS files won't start until you copy them back.") : "";
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
    // the panel's title: what the run is
    string title = _("Installing RetroArch");
    if (op == rajob::Op::Remove)
        title = _("Removing RetroArch");
    else if (op == rajob::Op::Update)
        title = _("Updating RetroArch");
    // the job line: "Step 3 of 9 - Downloading cores", the percent at its right
    string jobLine = _("Loading ... Please Wait ...");
    if (p.phase > 0)
        jobLine = fill(_("Step %s of %s"), {to_string(p.phase), to_string(p.phases)}) + " - " + stepTitle(p.key);
    const string percentText = to_string(static_cast<int>(rajob::jobFraction(p) * 100.0)) + "%";

    // the bar belongs to one phase: a new phase starts it again, it never keeps the previous phase's fraction. When
    // the phase, the package and the count have not changed for a second, a sweep runs over the real value.
    const unsigned int now = gui->platform().ticks();
    if (p.phase != barPhase) {
        barPhase = p.phase;
        barPhaseStartMs = now;
    }
    stall.feed(now, p);
    // the speed follows one package of a download; a new package or phase starts the meter again
    const bool downloading = rajob::effectiveUnit(p) == "bytes" && p.key.compare(0, 8, "download") == 0;
    const int itemKey = p.phase * 1000 + p.itemIndex;
    if (itemKey != meterItem) {
        meterItem = itemKey;
        meter.reset();
    }
    if (downloading)
        meter.feed(now, p.done);
    const rajob::Bar bar = rajob::barWithStall(p, now - barPhaseStartMs, stall.stalledMs(now));

    // the package line (bold): "<what> - 128 of 289 MB", "Package 3 of 9" at its right
    string package;
    if (p.phase > 0) {
        package = rajob::packageWhat(phaseTitle(p.key), p.item, p.key);
        if (p.total > 0)
            package += " - " + amountText(rajob::amountFor(p));
    }
    string packageCount;
    if (p.itemCount > 0)
        packageCount = fill(_("Package %s of %s"), {to_string(p.itemIndex), to_string(p.itemCount)});
    const string why = stopping ? _("Stopping...") : phaseWhy(p.key, p.item);
    // the meter: nothing until the speed has three readings; a Remove has no speed, it has the wait
    string meterLine;
    if (op == rajob::Op::Remove)
        meterLine = _("Please wait - this takes about half a minute");
    else if (downloading && meter.known()) {
        const char decimal = decimalSign();
        const double secondsLeft = meter.secondsLeft(p.total - min(p.done, p.total));
        const string timeLeft = fill(_("About %s left"), {rajob::roughDuration(secondsLeft)});
        meterLine = timeLeft + " \xC2\xB7 " + rajob::speedText(meter.bytesPerSecond(), decimal);
    }

    const int bodyHeight = 12 + LineHeight + 34 + 36 + 34 + 3 * LineHeight + 12; // the why takes up to two lines
    const int panelHeight = PanelStyle::HeaderHeight + bodyHeight + PanelStyle::FooterHeight;
    ableem::Rect panel = drawPanel(
        *gui, style, panelWidthFor(*gui, {{&fonts[FONT_28_BOLD], title}, {&fonts[FONT_22_MED], jobLine}}), panelHeight);

    const TextRenderer::Shadow classicShadow = gui->text().shadow();
    TextRenderer::Shadow shadow;
    shadow.enabled = style.textShadow;
    gui->text().setShadow(shadow);

    const int inner = panel.w - 2 * TextInset;
    const int left = panel.x + TextInset;
    const int right = panel.x + panel.w - TextInset;
    // a line of text at (x, y), elided to maxWidth
    const auto put = [&](const ableem::Font &font, const string &text, int x, int y, int maxWidth,
                         const ableem::Color &color) {
        gui->text().renderText_WithColor(font, gui->text().elide(font, text, maxWidth), x, y, color, XALIGN_LEFT);
    };
    put(fonts[FONT_28_BOLD], title, left, panel.y + 18, inner, style.text);
    style.rule(gui->uiContext(), panel, panel.y + PanelStyle::HeaderHeight - 8);
    int y = panel.y + PanelStyle::HeaderHeight + 12;
    // the whole job
    const int percentWidth = gui->text().textWidth(fonts[FONT_22_MED], percentText);
    put(fonts[FONT_22_MED], jobLine, left, y, inner - percentWidth - 16, style.text);
    put(fonts[FONT_22_MED], percentText, right - percentWidth, y, percentWidth, style.text);
    y += LineHeight;
    const ableem::Rect jobBar(left, y, inner, 22);
    style.progressBox(gui->uiContext(), jobBar, min(1.0, rajob::jobFraction(p)));
    y += 34;
    // the current package
    const int countWidth = gui->text().textWidth(fonts[FONT_22_MED], packageCount);
    put(fonts[FONT_28_BOLD], package, left, y, inner - countWidth - 16, style.text);
    put(fonts[FONT_22_MED], packageCount, right - countWidth, y + 4, countWidth, style.description);
    y += 36;
    const ableem::Rect packageBar(left, y, inner, 22);
    style.progressBox(gui->uiContext(), packageBar, min(1.0, bar.value));
    if (bar.overReal) {
        // the sweep: a mark running to and fro over the bar - dark over the fill, light over the empty part
        const int markWidth = inner / 6;
        const int markX = packageBar.x + static_cast<int>(bar.sweep * (inner - markWidth));
        const bool overFill = markX + markWidth / 2 < packageBar.x + static_cast<int>(bar.value * inner);
        ableem::Color mark(style.text.r, style.text.g, style.text.b, 150);
        if (overFill)
            mark = ableem::Color(0, 0, 0, 120);
        style.plate(gui->uiContext(), ableem::Rect(markX, packageBar.y + 2, markWidth, packageBar.h - 4), mark);
    }
    y += 34;
    // the why wraps to two lines of the panel; the meter keeps its place under both
    const int whyTop = y;
    const ableem::Font &whyFont = fonts[FONT_22_MED];
    const auto measure = [&](const string &s) { return gui->text().textWidth(whyFont, s); };
    for (const string &line : rajob::wrapLines(why, inner, 2, measure)) {
        put(whyFont, line, left, y, inner, style.description);
        y += LineHeight;
    }
    y = whyTop + 2 * LineHeight;
    put(fonts[FONT_22_MED], meterLine, left, y, inner, style.description);

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
