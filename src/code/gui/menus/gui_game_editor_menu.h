//
// Created by screemer on 2019-01-25.
//
#pragma once

#include "gui/gui_screen.h"
#include "../game_detail_pane.h"

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
    void render() override;
    void loop() override;

    PsGamePtr gameData;    // set by the caller before show()
    GameSettings settings; // opened from gameData in init(); still valid after show() returns

    int selOption = 0;    // the OPT_ id of the selected row
    std::string lastName; // an internal game's new title - the caller writes it to the database
    bool changes = false; // the game was renamed

    void processOptionChange(bool direction);
    bool nxtEmulator() const; // Options -> PS1 Emulator is pcsx-abnxt: its own rows (Smoothing) are shown

    // the rows as they read on screen: heading bands ("Game", "Video", "Emulator") between the options,
    // each option a label with its switch or value at the pane's edge. Built by buildRows() on every
    // render from the settings; the cursor moves over the options in this order and the list scrolls
    // when there are more rows than fit
    struct Row {
        enum class Kind { Heading, Bool, Value } kind;
        std::string label;
        std::string value; // Value rows
        bool on = false;   // Bool rows
        int opt = -1;      // the OPT_ id; -1 for a heading
    };
    std::vector<Row> rows;
    int firstVisible = 0;
    void buildRows();
    void moveSelection(int step); // to the next/previous option row
    int selectedRow() const;      // the index in `rows` of selOption

    using GuiScreen::GuiScreen;
    ableem::Texture cover;
    GameDetailPane pane; // the cover and the game's facts on the right
};
