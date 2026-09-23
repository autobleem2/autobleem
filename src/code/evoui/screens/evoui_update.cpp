#include "evoui_update.h"
#include "../../app.h"
#include "gui/gui.h"

#include <algorithm>
#include <ctime>

using namespace std;

namespace {
const int PanelWidth = 800;
const int RowHeight = PanelStyle::RowHeight;
const int RowInset = PanelStyle::RowInset;
const int LineHeight = 30;

// "42.1 MB"
string human(uint64_t bytes) {
    char buf[32];
    if (bytes >= 1000000)
        snprintf(buf, sizeof(buf), "%.1f MB", bytes / 1000000.0);
    else if (bytes >= 1000)
        snprintf(buf, sizeof(buf), "%.0f KB", bytes / 1000.0);
    else
        snprintf(buf, sizeof(buf), "%u B", static_cast<unsigned>(bytes));
    return buf;
}

// the dimmed launcher under the panel; returns the panel's rect
ableem::Rect drawPanel(ableem::Renderer &renderer, Gui &gui, const ableem::Texture &background, const PanelStyle &style,
                       int height) {
    if (background.valid())
        renderer.copy(background, nullptr, nullptr);
    else
        gui.renderBackground();
    style.dim(renderer);
    ableem::Rect panel{(SCREEN_WIDTH - PanelWidth) / 2, (SCREEN_HEIGHT - height) / 2, PanelWidth, height};
    style.sheet(renderer, panel);
    return panel;
}
} // namespace

//*******************************
// GuiUpdatePrompt::init
//*******************************
void GuiUpdatePrompt::init() {
    lines.clear();
    if (!info.autobleemVersion.empty())
        lines.push_back("AutoBleem " + info.autobleemVersion + "  (" + human(info.autobleem.size) + ")");
    if (!info.retroarchVersion.empty())
        lines.push_back("RetroArch " + info.retroarchVersion + "  (" + human(info.retroarch.size) + ")");
    items.clear();
    items.push_back({UpdateChoice::Now, _("Update now"), _("Download it and run the installer - a few minutes")});
    items.push_back({UpdateChoice::Later, _("Remind me tomorrow"), _("Ask again in a day")});
    items.push_back({UpdateChoice::Skip, _("Skip this version"), _("Do not ask about this one again")});
    selected = 0;
    result = UpdateChoice::Later;
    style = gui->panelStyle();
}

//*******************************
// GuiUpdatePrompt::render
//*******************************
void GuiUpdatePrompt::render() {
    // the header holds the title and the version lines under it; the rule sits under those
    const int headerHeight = PanelStyle::HeaderHeight + static_cast<int>(lines.size()) * LineHeight + 12;
    const int footerHeight = PanelStyle::FooterHeight;
    const int panelHeight = headerHeight + static_cast<int>(items.size()) * RowHeight + footerHeight;
    ableem::Rect panel = drawPanel(renderer, *gui, background, style, panelHeight);

    const TextRenderer::Shadow classicShadow = gui->text().shadow();
    TextRenderer::Shadow shadow;
    shadow.enabled = style.textShadow;
    gui->text().setShadow(shadow);

    Fonts &fonts = gui->assets().themeFonts;
    gui->text().renderText_WithColor(fonts[FONT_28_BOLD], _("Update available"), panel.x + RowInset, panel.y + 18,
                                     style.text, XALIGN_LEFT);
    int y = panel.y + 66;
    for (const string &line : lines) {
        gui->text().renderText_WithColor(fonts[FONT_22_MED], line, panel.x + RowInset + 8, y, style.secondary,
                                         XALIGN_LEFT);
        y += LineHeight;
    }
    style.rule(renderer, panel, panel.y + headerHeight - 8);

    int rowY = panel.y + headerHeight;
    for (int i = 0; i < static_cast<int>(items.size()); i++) {
        if (i == selected)
            style.selection(renderer, ableem::Rect(panel.x + 1, rowY, panel.w - 2, RowHeight));
        gui->text().renderText_WithColor(fonts[FONT_22_MED], items[i].title, panel.x + RowInset + 8, rowY + 7,
                                         i == selected ? style.text : style.secondary, XALIGN_LEFT);
        gui->text().renderText_WithColor(fonts[FONT_15_BOLD], items[i].description, panel.x + RowInset + 8, rowY + 35,
                                         style.secondary, XALIGN_LEFT);
        rowY += RowHeight;
    }

    style.footer(*gui, ableem::Rect(panel.x, panel.y + panel.h - footerHeight, panel.w, footerHeight),
                 {{{"X"}, _("Select")}, {{"O"}, _("Later")}}, "", false);

    gui->text().setShadow(classicShadow);
    renderer.present();
}

