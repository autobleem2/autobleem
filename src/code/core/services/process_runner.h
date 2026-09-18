//
// ProcessRunner: how LaunchService starts an emulator and waits for it - the one seam introduced purely so
// a launch can be tested without forking.
//
#pragma once

#include <string>
#include <vector>

//******************
// ProcessRunner
//******************
class ProcessRunner {
public:
    virtual ~ProcessRunner() {}

    // runs `exe` with `args` and does not return until it has exited
    virtual void run(const std::string &exe, const std::vector<std::string> &args) = 0;

    // true when what run() starts needs the display for itself - the launcher then gives up its window
    // (and the DRM master with it) before run() and rebuilds it after, see Gui::releaseDisplay(). The one
    // runner that draws *on* the launcher's window instead (the dev host's splash) answers false.
    virtual bool needsExclusiveDisplay() const { return true; }
};

//******************
// ForkProcessRunner
//******************
// The real one: System::runAndWait, i.e. the fork/exec of an rc/*.sh launcher script on the console. The dev
// host installs a different runner from the composition root (see App), because there is nothing to run.
class ForkProcessRunner : public ProcessRunner {
public:
    void run(const std::string &exe, const std::vector<std::string> &args) override;
};
