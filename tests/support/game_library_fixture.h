//
// GameLibraryFixture: a real regional.db + internal.db in a temp tree, filled row by row.
//
#pragma once

#include "env_fixture.h"
#include "temp_dir.h"

#include <ableem/engine/game_library.h>

#include <string>

//******************
// GameLibraryFixture
//******************
// The services that read games need a GameLibrary, and a GameLibrary needs two sqlite files at the paths
// Environment names. This builds both in a TempDir, creates the schema in each (openInternalGames() only
// adds the four AutoBleem columns - the stock internal.db arrives with its tables already there) and lets a
// test add exactly the rows it wants to assert about.
//
//     GameLibraryFixture lib;
//     lib.addUsbGame(1, "Crash Bandicoot");
//     lib.addSubDirRow(0, "Games", 0, 1);
//     lib.putGameInSubDirRow(0, 1);
//
// Building the rows from the database's own API rather than checking in a binary .db keeps the fixture
// readable in a diff and stops it rotting silently when the schema changes.
class GameLibraryFixture {
public:
    GameLibraryFixture() : tmp("gamelib") {
        env.setUsbRoot(tmp.path());
        env.setGamesDir(tmp.makeSubDir("Games"));
        env.setCoversDbDir(tmp.makeSubDir("db"));
        env.setRegionalDbFile(tmp.at("regional.db"));
        env.setInternalDbFile(tmp.at("internal.db"));

        library.openCoversAndUsbGames(); // creates regional.db's schema itself

        // openInternalGames() only ALTERs in the four AutoBleem columns, because the console's own
        // internal.db already has its tables. A brand-new file does not, so create them first and then
        // add the columns the same way, or every internal-games query silently returns nothing.
        library.openInternalGames();
        library.internalGames().createSchema();
        library.internalGames().addFavoriteColumnIfMissing();
        library.internalGames().addPlayUsingRAColumnIfMissing();
        library.internalGames().addLightgunColumnIfMissing();
        library.internalGames().addHistoryColumnIfMissing();
        library.internalGames().addLastPlayedColumnIfMissing();
    }

    // A game is only visible to the loaders through a join on DISC, so every game gets one - which is what
    // the scanner does too. Omitting it here would make a game silently invisible rather than fail loudly.
    void addUsbGame(int id, const std::string &title, const std::string &memcard = "SONY") {
        library.usbGames().insertGame(id, title, "Publisher", 1, 1997, tmp.at("Games/" + title),
                                      tmp.at("Games/" + title + "/sstates"), memcard);
        library.usbGames().insertDisc(id, 1, title);
        tmp.makeSubDir("Games/" + title);
        writeGameIni(title, false);
    }

    void addInternalGame(int id, const std::string &title) {
        library.internalGames().insertGame(id, title, "Sony", 1, 1995, "/gaadata/" + std::to_string(id),
                                           "/gaadata/" + std::to_string(id) + "/sstates", "SONY");
        library.internalGames().insertDisc(id, 1, title);
    }

    // the /Games directory tree as the scanner records it: row 0 is /Games itself
    void addSubDirRow(int rowIndex, const std::string &name, int indentLevel, int numGames) {
        library.usbGames().insertSubDirRow(rowIndex, name, indentLevel, numGames);
    }
    void putGameInSubDirRow(int rowIndex, int gameId) { library.usbGames().insertSubDirRowGame(rowIndex, gameId); }

    // A USB game's favorite flag lives in its Game.ini, not in regional.db - the database's own comment
    // says so ("USB games don't need it as they use the game.ini to flag favorites"), and loadUsbGames
    // merges the ini in. Only internal games use the FAVORITE column.
    void markFavorite(const std::string &title) { writeGameIni(title, true); }
    void markHistory(int id, int rank) { library.usbGames().updateHistory(id, rank); }
    void markInternalFavorite(int id) { library.internalGames().updateFavorite(id, 1); }
    void markInternalHistory(int id, int rank) { library.internalGames().updateHistory(id, rank); }

    // Declaration order is destruction order reversed, and it matters: the library has to close its sqlite
    // files before TempDir deletes the directory holding them (Windows will not unlink an open file), and
    // EnvFixture has to outlive both so the paths are still valid while they shut down.
    EnvFixture env;
    TempDir tmp;
    ableem::GameLibrary library;

private:
    void writeGameIni(const std::string &title, bool favorite) const {
        std::string ini = "[Game]\nAutomation=1\nFavorite=";
        ini += favorite ? "1" : "0";
        ini += "\n";
        tmp.writeFile("Games/" + title + "/" + ableem::GAME_INI, ini);
    }
};
