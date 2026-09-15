//
// Created by screemer on 2019-01-24.
//

#include "gui_about.h"
#include <string>
#include "gui.h"
#include "../lang.h"
#include "../engine/scanner.h"
#include "../environment.h"

void GuiAbout::init() {
    std::shared_ptr<Gui> gui(Gui::getInstance());
    fx.renderer = &renderer;
    font = Fonts::openNewSharedCachedFont(Env::getWorkingPath() + sep + "about.ttf", 17, renderer);
    logo = ableem::Texture::loadFile(renderer, Env::getWorkingPath() + sep + "ablogo.png");
}

//*******************************
// GuiAbout::render
//*******************************
void GuiAbout::render() {
    std::shared_ptr<Gui> gui(Gui::getInstance());
    vector<string> credits = {gui->cfg.inifile.values["version"], " ",
                              _(".-= Code C++ and shell scripts =-."),
                              "screemer, Axanar, mGGk, nex, genderbent",
                              _(".-= Graphics =-."),
                              "KaonashiFTW, GeekAndy, rubixcube6, NewbornfromHell",
                              _(".-= Testing =-."),
                              "MagnusRC, xboxiso, Azazel, Solidius, SupaSAIAN, Kingherb, saptis",
                              _(".-= Database maintenance =-."),
                              "Screemer,Kingherb",
                              _(".-= Localization support =-."),
                              "nex(German), Azazel(Polish), gadsby(Turkish), GeekAndy(Dutch), Pardubak(Slovak), SupaSAIAN(Spanish), Mate(Czech)",
                              "Sasha(Italian), Jakejj(BR_Portuguese), jolny(Swedish), StepJefli(Danish), alucard73 / MagnusRC(French), Quenti(Occitan), ",
                              _(".-= Retroboot and emulation cores =-."),
                              "genderbent, KMFDManic"," ",
                              _("Support via Discord:") + " https://discord.gg/AHUS3RM",
                              _("This is free software. It works AS IS and We take no responsibility for any issues or damage."),
                              //_("Download latest:") + " https://github.com/autobleem/AutoBleem"
    };


    gui->renderBackground();

    renderer.setDrawColor(ableem::Color(0, 0, 0, 235));
    renderer.setBlendMode(ableem::BlendMode::Blend);

    ableem::Rect rect2;
    rect2.x = 0;
    rect2.y = 0;
    rect2.w = SCREEN_WIDTH;
    rect2.h = SCREEN_HEIGHT;

    renderer.fillRect(rect2);

    fx.render(gui->platform().ticks());

    int yoffset = 150;
    ableem::Rect rect;
    rect.x = SCREEN_WIDTH/2-100;
    rect.y = 5;
    rect.w = 200;
    rect.h = 141;
    renderer.copy(logo, nullptr, &rect);


    int line = 1;
    for (const string &s:credits) {
        gui->renderTextLine(s, line, yoffset, XALIGN_CENTER, 0, font);
        line++;
    }

    gui->renderStatus("|@O| " + _("Go back") + "|",680);
    renderer.present();
}

//*******************************
// GuiAbout::loop
//*******************************
void GuiAbout::loop() {
    std::shared_ptr<Gui> gui(Gui::getInstance());
    menuVisible = true;
    while (menuVisible) {
        render();
        Event e;
        while (gui->input().poll(e)) {
            // this is for pc Only
            if (e.type == Event::Type::Quit) {
                menuVisible = false;
            }
            switch (e.type) {
                case Event::Type::ButtonDown:
                    if (e.button == Button::Circle) {
                        gui->cancel.play();
                        menuVisible = false;

                    };
                    break;
                default:
                    break;

            }

        }
    }
}
