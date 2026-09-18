#include "evoui_system_menu.h"
#include "../../gui/gui.h"

using namespace std;

namespace {
const ableem::Color White{255, 255, 255, 255};
const ableem::Color Dim{180, 180, 180, 255};
const ableem::Color Faint{150, 150, 150, 255};
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
    items.push_back({SystemMenuAction::About, _("About"), _("About AutoBleem")});
    items.push_back({SystemMenuAction::PowerOff, _("Power Off"), _("Safely power off the console")});
    selected = 0;
    result = SystemMenuAction::None;
}

//*******************************
// GuiSystemMenu::render
//*******************************
void GuiSystemMenu::render() {
    gui->renderBackground();

    ableem::Rect panel{290, 60, 700, 600};
    renderer.setDrawColor(ableem::Color(0, 0, 0, 175));
    renderer.setBlendMode(ableem::BlendMode::Blend);
    renderer.fillRect(panel);

    auto titleFont = gui->assets().themeFonts[FONT_28_BOLD];
    auto rowFont = gui->assets().themeFonts[FONT_22_MED];

    gui->text().renderText_WithColor(titleFont, "-=" + _("SYSTEM") + "=-", 0, panel.y + 20, White, XALIGN_CENTER);

    int rowY = panel.y + 90;
    const int rowHeight = 62;
    for (int i = 0; i < static_cast<int>(items.size()); i++) {
        if (i == selected) {
            ableem::Rect hi{panel.x + 20, rowY - 6, panel.w - 40, rowHeight - 10};
            renderer.setDrawColor(ableem::Color(255, 255, 255, 40));
            renderer.setBlendMode(ableem::BlendMode::Blend);
            renderer.fillRect(hi);
        }
        ableem::Color titleColor = (i == selected) ? White : Dim;
        gui->text().renderText_WithColor(titleFont, items[i].title, panel.x + 40, rowY, titleColor, XALIGN_LEFT);
        gui->text().renderText_WithColor(rowFont, items[i].description, panel.x + 40, rowY + 32, Faint, XALIGN_LEFT);
        rowY += rowHeight;
    }

    gui->renderStatus("|@X| " + _("Select") + "     |@O| / |@R2| " + _("Close") + "|");
    renderer.present();
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
                    selected = (selected == 0) ? static_cast<int>(items.size()) - 1 : selected - 1;
                } else if (gui->input().dpadDown()) {
                    app.audio().cursor.play();
                    selected = (selected + 1) % static_cast<int>(items.size());
                }
                break;
            case Event::Type::ButtonDown:
                if (e.button == Button::Cross) {
                    app.audio().cursor.play();
                    result = items[selected].action;
                    menuVisible = false;
                } else if (e.button == Button::Circle || e.button == Button::R2) {
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
