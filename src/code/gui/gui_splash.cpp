//
// Created by screemer on 2019-01-24.
//

#include "gui_splash.h"
#include "gui.h"
#include "../lang.h"
#include "../engine/scanner.h"
using namespace std;

//*******************************
// GuiSplash::render
//*******************************
void GuiSplash::render() {
    std::shared_ptr<Gui> gui(Gui::getInstance());
    gui->backgroundImg.setBlendMode(ableem::BlendMode::Blend);
    ableem::Size size = gui->backgroundImg.size();
    gui->backgroundRect.x = 0;
    gui->backgroundRect.y = 0;
    gui->backgroundRect.w = size.w;
    gui->backgroundRect.h = size.h;

    renderer.setDrawColor(ableem::Color(0x00, 0x00, 0x00, 0x00));
    renderer.clear();
    gui->backgroundImg.setAlphaMod(alpha);
    gui->logo.setAlphaMod(alpha);
    gui->music.setVolume(alpha / 3);

    renderer.copy(gui->backgroundImg, nullptr, &gui->backgroundRect);
    renderer.copy(gui->logo, nullptr, &gui->logoRect);

    string bg = app.theme().data.values["text_bg"];

    int bg_alpha = atoi(app.theme().data.values["textalpha"].c_str()) * alpha / 255;

    renderer.setDrawColor(ableem::Color(gui->getR(bg), gui->getG(bg), gui->getB(bg), bg_alpha));
    renderer.setBlendMode(ableem::BlendMode::Blend);
    ableem::Rect rect = gui->getTextRectOfTheme();
    renderer.fillRect(rect);

    int y = atoi(app.theme().data.values["ttop"].c_str());
    string splashText = _("AutoBleem")+" " + app.config().inifile.values["version"];
    gui->renderText(gui->themeFont, splashText, 0, y, XALIGN_CENTER);

    renderer.present();
}

//*******************************
// GuiSplash::loop
//*******************************
void GuiSplash::loop() {
    shared_ptr<Gui> gui(Gui::getInstance());

    gui->music.setVolume(0);
    alpha = 0;
    start = gui->platform().ticks();
    while (1) {
        Event e;
        while (gui->input().poll(e)) {
            if (e.type == Event::Type::Quit)
                break;
            else if (e.type == Event::Type::KeyUp && e.key == Key::Escape)
                break;
        }
        render();
        int current = gui->platform().ticks();
        int time = current - start;
        if (time > 2) {
            if (alpha < 255) {
                alpha += 10;
                if (alpha > 255) {
                    alpha = 255;
                }
            } else {

                break;
            }
            start = gui->platform().ticks();
        }
    }
}
