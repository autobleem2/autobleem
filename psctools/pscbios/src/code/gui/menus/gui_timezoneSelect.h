//
// Created by screemer on 21.03.2020.
//

#pragma once


#include "gui_stringMenu.h"

class gui_timezoneSelect : public GuiStringMenu {
public:
    gui_timezoneSelect(SDL_Shared<SDL_Renderer>_renderer) :
            GuiStringMenu(_renderer) {}

    void init() override;

    void fill();

    std::string getTitle() override {
        return "-=" + _("-=" + _("Select Time Zone for Your Location") + "=-") + "=-";
    }



    void doCircle_Pressed() override;

    void doCross_Pressed() override;

    void doEnter() { doCross_Pressed(); }

    void doEscape() { doCircle_Pressed(); }

    string newTimeZone = "";
};
