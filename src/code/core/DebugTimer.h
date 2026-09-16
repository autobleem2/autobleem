//
// DebugTimer: a scoped stopwatch that prints how long a block took. Debug builds only.
//
#pragma once

#include <string>
#include <cstdint>

#ifndef NDEBUG   // debug build

//******************
// DebugTimer
//******************
// Create one at the top of a function, passing the function name (or any label). When it goes out of scope it
// prints the elapsed time to cout:
//
//     void GuiLauncher::loadAssets() {
//         DebugTimer t("GuiLauncher::loadAssets");
//         ...
//
struct DebugTimer {
    std::string description;
    uint32_t ticks_start = 0;
    uint32_t ticks_end = 0;

    DebugTimer(const std::string &_description);
    ~DebugTimer();
};

#else            // release build: compiles away to nothing

struct DebugTimer {
    DebugTimer(const std::string &_description = "") {}
};

#endif
