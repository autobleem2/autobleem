#pragma once

#include "gui_string_menu.h"

//*******************************
// class GuiPlaylists
//*******************************
class GuiPlaylists : public GuiStringMenu {
public:
    GuiPlaylists(ableem::GuiBase &_gui) : GuiStringMenu(_gui) {}

    void init() override {
        for (const string& playlist : playlists) {
            lines.emplace_back(playlist + " (" + to_string(app.retroArch().gameCount(playlist)) + " " + _("games") + ")");
        }
        GuiStringMenu::init();
    }

    virtual std::string getTitle() override { return "-=" + _("Select RetroArch Playlist") + "=-"; }
    virtual std::string getStatusLine() override { return GuiStringMenu::getStatusLine(); }

    void doEnter() { doCross_Pressed(); }
    void doEscape() { doCircle_Pressed(); }

    std::vector<std::string> playlists;
};


