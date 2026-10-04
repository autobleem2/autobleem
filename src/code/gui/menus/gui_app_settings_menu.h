//
// GuiAppSettings: the "Game settings" screen of an App - what the game editors are for a PS1 or a RetroArch
// game. One row for now: Pad mode.
//
#pragma once
#include "gui/gui_screen.h"
#include "../game_detail_pane.h"
#include "../../app.h"
#include "core/main.h"
#include "core/model/ps_game.h"

//********************
// GuiAppSettings
//********************
// Opens from the Game icon of the launcher's icon row on an App, drawn like GuiEditorRA: the App's picture and
// facts on the right, the rows on the left. Pad mode steps with Left/Right through Automatic (the App's own
// app.ini PadMode=), PSC pad, Xbox 360 pad, PSC pad (system), Xbox 360 pad (system); the choice is saved at once
// in the App's folder (AppSettings) and the launcher hands it to the App as AB_APP_PAD_MODE. Circle leaves.
class GuiAppSettings : public GuiScreen {
public:
    App &app = App::get();
    void init() override;
    void draw() override;
    void loop() override;
    PsGamePtr gameData; // set by the caller before show()
    using GuiScreen::GuiScreen;
    ableem::Texture cover;
    GameDetailPane pane;

private:
    // the Pad mode row's text for `index` (0 = Automatic, then AppSettings::padModes() in order)
    std::string padModeName(int index) const;
    std::string padModeValue() const;
    // Left/Right on the row: the next (+1) or previous (-1) value, wrapping, saved at once
    void stepPadMode(int step);

    std::string appPadMode_; // the App's own app.ini PadMode= ("" = none)
    int padModeIndex_ = 0;   // 0 = Automatic, 1.. the AppSettings::padModes() entry
};
