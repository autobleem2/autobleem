//
// Created by screemer on 04.05.2020.
//

#include "dualshock3_info.h"
#include "../abl.h"
#include "../../lang.h"

void dualshock3_info::render()
{
    std::shared_ptr<Gui> gui(Gui::getInstance());
    vector<string> information = {_("DualShock3/SixAxis Wireless Pairing"), "--------------------------------","",
                                  _("In AutoBleem DualShock/SixAxis type controller are automatically paired if you have a compatible BlueTooth dongle and an AutoBleem Kernel installed."),
                                  "",
                                  _("To pair a controller of this kind please follow the procedure"),
                                  "  "+_("1. Using a micro USB charging cable connect the controller to one of available USB FRONT port of the console"),
                                  "  "+_("2. When the leds on the controller will start to blink press the PlayStation Home button"),
                                  "  "+_("3. The controller should be recognized as normal wired PS3 compatible controller"),
                                  "  "+_("4. Disconnect charging cable from the console and press PlayStation Home button again"),
                                  "  "+_("5. After 1-5 seconds controller will auto pair to AutoBleem and show player number LED (similar as paired to PS3)"),
                                  "  "+_("6. If the controller does not react for buttons or analog sticks are setup wrong - use mapping section to reconfigure"),
                                  "",
                                  "  "+_("NOTE 1:Genuine SONY DualShock3 and Sixaxis are supported. Also SHANWAN produced clones should work."),
                                  "",
                                  "  "+_("NOTE 2: In case you pair the controller back to PS3 or other console you have to follow this procedure again."),
                                  "  "+_("NOTE 3: You do not need to do this procedure in this screen. USB pairing works on any screen as soon console is powered on."),
            "  "+_("NOTE 4: Pairing information is saved in console, so next time just press PlayStation Home on the controller and it will work in AutoBleem")
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

    int line = 1;
    for (const string &s:information) {
        gui->renderTextLine(s, line, 0, (line>2?POS_LEFT:POS_CENTER), 0);
        line++;
    }

    gui->renderStatus("|@O| " + _("Go back") + "|",680);
    SDL_RenderPresent(renderer);
};

void dualshock3_info::loop()
{
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
};