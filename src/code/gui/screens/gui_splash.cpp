//
// Created by screemer on 2019-01-24.
//

#include "gui_splash.h"
#include "../gui.h"
#include "core/version.h" // generated into the build tree
#include "../../core/model/timing.h"
#include "../../core/services/environment.h"

#include <cstdlib>
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
    // Texture is a shared handle: these are the very textures every classic screen draws its background
    // and logo with (Gui::renderBackground/renderLogo), so the fade's alpha must not outlive this frame -
    // show() renders once before loop(), and loop() may return at once (AB_NO_SPLASH), which used to
    // leave both at alpha 0 and every panel until the next asset reload on plain black
    gui->assets().backgroundImg.setAlphaMod(255);
    gui->assets().logo.setAlphaMod(255);

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
#ifdef AB_DEBUG_HOST
    // AB_NO_SPLASH=1: straight through - the DebugDriver's tests (tools/ab_drive.py) start that way
    if (const char *skip = getenv("AB_NO_SPLASH")) {
        if (*skip == '1')
            return;
    }
#endif

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
                    break; // faded out; render() has put the shared textures back to alpha 255 already
                }
            }
            start = gui->platform().ticks();
        }
    }
}
