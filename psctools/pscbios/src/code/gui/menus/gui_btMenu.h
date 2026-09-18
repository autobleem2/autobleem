//
// Created by screemer on 24.03.2020.
//

#pragma once


#include "gui_stringMenu.h"
#include "../../bluetooth/bluetool.h"

using namespace std;
class gui_btMenu  : public GuiStringMenu {
public:
    bluetool * bt;
    bool active = false;
    string controllerName="";
    gui_btMenu(SDL_Shared<SDL_Renderer> _renderer) : GuiStringMenu(_renderer) {
        bt = new bluetool();
    }
    ~gui_btMenu()
    {
        if (active)
        {
            bt->quit();
        }
        delete bt;
    }
    void init() override;
    void fill();
    void render() override ;
    virtual std::string getTitle() override { return "-=" + _("-=" + _("Bluetooth Configuration") + "=-") + "=-"; }


    void doCircle_Pressed() override;
    void doCross_Pressed() override;

    void doEnter() { doCross_Pressed(); }
    void doEscape() { doCircle_Pressed(); }
};



