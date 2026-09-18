// lib_ableem - engine: walks the games hierarchy, repairs what it can (comma names, missing/broken cue files,
// ECM compressed bins, missing lic/cover/pcsx.cfg/Game.ini), reads each game's serial and metadata and
// finally writes regional.db + autobleem.list. Progress goes to a listener the application supplies; the
// library never draws, sleeps or translates.
#pragma once

#include <map>
#include <string>

#include "metadata_lookup.h"
#include "game_database.h"
#include "games_hierarchy.h"
#include "usb_game.h"

namespace ableem {

//******************
// ScanStage
//******************
enum class ScanStage {
    Scanning,         // detail: ""
    Game,             // detail: the game dir name
    DecompressingEcm, // detail: "" at the start, then the decoder's "Decoding ECMed bin (nn%)" messages
    UpdatingDatabase, // detail: ""
    GameFailedVerify, // detail: the game's full path
    MovingFile,       // detail: the file being moved into its own game sub-directory
    MergingDiscs      // detail: the multi-disc game whose "(Disc n)" folders are being merged into one
};

//******************
// ScanProgressListener
//******************
class ScanProgressListener {
public:
    virtual ~ScanProgressListener() {}
    // done/total are both 0 except during ScanStage::Game, where they are this game's 1-based index and the
    // total game count - everything a progress display needs to show "n/total (nn%)".
    virtual void onScanProgress(ScanStage stage, const std::string &detail, int done = 0, int total = 0) = 0;
    // called right after a game passes verify(), so a caller can add it to a database/UI immediately instead
    // of waiting for the whole scan to finish. game is only valid for the duration of the call.
    virtual void onGameVerified(const UsbGame &game) {}
    // called instead of onGameVerified when a game fails verify() and is dropped (see ScanStage::GameFailedVerify
    // for the human-readable reasons); fullPath is the game's directory.
    virtual void onGameFailedVerify(const std::string &fullPath) {}
};

//******************
// GameScanner
//******************
class GameScanner {
public:
    explicit GameScanner(ScanProgressListener *listener = nullptr) : listener(listener) {}
    GameScanner(GameScanner const &) = delete;
    GameScanner &operator=(GameScanner const &) = delete;

    UsbGames gamesToAddToDB; // filled by scanGamesDirectory: every game that verified
    bool noGamesFoundDuringScan = false;

    // metadata supplies title/publisher/year/cover art for games whose Game.ini is missing or incomplete
    void scanGamesDirectory(GamesHierarchy &gamesHierarchy, MetadataLookup &metadata);

    // SUBDIR_ROWS + SUBDIR_GAMES_TO_DISPLAY_ON_ROW, cleared and rewritten in one transaction. This class no
    // longer assigns game ids (a caller doing an incremental scan has to reuse an existing game's id rather
    // than renumber it - see GameDatabase::findGameIdByPath/insertGame), so idByPath supplies them.
    static void writeSubDirRows(GamesHierarchy &gamesHierarchy, GameDatabase &db,
                                const std::map<std::string, int> &idByPath);
    // autobleem.list in the working path (id,path,sspath one game per line, read by the rc shell scripts)
    static void writeAutobleemList(const UsbGames &games, const std::map<std::string, int> &idByPath);

    void repairBrokenCueFiles(const std::string &path);
    void decompressEcmFiles(const std::string &path); // every .ecm in the dir becomes a .bin

    // true when game files (pbp/bin/cue/img/chd) sit directly in the dir instead of in sub-dirs
    static bool hasLooseGameFiles(const std::string &path);

    // moves every pbp/cue(+its bins)/img/bin sitting directly in `path` into its own new sub-directory (one
    // per game), the layout the rest of the scanner expects. reports each move as ScanStage::MovingFile.
    // returns true if anything was moved.
    bool moveLooseGameFilesIntoSubDirs(const std::string &path);

    // "Game (Disc 1)", "Game (Disc 2)", ... sibling folders (see DiscSuffix) become one "Game" folder holding
    // every disc image, ready for an .m3u: the lowest disc's folder is renamed, the other discs' images
    // are moved in and their folders - with their Game.ini, pcsx.cfg and save states - are deleted. A group
    // is skipped when "Game" already exists and is not the first disc, or when a file would be overwritten.
    // Runs before the scan proper, so what the scan sees is the merged tree. Returns the number of games merged.
    int mergeMultiDiscFolders(const std::string &gamesDir);

private:
    ScanProgressListener *listener;

    void report(ScanStage stage, const std::string &detail = "", int done = 0, int total = 0);
    void moveFolderIfNeeded(const std::string &gameDirName, std::string gameDataPath, std::string path);
};

} // namespace ableem
