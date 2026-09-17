//
// Created by screemer on 2019-01-24.
//

#include "gui_splash.h"
#include "../gui.h"
#include "../../core/model/timing.h"
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
    phase = Phase::FadeIn;
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
            if (phase == Phase::FadeIn) {
                if (alpha < 255) {
                    alpha += 10;
                    if (alpha > 255) {
                        alpha = 255;
                    }
                } else {
                    phase = Phase::Hold;
                    holdStart = gui->platform().ticks();
                }
            } else if (phase == Phase::Hold) {
                if (gui->platform().ticks() - holdStart >= SplashHoldDuration) {
                    phase = Phase::FadeOut;
                }
            } else {   // FadeOut
                if (alpha > 0) {
                    alpha -= 10;
                    if (alpha < 0) {
                        alpha = 0;
                    }
                } else {
                    // render() ties the background/logo alpha and the music volume to `alpha` for the fade.
                    // Texture is a shared handle - backgroundImg/logo are the same ones every classic screen
                    // draws with gui->renderBackground()/renderLogo() - so leaving them at alpha 0 here would
                    // make every one of those render invisible from now on; the old code never had this
                    // problem because it always ended a fade at alpha 255, never faded back out. Put both
                    // back to normal before handing off to the launcher.
                    gui->assets().backgroundImg.setAlphaMod(255);
                    gui->assets().logo.setAlphaMod(255);
                    app.audio().music.setVolume(128);   // SDL_mixer's own max
                    break;
                }
            }
            start = gui->platform().ticks();
        }
    }
}
