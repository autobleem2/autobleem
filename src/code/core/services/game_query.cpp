//
// Created by lifting GuiLauncher::getGames_SET_* / getAllPS1Games out of the launcher screen.
//

#include "game_query.h"
#include "lightgun.h"
#include "config.h"
#include "../main.h"
#include "environment.h"

#include <algorithm>
#include <iostream>
#include <memory>
#include <ableem/engine/log.h>

using namespace std;

//*******************************
// GameQueryService::showInternalGames
//*******************************
bool GameQueryService::showInternalGames() const {
#ifndef AB_HAS_INTERNAL_GAMES
    return false; // an appliance or a Windows PC has no built-in games; the option is not offered either
                  // (GuiOptions::fill)
#else
    return config_.inifile.values["origames"] == "true";
#endif
}

//*******************************
// GameQueryService::ps1GamesInSubDirRow
//*******************************
// the games on one row of the /Games sub-directory tree. Row 0 is /Games itself, i.e. everything.
PsGames GameQueryService::ps1GamesInSubDirRow(int rowIndex, string *rowName) {
    PsGames games;

    SubDirRowInfos rowInfos;
    library_.usbGames().loadSubDirRows(&rowInfos);
    if (rowInfos.empty())
        return games; // no games at all
    if (rowIndex < 0 || static_cast<size_t>(rowIndex) >= rowInfos.size())
        return games;

    if (rowName != nullptr)
        *rowName = rowInfos[rowIndex].rowName;

    vector<int> gameIdsInRow;
    library_.usbGames().loadGameIdsInSubDirRow(&gameIdsInRow, rowIndex);

    for (auto &game : PsGame::fromRecords(library_.usbGames().loadUsbGames())) {
        if (find(begin(gameIdsInRow), end(gameIdsInRow), game->gameId) != end(gameIdsInRow))
            games.emplace_back(game);
    }
    return games;
}

//*******************************
// GameQueryService::lightgunGames
//*******************************
PsGames GameQueryService::lightgunGames() {
    PsGames games;
    if (lightguns_ == nullptr)
        return games;
    for (auto &game : allPs1Games(true, showInternalGames())) {
        if (lightguns_->isLightgun(*game))
            games.emplace_back(game);
    }
    if (retroArch_ != nullptr) {
        for (auto &game : retroArch_->allGames()) {
            if (lightguns_->isLightgun(*game))
                games.emplace_back(game);
        }
    }
    sort(begin(games), end(games), byTitle);
    return games;
}

//*******************************
// GameQueryService::internalGames
//*******************************
PsGames GameQueryService::internalGames() {
    return PsGame::fromRecords(library_.internalGames().loadInternalGames());
}

//*******************************
// GameQueryService::allPs1Games
//*******************************
PsGames GameQueryService::allPs1Games(bool includeUSB, bool includeInternal) {
    PsGames games;
    if (includeUSB)
        games = ps1GamesInSubDirRow(0);
    if (includeInternal)
        games += internalGames();
    return games;
}

//*******************************
// GameQueryService::favorites
//*******************************
PsGames GameQueryService::favorites() {
    PsGames games;
    PsGames all = allPs1Games(true, showInternalGames());
    copy_if(begin(all), end(all), back_inserter(games), [](const PsGamePtr &game) { return game->favorite; });
    return games;
}

//*******************************
// GameQueryService::history
//*******************************
PsGames GameQueryService::history() {
    PsGames games;
    PsGames all = allPs1Games(true, showInternalGames());
    copy_if(begin(all), end(all), back_inserter(games), [](const PsGamePtr &game) { return game->history > 0; });
    return games;
}

//*******************************
// GameQueryService::retroArchGames
//*******************************
PsGames GameQueryService::retroArchGames(const string &playlistName) {
    PLOG_INFO << "Getting RA games for playlist: " << playlistName;
    if (playlistName.empty() || retroArch_ == nullptr)
        return PsGames();
    return retroArch_->gamesInPlaylist(playlistName);
}

//*******************************
// GameQueryService::apps
//*******************************
// usb:/Apps/<name>/app.ini, one launchable app each. These are "foreign" games: no database row, no serial,
// everything the UI shows comes out of the ini.
PsGames GameQueryService::apps() {
    PsGames games;

    string appPath = Env::getPathToAppsDir();
    if (!DirEntry::exists(appPath))
        return games;

    PLOG_INFO << "Scanning apps in: " << appPath;
    for (auto &dir : DirEntry::diru_DirsOnly(appPath)) {
        string appIni = appPath + sep + dir.name + sep + "app.ini";
        PLOG_INFO << "AppIni: " << appIni;
        if (!DirEntry::exists(appIni))
            continue;

        IniFile file;
        file.load(appIni);

        PsGamePtr game = std::make_shared<PsGame>();
        game->gameId = 0;
        game->year = 0;
        game->players = 0;
        game->memcard = "";
        game->cds = 0;

        game->title = file.values["title"];
        game->publisher = file.values["author"];
        game->readme_path = appPath + sep + dir.name + sep + file.values["readme"];
        game->startup = file.values["startup"];
        game->image_path = appPath + sep + dir.name + sep + file.values["image"];
        game->base = appPath + sep + dir.name;
        game->kernel = file.values["kernel"] == "true";
        game->app = true;
        game->foreign = true;

        games.push_back(game);
    }
    return games;
}

//*******************************
// GameQueryService::gamesFor
//*******************************
PsGames GameQueryService::gamesFor(GameSetSelection &selection) {
    PsGames games;

    if (selection.set == GameSet::PS1) {
        // with internal games switched off there is nothing to show on the two views that include them,
        // so fall back to the sub-directory view
        if (!showInternalGames()) {
            if (selection.ps1SelectState == Ps1SelectState::AllGames ||
                selection.ps1SelectState == Ps1SelectState::InternalOnly) {
                selection.ps1SelectState = Ps1SelectState::GamesSubdir;
            }
        }

        switch (selection.ps1SelectState) {
        case Ps1SelectState::AllGames:
            games = allPs1Games(true, showInternalGames());
            break;
        case Ps1SelectState::InternalOnly:
            games = internalGames();
            break;
        case Ps1SelectState::GamesSubdir:
            games = ps1GamesInSubDirRow(selection.usbGameDirIndex, &selection.usbGameDirName);
            break;
        case Ps1SelectState::Favorites:
            games = favorites();
            break;
        case Ps1SelectState::History:
            games = history();
            break;
        }
    } else if (selection.set == GameSet::RetroArch) {
        games = retroArchGames(selection.raPlaylistName);
    } else if (selection.set == GameSet::Lightgun) {
        games = lightgunGames();
    } else if (selection.set == GameSet::Apps) {
        games = apps();
    }

    // the RetroArch history playlist arrives most-recently-played first and must keep that order
    bool alreadyOrdered = selection.set == GameSet::RetroArch && retroArch_ != nullptr &&
                          selection.raPlaylistName == retroArch_->historyPlaylistName();
    if (!alreadyOrdered) {
        if (selection.set == GameSet::PS1 && selection.ps1SelectState == Ps1SelectState::History)
            sort(begin(games), end(games), byHistory);
        else
            sort(begin(games), end(games), byTitle);
    }
    return games;
}
