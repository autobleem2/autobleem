//
// Created by screemer on 2018-12-19.
//

#include "gui.h"
#include "screens/gui_splash.h"
#include "../app.h"
#include <unistd.h>
#include <iostream>
#include <iomanip>
#include <cstring>
#include <cstdlib>
#include <cassert>
#include <ableem/engine/log.h>

using namespace std;
using ableem::Button;
using ableem::Color;
using ableem::Event;
using ableem::Rect;
using ableem::Size;
using ableem::Texture;
//********************
// Gui::outputScale
//********************
// How much bigger than the 1280x720 canvas the window is. The console's display is 720p and its SDL is old, so
// there it is always 1. A Raspberry Pi on a 1080p (or bigger) display gets 1.5: the launcher then draws its
// covers and text at the display's resolution instead of being upscaled by the TV (see ableem::Renderer). On
// a dev host AB_OUTPUT_SCALE in the environment tries any scale in a window that size.
float Gui::outputScale() {
#if defined(AB_DEBUG_HOST)
    const char *env = getenv("AB_OUTPUT_SCALE");
    if (env && atof(env) >= 1.0) {
        return static_cast<float>(atof(env));
    }
#elif defined(AB_PLATFORM_RPI)
    ableem::Size display = ableem::Platform::desktopDisplaySize();
    if (display.w >= 1920 && display.h >= 1080) {
        PLOG_INFO << "Display is " << display.w << "x" << display.h << ", drawing the 1280x720 UI at 1.5x";
        return 1.5f;
    }
#endif
    return 1.0f;
}

//********************
// Gui::multisampleSamples
//********************
// Anti-aliasing for the carousel's turned covers and everything else the renderer draws: MSAA on the
// window's GL context, on a Pi and a dev host (AB_MSAA in the environment overrides the default there - 0
// turns it off). Not on the console: whether its GL driver has it is unknown until the build has run
// there, and it would cost fill rate on a GPU that has little.
int Gui::multisampleSamples() {
#if defined(AB_DEBUG_HOST) || defined(AB_PLATFORM_RPI)
    const char *env = getenv("AB_MSAA");
    if (env) {
        return atoi(env);
    }
    return 4;
#else
    return 0;
#endif
}

//********************
// Gui::Gui
//********************
Gui::Gui()
    : ableem::GuiBase("AutoBleem", ScreenWidth, ScreenHeight, outputScale(), multisampleSamples()),
      assets_(renderer(), App::get().theme(), App::get().config()),
      text_(renderer(), App::get().theme(), assets_.themeFont, assets_.buttonTextureMap) {
    input().probePads();
}

//*******************************
// Gui::splash
//*******************************
void Gui::splash(const string &message) {
    shared_ptr<Gui> gui(Gui::getInstance());
    gui->drawText(message);
}

//*******************************
// Gui::loadAssets
//*******************************
void Gui::loadAssets(bool reloadMusic) {
    text_.clearTextCache(); // keyed on the font handles about to be replaced
    assets_.load();
    App::get().audio().loadTheme(reloadMusic);

    // the classic screens' text halo, on unless the theme says otherwise; the launcher sets its own
    // around its frame and puts this one back
    TextRenderer::Shadow shadow;
    const ableem::Opt<bool> &textShadow = App::get().theme().classic().textShadow;
    shadow.enabled = !textShadow.set || textShadow;
    text_.setShadow(shadow);
    text_.setFonts(&assets_.themeFonts);
}

//*******************************
// Gui::hideMouseCursor
//*******************************
void Gui::hideMouseCursor() {
    if (!platform().isDevHost()) {
        platform().hideAndGrabCursor();
    }
}

//*******************************
// Gui::criticalException
//*******************************
void Gui::criticalException(const string &text) {
    drawText(text);
    while (true) {
        Event e;
        while (input().poll(e)) {
            if (e.type == Event::Type::Quit)
                return;
            else if (e.type == Event::Type::KeyUp && e.key == ableem::Key::Escape)
                return;

            if (e.type == Event::Type::ButtonDown) {
                return;
            }
        }
    }
}

//*******************************
// Gui::display
//*******************************
void Gui::display(bool resume) {
    PLOG_INFO << platform().versionString();

    if (!platform().hasDisplay()) {
        acquireDisplay(); // released for an emulator - see releaseDisplay()
    }
    platform().setScaleQuality(2);

    loadAssets();

    if (!resume) {
        GuiSplash splashScreen(*this);
        splashScreen.show();
        hideMouseCursor();
    } else {
        App::get().session().resumingGui = true;
    }
}

//*******************************
// Gui::finish
//*******************************
void Gui::finish() {
    App::get().audio().shutdown();
    assets_.backgroundImg = Texture();
}

//*******************************
// Gui::releaseDisplay
//*******************************
void Gui::releaseDisplay() {
    text_.clearTextCache();
    assets_.unload(); // before the renderer goes: SDL frees the textures with it
    GuiBase::releaseDisplay();
}

//*******************************
// Gui::renderFreeSpace
//*******************************
void Gui::renderFreeSpace() {
    const ableem::ThemePoint &pos = App::get().theme().classic().freeSpaceText;
    text_.renderText(assets_.themeFont, _("Free space") + " : " + System::getAvailableSpace(), pos.x, pos.y);
}

//*******************************
// Gui::renderBackground
//*******************************
void Gui::renderBackground() {
    renderer().setDrawColor(Color(0x00, 0x00, 0x00, 0x00));
    renderer().clear();
    renderer().copy(assets_.backgroundImg, nullptr, &assets_.backgroundRect);
}

//*******************************
// Gui::renderLogo
//*******************************
int Gui::renderLogo(bool small) {
    if (!small) {
        renderer().copy(assets_.logo, nullptr, &assets_.logoRect);
        return 0;
    } else {
        Rect rect;
        rect.x = App::get().theme().classic().menuPanel.x;
        rect.y = App::get().theme().classic().menuPanel.y;
        rect.w = assets_.logoRect.w / 3;
        rect.h = assets_.logoRect.h / 3;
        renderer().copy(assets_.logo, nullptr, &rect);
        return rect.y + rect.h;
    }
}

//*******************************
// Gui::renderStatus
//*******************************
void Gui::renderStatus(const string &text, int posy) {
    const ableem::ThemeStatusBar &bar = App::get().theme().classic().statusBar;

    renderer().setDrawColor(TextRenderer::toColor(bar.color, bar.alpha));
    renderer().setBlendMode(ableem::BlendMode::Blend);
    Rect rect = text_.getTextRectOfTheme();
    renderer().fillRect(rect);

    int y = bar.textY;
    if (posy != -1)
        y = posy; // override the bottom status y position.  so far this has never been used.

    text_.renderText(assets_.themeFont, text, 0, y, XALIGN_CENTER);
}

//*******************************
// Gui::renderTextBar
//*******************************
void Gui::renderTextBar() {
    const ableem::ThemePanel &panel = App::get().theme().classic().menuPanel;
    renderer().setDrawColor(TextRenderer::toColor(panel.color, panel.alpha));
    renderer().setBlendMode(ableem::BlendMode::Blend);

    Rect rect2 = text_.getOpscreenRectOfTheme();

    renderer().fillRect(rect2);
}

//*******************************
// Gui::drawText
//*******************************
void Gui::drawText(const string &text, const string &topLine) {
    renderBackground();
    renderLogo(false);
    renderStatus(text);
    renderStatus(topLine, 5);
    renderer().present();
}
