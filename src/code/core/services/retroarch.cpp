//
// RetroArchService: what the RAIntegrator singleton used to be.
//
#include "retroarch.h"
#include "environment.h"
#include "../main.h"

#include <ableem/engine/rdb_reader.h>
#include <ableem/engine/retroarch_playlist.h>
#include <ableem/engine/thumbnail_lookup.h>

#include <algorithm>
#include <memory>
#include <set>
#include <fstream>
#include <iostream>
#include <sstream>
#include <ableem/engine/log.h>

using namespace std;

//********************
// RetroArchService::ensureLoaded
//********************
// The singleton read everything the first time it was asked for, which is after main() has configured
// Environment. Same here: nothing is read until the first question.
void RetroArchService::ensureLoaded() {
    if (loaded_)
        return;
    loaded_ = true;
    loadCores();
    loadPlaylists();
}

//********************
// RetroArchService::escapeName
//********************
string RetroArchService::escapeName(const string &title) {
    return ableem::ThumbnailLookup::escapeName(title); // one rule for every thumbnail file name
}

//********************
// RetroArchService::gamesInPlaylist
//********************
PsGames RetroArchService::gamesInPlaylist(const string &playlistName) {
    ensureLoaded();
    int index;
    if (findPlaylist(playlistName, &index)) {
        ensureMetadata(playlistInfos_[index]);
        return playlistInfos_[index].psGames;
    }
    return PsGames();
}

//********************
// RetroArchService::ensureMetadata
//********************
// The playlist's games are named as the database names them once the ROM scan has identified them, so a
// lookup by name gives their publisher, year and player count. Favorites and History copy theirs from the
// playlist the game came from (reloadSpecialPlaylist) and never come here with anything to read.
void RetroArchService::ensureMetadata(RAPlaylistInfo &info) {
    if (info.metadataLoaded)
        return;
    info.metadataLoaded = true;
    if (info.displayName == favoritesDisplayName_ || info.displayName == historyDisplayName_)
        return;
    const string rdbPath = Env::getPathToRetroarchRdbDir() + sep + info.displayName + ".rdb";
    if (!DirEntry::exists(rdbPath))
        return;
    ableem::RdbReader rdb;
    if (!rdb.open(rdbPath))
        return;
    int found = 0;
    for (PsGamePtr &game : info.psGames) {
        const ableem::RdbReader::Record *record = rdb.findByName(game->title);
        if (!record)
            continue;
        game->publisher = record->publisher;
        game->year = record->releaseyear;
        game->players = record->users;
        found++;
    }
    PLOG_INFO << "Metadata for " << info.displayName << ": " << found << " of " << info.psGames.size()
              << " games in the database";
}

//********************
// RetroArchService::allGames
//********************
// every playlist but Favorites and History (those repeat games the platform playlists hold), one entry
// per image path
PsGames RetroArchService::allGames() {
    ensureLoaded();
    PsGames games;
    std::set<string> seen;
    for (auto &info : playlistInfos_) {
        if (info.displayName == favoritesDisplayName_ || info.displayName == historyDisplayName_)
            continue;
        ensureMetadata(info);
        for (auto &game : info.psGames) {
            if (seen.insert(game->image_path).second)
                games.push_back(game);
        }
    }
    return games;
}

//********************
// RetroArchService::playlistNames
//********************
vector<string> RetroArchService::playlistNames() {
    ensureLoaded();
    vector<string> names;
    for (auto &info : playlistInfos_)
        names.emplace_back(info.displayName);
    return names;
}

//********************
// RetroArchService::gameCount
//********************
int RetroArchService::gameCount(const string &playlistName) {
    ensureLoaded();
    int index;
    if (findPlaylist(playlistName, &index))
        return playlistInfos_[index].psGames.size();
    return 0;
}

//********************
// RetroArchService::reloadFavoritesAndHistory
//********************
void RetroArchService::reloadFavoritesAndHistory() {
    ensureLoaded();
    reloadFavorites();
    reloadHistory();
}

//********************
// RetroArchService::reloadPlaylists
//********************
void RetroArchService::reloadPlaylists() {
    ensureLoaded();
    playlistInfos_.clear();
    loadPlaylists();
}

//********************
// RetroArchService::coresCfgPath
//********************
string RetroArchService::coresCfgPath() {
    return Env::getWorkingPath() + sep + "platform" + sep + Env::platformName() + ".cores.cfg";
}

