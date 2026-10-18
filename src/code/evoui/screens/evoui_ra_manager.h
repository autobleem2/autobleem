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
// The caller sets `label` (the body), `detail` (a second, quieter line) and `title`, then show(). A `tag` ("ERROR") is
// the chip right of the title.
class GuiRaMessage : public GuiConfirm {
public:
    explicit GuiRaMessage(ableem::GuiBase &g) : GuiConfirm(g) {}

    std::string tag;
    std::string detail;

    void draw() override;
    void onAction(const abgui::ActionEvent &action) override;
    void onUnmapped(const ableem::Event &event) override;
};

//******************
// GuiRaNotice
//******************
// The box before an Install / Update: the title, a body, bullets, an accent line, Cross "Start" / Circle "Cancel".
// GuiConfirm's answers (Cross = true, Circle = false); the caller sets title, label (body), bullets, accent,
// confirmLabel and cancelLabel, then show().
class GuiRaNotice : public GuiConfirm {
public:
    explicit GuiRaNotice(ableem::GuiBase &g) : GuiConfirm(g) {}

    std::vector<std::string> bullets;
    std::string accent; // drawn last, in the accent colour

    void draw() override;
    // the title, body, bullets, accent and labels for an Install or an Update
    void setFor(rajob::Op op);
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
    std::string version;     // the VERSION stamp
    bool spaceKnown = false; // the stick's free / total space (one statvfs); no size of RetroArch is shown
    uint64_t freeBytes = 0;
    uint64_t totalBytes = 0;

    RaChoice result = RaChoice::None;
    // the row that was chosen (set when result is not None); give it back as `startRow` with `keepRow` to open the
    // panel with that row selected again (a Cancel in the box that follows returns here)
    rajob::RowAction chosenRow = rajob::RowAction::Install;
    rajob::RowAction startRow = rajob::RowAction::Install;
    bool keepRow = false;
    // the "Also delete the BIOS files" switch, off at the first open; given back the same way as the row
    bool deleteBios = false;

    explicit GuiRaManager(ableem::GuiBase &g) : GuiScreen(g) {}

    // the line under the title: "Installed: v1.22.2" or "Not installed" (static: tested without a screen)
    static std::string summary(bool installed, const std::string &version);
    // "18.4 GB free of 58.2 GB"
    static std::string spaceLine(uint64_t freeBytes, uint64_t totalBytes);
    // "18.4 GB" with the language's decimal sign
    static std::string spaceText(uint64_t bytes);

private:
    std::vector<rajob::Row> rows_; // panelRows()
    int selected = 0;
    PanelStyle style;

    void rebuild();
    std::string titleOf(rajob::RowAction action) const;
    std::string descriptionOf(const rajob::Row &row) const;
};

//******************
// GuiRaRemoveConfirm
//******************
// "Remove RetroArch?" in two columns: what will be deleted / what stays on the stick. With `withBios` the BIOS files
// move to the left column and a warning line comes under them. result = Remove (Cross) or Cancel (Circle).
class GuiRaRemoveConfirm : public GuiScreen {
public:
    void init() override;
    void draw() override;
    void loop() override;

    bool withBios = false;
    bool result = false;

    explicit GuiRaRemoveConfirm(ableem::GuiBase &g) : GuiScreen(g) {}

private:
    PanelStyle style;
};

//******************
// GuiRaProgress
//******************
// Polls the job every frame until it leaves Running. Two bars: the whole job (Step i of n, %) and the current package
// (MB or files, its name, Package i of n, the phase's why; downloads: speed and time left). Circle asks "Stop?" and,
// on yes, SIGTERMs the runner, which ends with end|stopped - for Install / Update only: a Remove has no Stop and runs
// to the end. The outcome's messages are the caller's (GuiLauncher::loop_raManager): it sees the final state in
// job->status().
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
    int meterItem = -1;           // the package the speed meter is following
    rajob::RateMeter meter;       // download speed, smoothed over a few seconds
    rajob::StallClock stall;      // when the phase, package or count last changed
    rajob::JobState shown;        // the job's state this frame
};
