//
// The UpdateRoms window: plain Win32 - a stage line, a progress bar, the log, and a Stop/Close button.
// No SDL, no AutoBleem theme: this is a utility on a PC, and a plain window is what makes the exe one
// self-contained file (nothing but user32/gdi32/comctl32 from the system).
//
#pragma once

#ifdef _WIN32

#include "core/update_roms_job.h"

#include <string>

// shows the window, runs the job on a thread while it is up, returns when it is closed. `error` (a
// detect() failure) is shown as a message box instead, and 1 returned.
int runUpdateRomsWindow(const UpdateRomsJob::Setup &setup, const std::string &error);

// attaches to the console the program was started from, when there is one, so --quiet's lines show up
// although the exe is a GUI-subsystem program
void attachParentConsole();

#endif
