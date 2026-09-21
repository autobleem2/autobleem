//
// Created by screemer on 2018-12-19.
//
#pragma once

#include "../core/main.h"
#include <ableem/ableem.h>
#include <string>
#include <memory>
#include "../core/services/system.h"
#include "gui_font.h"
#include "panel_style.h"

#include <functional>
#include "text_renderer.h"
#include "theme_assets.h"
#include "../core/services/environment.h"

using namespace std;

#define SCREEN_WIDTH ableem::GuiBase::ScreenWidth
#define SCREEN_HEIGHT ableem::GuiBase::ScreenHeight

//********************
// Gui
//********************
// All SDL access lives in lib_ableem; Gui derives from ableem::GuiBase (window/renderer/input/audio) and adds
// the theme's assets, the text renderer, and the few drawing helpers that combine the two (background, logo,
// status bar). Nothing here decides anything: the screens do, and the program's run() shows them.
class Gui : public ableem::GuiBase {
private:
    Gui();
    static float outputScale();
    static int multisampleSamples();
    static bool fullscreen();
    static std::string windowTitle_;

public:
    // the window's title: the program's name. Set by AppBase before the first getInstance() - it cannot
    // change once the window exists
    static void setWindowTitle(const std::string &title) { windowTitle_ = title; }
    // (re)loads the theme's textures and fonts, and its music unless told not to
    void loadAssets(bool reloadMusic = true);

    void display(bool resume);

    void hideMouseCursor();

    void finish();

    static void splash(const std::string &message);

    void criticalException(const std::string &text);

    Gui(Gui const &) = delete;

    Gui &operator=(Gui const &) = delete;

    static std::shared_ptr<Gui> getInstance() {
        static std::shared_ptr<Gui> s{new Gui};
        return s;
    }

    // the theme's textures and fonts
    ThemeAssets &assets() { return assets_; }

    // Hands the display to an emulator: drops every texture and font, then the renderer and the window (on
    // a Pi with no compositor the window is the DRM master and the emulator cannot open the display while it
    // exists). display(true) afterwards notices the window is gone, brings it back and reloads the assets.
    void releaseDisplay();
    // The desktop's alternative: the window minimised for the run and raised again after it - nothing is
    // dropped or reloaded (a WinProcessRunner launch, see ProcessRunner::minimisesLauncherWindow()).
    void minimizeWindow() { platform().minimizeWindow(); }
    void restoreWindow() { platform().restoreWindow(); }
    // the text drawing: lines, columns, option rows with their check icons, the |@X| button markers
    TextRenderer &text() { return text_; }

    void renderFreeSpace();

    void renderBackground();

    // the theme's logo at its place (the splash); small = nothing drawn any more, just where the rows start
    // (the classic screens used to put a third-size logo at the panel's corner; renderHeader replaced it)
    int renderLogo(bool small);

    // the classic screens' panel in the shared look (PanelStyle): the screen dimmed, the sheet over the
    // theme's menu panel rect extended down to the status line, and the header's title ruled off from the
    // rows. renderHeader returns the y the rows start at; renderTextBar draws the sheet alone.
    void renderTextBar();
    int renderHeader(const std::string &title);
    // the status line as the panel's footer, in the shared look (PanelStyle::footer): "Card 1/12" at the
    // right edge, the "|@X| Label" hints from the left. `pos` is what the old status bar took and is ignored.
    void renderStatus(const std::string &text, int pos = -1);

    // the shared look, resolved from the current theme
    PanelStyle panelStyle();

    // A busy state for a long job that runs on the main thread (applying settings, reloading the theme,
    // deleting a game): beginBusy keeps the screen as it is - `redraw` renders and presents it once, and
    // that frame is the backdrop - and busyTick(), called from inside the job wherever it loops, draws the
    // backdrop dimmed with a spinner and the message over it (at most every 40 ms, so a tight loop is not
    // slowed). endBusy drops the backdrop. Gui::tickBusy() is the static form for code without a Gui at
    // hand (the theme loader, the carousel's texture loads) and is a no-op when nothing is busy.
    void beginBusy(const std::string &message, const std::function<void()> &redraw);
    void busyTick();
    // a bar under the spinner's message, done/total (total 0 = no bar); drawn by the next busyTick
    void setBusyProgress(int done, int total);
    void endBusy();
    static void tickBusy();
    // the classic panel: the theme's menu panel rect, its bottom at the status line's foot - or, while a
    // compact panel is set, that one
    ableem::Rect classicPanel();
    // A short list (Memory Cards, the memory card picker, a tool's menu) draws in a compact panel centred
    // on the screen instead of the full one, like a dialog: 800 wide, as tall as its `rows` rows of `font`
    // plus the header and the footer. Set before the screen draws, cleared after present()
    void setCompactPanel(int rows, const ableem::Font &font);
    void clearCompactPanel();
    // the part of it between the header and the footer band: where a screen's rows go
    ableem::Rect classicContent();
    // the footer band at the bottom of the classic panel (PanelStyle::FooterHeight tall)
    ableem::Rect classicFooter();
    // how many rows of `font` fit the content rect, one under the other at the font's line height
    int classicRowsThatFit(const ableem::Font &font);
    // the scroll markers at the content's edges: a triangle at the top when rows are hidden above the
    // first shown, at the bottom when below the last
    void renderScrollMarkers(bool moreAbove, bool moreBelow);

    // the "please wait" screen a program shows around a blocking call it cannot tick from (the tools'
    // network scan, the flasher, the exit): the theme's background and logo, the spinner, the message
    void drawText(const std::string &text, const string &topLine = "");

private:
    void drawBusyFrame();
    // the ring of dots turning about (cx, cy) with `message` under it - the busy frames and drawText share it
    void drawSpinner(int cx, int cy, const std::string &message);

    ThemeAssets assets_;
    TextRenderer text_; // after assets_: it holds references to the theme font and the button textures
    bool compact_ = false;
    ableem::Rect compactPanel_;
    bool busy_ = false;
    int busyDone_ = 0, busyTotal_ = 0;
    std::string busyMessage_;
    ableem::Texture busyBackdrop_;
    unsigned int busyStarted_ = 0, busyLastFrame_ = 0;
};
