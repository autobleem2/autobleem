//
// Created by screemer on 2019-01-24.
//

#include "gui_splash.h"
#include "../gui.h"
#include "core/version.h" // generated into the build tree
#include "../../core/model/timing.h"
using namespace std;

//*******************************
// GuiSplash::render
//*******************************
void GuiSplash::render() {
    std::shared_ptr<Gui> gui(Gui::getInstance());
    gui->assets().backgroundImg.setBlendMode(ableem::BlendMode::Blend);

    renderer.setDrawColor(ableem::Color(0x00, 0x00, 0x00, 0x00));
    renderer.clear();
    gui->assets().backgroundImg.setAlphaMod(alpha);
    gui->assets().logo.setAlphaMod(alpha);

    renderer.copy(gui->assets().backgroundImg, nullptr, &gui->assets().backgroundRect);
    renderer.copy(gui->assets().logo, nullptr, &gui->assets().logoRect);

    const ableem::ThemeStatusBar &bar = app.theme().classic().statusBar;
    int bg_alpha = bar.alpha * alpha / 255;

    renderer.setDrawColor(TextRenderer::toColor(bar.color, bg_alpha));
    renderer.setBlendMode(ableem::BlendMode::Blend);
    ableem::Rect rect = gui->text().getTextRectOfTheme();
    renderer.fillRect(rect);

    int y = bar.textY;
    string splashText = _("AutoBleem") + " " + Version::VERSION;
    gui->text().renderText(gui->assets().themeFont, splashText, 0, y, XALIGN_CENTER);

    renderer.present();
}

//*******************************
// GuiSplash::loop
//*******************************
void GuiSplash::loop() {
    shared_ptr<Gui> gui(Gui::getInstance());

    alpha = 0;
    phase = Phase::Settle;
    start = gui->platform().ticks();
    holdStart = start; // the settle phase's start
    while (true) {
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
            if (phase == Phase::Settle) {
                // black frames until the display has had time to sync - see SplashSettleDuration
                if (gui->platform().ticks() - holdStart >= SplashSettleDuration) {
                    phase = Phase::FadeIn;
                }
            } else if (phase == Phase::FadeIn) {
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
            } else { // FadeOut
                if (alpha > 0) {
                    alpha -= 10;
                    if (alpha < 0) {
                        alpha = 0;
                    }
                } else {
                    // render() ties the background/logo alpha to `alpha` for the fade. Texture is a shared
                    // handle - backgroundImg/logo are the same ones every classic screen draws with
                    // gui->renderBackground()/renderLogo() - so leaving them at alpha 0 here would make every
                    // one of those render invisible from now on; the old code never had this problem because
                    // it always ended a fade at alpha 255, never faded back out. Put both back to normal
                    // before handing off to the launcher. Music volume is left alone throughout - it is not
                    // tied to this fade.
                    gui->assets().backgroundImg.setAlphaMod(255);
                    gui->assets().logo.setAlphaMod(255);
                    break;
                }
            }
            start = gui->platform().ticks();
        }
    }
}
