//
// FakeProcessRunner: records what LaunchService would have run instead of forking it.
//
#pragma once

#include "doctest/doctest.h"
#include "core/services/process_runner.h"

#include <functional>
#include <string>
#include <vector>

//******************
// FakeProcessRunner
//******************
// A launch is judged by the argv it hands to rc/launch.sh or rc/launch_rb.sh and by what the tree looks
// like while the emulator would be running - `whileRunning` is called from run() so a test can look at
// that moment (which memory card is in play, which config is in place) before the service puts things back.
struct FakeProcessRunner : ProcessRunner {
    struct Call {
        std::string exe;
        std::vector<std::string> args;
    };

    void run(const std::string &exe, const std::vector<std::string> &args) override {
        calls.push_back(Call{exe, args});
        if (whileRunning) whileRunning();
    }

    const Call &only() const {
        REQUIRE(calls.size() == 1);
        return calls[0];
    }

    std::vector<Call> calls;
    std::function<void()> whileRunning;
};
