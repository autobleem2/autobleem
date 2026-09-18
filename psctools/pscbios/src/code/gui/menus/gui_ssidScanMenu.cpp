//
// Created by screemer on 19.03.2020.
//

#include <zconf.h>
#include <algorithm>
#include "gui_ssidScanMenu.h"

void gui_ssidScanMenu::doCross_Pressed() {
    if (!lines.empty()) {
        Mix_PlayChannel(-1, gui->cursor, 0);
        newSSID = lines[selected];
        cancelled = false;
        menuVisible = false;
    }
}

void gui_ssidScanMenu::doCircle_Pressed() {
    Mix_PlayChannel(-1, gui->cancel, 0);
    menuVisible = false;
    cancelled = true;
}

std::string gui_ssidScanMenu::getStatusLine() {
    string menu;
    if (!lines.empty()) {
        menu += "|@X|   " + _("Select") + "    ";
    }
    menu += "|@O|   " + _("Cancel");
    return menu;
}

void gui_ssidScanMenu::init() {
    GuiMenuBase::init();    // call the base init
    Gui::splash(_("Scanning networks"));
    fill();
    if (lines.empty()) {
        Gui::splash(_("No networks found"));
        usleep(2 * 1000000);
    }

}

void gui_ssidScanMenu::fill() {
    auto *networkController = new NetworkController();
    vector<string> scannedNetworks = networkController->scan();
    delete networkController;
    lines.clear();
    for (string network:scannedNetworks) {
        if (!network.empty()) {
            lines.emplace_back(network);
        }
    }


}