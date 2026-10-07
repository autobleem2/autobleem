//
// The RetroArch manager (System menu -> "RetroArch...", Quick menu -> "Install RetroArch..."): GuiRaManager shows
// what is installed and offers Install / Update / Remove / "Also delete the BIOS files", GuiRaJobProgress shows the
// job RaJobService runs - the same panels as the online update's (evoui_update.h), over the launcher's dimmed
// background. What the rows are and what the texts say is ra_manager_logic.h's (pure, tested).
//
#pragma once

#include "gui/gui_screen.h"
#include "gui/panel_style.h"
#include "ra_manager_logic.h"

#include <string>
#include <vector>

//******************
// GuiRaManager
//******************
// The panel: a title, the state lines (installed version and size, or "Not installed"; "Needs a network connection"
// with no default route), and a row per action with its description. Cross asks the question and runs the job;
// Circle leaves. `changed` is true when a job ran (even a stopped or failed one - it may have changed what is on the
// stick): the caller reloads what depends on RetroArch.
class GuiRaManager : public GuiScreen {
public:
    void init() override;
    void draw() override; // the frame's picture: the stack clears before and presents after
    void loop() override;

    bool changed = false;

    using GuiScreen::GuiScreen;

private:
    void rebuild();                      // the rows from what the inspection found
    void run(const RaManager::Row &row); // the question, then the job and its progress panel
    void publish() const;

    RaJobService::Inspection state_;
    std::vector<RaManager::Row> rows_;
    int selected_ = 0;
    PanelStyle style;
};

//******************
// GuiRaJobProgress
//******************
// Polls the job every frame: its title, the phase (i/n and the bytes), the bar, the runner's last log lines. Circle
// asks "Stop?" and, on yes, sends the runner SIGTERM. When the job is over its result stays until a button.
class GuiRaJobProgress : public GuiScreen {
public:
    void init() override;
    void draw() override;
    void loop() override;

    RaJobService::Status finalStatus; // what the job ended in

    using GuiScreen::GuiScreen;

private:
    RaJobService::Status status_;
    PanelStyle style;
};
