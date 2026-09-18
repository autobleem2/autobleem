//
// Created by screemer on 19.03.2020.
//

#pragma once


#include "gui_stringMenu.h"

class gui_ssidScanMenu : public GuiStringMenu {
public:
    gui_ssidScanMenu(SDL_Shared<SDL_Renderer> _renderer) : GuiStringMenu(_renderer) {}
    void init() override;
    void fill();

    virtual std::string getTitle() override { return "-=" + _("-=" + _("Select WiFi Network to connect") + "=-") + "=-"; }
    virtual std::string getStatusLine() override;   // returns the status line at the bottom

    void doCircle_Pressed() override;
    void doCross_Pressed() override;

    void doEnter() { doCross_Pressed(); }
    void doEscape() { doCircle_Pressed(); }

    string newSSID="";
};