//********************
// RetroArchService::findPlaylist
//********************
bool RetroArchService::findPlaylist(const string &displayName, int *index) const {
    auto it = find_if(begin(playlistInfos_), end(playlistInfos_),
                      [&](const RAPlaylistInfo &info) { return displayName == info.displayName; });
    if (it == end(playlistInfos_))
        return false;
    *index = it - begin(playlistInfos_);
    return true;
}

//********************
// RetroArchService::isValidPlaylist
//********************
bool RetroArchService::isValidPlaylist(const string &path) const {
    // check file extension
    if (toLowerCopy(DirEntry::getFileExtension(path)) != "lpl") {
        PLOG_INFO << "Extension is not .lpl";
        return false;
    }
    // check if not empty
    std::ifstream in(path, std::ifstream::ate | std::ifstream::binary);
    if (in.tellg() <= 0) {
        PLOG_INFO << "Playlist looks like empty file";
        return false;
    }
    return true;
}

//********************
// RetroArchService::readGamesFromPlaylistFile
//********************
// Every entry becomes a foreign PsGame with its core resolved. RAIntegrator had this twice, once per
// playlist format, and the two had drifted: only the JSON one mapped "/media" onto the USB root for the dev
// host, and the six-line one skipped an entry whose core could not be detected where the JSON one let
// isGameValid drop it a few lines later. One copy now, and the same result for both formats.
PsGames RetroArchService::readGamesFromPlaylistFile(const string &path) {
    PLOG_INFO << "Parsing Playlist: " << path;
    PsGames psGames;
    ableem::RetroArchPlaylistEntries entries;
    if (!ableem::RetroArchPlaylist::load(path, entries)) {
        PLOG_INFO << "Games found: 0";
        return psGames;
    }

    const string usbRoot = Env::getPathToUSBRoot();
    int id = 0;
    for (const auto &entry : entries) {
        PsGamePtr game = std::make_shared<PsGame>();
        game->gameId = id++;
        game->title = entry.label;
        game->publisher = "";
        game->year = 0;
        game->players = 0;
        game->folder = "";
        game->ssFolder = "";
        game->base = "";
        game->memcard = "";
        game->cds = 0;
        game->locked = true;
        game->hd = false;
        game->favorite = false;
        game->foreign = true;
        game->core_name = entry.core_name;
        game->core_path = entry.core_path;
        game->db_name = entry.db_name;
        game->image_path = entry.path;

        game->image_path = mapPlaylistPath(entry.path, usbRoot);
        game->core_path = mapPlaylistPath(entry.core_path, usbRoot);

        if ((game->core_path == "DETECT") || (game->core_name == "DETECT")) {
            autoDetectCorePath(*game, game->core_name, game->core_path);
        }
        if (!DirEntry::exists(game->core_path)) {
            autoDetectCorePath(*game, game->core_name, game->core_path);
        }
        if (isGameValid(*game)) {
            psGames.emplace_back(game);
        } else {
            PLOG_WARNING << "Game invalid: title = '" << game->title << "'";
        }
    }
    PLOG_INFO << "Games found: " << psGames.size();
    return psGames;
}

//********************
// RetroArchService::mapPlaylistPath
//********************
string RetroArchService::mapPlaylistPath(const string &path, const string &usbRoot) {
    if (path.rfind("/media", 0) != 0 || usbRoot == "/media")
        return path;
    if (path.rfind(usbRoot + "/", 0) == 0 || path == usbRoot)
        return path;
    return usbRoot + path.substr(6);
}

//********************
// RetroArchService::isGameValid
//********************
bool RetroArchService::isGameValid(const PsGame &game) const {
    if (!DirEntry::exists(game.core_path)) {
        return false;
    }
    // an entry inside an archive is "archive.zip#rom"; the archive is what has to exist
    string path = game.image_path;
    if (path.find("#") != string::npos) {
        int pos = path.find("#");
        string check = path.substr(0, pos);
        if (!DirEntry::exists(check)) {
            return false;
        }
    } else {
        if (!DirEntry::exists(path)) {
            return false;
        }
    }
    return true;
}

//********************
// RetroArchService::specialPlaylistPath
//********************
string RetroArchService::specialPlaylistPath(const string &fileName) const {
    string defaultPath{Env::getPathToRetroarchDir() + sep + fileName};
    if (DirEntry::exists(defaultPath))
        return defaultPath;
    return "";
}

