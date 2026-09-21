//
// Created by screemer on 2018-12-19.
//

#include "gui.h"
#include <cmath>
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
    text_.setCheckIconRightMargin(assets_.checkIconRightMargin);
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
// Gui::beginBusy / busyTick / endBusy / tickBusy
//*******************************
void Gui::beginBusy(const string &message, const std::function<void()> &redraw) {
    busyMessage_ = message;
    renderer().captureNextFrame();
    redraw(); // presents, and the capture is that frame
    busyBackdrop_ = renderer().lastCapture();
    busy_ = true;
    busyStarted_ = platform().ticks();
    busyLastFrame_ = 0;
    drawBusyFrame();
}

void Gui::busyTick() {
    if (!busy_)
        return;
    const unsigned int now = platform().ticks();
    if (busyLastFrame_ != 0 && now - busyLastFrame_ < 40)
        return;
    drawBusyFrame();
}

void Gui::endBusy() {
    busy_ = false;
    busyBackdrop_ = Texture();
}

void Gui::tickBusy() {
    getInstance()->busyTick();
}

void Gui::drawBusyFrame() {
    const unsigned int now = platform().ticks();
    busyLastFrame_ = now;
    // the pads' events pile up meanwhile; nothing reads them until the job is done
    renderer().setDrawColor(Color(0, 0, 0, 255));
    renderer().clear();
    if (busyBackdrop_.valid())
        renderer().copy(busyBackdrop_, nullptr, nullptr);
    PanelStyle style = panelStyle();
    style.dim(renderer());

    // the spinner: twelve dots on a ring, the brightest leading, turning a dot every 70 ms
    const int cx = ScreenWidth / 2, cy = ScreenHeight / 2 - 20;
    const int radius = 30, dot = 8;
    const int lead = static_cast<int>((now - busyStarted_) / 70) % 12;
    renderer().setBlendMode(ableem::BlendMode::Blend);
    for (int i = 0; i < 12; i++) {
        const int behind = (lead - i + 12) % 12; // 0 for the leading dot, 11 for the one just ahead of it
        const int alpha = 255 - behind * 19;
        const double a = i * 3.14159265 / 6.0;
        const int x = cx + static_cast<int>(radius * cos(a)) - dot / 2;
        const int y = cy + static_cast<int>(radius * sin(a)) - dot / 2;
        renderer().setDrawColor(Color(style.text.r, style.text.g, style.text.b, static_cast<unsigned char>(alpha)));
        renderer().fillRect(Rect(x, y, dot, dot));
    }
    if (!busyMessage_.empty())
        text_.renderText_WithColor(assets_.themeFonts[FONT_22_MED], busyMessage_, cx, cy + radius + 24, style.text,
                                   XALIGN_CENTER);
    renderer().present();
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

int Gui::classicRowsThatFit(const ableem::Font &font) {
    const int lineHeight = font.valid() ? font.lineHeight() : assets_.themeFont.lineHeight();
    return std::max(1, (classicContent().h - 4) / std::max(1, lineHeight));
}

void Gui::renderScrollMarkers(bool moreAbove, bool moreBelow) {
    PanelStyle style = panelStyle();
    Rect content = classicContent();
    const int cx = content.x + content.w - PanelStyle::RowInset;
    if (moreAbove)
        style.scrollMarker(renderer(), cx, content.y - 4, -1);
    if (moreBelow)
        style.scrollMarker(renderer(), cx, content.y + content.h - 6, 1);
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
