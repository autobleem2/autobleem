//
// Created by screemer on 2019-01-24.
//
#pragma once

#include "gui_options_menu_base.h"
#include "gui/gui.h"
#include <string>
#include <vector>

enum {
    CFG_THEME = 0,
    CFG_SHOW_ORIGAMES,
    CFG_JEWEL,
    CFG_MUSIC,
    CFG_ENABLE_BACKGROUND_MUSIC,
    CFG_WIDESCREEN,
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
    CFG_COVER_SHINE
};
#define CFG_LAST CFG_COVER_SHINE
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
    void render() override;
    bool skipSelectingThisLineWhenMovingByOne(int index) override { return lines[index].id == CFG_HEADING; }
    void doKeyDown() override;
    void doKeyUp() override;

private:
    void settleOnOption(int direction);
    std::string outputModeOnEntry; // the Display row's value when the screen opened
    std::string autoLabel;         // "Auto (1080p)", made by getOutputModes()
    bool stepsOnePerPress();       // Left/Right without the held-button repeat on this row
    bool userFontInUse();          // "Use Default Font" off: the Font row's choice is what is drawn

public:
    std::vector<std::string> getThemes();
    std::vector<std::string> getFonts(); // every .ttf/.otf in Fonts::userFontDirs
    std::vector<std::string> getJewels();
    std::vector<std::string> getMusic();
    std::vector<std::string> getTimeoutValues();
    std::vector<std::string> getOutputModes(); // OutputMode tokens: the console 720/1080, elsewhere auto + EDID

    void fill();

    std::string getTitle() override { return _("Configuration"); }
    std::string getStatusLine() override;

    std::string valueText(const OptionsInfo &info, const std::string &value) override;
    // the reload a changed row needs (theme, language, font, music), with the spinner over the panel
    void reloadFor(int id, const std::string &nextValue);
    std::string doPrevNextOption(OptionsInfo &info, bool next) override;
    std::string doPrevNextOption(bool next) override { return GuiOptionsMenuBase::doPrevNextOption(next); }
    std::string doRandomOption() override; // only a few lines will use this.  most will just return.

    std::string doOptionIndex(unsigned int index) override;

    int exitCode = 0;
    // Options -> Display changed: the OutputMode token to try (config.ini keeps the old one until the new
    // one is confirmed - GuiKeepDisplay); "" when the row was not changed
    std::string newOutputMode;
    // the value config.ini keeps until the new one is confirmed (the launcher puts it back in memory)
    const std::string &previousOutputMode() const { return outputModeOnEntry; }

    void doCircle_Pressed() override;
    void doCross_Pressed() override;

    void doJoyRight() override; // move option to the right, may fast forwward
    void doJoyLeft() override;  // move option to the left, may fast forwward

    void doKeyRight() override; // move option to the right
    void doKeyLeft() override;  // move option to the left

    void doEnter() override { doCross_Pressed(); }
    void doEscape() override { doCircle_Pressed(); }
};
