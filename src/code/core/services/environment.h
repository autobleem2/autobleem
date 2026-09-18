#pragma once

#include <ableem/engine/environment.h>
#include <string>
#include <vector>

//*******************************
// AB_DEBUG_HOST
//*******************************
// defined when building for a development machine (PC/Mac/Windows/Raspberry Pi) instead of the PlayStation Classic.
// on a debug host: files are read relative to the working dir / the usb root passed on the command line,
// emulators are not forked, and console-only paths (/media, /usr/sony) are not used.
#if defined(__x86_64__) || defined(_M_X64) || defined(_WIN32) || defined(PI_DEBUG)
#define AB_DEBUG_HOST 1
#endif

//*******************************
// AB_PLATFORM_RPI
//*******************************
// defined by CMake (-DAB_TARGET_RPI=ON, which toolchains/rpi/RPitoolchain.cmake forces on) for the Raspberry
// Pi port. A Pi is a real target, not a debug host - it forks the emulators and halts the machine for real -
// but it has no console tree behind it: everything lives on the exFAT data partition whose mount point is
// passed on the command line, and there are no built-in games (no /gaadata, no internal.db to import).
// PI_DEBUG above is the other thing: running the Pi build on a Pi as a *debug host*, without forking.

//*******************************
// AB_ROOT_RELATIVE_LAYOUT
//*******************************
// both of the above read every path from a root given on the command line, instead of the console's fixed
// /media + /usr/sony tree. See setupEnvironment() in main.cpp, the one place that decides the layout.
#if defined(AB_DEBUG_HOST) || defined(AB_PLATFORM_RPI)
#define AB_ROOT_RELATIVE_LAYOUT 1
#endif

// Every path comes from ableem::Environment, configured once in main() (see setupEnvironment there - that is
// where the debug-host vs console decisions are made). The app only adds its two runtime flags.
struct Environment : ableem::Environment {
    static bool autobleemKernel; // true if the kernel is the AutoBleem Kernel
    static bool hiddenMenuEnabled;

    // the platform this build is for - "psc" (the console), "rpi" or "pc" - which names the
    // resources/platform/<name>.ini that PlatformConfig reads at start. The only place the build macros
    // decide a path.
    static const char *platformName();

    // where the RetroArch executable may be (PlatformConfig's retroarch_binary, resolved), and whether one
    // of them is there - what "RetroArch" in the system menu and Square on a game check
    static void setRetroArchBinaries(const std::vector<std::string> &paths);
    static const std::vector<std::string> &retroArchBinaries();
    static bool retroArchInstalled();

    // the gamecontrollerdb.txt files SDL's pad mappings come from, first existing wins: the kernel's
    // (/etc/autobleem on the console - what the pscbios wizard writes when it is there), then the shipped
    // one in the resources dir (what the wizard writes otherwise)
    static std::vector<std::string> padMappingFiles();
};

using Env = Environment;
