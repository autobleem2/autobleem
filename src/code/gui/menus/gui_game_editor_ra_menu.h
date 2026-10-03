//
// GuiEditorRA: the game editor for a RetroArch game - the light-gun flag and the core that plays it.
//
#pragma once
#include "gui/gui_screen.h"
#include "../game_detail_pane.h"
#include "../../app.h"
#include "core/main.h"
#include "core/model/ps_game.h"

#include <ableem/engine/retroarch_cores.h>

//********************
// GuiEditorRA
//********************
// What GuiEditor is for a PS1 game, for a RetroArch playlist entry: its title and file, the light-gun flag,
// which lives in LightgunService's list since a RetroArch game has no Game.ini, and the core. Up/Down move
// between the two rows, Left/Right change the one the cursor is on: the flag toggles, the core cycles through
// the installed cores that play the game's system - the platform's default first, marked "(default)" - and the
// pick is written into the game's playlist entry (RetroArchService::setGameCore), where the launch reads it.
// Circle leaves. From AutoBleem-NG's gui_gameEditorMenu_RA.
class GuiEditorRA : public GuiScreen {
public:
    App &app = App::get(); // the game model, over GuiScreen's AppBase (see gui_screen.h)
    void init() override;
    void draw() override; // the frame's picture: the stack clears before and presents after
    void loop() override;
    PsGamePtr gameData;   // set by the caller before show()
    bool changed = false; // the flag was toggled - the caller reloads a Lightgun set
    using GuiScreen::GuiScreen;
    ableem::Texture cover;
    GameDetailPane pane;

private:
    // the Core row's text for the core at `coreIndex`: its short name, and "(default)" on the first
    std::string coreValue() const;
    // Left/Right on the Core row: the next (+1) or previous (-1) core, wrapping, written to the playlist
    void cycleCore(int step);

    ableem::CoreInfos cores_; // what plays the game's system, the default first
    int coreIndex = 0;        // the one the game uses now
    int selOption = 1;        // the row the cursor is on: 1 the light-gun flag, 2 the core
};
