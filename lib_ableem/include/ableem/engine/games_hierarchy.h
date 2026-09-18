// lib_ableem - engine: the sub-directory tree of the games dir. Every sub-dir that holds a game file is a
// game; every other sub-dir is a row of the "select game dir" menu that shows its own games plus the games
// of its children (duplicates removed once the scanner has filled in the serials).
#pragma once

#include <fstream>
#include <memory>
#include <string>
#include <vector>

#include "usb_game.h"

namespace ableem {

using GameSubDirPtr = std::shared_ptr<class GameSubDir>;
using GameSubDirRows = std::vector<GameSubDirPtr>;

//******************
// GameSubDir
//******************
struct GameSubDir {
    std::string fullPath = "";
    std::string subDirName = ""; // last part from the path
    GameSubDirRows *displayRows;
    unsigned int displayRowIndex = 0;
    unsigned int displayIndentLevel = 0;

    std::vector<GameSubDirPtr> childrenDirs;

    UsbGames gamesInThisDir;
    UsbGames gamesInChildrenDirs;
    UsbGames gamesToDisplay;

    GameSubDir(const std::string &_path, int _displayIndentLevel, GameSubDirRows *displayRows);
    void scanAll(); // recursive scan of the sub directories

    static bool sameGame(const UsbGamePtr &game1, const UsbGamePtr &game2);
    void makeGamesToDisplayWhileRemovingChildDuplicates(std::ofstream &dupFile); // recursive

    void print(bool plusGames);

private:
    static void removeGamesInSecondListThatMatchAGameInFirstList(UsbGames &games1, UsbGames &games2,
                                                                 std::ofstream &dupFile);
    static void removeDuplicateGamesLeavingOne(UsbGames &games, std::ofstream &dupFile);
};

//******************
// GamesHierarchy
//******************
struct GamesHierarchy {
    GameSubDirRows gameSubDirRows; // these rows are displayed in the select game dir menu
    std::ofstream dupFile;

    GamesHierarchy() {}
    void getHierarchy(const std::string &_path); // scans; also dumps gameHierarchy_beforeScan.txt to the working path
    UsbGames getAllGames();

    // autobleem.prev is the list of game folders at the last scan: a difference means a rescan is needed
    bool gamesDoNotMatchAutobleemPrev(const std::string &autobleemPrevPath);
    void writeAutobleemPrev(const std::string &autobleemPrevPath);

    // run this after the scanner has filled in the serial so we can correctly match duplicate games
    void makeGamesToDisplayWhileRemovingChildDuplicates();

    // if a game failed to verify in the scanner it needs to be removed
    void removeGameFromEntireHierarchy(UsbGamePtr &game);

    void dumpRowGameInfo(std::ostream &o, bool alsoPrintGames);
    void dumpRowDisplayGameInfo(std::ostream &o, bool alsoPrintGames);
    void printRowGameInfo(bool alsoPrintGames);
    void printRowDisplayGameInfo(bool alsoPrintGames);
};

} // namespace ableem
