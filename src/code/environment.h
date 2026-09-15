#pragma once

#include <string>

//*******************************
// AB_DEBUG_HOST
//*******************************
// defined when building for a development machine (PC/Mac/Windows/Raspberry Pi) instead of the PlayStation Classic.
// on a debug host: files are read relative to the working dir / the usb root passed on the command line,
// emulators are not forked, and console-only paths (/media, /usr/sony) are not used.
#if defined(__x86_64__) || defined(_M_X64) || defined(_WIN32) || defined(PI_DEBUG)
#define AB_DEBUG_HOST 1
#endif

struct Environment {
    static std::string getPathToUSBRoot();
    static std::string getPathToAutobleemDir();
    static std::string getPathToAppsDir();
    static std::string getPathToRCDir();
    static std::string getPathToGamesDir();
    static std::string getPathToMemCardsDir();
    static std::string getPathToSaveStatesDir();
    static std::string getPathToSystemDir();
    static std::string getPathToRetroarchDir();
    static std::string getPathToRetroarchPlaylistsDir();
    static std::string getPathToRetroarchCoreFile();
    static std::string getPathToRomsDir();
    static std::string getPathToRegionalDBFile();   // includes the "regional.db" filename
    static std::string getPathToInternalDBFile();   // includes the "internal.db" filename

    static std::string getWorkingPath();  // 1 arg: "usb:/Autobleem/bin/autobleem", 2 arg: autobleem-gui executable dir
    static std::string getSonyPath();  // 1 arg: "usb:/Autobleem/bin/autobleem/sony", 2 arg: "" + sep + "sony"
    static std::string getSonyFontPath();  // 1 arg: "usb:/Autobleem/bin/autobleem/sony", 2 arg: "" + sep + "sony"
//    static std::string getPathToWorkingPathFile(const std::string &filename);   // return path to file in working path

    static std::string getPathToThemesDir();        // "usb:/themes" or "./themes"
    static std::string getPathToCoversDBDir();   // "usb:/Autobleem/bin/db" or "../db"

    static bool autobleemKernel;        // true if the kernel is the AutoBleem Kernel
    static bool hiddenMenuEnabled;
};

using Env = Environment;
