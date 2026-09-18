//
// Created by screemer on 04.05.2020.
//

#include "gui_gamepadMenu.h"
#include "gui_padconfig.h"
#include "dualshock3_info.h"
#include "bluetooth_info.h"

void GuiGamepadMenu::fill() {
    std::shared_ptr<Gui> gui(Gui::getInstance());
    lines.clear();
    lines.emplace_back(_("Setup/Test Game Controller or update mapping"));
    lines.emplace_back(_("Connect DualShock3 by Bluetooth (Wireless mode)"));
    lines.emplace_back(_("Connect other gamepad by Bluetooth"));
}

void GuiGamepadMenu::render() {
    fill();

    SDL_RenderClear(renderer);
    gui->renderBackground();
    gui->renderTextBar();
    offset = gui->renderLogo(true);
    gui->renderTextLine(getTitle(), 0, offset, POS_CENTER);

    if (firstRender) {
        computePagePosition();
        firstRender = false;
    }
    renderLines();
    renderSelectionBox();

    gui->renderStatus(getStatusLine());
    string controllerNumInfo =
            "   " + _("Game Controllers number: ") + to_string(gui->mapper.getActivePadNum()) + "/" +
            to_string(SDL_NumJoysticks());

    gui->renderTextLine(_("Game controller DB:") + gui->mapper.getControllerDBPath(), 6, offset, false);
    gui->renderTextLine(_("Game controller information:"), 7, offset, false);
    gui->renderTextLine(controllerNumInfo, 8, offset, false);
    int ctrlnum = 0;
    for (int i = 0; i < SDL_NumJoysticks(); i++) {
        char guid_str[100];
        SDL_JoystickGUID guid = SDL_JoystickGetDeviceGUID(i);
        SDL_JoystickGetGUIDString(guid, guid_str, 100);
        string name = SDL_JoystickNameForIndex(i);
        if (SDL_IsGameController(i)) {
            name += string(_(" - Mapping (Available):")) + SDL_GameControllerNameForIndex(i);
        } else {
            name += string(_(" - Mapping (Not found)"));
        }
        gui->renderTextLine("     #" + to_string(i) + /* "    (" + guid_str + ")   " + */ +" " + name, 9+ i, offset,
                            false);
        ctrlnum++;

    }
    SDL_RenderPresent(renderer);

}

void GuiGamepadMenu::doCircle_Pressed() {
    Mix_PlayChannel(-1, gui->cancel, 0);
    menuVisible = false;
}

void GuiGamepadMenu::doCross_Pressed() {
    Mix_PlayChannel(-1, gui->cursor, 0);
    GuiScreen *screen;
    switch (selected) {
        case Mapping:
            gui->mapper.flushPads();
            Mix_PlayChannel(-1, gui->cursor, 0);
            screen = new GuiPadconfig(renderer);
            screen->show();
            delete screen;
            gui->mapper.probePads();
            SDL_PumpEvents();
            SDL_FlushEvents(SDL_FIRSTEVENT, SDL_LASTEVENT);
            break;
        case DualShock:
            Mix_PlayChannel(-1, gui->cursor, 0);
            screen = new dualshock3_info(renderer);
            screen->show();
            delete screen;
            break;
        case BTPair:
            Mix_PlayChannel(-1, gui->cursor, 0);
            screen = new bluetooth_info(renderer);
            screen->show();
            delete screen;
            break;

    }
}
