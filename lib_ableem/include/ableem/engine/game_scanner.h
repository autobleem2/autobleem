// lib_ableem - engine: walks the games hierarchy, repairs what it can (comma names, missing/broken cue files,
// ECM compressed bins, missing lic/cover/pcsx.cfg/Game.ini), reads each game's serial and metadata and
// finally writes regional.db + autobleem.list. Progress goes to a listener the application supplies; the
// library never draws, sleeps or translates.
#pragma once

#include <string>

#include "cover_database.h"
#include "game_database.h"
#include "games_hierarchy.h"
#include "usb_game.h"

namespace ableem {

//******************
// ScanStage
//******************
enum class ScanStage {
    Scanning,           // detail: ""
    Game,               // detail: the game dir name
    DecompressingEcm,   // detail: "" at the start, then the decoder's "Decoding ECMed bin (nn%)" messages
    UpdatingDatabase,   // detail: ""
    GameFailedVerify    // detail: the game's full path
};

//******************
// ScanProgressListener
//******************
class ScanProgressListener {
public:
    virtual ~ScanProgressListener() {}
    virtual void onScanProgress(ScanStage stage, const std::string &detail) = 0;
};

//******************
// GameScanner
//******************
class GameScanner {
public:
    explicit GameScanner(ScanProgressListener *listener = nullptr) : listener(listener) {}
    GameScanner(GameScanner const &) = delete;
    GameScanner &operator=(GameScanner const &) = delete;

    UsbGames gamesToAddToDB;            // filled by scanGamesDirectory: every game that verified
    bool noGamesFoundDuringScan = false;

    // coverDb supplies title/publisher/year/cover art for games whose Game.ini is missing or incomplete
    void scanGamesDirectory(GamesHierarchy &gamesHierarchy, CoverDatabase &coverDb);
    void writeRegionalDatabase(GamesHierarchy &gamesHierarchy, GameDatabase &db);   // + autobleem.list in the working path

    void repairBrokenCueFiles(const std::string &path);
    void decompressEcmFiles(const std::string &path);   // every .ecm in the dir becomes a .bin

    // true when game files (pbp/bin/cue/img/chd) sit directly in the dir instead of in sub-dirs
    static bool hasLooseGameFiles(const std::string &path);

private:
    ScanProgressListener *listener;
    bool complete = false;

    void report(ScanStage stage, const std::string &detail = "");
    void moveFolderIfNeeded(const std::string &gameDirName, std::string gameDataPath, std::string path);
};

} // namespace ableem
