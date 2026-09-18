//
// Created by screemer on 04.05.2020.
//


#pragma once


#include "../menus/gui_stringMenu.h"

enum {
    Mapping = 0, DualShock, BTPair
};

class GuiGamepadMenu : public GuiStringMenu {
    void fill();
    void render() override ;
    virtual std::string getTitle() override { return "-=" + _("-=" + _("Gamepad Support Configuration") + "=-") + "=-"; }
    virtual std::string getStatusLine() override
    {
        return "|@X|"+_("Select")+"   |@O|"+_("Cancel");
    }
    virtual bool skipSelectingThisLineWhenMovingByOne(int index) { return (lines[index] == "") || (index>3); }

    void doCircle_Pressed() override;
    void doCross_Pressed() override;

    void doEnter() { doCross_Pressed(); }
    void doEscape() { doCircle_Pressed(); }

public:
    GuiGamepadMenu(SDL_Shared<SDL_Renderer> _renderer) : GuiStringMenu(_renderer) {}
};


