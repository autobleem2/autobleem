//
// ProcessRunner: how LaunchService starts an emulator and waits for it - the one seam introduced purely so
// a launch can be tested without forking. LaunchPlan is what it is given: the program, its arguments and
// the directory to start it in - data, so a test can assert the whole of a launch.
//
#pragma once

#include <functional>
#include <string>
#include <vector>

//******************
// LaunchPlan
//******************
struct LaunchPlan {
    std::string exe;
    std::vector<std::string> args;
    std::string cwd; // "" = the launcher's own

    // the command as one line, for the log
    std::string toString() const;
};

//******************
// ProcessRunner
//******************
class ProcessRunner {
public:
    virtual ~ProcessRunner() = default;

    // starts the plan's program and does not return until it has exited
    virtual void run(const LaunchPlan &plan) = 0;

    // true when what run() starts needs the display for itself - the launcher then gives up its window
    // (and the DRM master with it) before run() and rebuilds it after, see Gui::releaseDisplay(). The one
    // runner that draws *on* the launcher's window instead (the dev host's splash) answers false.
    virtual bool needsExclusiveDisplay() const { return true; }

    // true when the launcher's window stays up for the run, behind the emulator's own (a desktop with a
    // window manager - the Windows product): it shows a picture meanwhile and is raised again after,
    // never minimised - a minimised window did not come back reliably, and the desktop showed between
    virtual bool keepsLauncherWindow() const { return false; }
};

//******************
// ForkProcessRunner
//******************
// The real one: System::runAndWait, i.e. the fork/exec of an rc/*.sh launcher script on the console and
// the appliances. The dev host installs a different runner from the composition root (see AutoBleem),
// because there is nothing to run.
class ForkProcessRunner : public ProcessRunner {
public:
    void run(const LaunchPlan &plan) override;
};

//******************
// WinProcessRunner
//******************
// The Windows product's: the same System::runAndWait (CreateProcess there), but the display stays ours -
// the window stays up behind the emulator's, with `whileWaiting` (the composition root gives it the event
// pump) called through the run so Windows never takes the launcher for hung.
class WinProcessRunner : public ForkProcessRunner {
public:
    explicit WinProcessRunner(std::function<void()> whileWaiting) : whileWaiting_(std::move(whileWaiting)) {}
    void run(const LaunchPlan &plan) override;
    bool needsExclusiveDisplay() const override { return false; }
    bool keepsLauncherWindow() const override { return true; }

private:
    std::function<void()> whileWaiting_;
};
