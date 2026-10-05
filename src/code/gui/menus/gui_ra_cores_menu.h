//
// GuiRaCores: the Quick menu's "RetroArch cores" window - the core that plays each system, for every system two
// or more installed cores play.
//
#pragma once
#include "gui/gui_screen.h"
#include "gui/hold_repeat.h"
#include "value_hold.h"
#include "../../app.h"
#include "core/main.h"
#include "core/services/retroarch.h"

#include <string>
#include <vector>

//********************
// GuiRaCores
//********************
// One row per system (sorted by name) with the core in use: Left/Right cycle that system's cores, the platform
// file's own pick first and marked "(default)", any other marked "(changed)"; Up/Down move, L1/R1 jump to the
// first/last row, L2/R2 page. Circle leaves and saves (RetroArchService::saveCorePicks): the choices go into the
// user's file, and the playlist entries still on the system's old core move to the new one.
class GuiRaCores : public GuiScreen {
public:
    App &app = App::get(); // the game model, over GuiScreen's AppBase (see gui_screen.h)
    void init() override;
    void draw() override; // the frame's picture: the stack clears before and presents after
    void loop() override;
    using GuiScreen::GuiScreen;

    // a system's core was changed - the games' cores moved, the caller reloads what it shows
    bool changed = false;

    // the row's value as drawn: the short name of the core, then "(default)" on the platform's own pick, else
    // "(changed)"
    static std::string valueText(const RACorePlatform &row, int choice);

private:
    void change(int step, bool repeat = false); // the core on the row (a press wraps, a repeat stops)
    void move(int step, bool repeat = false);   // the cursor a row (a press wraps, a repeat stops)
    void select(int row);
    void publish() const;
    void save();
    int visibleRows() const; // the rows that fit under the heading

    std::vector<RACorePlatform> rows_;
    std::vector<int> start_; // each row's core when the window opened
    int selected_ = 0;
    int firstVisible_ = 0;
    DpadHold hold_;       // Up/Down held: the cursor goes on at the shared HoldRepeat pace
    ValueHold valueHold_; // Left/Right held: the value goes on at the same pace
};
