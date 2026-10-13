//
// GuiDiscRip: the popup of a disc being copied to Games as a CHD (AUTOBLEEM-21). It blocks the whole launcher - the
// screen runs its own loop, the RipJob on a worker thread - and shows the percent with a bar, the drive's speed as Nx,
// the time left and the unreadable sectors so far. Its only action is Circle: Cancel (no second question).
//
#pragma once

#include "gui/gui_screen.h"
#include "gui/panel_style.h"
#include "core/services/rip_job.h"

#include <atomic>
#include <mutex>
#include <string>
#include <thread>

class GuiDiscRip : public GuiScreen {
public:
    void init() override;
    void draw() override; // the frame's picture: the stack clears before and presents after
    void loop() override;
    void onAction(const abgui::ActionEvent &action) override;
    void onUnmapped(const ableem::Event &event) override;

    // in: the drive (open, borrowed: it outlives the screen), the CHD to write and whether the marks of LibCrypt are
    // read from the drive for an .sbi next to it
    CdDrive *drive = nullptr;
    std::string target;
    bool writeSbi = false;
    // out: how the rip ended
    RipJob::Result result;

    using GuiScreen::GuiScreen;

private:
    RipJob job;
    std::mutex lock; // the progress, written by the worker and read by the frame
    RipJob::Progress progress;
    std::atomic<bool> finished{false};
    std::atomic<bool> cancelling{false};
    long startedAt = 0;
    PanelStyle style;
};
