//
// Created by screemer on 2019-01-24.
//

#include "gui_confirm.h"

#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_mixer.h>
#include <SDL2/SDL_ttf.h>
#include <string>
#include "gui.h"
#include "../lang.h"

using namespace std;

//*******************************
// GuiConfirm::render
//*******************************
void GuiConfirm::render()
{
    shared_ptr<Gui> gui(Gui::getInstance());
    gui->renderBackground();
    gui->renderTextBar();
    int offset = 20;
    gui->renderTextLine(string("-=") + _("Please confirm") + "=-",0,offset, POS_CENTER);
    gui->renderTextLine(label,2,offset, POS_CENTER);


    gui->renderStatus("|@X|  "+_("Confirm")+"   |@O|  "+_("Cancel")+"  |");
    SDL_RenderPresent(renderer);
}

//*******************************
// GuiConfirm::loop
//*******************************
void GuiConfirm::loop()
{
    shared_ptr<Gui> gui(Gui::getInstance());
    menuVisible = true;
    while (menuVisible) {

        SDL_Event e;
        if (SDL_PollEvent(&e)) {
            if (e.type == SDL_KEYDOWN) {
                if (e.key.keysym.scancode == SDL_SCANCODE_SLEEP) {
                    gui->drawText(_("POWERING OFF... PLEASE WAIT"));


                }
            }
            // this is for pc Only
            if (e.type == SDL_QUIT) {
                menuVisible = false;
            }
            switch (e.type) {
                case SDL_JOYBUTTONDOWN:


                    if (e.jbutton.button == PCS_BTN_CROSS) {

                        result = true;
                        menuVisible = false;

                    };
                    if (e.jbutton.button == PCS_BTN_CIRCLE) {

                        result = false;
                        menuVisible = false;

                    };


            }

        }
    }
}