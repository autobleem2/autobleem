//
// Created by screemer on 2019-01-24.
//
#pragma once

#include "gui_options_menu_base.h"
#include "gui/gui.h"
#include "gui/hold_repeat.h"
#include <functional>
#include <string>
#include <vector>

enum {
    CFG_THEME = 0,
    CFG_SHOW_ORIGAMES,
    CFG_JEWEL,
    CFG_MUSIC,
    CFG_ENABLE_BACKGROUND_MUSIC,
    CFG_SCALER, // "Emulator screen scaling" (was the Widescreen switch, config.ini aspect)
    CFG_EMULATOR,
    CFG_RACONFIG,
    CFG_PLAY_ALL_PSX_WITH_RA,
    CFG_ONLINE,
    CFG_UPDATES,
    CFG_SHOWINGTIMEOUT,
    CFG_LANG,
    CFG_THEME_FONT,
    CFG_FONT,
    CFG_KEEPLOGS,
    CFG_PERFOVERLAY,
    CFG_RA_PERSIST,
    CFG_PAD_SWAP,
    CFG_DISPLAY,
    CFG_COVER_SHINE,
    CFG_SPLASH_SCREEN,
    CFG_ANIMATIONS
};
#define CFG_LAST CFG_ANIMATIONS
#define CFG_SIZE (CFG_LAST + 1)
#define CFG_HEADING (-1) // a group heading row: not an option, never selected

//********************
// GuiOptions
//********************
class GuiOptions : public GuiOptionsMenuBase {
public:
    explicit GuiOptions(ableem::GuiBase &_gui) : GuiOptionsMenuBase(_gui) {}

    void init() override;
    // the rows packed at the font's height, scrolling when more than fit (the base's paging), in groups
    // under heading rows the cursor skips
    void draw() override;
    // before each frame: the DebugDriver's rows and cursor, the held row's next step (holdTick)
    bool prepareFrame() override;
    bool skipSelectingThisLineWhenMovingByOne(int index) override { return lines[index].id == CFG_HEADING; }
    void doKeyDown() override;
    void doKeyUp() override;

private:
    void settleOnOption(int direction);
    void stepValue(bool next);     // doKeyRight/doKeyLeft: the row's next/previous value
    std::string outputModeOnEntry; // the Display row's value when the screen opened
    std::string autoLabel;         // "Auto (1080p)", made by getOutputModes()
    bool userFontInUse();          // "Use Default Font" off: the Font row's choice is what is drawn
    // a held Left/Right (see doJoyRight): the step at the press, repeats from render(), the reload a row needs
    // put off to the release
    static HoldRepeat::Timing valueHoldTiming() { return HoldRepeat::rows(); }
    HoldRepeat valueHold;
    bool holdTicking = false;
    bool pendingReload = false;
    int pendingReloadId = 0;
    std::string pendingReloadValue;
    // after the release the reload waits this long for another press on the row, so quick taps in a row load
    // only the value they stop on
    static uint32_t reloadSettleTime() { return 450; }
    uint32_t pendingReloadAt = 0; // 0 while a hold is on (or nothing waits)
    void startHold(int step);
    void holdTick();
    void endHold();
    void flushPendingReload();
    void loadFor(int id, const std::string &nextValue); // the load itself, under the spinner

public:
    std::vector<std::string> getThemes();
    std::vector<std::string> getFonts(); // every .ttf/.otf in Fonts::userFontDirs
    std::vector<std::string> getJewels();
    std::vector<std::string> getMusic();
    std::vector<std::string> getTimeoutValues();
    std::vector<std::string> getOutputModes(); // OutputMode tokens: the console 720/1080, elsewhere auto + EDID

    void fill();

    std::string getTitle() override { return _("Options"); }
    std::string getStatusLine() override;

    std::string valueText(const OptionsInfo &info, const std::string &value) override;
    // the reload a changed row needs (theme, language, font, music), with the spinner over the panel
    void reloadFor(int id, const std::string &nextValue);
    std::string doPrevNextOption(OptionsInfo &info, bool next) override;
    std::string doPrevNextOption(bool next) override { return GuiOptionsMenuBase::doPrevNextOption(next); }
    std::string doRandomOption() override; // only a few lines will use this.  most will just return.

    std::string doOptionIndex(unsigned int index) override;

    int exitCode = 0;
    // set by the screen that opened Options (the launcher): called once after a language or font reload, to take the
    // snapshot under Options again - the old one is of the launcher in the old language and font (BUG-49)
    std::function<void()> backdropRefresh;
    // Options -> Display changed: the OutputMode token to try (config.ini keeps the old one until the new
    // one is confirmed - GuiKeepDisplay); "" when the row was not changed
    std::string newOutputMode;
    // the value config.ini keeps until the new one is confirmed (the launcher puts it back in memory)
    const std::string &previousOutputMode() const { return outputModeOnEntry; }

    void doCircle_Pressed() override;
    void doCross_Pressed() override;

    void doJoyRight() override;  // the value to the right; held, it goes on
    void doJoyLeft() override;   // the value to the left; held, it goes on
    void doJoyCenter() override; // the release: a put-off reload happens now

    void doKeyRight() override; // move option to the right
    void doKeyLeft() override;  // move option to the left

    void doEnter() override { doCross_Pressed(); }
    void doEscape() override { doCircle_Pressed(); }
};
