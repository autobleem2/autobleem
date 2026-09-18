//
// Created by lifting GuiLauncher::addGameToPS1GameHistoryAsLatestGamePlayed and GuiManager's delete /
// cover flush out of the screens that held them.
//

#include "game_catalog.h"
#include "game_query.h"
#include "environment.h"
#include "../main.h"

#include <algorithm>
#include <cstdio>
#include <iostream>
#include <ableem/engine/log.h>

using namespace std;

namespace {

// every .png under path, at any depth. The cover flush used nftw() for this; walking with DirEntry keeps
// the service free of <ftw.h>, which is not portable, and makes it testable against a temp tree.
int removePngFilesUnder(const string &path, int removed = 0) {
    for (auto &entry : DirEntry::diru(path)) {
        string child = path + sep + entry.name;
        if (DirEntry::isDirectory(child)) {
            removed = removePngFilesUnder(child, removed);
        } else if (DirEntry::matchExtension(entry.name, EXT_PNG)) {
            if (DirEntry::removeFile(child))
                ++removed;
        }
    }
    return removed;
}

} // namespace

//*******************************
// GameCatalogService::recordGamePlayed
//*******************************
// Renumbers the whole history so the game just played is 1 and everything else shifts down. Internal games
// are included: they share the one ranking, so leaving them out would let two games claim the same rank.
void GameCatalogService::recordGamePlayed(const PsGamePtr &game) {
    PsGames everything = query_.allPs1Games(true, true);

    // everything already in the history except the game being added - it is about to become number 1
    PsGames ranked;
    copy_if(begin(everything), end(everything), back_inserter(ranked),
            [&](const PsGamePtr &other) { return other->history > 0 && other->gameId != game->gameId; });

    sort(begin(ranked), end(ranked), [](const PsGamePtr &l, const PsGamePtr &r) { return l->history < r->history; });

    int rank = 2;
    for (auto &other : ranked) {
        other->history = rank <= HistoryLimit ? rank++ : 0; // 0 drops it out of the history
    }

    game->history = 1;
    ranked.emplace_back(game);

    // One transaction rather than up to HistoryLimit+1 separate commits: every launch rewrites the whole
    // ranking, and on the console each loose UPDATE is its own disk sync. Both databases are wrapped
    // because the ranking spans USB and internal games; an empty transaction costs nothing.
    library_.usbGames().beginTransaction();
    library_.internalGames().beginTransaction();
    for (auto &each : ranked) {
        library_.updateHistory(*each);
    }
    library_.internalGames().commit();
    library_.usbGames().commit();
}

//*******************************
// GameCatalogService::deleteUsbGame
//*******************************
GameCatalogService::DeleteResult GameCatalogService::deleteUsbGame(const PsGame &game) {
    DeleteResult result;
    result.saveStateFolder = game.ssFolder;

    if (!library_.usbGames().deleteGame(game.gameId)) {
        PLOG_WARNING << "Failed to delete game " << game.gameId << " from the database";
        return result;
    }
    if (!DirEntry::removeDirAndContents(game.folder)) {
        PLOG_WARNING << "Failed to delete directory " << game.folder;
        return result;
    }
    result.removed = true;

    // a !SaveStates folder can be shared between games, so it only becomes deletable once the last game
    // using it is gone
    PsGames remaining = PsGame::fromRecords(library_.usbGames().loadUsbGames());
    result.saveStateFolderIsNowUnused = none_of(
        begin(remaining), end(remaining), [&](const PsGamePtr &other) { return other->ssFolder == game.ssFolder; });
    return result;
}

//*******************************
// GameCatalogService::removeSaveStateFolder
//*******************************
bool GameCatalogService::removeSaveStateFolder(const string &ssFolder) {
    return DirEntry::removeDirAndContents(ssFolder);
}

//*******************************
// GameCatalogService::flushAllCovers
//*******************************
int GameCatalogService::flushAllCovers() {
    string gamesDir = DirEntry::fixPath(Env::getPathToGamesDir());
    if (!DirEntry::exists(gamesDir))
        return 0;
    return removePngFilesUnder(gamesDir);
}
