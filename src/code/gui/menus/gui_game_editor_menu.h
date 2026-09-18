//
// Created by screemer on 2019-01-25.
//
#pragma once

#include "../gui_screen.h"
#include "../../app.h"
#include "../../core/main.h"
#include "../../core/model/ps_game.h"
#include "../../core/services/game_settings.h"

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

    int selOption = 5;
    std::string lastName; // an internal game's new title - the caller writes it to the database
    bool changes = false; // the game was renamed

    void processOptionChange(bool direction);

    using GuiScreen::GuiScreen;
    ableem::Texture cover;
};
