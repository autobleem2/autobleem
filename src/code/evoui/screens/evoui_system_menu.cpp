#include "evoui_system_menu.h"
#include "../../gui/gui.h"

#include <algorithm>

using namespace std;

namespace {
// the panel: as tall as its rows need, up to the screen less a margin; more rows than fit scroll
const int PanelWidth = 640;
const int PanelMargin = 40;
const int HeaderHeight = 74;
const int FooterHeight = 54;
const int RowHeight = 60;
const int RowInset = 24; // the rows' text from the panel's edge
} // namespace

//*******************************
// GuiSystemMenu::init
//*******************************
void GuiSystemMenu::init() {
    items.clear();
    items.push_back(
        {SystemMenuAction::RescanGames, _("Re-Scan Games"),
         scanInProgress ? _("A scan is already in progress") : _("Look for new, changed or removed games")});
    items.push_back({SystemMenuAction::RetroArch, retroArchLabel, _("Exit to") + " " + retroArchLabel});
    items.push_back({SystemMenuAction::MemoryCards, _("Memory Cards"), _("Create, rename or manage memory card sets")});
    items.push_back({SystemMenuAction::GameManager, _("Game Manager"), _("Delete games, flush covers")});
    items.push_back(
        {SystemMenuAction::HardwareInfo, _("Hardware Information"), _("Controller and system information")});
    items.push_back({SystemMenuAction::Options, _("Options"), _("Customize AutoBleem settings")});
#ifdef AB_ONLINE_UPDATE
    items.push_back({SystemMenuAction::SoftwareUpdate, _("Software Update"),
                     updateAvailable ? _("An update is available") : _("Check the download site for a newer version")});
#endif
    items.push_back({SystemMenuAction::About, _("About"), _("About AutoBleem")});
    items.push_back({SystemMenuAction::PowerOff, _("Power Off"), _("Safely power off the console")});
    selected = 0;
    firstVisible = 0;
    result = SystemMenuAction::None;

    // the launcher's colours, resolved the way GuiLauncher resolves them
    const ableem::LauncherTheme &theme = app.theme().launcher();
    textColor = ableem::Color(255, 255, 255, 255);
    secondaryColor = ableem::Color(100, 100, 100, 255);
    if (theme.colors.text.set)
        textColor = TextRenderer::toColor(theme.colors.text, 255);
    if (theme.colors.secondary.set)
        secondaryColor = TextRenderer::toColor(theme.colors.secondary, 255);
    hintColor = theme.colors.hint.set ? TextRenderer::toColor(theme.colors.hint, 255) : secondaryColor;
    crossIcon = ableem::Texture::loadFile(renderer, theme.hints.cross);
    circleIcon = ableem::Texture::loadFile(renderer, theme.hints.circle);
}

//*******************************
// GuiSystemMenu::visibleRows
//*******************************
int GuiSystemMenu::visibleRows() const {
    int roomForRows = SCREEN_HEIGHT - 2 * PanelMargin - HeaderHeight - FooterHeight;
    return max(1, min(static_cast<int>(items.size()), roomForRows / RowHeight));
}

//*******************************
// GuiSystemMenu::render
//*******************************
void GuiSystemMenu::render() {
    // the launcher's own background, dimmed, so the menu reads as an overlay on the screen it came from
    if (background.valid())
        renderer.copy(background, nullptr, nullptr);
    else
        gui->renderBackground();
    renderer.setBlendMode(ableem::BlendMode::Blend);
    renderer.setDrawColor(ableem::Color(0, 0, 0, 110));
    renderer.fillRect();

    const int rows = visibleRows();
    const int panelHeight = HeaderHeight + rows * RowHeight + FooterHeight;
    ableem::Rect panel{(SCREEN_WIDTH - PanelWidth) / 2, (SCREEN_HEIGHT - panelHeight) / 2, PanelWidth, panelHeight};

    // the panel: a dark sheet with a one-pixel edge in the secondary colour, the header ruled off from the rows
    renderer.setDrawColor(ableem::Color(0, 0, 0, 200));
    renderer.fillRect(panel);
    renderer.setDrawColor(ableem::Color(secondaryColor.r, secondaryColor.g, secondaryColor.b, 160));
    renderer.drawRect(panel);
    renderer.fillRect(ableem::Rect(panel.x + RowInset, panel.y + HeaderHeight - 8, panel.w - 2 * RowInset, 1));

    // every text on this screen gets the launcher's halo, like the launcher's own
    const TextRenderer::Shadow classicShadow = gui->text().shadow();
    TextRenderer::Shadow shadow;
    const ableem::Opt<bool> &textShadow = app.theme().launcher().textShadow;
    shadow.enabled = !textShadow.set || textShadow;
    gui->text().setShadow(shadow);

    Fonts &fonts = gui->assets().themeFonts;
    gui->text().renderText_WithColor(fonts[FONT_28_BOLD], _("System"), panel.x + RowInset, panel.y + 18, textColor,
                                     XALIGN_LEFT);

    int rowY = panel.y + HeaderHeight;
    for (int i = firstVisible; i < firstVisible + rows && i < static_cast<int>(items.size()); i++) {
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

    // scroll markers: a small triangle at the top or bottom edge of the rows when more are that way
    renderer.setDrawColor(textColor);
    auto marker = [&](int cy, int direction) {
        const int cx = panel.x + panel.w - RowInset;
        for (int i = 0; i < 5; i++)
            renderer.fillRect(ableem::Rect(cx - i, cy + direction * i, 2 * i + 1, 1));
    };
    if (firstVisible > 0)
        marker(panel.y + HeaderHeight - 4, -1);
    if (firstVisible + rows < static_cast<int>(items.size()))
        marker(panel.y + HeaderHeight + rows * RowHeight + 2, 1);

    // the footer: the launcher's own button hints
    const int hintY = panel.y + panel.h - FooterHeight + 14;
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
    hint(circleIcon, _("Close"));

    gui->text().setShadow(classicShadow);
    renderer.present();
}

//*******************************
// GuiSystemMenu::moveSelection
//*******************************
void GuiSystemMenu::moveSelection(int step) {
    const int count = static_cast<int>(items.size());
    selected = (selected + step + count) % count;
    const int rows = visibleRows();
    if (selected < firstVisible)
        firstVisible = selected;
    else if (selected >= firstVisible + rows)
        firstVisible = selected - rows + 1;
}

//*******************************
// GuiSystemMenu::loop
//*******************************
void GuiSystemMenu::loop() {
    menuVisible = true;
    while (menuVisible) {
        render();
        Event e;
        while (gui->input().poll(e)) {
            if (e.type == Event::Type::Quit) {
                result = SystemMenuAction::None;
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
                if (e.button == Button::Cross) {
                    app.audio().cursor.play();
                    result = items[selected].action;
                    menuVisible = false;
                } else if (e.button == Button::Circle) {
                    app.audio().cancel.play();
                    result = SystemMenuAction::None;
                    menuVisible = false;
                }
                break;
            default:
                break;
            }
        }
    }
}
