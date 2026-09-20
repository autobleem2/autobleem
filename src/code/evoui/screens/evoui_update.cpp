#include "evoui_update.h"
#include "../../app.h"
#include "../../gui/gui.h"

#include <algorithm>
#include <ctime>

using namespace std;

namespace {
const int PanelWidth = 720;
const int RowHeight = 60;
const int RowInset = 24;
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

// the launcher's colours, resolved the way GuiLauncher resolves them
void launcherColors(AppBase &app, ableem::Color &text, ableem::Color &secondary, ableem::Color &hint) {
    const ableem::LauncherTheme &theme = app.theme().launcher();
    text = ableem::Color(255, 255, 255, 255);
    secondary = ableem::Color(100, 100, 100, 255);
    if (theme.colors.text.set)
        text = TextRenderer::toColor(theme.colors.text, 255);
    if (theme.colors.secondary.set)
        secondary = TextRenderer::toColor(theme.colors.secondary, 255);
    hint = theme.colors.hint.set ? TextRenderer::toColor(theme.colors.hint, 255) : secondary;
}

// the dimmed launcher under a dark panel with a one-pixel edge; returns the panel's rect
ableem::Rect drawPanel(ableem::Renderer &renderer, Gui &gui, const ableem::Texture &background,
                       const ableem::Color &secondary, int height) {
    if (background.valid())
        renderer.copy(background, nullptr, nullptr);
    else
        gui.renderBackground();
    renderer.setBlendMode(ableem::BlendMode::Blend);
    renderer.setDrawColor(ableem::Color(0, 0, 0, 110));
    renderer.fillRect();
    ableem::Rect panel{(SCREEN_WIDTH - PanelWidth) / 2, (SCREEN_HEIGHT - height) / 2, PanelWidth, height};
    renderer.setDrawColor(ableem::Color(0, 0, 0, 200));
    renderer.fillRect(panel);
    renderer.setDrawColor(ableem::Color(secondary.r, secondary.g, secondary.b, 160));
    renderer.drawRect(panel);
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
    launcherColors(app, textColor, secondaryColor, hintColor);
    const ableem::LauncherTheme &theme = app.theme().launcher();
    crossIcon = ableem::Texture::loadFile(renderer, theme.hints.cross);
    circleIcon = ableem::Texture::loadFile(renderer, theme.hints.circle);
}

//*******************************
// GuiUpdatePrompt::render
//*******************************
void GuiUpdatePrompt::render() {
    const int headerHeight = 74 + static_cast<int>(lines.size()) * LineHeight + 12;
    const int footerHeight = 54;
    const int panelHeight = headerHeight + static_cast<int>(items.size()) * RowHeight + footerHeight;
    ableem::Rect panel = drawPanel(renderer, *gui, background, secondaryColor, panelHeight);

    const TextRenderer::Shadow classicShadow = gui->text().shadow();
    TextRenderer::Shadow shadow;
    const ableem::Opt<bool> &textShadow = app.theme().launcher().textShadow;
    shadow.enabled = !textShadow.set || textShadow;
    gui->text().setShadow(shadow);

    Fonts &fonts = gui->assets().themeFonts;
    gui->text().renderText_WithColor(fonts[FONT_28_BOLD], _("Update available"), panel.x + RowInset, panel.y + 18,
                                     textColor, XALIGN_LEFT);
    int y = panel.y + 66;
    for (const string &line : lines) {
        gui->text().renderText_WithColor(fonts[FONT_22_MED], line, panel.x + RowInset + 8, y, secondaryColor,
                                         XALIGN_LEFT);
        y += LineHeight;
    }
    renderer.setDrawColor(ableem::Color(secondaryColor.r, secondaryColor.g, secondaryColor.b, 160));
    renderer.fillRect(ableem::Rect(panel.x + RowInset, panel.y + headerHeight - 8, panel.w - 2 * RowInset, 1));

    int rowY = panel.y + headerHeight;
    for (int i = 0; i < static_cast<int>(items.size()); i++) {
        if (i == selected) {
            renderer.setDrawColor(ableem::Color(textColor.r, textColor.g, textColor.b, 38));
            renderer.fillRect(ableem::Rect(panel.x + 1, rowY, panel.w - 2, RowHeight));
            renderer.setDrawColor(textColor);
            renderer.fillRect(ableem::Rect(panel.x + 1, rowY, 5, RowHeight));
        }
        gui->text().renderText_WithColor(fonts[FONT_22_MED], items[i].title, panel.x + RowInset + 8, rowY + 7,
                                         i == selected ? textColor : secondaryColor, XALIGN_LEFT);
        gui->text().renderText_WithColor(fonts[FONT_15_BOLD], items[i].description, panel.x + RowInset + 8, rowY + 35,
                                         secondaryColor, XALIGN_LEFT);
        rowY += RowHeight;
    }

    const int hintY = panel.y + panel.h - footerHeight + 14;
    int hintX = panel.x + RowInset;
    auto hint = [&](const ableem::Texture &icon, const string &label) {
        if (icon.valid()) {
            ableem::Size s = icon.size();
            ableem::Rect dst(hintX, hintY + (28 - s.h) / 2, s.w, s.h);
            renderer.copy(icon, nullptr, &dst);
            hintX += s.w + 8;
        }
        gui->text().renderText_WithColor(fonts[FONT_22_MED], label, hintX, hintY, hintColor, XALIGN_LEFT);
        hintX += gui->text().textWidth(fonts[FONT_22_MED], label) + 36;
    };
    hint(crossIcon, _("Select"));
    hint(circleIcon, _("Later"));

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
    ableem::Color hint;
    launcherColors(app, textColor, secondaryColor, hint);
    shownSince = 0;
}

//*******************************
// GuiUpdateProgress::render
//*******************************
void GuiUpdateProgress::render() {
    const int panelHeight = 190;
    ableem::Rect panel = drawPanel(renderer, *gui, background, secondaryColor, panelHeight);

    const TextRenderer::Shadow classicShadow = gui->text().shadow();
    TextRenderer::Shadow shadow;
    const ableem::Opt<bool> &textShadow = app.theme().launcher().textShadow;
    shadow.enabled = !textShadow.set || textShadow;
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
    gui->text().renderText_WithColor(fonts[FONT_28_BOLD], title, panel.x + RowInset, panel.y + 24, textColor,
                                     XALIGN_LEFT);
    if (!detail.empty())
        gui->text().renderText_WithColor(fonts[FONT_22_MED], detail, panel.x + RowInset, panel.y + 74, secondaryColor,
                                         XALIGN_LEFT);
    if (fraction >= 0) {
        ableem::Rect bar(panel.x + RowInset, panel.y + 124, panel.w - 2 * RowInset, 22);
        renderer.setDrawColor(ableem::Color(secondaryColor.r, secondaryColor.g, secondaryColor.b, 120));
        renderer.drawRect(bar);
        renderer.setDrawColor(textColor);
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
