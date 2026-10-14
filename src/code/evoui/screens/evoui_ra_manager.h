//
// The RetroArch manager's screens (PLATFORM-24): GuiRaManager is the panel of the System menu's "RetroArch..." row -
// what is installed, and the rows Install / Update / Remove (and "Also delete the BIOS files" while Remove is the
// chosen row); GuiRaProgress follows a RaJobService job - phase i/n, a title per phase key, a bar, Circle to stop.
// Both are panels over the launcher's dimmed background, like GuiUpdatePrompt / GuiUpdateProgress.
//
#pragma once

#include "gui/gui_screen.h"
#include "gui/screens/gui_confirm.h"
#include "gui/panel_style.h"
#include "../ra_job.h"
#include "../ra_job_service.h"

#include <string>
#include <vector>

//******************
// RaChoice
//******************
enum class RaChoice { None, Install, Update, Remove, RemoveWithBios };

//******************
// GuiRaMessage
//******************
// A plain message with one OK (Cross, Enter or Circle, Escape): GuiConfirm's look with the one answer in the footer.
// The caller sets `label` and `title`, then show().
class GuiRaMessage : public GuiConfirm {
public:
    explicit GuiRaMessage(ableem::GuiBase &g) : GuiConfirm(g) {}

    void draw() override;
    void onAction(const abgui::ActionEvent &action) override;
    void onUnmapped(const ableem::Event &event) override;
};

//******************
// GuiRaManager
//******************
class GuiRaManager : public GuiScreen {
public:
    void init() override;
    void draw() override; // the frame's picture: the stack clears before and presents after
    void loop() override;

    // filled in by the caller before show()
    bool installed = false;
    bool networkUp = true;
    std::string version; // the VERSION stamp
    uint64_t bytes = 0;  // size on disk

    RaChoice result = RaChoice::None;

    explicit GuiRaManager(ableem::GuiBase &g) : GuiScreen(g) {}

    // the line under the title: "Installed: v1.22.2, 120.4 MB" or "Not installed" (static: tested without a screen)
    static std::string summary(bool installed, const std::string &version, uint64_t bytes);

private:
    std::vector<rajob::Row> rows_; // panelRows() for the chosen row
    int selected = 0;
    PanelStyle style;

    void rebuild();
    std::string titleOf(rajob::RowAction action) const;
    std::string descriptionOf(const rajob::Row &row) const;
};

//******************
// GuiRaProgress
//******************
// Polls the job every frame until it leaves Running. Circle asks "Stop?" and, on yes, SIGTERMs the runner, which
// ends with end|stopped. The outcome's messages are the caller's (GuiLauncher::loop_raManager): it sees the final
// state in job->status().
class GuiRaProgress : public GuiScreen {
public:
    void init() override;
    void draw() override;
    void loop() override;

    RaJobService *job = nullptr;
    rajob::Op op = rajob::Op::Install;

    explicit GuiRaProgress(ableem::GuiBase &g) : GuiScreen(g) {}

private:
    PanelStyle style;
    bool stopping = false;
    int barPhase = 0;             // the phase the bar was last drawn for; a change restarts the bar
    unsigned barPhaseStartMs = 0; // when that phase began
    rajob::JobState shown;        // the job's state this frame
};
