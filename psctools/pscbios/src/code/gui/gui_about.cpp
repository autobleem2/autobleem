//
// Created by screemer on 2019-01-24.
//

#include "gui_about.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_mixer.h>
#include <SDL2/SDL_ttf.h>
#include <string>
#include "gui.h"
#include "../gui/abl.h"
#include "../lang.h"
#include "../environment.h"

void GuiAbout::init() {
    std::shared_ptr<Gui> gui(Gui::getInstance());
    fx.renderer = renderer;
    logo = IMG_LoadTexture(renderer, (Env::getWorkingPath() + sep + "ablogo.png").c_str());
}

//*******************************
// GuiAbout::render
//*******************************
void GuiAbout::render() {
    std::shared_ptr<Gui> gui(Gui::getInstance());
    vector<string> credits = {"PSC BIOS v0.2", "----------------",
                              _(".-= Code C++ and shell scripts =-."),
                              "screemer(AutoBleem), madmonkey(Hakchi)", " ",
                              _(".-= Linux Kernel Patching =-."),
                              "screemer, madmonkey", " ",
                              _(".-= Testing =-."),
                              "MagnusRC, xboxiso, Azazel, Solidius, SupaSAIAN, Kingherb, saptis",
                              _(".-= Localization support =-."),
                              "nex(German), Azazel(Polish), gadsby(Turkish), GeekAndy(Dutch), Pardubak(Slovak), SupaSAIAN(Spanish), Mate(Czech)",
                              "Sasha(Italian), Jakejj(BR_Portuguese), jolny(Swedish), StepJefli(Danish), alucard73 / MagnusRC(French), Quenti(Occitan), ",
                              " ",
                              _("Support via Discord:") + " https://discord.gg/AHUS3RM",
                              _("This project is freeware. It works AS IS and We take no responsibility for any issues or damage."),
                              _("Download latest:") + " " + _("(Ask on our discord for a link) ")
    };


    gui->renderBackground();

    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 235);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

    SDL_Rect rect2;
    rect2.x = 0;
    rect2.y = 0;
    rect2.w = 1280;
    rect2.h = 720;

    SDL_RenderFillRect(renderer, &rect2);

    fx.render();


    int line = 1;
    for (const string &s:credits) {
        gui->renderTextLine(s, line, 0, POS_CENTER, 0);
        line++;
    }

    gui->renderStatus("|@O| " + _("Go back") + "|",680);
    SDL_RenderPresent(renderer);
}

//*******************************
// GuiAbout::loop
//*******************************
void GuiAbout::loop() {
    std::shared_ptr<Gui> gui(Gui::getInstance());
    menuVisible = true;
    while (menuVisible) {
        render();
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            gui->mapper.handleHotPlug(&e);
            gui->mapper.handlePowerBtn(&e);
            // this is for pc Only
            if (e.type == SDL_QUIT) {
                menuVisible = false;
            }
            switch (e.type) {
                case SDL_CONTROLLERBUTTONUP:


                    if (e.cbutton.button == SDL_BTN_CIRCLE) {
                        Mix_PlayChannel(-1, gui->cancel, 0);
                        menuVisible = false;

                    };


            }

        }
    }
}
