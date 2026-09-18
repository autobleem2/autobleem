//
// GuiEditorRA: the game editor for a RetroArch game - the one thing it can set is the light-gun flag.
//
#pragma once
#include "../gui_screen.h"
#include "../../core/main.h"
#include "../../core/model/ps_game.h"

//********************
// GuiEditorRA
//********************
// What GuiEditor is for a PS1 game, for a RetroArch playlist entry: its title, file and core, and the
// light-gun flag, which lives in LightgunService's list since a RetroArch game has no Game.ini. Left/Right
// toggle it, Circle leaves. From AutoBleem-NG's gui_gameEditorMenu_RA.
class GuiEditorRA : public GuiScreen {
public:
    void init() override;
    void render() override;
    void loop() override;
    PsGamePtr gameData;   // set by the caller before show()
    bool changed = false; // the flag was toggled - the caller reloads a Lightgun set
    using GuiScreen::GuiScreen;
    ableem::Texture cover;
};