//*******************************
// GuiUpdatePrompt::loop
//*******************************
void GuiUpdatePrompt::loop() {
    menuVisible = true;
    while (menuVisible) {
        render();
        Event e;
        while (gui->input().poll(e)) {
            if (e.type == Event::Type::Quit) {
                result = UpdateChoice::Later;
                menuVisible = false;
            }
            switch (e.type) {
            case Event::Type::DpadDown:
            case Event::Type::DpadUp:
                if (gui->input().dpadUp()) {
                    app.audio().cursor.play();
                    selected = (selected + static_cast<int>(items.size()) - 1) % static_cast<int>(items.size());
                } else if (gui->input().dpadDown()) {
                    app.audio().cursor.play();
                    selected = (selected + 1) % static_cast<int>(items.size());
                }
                break;
            case Event::Type::ButtonDown:
                if (e.button == Button::Cross) {
                    app.audio().cursor.play();
                    result = items[selected].choice;
                    menuVisible = false;
                } else if (e.button == Button::Circle) {
                    app.audio().cancel.play();
                    result = UpdateChoice::Later;
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
// GuiUpdateProgress::init
//*******************************
void GuiUpdateProgress::init() {
    style = gui->panelStyle();
    shownSince = 0;
}

//*******************************
// GuiUpdateProgress::render
//*******************************
void GuiUpdateProgress::render() {
    const int panelHeight = 190;
    ableem::Rect panel = drawPanel(renderer, *gui, background, style, panelHeight);

    const TextRenderer::Shadow classicShadow = gui->text().shadow();
    TextRenderer::Shadow shadow;
    shadow.enabled = style.textShadow;
    gui->text().setShadow(shadow);

    Fonts &fonts = gui->assets().themeFonts;
    string title, detail;
    double fraction = -1; // < 0: no bar
    switch (status.phase) {
    case UpdateService::Phase::Checking:
        title = _("Checking for updates...");
        break;
    case UpdateService::Phase::Downloading:
        title = _("Downloading the update");
        detail = status.currentFile + "  " + human(status.bytesDone) + " / " + human(status.bytesTotal);
        if (status.bytesTotal > 0)
            fraction = min(1.0, static_cast<double>(status.bytesDone) / static_cast<double>(status.bytesTotal));
        break;
    case UpdateService::Phase::Downloaded:
        title = _("Downloaded");
        detail = _("Starting the installer...");
        fraction = 1;
        break;
    case UpdateService::Phase::Checked:
        title = status.info.any() ? _("Update available") : _("AutoBleem is up to date");
        break;
    case UpdateService::Phase::Failed:
        title = _("Update failed");
        detail = status.error;
        break;
    default:
        title = "";
        break;
    }
    gui->text().renderText_WithColor(fonts[FONT_28_BOLD], title, panel.x + RowInset, panel.y + 24, style.text,
                                     XALIGN_LEFT);
    if (!detail.empty())
        gui->text().renderText_WithColor(fonts[FONT_22_MED], detail, panel.x + RowInset, panel.y + 74, style.secondary,
                                         XALIGN_LEFT);
    if (fraction >= 0) {
        ableem::Rect bar(panel.x + RowInset, panel.y + 124, panel.w - 2 * RowInset, 22);
        renderer.setDrawColor(ableem::Color(style.secondary.r, style.secondary.g, style.secondary.b, 120));
        renderer.drawRect(bar);
        renderer.setDrawColor(style.text);
        renderer.fillRect(ableem::Rect(bar.x + 2, bar.y + 2, static_cast<int>((bar.w - 4) * fraction), bar.h - 4));
    }

    gui->text().setShadow(classicShadow);
    renderer.present();
}

//*******************************
// GuiUpdateProgress::loop
//*******************************
void GuiUpdateProgress::loop() {
    UpdateService &updates = App::get().updates();
    menuVisible = true;
    while (menuVisible) {
        status = updates.poll();
        render();
        const bool over =
            status.phase != UpdateService::Phase::Checking && status.phase != UpdateService::Phase::Downloading;
        if (over) {
            if (shownSince == 0)
                shownSince = gui->platform().ticks();
            // an outcome that needs reading (up to date, failed) stays a moment; a download that
            // succeeded hands over at once - the installer's own screen follows
            const unsigned int hold = status.phase == UpdateService::Phase::Downloaded ? 300 : 1500;
            if (gui->platform().ticks() - shownSince >= hold)
                menuVisible = false;
        }
        Event e;
        while (gui->input().poll(e)) {
            if (e.type == Event::Type::Quit)
                menuVisible = false;
            if (over && e.type == Event::Type::ButtonDown)
                menuVisible = false;
        }
        gui->platform().delay(16);
    }
    finalStatus = status;
}
