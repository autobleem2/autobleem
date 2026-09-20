#pragma once

#include <ableem/engine/environment.h>
#include <string>
#include <vector>

//*******************************
// AB_PLATFORM_<TARGET>
//*******************************
// Exactly one is defined by CMake from AB_TARGET (root CMakeLists.txt): AB_PLATFORM_PSC (the console),
// AB_PLATFORM_RPI (the Raspberry Pi appliance), AB_PLATFORM_PCUSB (the 32-bit PC USB-stick appliance),
// AB_PLATFORM_WIN (the Windows product) or AB_PLATFORM_DEV (a development host). The sources never test the
// CPU or the OS to tell them apart; where a build differs, it is by the four macros derived below - each
// named for what it means - or, for a path, by resources/platform/<name>.ini (Env::platformName()).
#if defined(AB_PLATFORM_PSC) + defined(AB_PLATFORM_RPI) + defined(AB_PLATFORM_PCUSB) + defined(AB_PLATFORM_WIN) +      \
        defined(AB_PLATFORM_DEV) !=                                                                                    \
    1
#error "exactly one AB_PLATFORM_* must be defined - configure with -DAB_TARGET=psc|rpi|pcusb|win|dev"
#endif

//*******************************
// AB_DEBUG_HOST
//*******************************
// a development machine: the emulators are not forked (a splash stands in), power off is exit(), free
// space is not measured, the internal games' covers come from the rdb. Only the dev target.
#if defined(AB_PLATFORM_DEV)
#define AB_DEBUG_HOST 1
#endif

//*******************************
// AB_APPLIANCE
//*******************************
// a machine of ours that is not the console: it forks the emulators and halts for real, but has no console
// tree behind it - everything lives on the data partition whose mount point is passed on the command line,
// there are no built-in games, and an update is the installer re-run by the session script (MENU_OPTION_UPDATE).
#if defined(AB_PLATFORM_RPI) || defined(AB_PLATFORM_PCUSB)
#define AB_APPLIANCE 1
#endif

//*******************************
// AB_ROOT_RELATIVE_LAYOUT
//*******************************
// every path comes from a root given on the command line (or found by the Windows product), instead of the
// console's fixed /media + /usr/sony tree. See EnvironmentSetup, the one place that decides the layout.
#if !defined(AB_PLATFORM_PSC)
#define AB_ROOT_RELATIVE_LAYOUT 1
#endif

//*******************************
// AB_HAS_INTERNAL_GAMES
//*******************************
// the console's built-in games (/gaadata, internal.db) can be shown: the console itself, and a dev host
// running against a copy of its database. An appliance or a Windows PC has none.
#if defined(AB_PLATFORM_PSC) || defined(AB_PLATFORM_DEV)
#define AB_HAS_INTERNAL_GAMES 1
#endif

// Every path comes from ableem::Environment, configured once in main() (see setupEnvironment there - that is
// where the debug-host vs console decisions are made). The app only adds its two runtime flags.
struct Environment : ableem::Environment {
    static bool autobleemKernel; // true if the kernel is the AutoBleem Kernel
    static bool hiddenMenuEnabled;

    // the platform this build is for - "psc" (the console), "rpi", "pcusb", "win" or "pc" (a dev host) -
    // which names the resources/platform/<name>.ini that PlatformConfig reads at start. The only place the
    // build macros decide a path.
    static const char *platformName();

    // where the RetroArch executable may be (PlatformConfig's retroarch_binary, resolved), and whether one
    // of them is there - what "RetroArch" in the system menu and Square on a game check
    static void setRetroArchBinaries(const std::vector<std::string> &paths);
    static const std::vector<std::string> &retroArchBinaries();
    static bool retroArchInstalled();

    // PlatformConfig's download_command: how this platform fetches a URL to a file (%u, %o), "" when it
    // cannot - what OnlineAssets runs
    static void setDownloadCommand(const std::string &command);
    static const std::string &downloadCommand();
    // PlatformConfig's repo_url and update_download_command: the download repository the online update
    // checks, and the command that fetches a package from it (no short timeout); "" = no update here.
    // retroarch_catalog is where that repository lists this platform's RetroArch builds
    // ("rpi/retroarch/latest.json", relative to repo_url); "" = RetroArch is not updated here
    static void setUpdateSource(const std::string &repoUrl, const std::string &downloadCommand,
                                const std::string &retroarchCatalog = "");
    static const std::string &repoUrl();
    static const std::string &updateDownloadCommand();
    static const std::string &retroArchCatalog();

    // PlatformConfig's launch_mode: "direct" - the launcher starts the emulators itself (the Windows
    // product); otherwise through the rc/launch*.sh scripts (the console, the appliances, a dev host)
    static void setDirectLaunch(bool direct);
    static bool directLaunch();
    // PlatformConfig's pcsx_dir / pcsxnxt_dir: where pcsx-ab and pcsx-abnxt are for a direct launch
    // (resolved, absolute); "" = the scripts know. Options -> "PS1 Emulator" picks between the two
    static void setPcsxDir(const std::string &path);
    static const std::string &pcsxDir();
    static void setPcsxNxtDir(const std::string &path);
    static const std::string &pcsxNxtDir();

    // the gamecontrollerdb.txt files SDL's pad mappings come from, first existing wins: the kernel's
    // (/etc/autobleem on the console - what the pscbios wizard writes when it is there), then the shipped
    // one in the resources dir (what the wizard writes otherwise)
    static std::vector<std::string> padMappingFiles();
};

using Env = Environment;
