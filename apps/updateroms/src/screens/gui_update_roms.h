//
// GuiUpdateRoms: the one screen - the job runs on a thread and this draws where it is: the system it is
// on, a bar over the whole run, the log lines, and Close when it is done.
//
#pragma once

#include "core/update_roms_job.h"
#include "gui/gui_screen.h"

#include <ableem/engine/game_scanner.h>

#include <atomic>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

//********************
// GuiUpdateRoms
//********************
class GuiUpdateRoms : public GuiScreen, private ableem::ScanProgressListener {
public:
    using GuiScreen::GuiScreen;
    ~GuiUpdateRoms() override;

    UpdateRomsJob::Setup setup;         // set by the caller before show()
    OnlineAssets::CommandRunner runner; // "" = the real one; a test's fake
    UpdateRomsJob::Report report;       // filled once the job is done

    void render() override;
    void loop() override;

private:
    void onScanProgress(ableem::ScanStage stage, const std::string &detail, int done, int total) override;
    void startJob();
    void stopJob();

    std::thread worker_;
    std::atomic<bool> stop_{false};
    std::atomic<bool> done_{false};
    std::mutex mutex_;
    std::string stageText_; // what the worker is on, for the status line
    int done = 0, total = 0;
    std::vector<std::string> lines_; // the report's lines as they come
};