//********************
// RetroArchService::reloadFavorites / reloadHistory
//********************
// When RetroArch adds a game to favorites its crc32 and db_name are empty, and for history the title is
// too; the db_name is needed for the boxart path. Both are filled in from the playlist the game came from.
void RetroArchService::reloadFavorites() {
    reloadSpecialPlaylist(favoritesDisplayName_, specialPlaylistPath("content_favorites.lpl"), false);
}

void RetroArchService::reloadHistory() {
    reloadSpecialPlaylist(historyDisplayName_, specialPlaylistPath("content_history.lpl"), true);
}

//********************
// RetroArchService::reloadSpecialPlaylist
//********************
void RetroArchService::reloadSpecialPlaylist(const string &displayName, const string &path, bool copyTitle) {
    if (path == "")
        return;

    PsGames games = readGamesFromPlaylistFile(path);

    // find the original game in the playlists and copy the title and db_name to the entry for the same game
    for (auto &game : games) {
        for (auto &info : playlistInfos_) {
            if (info.displayName != displayName) {
                auto it = find_if(begin(info.psGames), end(info.psGames),
                                  [&](const PsGamePtr &original) { return original->image_path == game->image_path; });
                if (it != end(info.psGames)) {
                    ensureMetadata(info); // the source playlist's database, if it has not been read yet
                    if (copyTitle)
                        game->title = (*it)->title;
                    game->db_name = (*it)->db_name;
                    game->publisher = (*it)->publisher;
                    game->year = (*it)->year;
                    game->players = (*it)->players;
                    break;
                }
            }
        }
    }

    // remove any games in the playlist that no longer exist
    auto it = remove_if(begin(games), end(games),
                        [&](const PsGamePtr &game) { return game->title == "" || game->db_name == ""; });
    games.erase(it, end(games));

    int index;
    if (findPlaylist(displayName, &index)) {
        // the prior list is in the list.  modify the existing entry
        RAPlaylistInfo &info = playlistInfos_[index];
        info.path = path;
        info.psGames = games;
    } else {
        // the list didn't exist before and isn't in the list.  add a new entry.
        playlistInfos_.emplace_back(displayName, path, games);
    }
}

//********************
// RetroArchService::loadPlaylists
//********************
void RetroArchService::loadPlaylists() {
    string path = Env::getPathToRetroarchPlaylistsDir();
    PLOG_INFO << "Checking playlists path" << path;

    if (!DirEntry::exists(path))
        return;

    vector<DirEntry> entries = DirEntry::diru_FilesOnly(path);
    PLOG_INFO << "Total Playlists:" << entries.size();
    vector<string> playlistNames;
    for (const DirEntry &entry : entries) {
        // AutoBleem's own export and the Apps list are not RetroArch platforms
        if (DirEntry::getFileNameWithoutExtension(entry.name) == "AutoBleem")
            continue;
        if (DirEntry::getFileNameWithoutExtension(entry.name) == "Applications")
            continue;
        playlistNames.emplace_back(entry.name);
    }

    // sort the playlist names and if any favorites or history add them at the end
    sort(begin(playlistNames), end(playlistNames));

    for (auto &playlistName : playlistNames) {
        PLOG_INFO << "Playlist: " << playlistName;
        string playlistPath = Env::getPathToRetroarchPlaylistsDir() + sep + playlistName;
        if (isValidPlaylist(playlistPath)) {
            PsGames games = readGamesFromPlaylistFile(playlistPath);
            string nameOnly = DirEntry::getFileNameWithoutExtension(playlistName);
            if (games.size() > 0)
                playlistInfos_.emplace_back(nameOnly, playlistPath, games);
            else
                PLOG_INFO << "Playlist has no games: " << playlistName;
        } else
            PLOG_WARNING << "Invalid Playlist: " << playlistName;
    }
    reloadFavorites(); // since it isn't already in the list, reloadFavorites() will add favorites at the end
    reloadHistory();   // since it isn't already in the list, reloadHistory() will add history at the end
}

//********************
// RetroArchService::autoDetectCorePath
//********************
bool RetroArchService::autoDetectCorePath(const PsGame &game, string &core_name, string &core_path) const {
    ableem::CoreInfoPtr core = cores_.coreForDatabase(game.db_name);
    if (!core) {
        core_name = "DETECT";
        core_path = "DETECT";
        return false;
    }
    core_name = core->name;
    core_path = core->core_path;
    return true;
}

//********************
// RetroArchService::loadCores
//********************
void RetroArchService::loadCores() {
    PLOG_INFO << "Building core list";
    cores_.load(Env::getPathToRetroarchDir(), coresCfgPath());
}
