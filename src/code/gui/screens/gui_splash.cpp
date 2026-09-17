//
// Created by screemer on 2019-01-24.
//

#include "gui_splash.h"
#include "../gui.h"
using namespace std;

//*******************************
// GuiSplash::render
//*******************************
void GuiSplash::render() {
    std::shared_ptr<Gui> gui(Gui::getInstance());
    gui->assets().backgroundImg.setBlendMode(ableem::BlendMode::Blend);
    ableem::Size size = gui->assets().backgroundImg.size();
    gui->assets().backgroundRect.x = 0;
    gui->assets().backgroundRect.y = 0;
    gui->assets().backgroundRect.w = size.w;
    gui->assets().backgroundRect.h = size.h;

    renderer.setDrawColor(ableem::Color(0x00, 0x00, 0x00, 0x00));
    renderer.clear();
    gui->assets().backgroundImg.setAlphaMod(alpha);
    gui->assets().logo.setAlphaMod(alpha);
    app.audio().music.setVolume(alpha / 3);

    renderer.copy(gui->assets().backgroundImg, nullptr, &gui->assets().backgroundRect);
    renderer.copy(gui->assets().logo, nullptr, &gui->assets().logoRect);

    const ableem::ThemeStatusBar &bar = app.theme().classic().statusBar;
    int bg_alpha = bar.alpha * alpha / 255;

    renderer.setDrawColor(TextRenderer::toColor(bar.color, bg_alpha));
    renderer.setBlendMode(ableem::BlendMode::Blend);
    ableem::Rect rect = gui->text().getTextRectOfTheme();
    renderer.fillRect(rect);

    int y = bar.textY;
    string splashText = _("AutoBleem")+" " + app.config().inifile.values["version"];
    gui->text().renderText(gui->assets().themeFont, splashText, 0, y, XALIGN_CENTER);

    renderer.present();
}

//*******************************
// GuiSplash::loop
//*******************************
void GuiSplash::loop() {
    shared_ptr<Gui> gui(Gui::getInstance());

    app.audio().music.setVolume(0);
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
