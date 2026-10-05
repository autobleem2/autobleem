//
// GuiAppSettings: the "Game settings" screen of an App - what the game editors are for a PS1 or a RetroArch
// game. Rows: Pad mode, D-pad as stick, Stick as d-pad.
//
#pragma once
#include "gui/gui_screen.h"
#include "gui/hold_repeat.h"
#include "value_hold.h"
#include "../game_detail_pane.h"
#include "../../app.h"
#include "core/main.h"
#include "core/model/ps_game.h"

//********************
// GuiAppSettings
//********************
// Opens from the Game icon of the launcher's icon row on an App, drawn like GuiEditorRA: the App's picture and
// facts on the right, the rows on the left. Up/Down picks a row, Left/Right steps its value, the choice is saved at
// once in the App's folder (AppSettings) and the launcher hands it to the App (AB_APP_PAD_MODE, AB_APP_DPAD2ANALOG,
// AB_APP_ANALOG2DPAD). Pad mode: Automatic (the App's own app.ini PadMode=), PSC pad, Xbox 360 pad, PSC pad (system),
// Xbox 360 pad (system). D-pad as stick / Stick as d-pad: Automatic (the App's own Dpad2Analog= / Analog2Dpad=), On,
// Off. Circle leaves.
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

    // a flag row (Dpad2Analog / Analog2Dpad): its label, its value text, a step of it
    struct FlagRow {
        const char *key = nullptr;    // AppSettings::Dpad2AnalogKey / Analog2DpadKey (ab_settings.ini)
        const char *iniKey = nullptr; // the same key as AppManifest::value() takes it (lower case)
        std::string appOwn;           // the App's own app.ini value ("1", "0" or "")
        int index = 0;                // 0 = Automatic, 1 = On, 2 = Off
    };
    std::string flagLabel(int row) const;
    std::string flagValue(const FlagRow &flag) const;
    void stepFlag(FlagRow &flag, int step);

    std::string appPadMode_; // the App's own app.ini PadMode= ("" = none)
    int padModeIndex_ = 0;   // 0 = Automatic, 1.. the AppSettings::padModes() entry
    FlagRow flags_[2];       // D-pad as stick, Stick as d-pad
    int selected_ = 1;       // the row the cursor is on (the heading is row 0)
    DpadHold hold_;          // Up/Down held: the cursor goes on at the shared HoldRepeat pace
    ValueHold valueHold_;    // Left/Right held: the value goes on at the same pace
};
