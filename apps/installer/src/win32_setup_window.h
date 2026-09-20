//
// The Windows product's setup window (AutoBleemWinSetup.exe): the same look as the console installer's
// window - the AutoBleem 2 picture on top, then either the questions (the data folder, the cover
// databases, RetroArch, the BIOS files, the sample games) or the progress. What the NSIS installer runs
// after the program is in place, and what the Start Menu's "AutoBleem Setup" runs again later.
//
#pragma once

#ifdef _WIN32

#include "core/windows_install_job.h"

// shows the window and returns when it is closed; `defaults` are what the questions start from (the
// program folder, the data root the installer chose, the options it was given). With `autoStart` the
// questions are skipped and the job starts at once - the NSIS installer's wizard has asked them already,
// this is its progress page; the Start Menu's "AutoBleem Setup" shows the questions.
int runSetupWindow(const WindowsInstallOptions &defaults, bool autoStart);

#endif
