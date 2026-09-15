// lib_ableem - engine: the three databases that make up "the games" (regional.db for USB games, internal.db
// for the console's built-in games, and the covers*.db lookup) as one object, plus the handful of operations
// that need to pick the right one of the two game databases for a given record.
//
// Every path comes from ableem::Environment (getPathToCoversDBDir/RegionalDBFile/InternalDBFile/GamesDir/
// RetroarchDir/RetroarchPlaylistsDir/RetroarchCoreFile), so the application only has to configure Environment
// before calling openCoversAndUsbGames()/openInternalGames() - see the app's App::openLibrary().
#pragma once

#include <memory>

#include "cover_database.h"
#include "game_database.h"
#include "game_record.h"

namespace ableem {

//******************
// GameLibrary
//******************
class GameLibrary {
public:
    GameLibrary() {}
    ~GameLibrary();
    GameLibrary(const GameLibrary &) = delete;
    GameLibrary &operator=(const GameLibrary &) = delete;

    // split in two (rather than one open()) because the application needs to run a shell hook between them
    // (importing internal.db from the console) - see the "Importing internal games" step in App::openLibrary.
    bool openCoversAndUsbGames();   // CoverDatabase + regional.db (creates the schema if missing)
    bool openInternalGames();       // internal.db (adds the favorite/history/last_played/play_using_ra columns if missing)
    void close();                   // safe to call more than once; also runs at destruction

    GameDatabase &usbGames() { return *regionalDb; }
    GameDatabase &internalGames() { return *internalDb; }
    CoverDatabase &covers() { return *coverDb; }

    // internal games live in internal.db, USB games in regional.db - every call site that used to branch on
    // game.internal to pick one of "gui->db"/"gui->internalDB" can use this instead.
    GameDatabase &databaseFor(const GameRecord &game) { return game.internal ? internalGames() : usbGames(); }

    bool updateDatePlayed(const GameRecord &game, int secondsSinceEpoch) {
        return databaseFor(game).updateDatePlayed(game.gameId, secondsSinceEpoch);
    }
    bool updateHistory(const GameRecord &game) {
        return databaseFor(game).updateHistory(game.gameId, game.history);
    }
    bool updateTitle(const GameRecord &game, const std::string &title) {
        return databaseFor(game).updateTitle(game.gameId, title);
    }
    // re-reads game.gameId from whichever database it belongs to, overwriting game in place
    bool reload(GameRecord &game) {
        return game.internal ? internalGames().reloadInternalGame(game) : usbGames().reloadUsbGame(game);
    }

    // writes AutoBleem.lpl (every USB game, sorted by title) so RetroArch/RetroBoot can see the PS1 library
    bool exportToRetroArchPlaylist();
    // writes /Games/../retroboot/emulationstation/.../gamelists/psx/gamelist.xml for EmulationStation
    bool writeEmulationStationGamelist();

private:
    std::unique_ptr<CoverDatabase> coverDb;
    std::unique_ptr<GameDatabase> regionalDb;
    std::unique_ptr<GameDatabase> internalDb;
};

} // namespace ableem
