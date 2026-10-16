#include "evoui_ra_manager.h"
#include "../../app.h"
#include "gui/gui.h"
#include "gui/screens/gui_confirm.h"

#include <ab_gui/list.h>
#include <ab_gui/panel.h>
#include <ab_gui/text_page.h>

#include <algorithm>

using namespace std;

namespace {
const int PanelWidth = 800;                     // at least - wider for a long line
const int TextInset = PanelStyle::RowInset + 8; // a line's x in the panel, and its right margin
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

// where a panel of `height` starts: centred over the shelf, clear of the hint bar under it (the mockups' y0)
int panelTop(int canvasHeight, int height) {
    return max(PanelStyle::Margin / 2, (canvasHeight - 92 - height) / 2 + 20);
}

// the dimmed launcher's snapshot under the panel; returns the panel's rect
ableem::Rect drawPanel(Gui &gui, const PanelStyle &style, int width, int height) {
    gui.renderBackground();
    style.dim(gui.uiContext());
    ableem::Rect panel{(gui.renderer().width() - width) / 2, panelTop(gui.renderer().height(), height), width, height};
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
    const auto measure = [&font](const string &s) { return font.width(s); };
    const vector<string> rows = abgui::wrapText(label, width, measure);
    const vector<string> detailRows = abgui::wrapText(detail, width, measure);
    const int textHeight = static_cast<int>(rows.size() + detailRows.size()) * font.lineHeight();
    ableem::Rect rect = panelRect(style, textHeight, ctx.renderer().width(), ctx.renderer().height(), panelWidth);
    rect.y = panelTop(ctx.renderer().height(), rect.h);
    const abgui::Panel panel(rect, style);
    panel.sheet(ctx);

    const bool programShadow = ctx.setTextShadow(style.textShadow);
    const string heading = title.empty() ? _("RetroArch...") : title;
    int y = panel.header(ctx, heading) + TextGapTop;
    const int x = panel.rect().x + style.rowInset + 8;
    if (!tag.empty()) {
        // the ERROR chip right of the title (ra-4b)
        const int titleWidth = ctx.textWidth(ctx.font(abgui::FontRole::Title), heading);
        const int chipX = panel.rect().x + style.rowInset + titleWidth + 16;
        style.button(ctx, tag, chipX, panel.rect().y + 28, 24);
    }
    for (const string &row : rows) {
        if (!row.empty())
            ctx.drawText(font, row, x, y, style.text);
        y += font.lineHeight();
    }
    for (const string &row : detailRows) {
        if (!row.empty())
            ctx.drawText(font, row, x, y, style.description);
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
        bool bullet; // a cyan square before the row's text (the first row of a bullet)
        bool indented;
    };
    vector<Row> rows;
    // the intro breaks after its colon, as the mockup has it: "...on the console:" / "about 400 MB to download ..."
    string intro = label, rest;
    const size_t colon = label.find(": ");
    if (colon != string::npos) {
        intro = label.substr(0, colon + 1);
        rest = label.substr(colon + 2);
    }
    for (const string &row : abgui::wrapText(intro, width, measure))
        rows.push_back({row, style.text, false, false});
    if (!rest.empty())
        for (const string &row : abgui::wrapText(rest, width, measure))
            rows.push_back({row, style.text, false, false});
    rows.push_back({"", style.text, false, false});
    const int squareGap = 18; // the bullet's square and the space after it
    const int bulletWidth = width - squareGap;
    for (const string &bullet : bullets) {
        bool first = true;
        for (const string &row : abgui::wrapText(bullet, bulletWidth, measure)) {
            rows.push_back({row, style.description, first, true});
            first = false;
        }
    }
    if (!accent.empty()) {
        rows.push_back({"", style.text, false, false});
        for (const string &row : abgui::wrapText(accent, width, measure))
            rows.push_back({row, style.heading, false, false}); // the colour of the cyan headings
    }
    const int textHeight = static_cast<int>(rows.size()) * font.lineHeight();
    ableem::Rect rect = panelRect(style, textHeight, ctx.renderer().width(), ctx.renderer().height(), panelWidth);
    rect.y = panelTop(ctx.renderer().height(), rect.h);
    const abgui::Panel panel(rect, style);
    panel.sheet(ctx);

    const bool programShadow = ctx.setTextShadow(style.textShadow);
    int y = panel.header(ctx, title.empty() ? _("Before you start") : title) + TextGapTop;
    const int x = panel.rect().x + style.rowInset + 8;
    for (const Row &row : rows) {
        if (row.bullet) {
            // 7x7 square in the heading colour (cyan, not the focus colour), level with the text's middle
            ctx.renderer().setBlendMode(ableem::BlendMode::Blend);
            ctx.renderer().setDrawColor(style.heading);
            ctx.renderer().fillRect(ableem::Rect(x, y + font.lineHeight() / 2 - 3, 7, 7));
        }
        if (!row.text.empty())
            ctx.drawText(font, row.text, x + (row.indented ? squareGap : 0), y, row.color);
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
        return _("Only with Remove");
    }
    return "";
}

void GuiRaManager::rebuild() {
    rows_ = rajob::panelRows(installed, networkUp);
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
    if (keepRow) {
        // the row the user came back from
        for (int i = 0; i < static_cast<int>(rows_.size()); i++)
            if (rows_[i].action == startRow)
                selected = i;
    }
    result = RaChoice::None;
    style = gui->panelStyle();
}

void GuiRaManager::draw() {
    const string head = summary(installed, version);
    // the mockup's layout (ra-1a), in the 1280x720 canvas: the status line, the free-space line and its bar, the rule,
    // four rows 64 apart
    const int headerHeight = 188;
    const int rowPitch = 64;
    const int panelHeight = headerHeight + 4 * rowPitch + PanelStyle::FooterHeight + 18;
    Fonts &fonts = gui->assets().themeFonts;
    vector<pair<const ableem::Font *, string>> texts{{&fonts[FONT_20_BOLD], head}};
    for (const rajob::Row &row : rows_) {
        texts.emplace_back(&fonts[FONT_20_BOLD], titleOf(row.action));
        texts.emplace_back(&fonts[FONT_15_BOLD], descriptionOf(row));
    }
    ableem::Rect panel = drawPanel(*gui, style, panelWidthFor(*gui, texts), panelHeight);

    const TextRenderer::Shadow classicShadow = gui->text().shadow();
    TextRenderer::Shadow shadow;
    shadow.enabled = style.textShadow;
    gui->text().setShadow(shadow);

    const auto put = [&](const ableem::Font &f, const string &s, int x, int y, const ableem::Color &c) {
        gui->text().renderText_WithColor(f, s, x, y, c, XALIGN_LEFT);
    };
    const int left = panel.x + TextInset;
    const int right = panel.x + panel.w - TextInset;
    put(fonts[FONT_28_BOLD], _("RetroArch"), left - 8, panel.y + 18, style.text);
    put(fonts[FONT_20_BOLD], gui->text().elide(fonts[FONT_20_BOLD], head, right - left), left, panel.y + 84, style.text);
    if (spaceKnown) {
        // "Free on the stick" at the left, "18.4 GB free of 58.2 GB" at the right, the bar under both
        const string space = spaceLine(freeBytes, totalBytes);
        const int spaceWidth = gui->text().textWidth(fonts[FONT_22_MED], space);
        put(fonts[FONT_22_MED], _("Free on the stick"), left, panel.y + 122, style.description);
        put(fonts[FONT_22_MED], space, right - spaceWidth, panel.y + 122, style.text);
        ableem::Rect bar(left, panel.y + 158, right - left, 10);
        style.progressBox(gui->uiContext(), bar, rajob::usedFraction(freeBytes, totalBytes));
    }
    style.rule(gui->uiContext(), panel, panel.y + headerHeight - 8);

    int rowY = panel.y + headerHeight;
    for (int i = 0; i < static_cast<int>(rows_.size()); i++) {
        const rajob::Row &row = rows_[i];
        const bool toggle = row.action == rajob::RowAction::DeleteBios;
        const ableem::Rect band(panel.x + 2, rowY, panel.w - 4, rowPitch - 4);
        if (i == selected)
            style.selection(gui->uiContext(), band);
        const auto color = row.enabled ? style.rowColor(i == selected)
                                       : style.disabledColor(gui->uiContext(), style.rowColor(i == selected));
        const int textX = left + (toggle ? 28 : 0);
        put(fonts[FONT_20_BOLD], titleOf(row.action), textX, rowY + 6, color);
        put(fonts[FONT_15_BOLD], descriptionOf(row), textX, rowY + 36, style.description);
        if (toggle) {
            // the switch image at the row's right end; a theme without one gets the word
            if (!abgui::List::drawSwitch(gui->uiContext(), deleteBios, right, rowY, rowPitch)) {
                const string word = deleteBios ? _("ON") : _("OFF");
                put(fonts[FONT_20_BOLD], word, right - gui->text().textWidth(fonts[FONT_20_BOLD], word), rowY + 16, color);
            }
        }
        if (!row.enabled)
            style.disabled(gui->uiContext(), band);
        rowY += rowPitch;
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
                    if (row.action == rajob::RowAction::DeleteBios) {
                        deleteBios = !deleteBios; // a switch: Remove reads it
                        break;
                    }
                    chosenRow = row.action;
                    switch (row.action) {
                    case rajob::RowAction::Install:
                        result = RaChoice::Install;
                        break;
                    case rajob::RowAction::Update:
                        result = RaChoice::Update;
                        break;
                    case rajob::RowAction::Remove:
                    case rajob::RowAction::DeleteBios:
                        result = deleteBios ? RaChoice::RemoveWithBios : RaChoice::Remove;
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
    // ra-3a / ra-3b: the columns start 88 under the top, a header and 36 px later the rows, 30 apart; the BIOS note
    // takes one more line
    const int listTop = 88 + 36;
    const int panelHeight = listTop + rows * LineHeight + (withBios ? 38 : 0) + 24 + PanelStyle::FooterHeight;
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
    const int leftX = panel.x + TextInset;
    const int rightX = panel.x + panel.w / 2 + 10;
    const int columnWidth = panel.x + panel.w - TextInset - rightX;
    const int top = panel.y + 88;
    ableem::Renderer &renderer = gui->renderer();
    // the divider between the columns: the heading colour, faint
    renderer.setBlendMode(ableem::BlendMode::Blend);
    renderer.setDrawColor(ableem::Color(style.heading.r, style.heading.g, style.heading.b, 80));
    renderer.fillRect(ableem::Rect(panel.x + panel.w / 2 - 12, top - 4, 1, rows * LineHeight + 40));
    // the headers: upper case, in the heading colour
    gui->text().renderText_WithColor(fonts[FONT_20_BOLD], _("WILL BE DELETED"), leftX, top, style.heading, XALIGN_LEFT);
    gui->text().renderText_WithColor(fonts[FONT_20_BOLD], _("STAYS ON THE STICK"), rightX, top, style.heading,
                                     XALIGN_LEFT);
    int y = top + 36;
    for (const string &line : deleted) {
        gui->text().renderText_WithColor(
            fonts[FONT_22_MED], gui->text().elide(fonts[FONT_22_MED], "\xE2\x80\x93  " + line, columnWidth), leftX, y,
            style.text, XALIGN_LEFT);
        y += LineHeight;
    }
    y = top + 36;
    for (const string &line : stays) {
        // the tick is drawn, the font has none: two 1 px strokes make it 2 px wide
        const int tx = rightX, ty = y + 14;
        renderer.setDrawColor(style.heading);
        for (int d = 0; d < 2; d++) {
            renderer.drawLine({tx, ty + d}, {tx + 4, ty + 4 + d});
            renderer.drawLine({tx + 4, ty + 4 + d}, {tx + 12, ty - 5 + d});
        }
        gui->text().renderText_WithColor(fonts[FONT_22_MED],
                                         gui->text().elide(fonts[FONT_22_MED], line, columnWidth - 22), rightX + 22, y,
                                         style.description, XALIGN_LEFT);
        y += LineHeight;
    }
    if (withBios)
        gui->text().renderText_WithColor(
            fonts[FONT_15_BOLD], gui->text().elide(fonts[FONT_15_BOLD], warning, panel.w - 2 * TextInset), leftX,
            top + 36 + rows * LineHeight + 10, style.description, XALIGN_LEFT);
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
