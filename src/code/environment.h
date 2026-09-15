#pragma once

#include <ableem/engine/environment.h>

//*******************************
// AB_DEBUG_HOST
//*******************************
// defined when building for a development machine (PC/Mac/Windows/Raspberry Pi) instead of the PlayStation Classic.
// on a debug host: files are read relative to the working dir / the usb root passed on the command line,
// emulators are not forked, and console-only paths (/media, /usr/sony) are not used.
#if defined(__x86_64__) || defined(_M_X64) || defined(_WIN32) || defined(PI_DEBUG)
#define AB_DEBUG_HOST 1
#endif

// Every path comes from ableem::Environment, configured once in main() (see setupEnvironment there - that is
// where the debug-host vs console decisions are made). The app only adds its two runtime flags.
struct Environment : ableem::Environment {
    static bool autobleemKernel;        // true if the kernel is the AutoBleem Kernel
    static bool hiddenMenuEnabled;
};

using Env = Environment;
