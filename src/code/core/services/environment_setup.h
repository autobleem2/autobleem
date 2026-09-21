//
// EnvironmentSetup: tells ableem::Environment where everything is, from a program's command line. The one
// place that knows the difference between the console layout (/media, /usr/sony) and a root given on the
// command line (a debug host, or the Raspberry Pi port, where that root is the mount point of the exFAT
// data partition - see payload_linux/). Was main.cpp's setupEnvironment(); the console tools under apps/
// share it, which is how they find the main GUI's config.ini, themes and fonts.
//
#pragma once

#include <string>

//******************
// HostFacts
//******************
// What the Windows product's zero-argument start is decided from (EnvironmentSetup::fromWindowsInstall):
// where the program is, and where the user's data tree was put. Filled in by WindowsHost on Windows; a
// test fills it by hand - the decision itself has no Windows in it.
struct HostFacts {
    std::string programDir;          // the folder the exe is in - the resources are next to it
    std::string registryDataRoot;    // HKCU\Software\AutoBleem\DataRoot, what the installer wrote; "" = none
    std::string pointerFileDataRoot; // the first line of <programDir>/dataroot.txt (a portable copy); "" = none
    std::string documentsDir;        // the user's Documents folder - the default home of the data tree
};

//******************
// EnvironmentSetup
//******************
class EnvironmentSetup {
public:
    // everything under one root - what the console (/media), the Pi (its data partition) and the 1-arg
    // debug mode all are: usb:/Games, usb:/System/Databases/*.db, usb:/Autobleem/bin/autobleem as the
    // resources dir (config.ini, lang/, fonts), usb:/Autobleem/bin/db, usb:/Themes.
    static void fromRoot(const std::string &root);

    // the debug-only two-argument layout: a regional.db and a games dir, everything else from the current
    // directory (internal.db next to the binary, ../db for the covers)
    static void fromDbAndGames(const std::string &regionalDb, const std::string &gamesDir);

    // the Windows product (AB_PLATFORM_WIN), started with no arguments: the data tree is the registry's
    // DataRoot, else the pointer file's, else <Documents>/AutoBleem - made when missing, with the shipped
    // themes copied in the first time; the resources are the program's own folder, and what the program
    // writes about itself (config.ini, the scan's fingerprints) goes to <data>/System. Returns the data
    // root; "" (with the reason logged) when it cannot be made.
    static std::string fromWindowsInstall(const HostFacts &facts);
    static const char *const DefaultDataFolder; // "AutoBleem" - under Documents

    // autobleem-gui's command line: `<root>` or `<regional.db> <games dir>`; false, with the USAGE line
    // logged, for anything else (on the Windows product no argument at all is the normal case:
    // fromWindowsInstall with WindowsHost's facts). Options like --sysinfo are the caller's to strip first.
    static bool fromArguments(int argc, char *argv[]);

    // a tool in usb:/Apps/<tool>: an optional `<root>` (on the console it is /media and run.sh passes
    // nothing; a debug host must give one), the tool's own folder - where it was started from, its run.sh
    // cd's there - as the app dir. `toolName` is for the USAGE line.
    static bool forTool(int argc, char *argv[], const std::string &toolName);

    // resources/platform/<platform>.ini applied to the environment (RetroArch's place on this platform);
    // the from*() calls do it, a caller that set the paths by hand may need it
    static void applyPlatformConfig();
};
