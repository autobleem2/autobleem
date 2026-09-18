
//

#include "bluetooth_info.h"
#include "../abl.h"
#include "../../lang.h"

void bluetooth_info::render()
{
    std::shared_ptr<Gui> gui(Gui::getInstance());
    vector<string> information = {_("Bluetooth controller pairing"), "--------------------------------","",
                                  _("This section of hardware configuration is not available yet."),
                                  "",
                                  _("We are working on implementing Bluetooth pairing on console for a next release"),
                                  _("Until then please use BlueTool by DanTheMan (Included in the package)"),
                                  "",
                                  "",
                                  "  "+_("NOTE 1: After pairing in BlueTool controller is automatically mapped until you unpair it"),
                                  ""
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

void bluetooth_info::loop()
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