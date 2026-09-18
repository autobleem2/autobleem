// lib_ableem - engine: where everything lives on disk.
//
// The library never hard-codes a console path. The application decides the layout for the platform it runs on
// (PlayStation Classic: /media/..., a dev host: the usb folder passed on the command line, ...) and calls the
// setters once at start-up, before any other engine class is used. Every getter below derives from those
// few roots, so the rest of the engine only ever asks the Environment.
#pragma once

#include <string>

namespace ableem {

struct Environment {
    //*******************************
    // configuration - set once by the application
    //*******************************
    static void setUsbRoot(const std::string &path);            // root of the usb stick ("/media" on the console)
    static void setGamesDir(const std::string &path);           // where the user's games are (usb:/Games)
    static void setRegionalDbFile(const std::string &path);     // regional.db, the scanned-games database
    static void setInternalDbFile(const std::string &path);     // internal.db, the copy of the console's own database
    static void setWorkingPath(const std::string &path);        // resources dir: config.ini, default.png, memcard/, autobleem.list ...
    static void setSonyDataPath(const std::string &path);       // the console's own data (fonts): /usr/sony/share/data
    static void setThemesDir(const std::string &path);          // usb:/themes
    static void setCoversDbDir(const std::string &path);        // where coversU/P/J.db are
    static void setInternalGamesDir(const std::string &path);   // the console's built-in games; default "/gaadata"
    static void setRetroarchDir(const std::string &path);       // RetroArch's tree; "" (the default) means usb:/retroarch
    static void setRetroarchCoreFile(const std::string &path);  // the PS1 core the exported playlist names; "" = RetroBoot's

    //*******************************
    // paths
    //*******************************
    static std::string getPathToUSBRoot();
    static std::string getPathToAutobleemDir();         // usb:/Autobleem
    static std::string getPathToAppsDir();              // usb:/Apps
    static std::string getPathToRCDir();                // usb:/Autobleem/rc
    static std::string getPathToGamesDir();
    static std::string getPathToMemCardsDir();          // games:/!MemCards
    static std::string getPathToSaveStatesDir();        // games:/!SaveStates
    static std::string getPathToSystemDir();            // usb:/System
    static std::string getPathToRetroarchDir();         // usb:/retroarch unless setRetroarchDir() said otherwise
    static std::string getPathToRetroarchPlaylistsDir();
    static std::string getPathToRetroarchRdbDir();      // <retroarch>/database/rdb - libretro-database's .rdb files
    static std::string getPathToPlayStationRdbFile();  // "Sony - PlayStation.rdb" in there, what MetadataLookup reads
    static std::string getPathToRetroarchCoreFile();
    static bool hasRetroBoot();                         // usb:/retroarch/retroboot exists - the console's RetroArch bundle
    static std::string getPathToRomsDir();              // usb:/roms
    static std::string getPathToRegionalDBFile();       // includes the "regional.db" filename
    static std::string getPathToInternalDBFile();       // includes the "internal.db" filename
    static std::string getPathToInternalGamesDir();     // "/gaadata" unless configured otherwise

    static std::string getWorkingPath();                // the resources dir; the current dir when never set
    static std::string getPathToMemcardTemplateDir();   // working:/memcard - the blank card1.mcd/card2.mcd
    static std::string getPathToLangDir();              // working:/lang - the <Language>.txt translation files
    static std::string getSonyPath();
    static std::string getSonyFontPath();               // sony:/font

    static std::string getPathToThemesDir();
    static std::string getPathToCoversDBDir();
};

} // namespace ableem
