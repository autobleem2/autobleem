//
// The installer window: plain Win32 (no SDL, no AutoBleem theme - one self-contained exe), after the
// look of the Pi's first-boot screen: the AutoBleem 2 picture on top, then either the questions (which
// stick, format it, the cover databases, RetroArch, the BIOS files, the sample games) or the progress -
// the step, a bar for the steps and one for the step's own progress, and every line the job said.
//
#pragma once

#ifdef _WIN32

#include "core/installer_job.h"

#include <string>

// shows the window and returns when it is closed; `options` are the defaults the questions start from
// (the package found next to the program, the repository URL)
int runInstallerWindow(const InstallOptions &defaults);

#endif
