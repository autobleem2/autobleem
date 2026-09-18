//
// Created by screemer on 2019-01-24.
//
#pragma once

#include "gui_options_menu_base.h"
#include "../gui.h"
#include <string>
#include <vector>

enum {
    CFG_THEME = 0,
    CFG_SHOW_ORIGAMES,
    CFG_JEWEL,
    CFG_MUSIC,
    CFG_ENABLE_BACKGROUND_MUSIC,
    CFG_WIDESCREEN,
    CFG_GFX_FILTER,
    CFG_RACONFIG,
    CFG_PLAY_ALL_PSX_WITH_RA,
    CFG_SHOWINGTIMEOUT,
    CFG_LANG
};
#define CFG_LAST CFG_LANG
#define CFG_SIZE (CFG_LAST + 1)

//********************
// GuiOptions
//********************
class GuiOptions : public GuiOptionsMenuBase {
public:
    explicit GuiOptions(ableem::GuiBase &_gui) : GuiOptionsMenuBase(_gui) {}

    void init() override;
    // void render() override;
    // void loop() override;

    std::vector<std::string> getThemes();
    std::vector<std::string> getJewels();
    std::vector<std::string> getMusic();
    std::vector<std::string> getTimeoutValues();

    void fill();

    std::string getTitle() override { return "-=" + _("Configuration") + "=-"; }
    std::string getStatusLine() override;

    std::string getLineText(const OptionsInfo &info) override;
    std::string doPrevNextOption(OptionsInfo &info, bool next) override;
    std::string doPrevNextOption(bool next) override { return GuiOptionsMenuBase::doPrevNextOption(next); }
    std::string doRandomOption() override; // only a few lines will use this.  most will just return.

    std::string doOptionIndex(unsigned int index) override;

    int exitCode = 0;

    void doCircle_Pressed() override;
    void doCross_Pressed() override;

    void doJoyRight() override; // move option to the right, may fast forwward
    void doJoyLeft() override;  // move option to the left, may fast forwward

    void doKeyRight() override; // move option to the right
    void doKeyLeft() override;  // move option to the left

    void doEnter() override { doCross_Pressed(); }
    void doEscape() override { doCircle_Pressed(); }
};
