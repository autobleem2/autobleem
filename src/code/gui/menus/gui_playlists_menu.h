#pragma once

#include "gui_string_menu.h"

//*******************************
// class GuiPlaylists
//*******************************
class GuiPlaylists : public GuiStringMenu {
public:
    explicit GuiPlaylists(ableem::GuiBase &_gui) : GuiStringMenu(_gui) {}

    void init() override {
        for (const string &playlist : playlists) {
            lines.emplace_back(playlist + " (" + to_string(app.retroArch().gameCount(playlist)) + " " + _("games") +
                               ")");
        }
        GuiStringMenu::init();
    }

    std::string getTitle() override { return "-=" + _("Select RetroArch Playlist") + "=-"; }
    std::string getStatusLine() override { return GuiStringMenu::getStatusLine(); }

    void doEnter() override { doCross_Pressed(); }
    void doEscape() override { doCircle_Pressed(); }

    std::vector<std::string> playlists;
};
