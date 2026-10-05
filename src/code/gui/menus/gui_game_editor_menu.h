//
// Created by screemer on 2019-01-25.
//
#pragma once

#include "gui/gui_screen.h"
#include "../game_detail_pane.h"
#include "gui/hold_repeat.h"

#include <string>
#include <vector>
#include "../../app.h"
#include "core/main.h"
#include "core/model/ps_game.h"
#include "core/services/game_settings.h"

//********************
// GuiEditor
//********************
// The per-game settings screen. Callers set gameData and show(); the reading and writing of the game's
// Game.ini and pcsx.cfg is GameSettingsService's, this only renders `settings` and steps it with the d-pad.
class GuiEditor : public GuiScreen {
public:
    App &app = App::get(); // the game model, over GuiScreen's AppBase (see gui_screen.h)
    void init() override;
    void draw() override; // the frame's picture: the stack clears before and presents after
    void loop() override;

    PsGamePtr gameData;    // set by the caller before show()
    GameSettings settings; // opened from gameData in init(); still valid after show() returns

    int selOption = 0;    // the OPT_ id of the selected row
    std::string lastName; // an internal game's new title - the caller writes it to the database
    bool changes = false; // the game was renamed

    void processOptionChange(bool direction, bool repeat = false); // repeat: a held key's step - no wrap
    // Cross on the "Unlock the settings" row: after a confirmation, the game's own config (saved in an
    // emulator's menu) is deleted and its pcsx.cfg rows are AutoBleem's to edit again (PcsxConfig)
    void unlockSettings();
    // Options -> PS1 Emulator is pcsx-abnxt: its own rows are live; with the classic pcsx-ab the rows it does
    // not read are shown greyed
    bool nxtEmulator() const;

    // the rows as they read on screen: heading bands ("Game", "Display", "Rendering", "Emulator") between
    // the options, each option a label with its switch or value at the pane's edge. Built by buildRows() on
    // every render from the settings; the cursor moves over the options in this order and the list scrolls
    // when there are more rows than fit. Display is pcsx-abnxt's in-game menu's Picture section, its rows
    // and values, but for Scaling and Display (the output mode), which are global Options
    struct Row {
        enum class Kind { Heading, Bool, Value } kind;
        std::string label;
        std::string value; // Value rows
        bool on = false;   // Bool rows
        int opt = -1;      // the OPT_ id; -1 for a heading
        // greyed, not changeable: the game has its own config (settings.custom), the emulator selected does
        // not read the setting, or another setting rules it out (as pcsx-abnxt's menu greys it)
        bool locked = false;
    };
    std::vector<Row> rows;
    void publishToDriver(int selectedIndex) const; // rows + cursor for the DebugDriver (render())
    int firstVisible = 0;
    void buildRows();
    void moveSelection(int step, bool repeat = false); // to the next/previous option row (a press wraps)
    void selectNear(int index, int dir);               // the option row at index, else the next one in dir, else back
    void pageSelection(int dir);                       // L2/R2: a page of rows up or down
    int selectedRow() const;                           // the index in `rows` of selOption
    // a held d-pad: Up/Down (the cursor) or Left/Right (the value, holdOnValue) - see startHold
    HoldRepeat hold;
    bool holdOnValue = false;
    void startHold(bool value, int step);
    void holdTick();
    void holdStep(int step, bool repeat);

    using GuiScreen::GuiScreen;
    ableem::Texture cover;
    GameDetailPane pane; // the cover and the game's facts on the right
};
