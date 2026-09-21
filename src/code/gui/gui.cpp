//
// Created by screemer on 2018-12-19.
//

#include "gui.h"
#include "screens/gui_splash.h"
#include "../app_base.h"
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
#elif defined(AB_APPLIANCE) || defined(AB_PLATFORM_WIN)
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
// window's GL context. 4x on a dev host and a Windows PC. None on an appliance: measured on a Pi 400 at 1080p
// (2026-09-18), even 2x misses vsync and halves the frame rate for stretches, where 0x holds 60 fps with an 18 ms worst
// frame - the covers get their smooth edges from the transparent margin PsCarouselGame composes them
// with instead. AB_MSAA in the environment overrides either. Not on the console: whether its GL driver
// has it is unknown until the build has run there.
int Gui::multisampleSamples() {
#if !defined(AB_PLATFORM_PSC)
    const char *env = getenv("AB_MSAA");
    if (env) {
        return atoi(env);
    }
#endif
#if defined(AB_DEBUG_HOST) || defined(AB_PLATFORM_WIN)
    return 4;
#else
    return 0;
#endif
}

//********************
// Gui::fullscreen
//********************
// The whole screen on every real target - a console-like launcher has no window to be a window in. The
// console and the appliances already are (Wayland on the PSC, KMS/DRM on a Pi and the PC stick: the window
// is the display); on the Windows product it is SDL's desktop full screen. Only the dev build keeps its
// 1280x720 window (tools/win_drive.ps1 posts keys to it); AB_WINDOWED=1 in the environment asks a product
// build for one too, for a look.
bool Gui::fullscreen() {
#if defined(AB_DEBUG_HOST)
    return false;
#else
    return getenv("AB_WINDOWED") == nullptr;
#endif
}

//********************
// Gui::Gui
//********************
string Gui::windowTitle_ = "AutoBleem";

Gui::Gui()
    : ableem::GuiBase(windowTitle_, ScreenWidth, ScreenHeight, outputScale(), multisampleSamples(), fullscreen()),
      assets_(renderer(), AppBase::get().theme(), AppBase::get().config()),
      text_(renderer(), AppBase::get().theme(), assets_.themeFont, assets_.buttonTextureMap) {
    // the pad mappings the launcher and the pscbios wizard share; probePads() reads the first that exists
    input().loadMappings(Env::padMappingFiles());
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
    AppBase::get().audio().loadTheme(reloadMusic);

    // the classic screens' text halo, on unless the theme says otherwise; the launcher sets its own
    // around its frame and puts this one back
    TextRenderer::Shadow shadow;
    const ableem::Opt<bool> &textShadow = AppBase::get().theme().classic().textShadow;
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
    }
}

//*******************************
// Gui::finish
//*******************************
void Gui::finish() {
    AppBase::get().audio().shutdown();
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
    // at the header's right edge, in the secondary colour, level with the title (the theme's
    // classic.freeSpaceText used to place it; the panel's header decides now)
    PanelStyle style = panelStyle();
    Rect panel = classicPanel();
    const string line = _("Free space") + " : " + System::getAvailableSpace();
    const ableem::Font &font = assets_.themeFonts[FONT_22_MED];
    const int y = panel.y + 18 + (assets_.themeFonts[FONT_28_BOLD].lineHeight() - font.lineHeight()) / 2;
    text_.renderText_WithColor(font, line, panel.x + panel.w - PanelStyle::RowInset - text_.textWidth(font, line), y,
                               style.text, XALIGN_LEFT);
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
    }
    return classicPanel().y + PanelStyle::HeaderHeight;
}

//*******************************
// Gui::panelStyle / classicPanel
//*******************************
PanelStyle Gui::panelStyle() {
    return PanelStyle::fromTheme(AppBase::get().theme().launcher());
}

Rect Gui::classicPanel() {
    Rect panel = text_.getOpscreenRectOfTheme();
    // the footer band ends where the theme's status line used to end
    const int statusFoot = AppBase::get().theme().classic().statusBar.textY + PanelStyle::FooterHeight - 14;
    if (statusFoot > panel.y + panel.h)
        panel.h = statusFoot - panel.y;
    return panel;
}

Rect Gui::classicContent() {
    Rect panel = classicPanel();
    return Rect(panel.x, panel.y + PanelStyle::HeaderHeight, panel.w,
                panel.h - PanelStyle::HeaderHeight - PanelStyle::FooterHeight);
}

Rect Gui::classicFooter() {
    Rect panel = classicPanel();
    return Rect(panel.x, panel.y + panel.h - PanelStyle::FooterHeight, panel.w, PanelStyle::FooterHeight);
}

//*******************************
// Gui::renderTextBar
//*******************************
void Gui::renderTextBar() {
    PanelStyle style = panelStyle();
    style.dim(renderer());
    style.sheet(renderer(), classicPanel());
}

//*******************************
// Gui::renderHeader
//*******************************
int Gui::renderHeader(const string &title) {
    return panelStyle().header(*this, classicPanel(), title);
}

//*******************************
// Gui::renderStatus
//*******************************
void Gui::renderStatus(const string &text, int /*posy*/) {
    panelStyle().footer(*this, classicFooter(), text);
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
