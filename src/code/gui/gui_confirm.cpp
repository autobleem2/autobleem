//
// Created by screemer on 2019-01-24.
//

#include "gui_confirm.h"
#include "gui_about.h"
#include <string>
#include "gui.h"
#include "../engine/scanner.h"
using namespace std;

//*******************************
// GuiConfirm::render
//*******************************
void GuiConfirm::render()
{
    shared_ptr<Gui> gui(Gui::getInstance());
    gui->renderBackground();
    gui->renderTextBar();
    int yoffset = gui->renderLogo(true);
    gui->text().renderTextLine("-=" + _("Please confirm") + "=-",0,yoffset, XALIGN_CENTER);
    gui->text().renderTextLine(label,2,yoffset, XALIGN_CENTER);


    gui->renderStatus("|@X| "+_("Confirm")+"  |@O| "+_("Cancel")+" |");
    renderer.present();
}

//*******************************
// GuiConfirm::loop
//*******************************
void GuiConfirm::loop()
{
    shared_ptr<Gui> gui(Gui::getInstance());
    menuVisible = true;
    while (menuVisible) {
        Event e;
        while (gui->input().poll(e)) {
            // this is for pc Only
            if (e.type == Event::Type::Quit) {
                menuVisible = false;
            }

            switch (e.type) {
                case Event::Type::ButtonDown:
                    if (e.button == Button::Cross) {
                        app.audio().cursor.play();
                        result = true;
                        menuVisible = false;
                    };

                    if (e.button == Button::Circle) {
                        app.audio().cancel.play();
                        result = false;
                        menuVisible = false;
                    };
                    break;

                case Event::Type::KeyDown:
                    if (e.key == Key::Return) {
                        app.audio().cursor.play();
                        result = true;
                        menuVisible = false;
                    }
                    if (e.key == Key::Escape) {
                        app.audio().cancel.play();
                        result = false;
                        menuVisible = false;
                    }
                    break;
                default:
                    break;
            }
        }
    }
}
