//
// RetroArchService: what the RAIntegrator singleton used to be.
//
#include "retroarch.h"
#include "environment.h"
#include "../main.h"

#include <ableem/engine/retroarch_playlist.h>

#include <algorithm>
#include <fstream>
#include <iostream>
#include <sstream>

using namespace std;

namespace {

bool sortByMaxExtensions(const CoreInfoPtr &i, const CoreInfoPtr &j) {
    return i->extensions.size() > j->extensions.size();
}

} // namespace

//********************
// RetroArchService::ensureLoaded
//********************
// The singleton read everything the first time it was asked for, which is after main() has configured
// Environment. Same here: nothing is read until the first question.
void RetroArchService::ensureLoaded() {
    if (loaded_) return;
    loaded_ = true;
    loadCores();
    loadPlaylists();
}

//********************
// RetroArchService::escapeName
//********************
string RetroArchService::escapeName(const string &title) {
    return DirEntry::replaceTheseCharsWithThisChar(title, "&*/:`<>?\\|", '_');
}

//********************
// RetroArchService::gamesInPlaylist
//********************
PsGames RetroArchService::gamesInPlaylist(const string &playlistName) {
    ensureLoaded();
    int index;
    if (findPlaylist(playlistName, &index))
        return playlistInfos_[index].psGames;
    return PsGames();
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
        cout << "Extension is not .lpl" << endl;
        return false;
    }
    // check if not empty
    std::ifstream in(path, std::ifstream::ate | std::ifstream::binary);
    if (in.tellg() <= 0) {
        cout << "Playlist looks like empty file" << endl;
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
    cout << "Parsing Playlist: " << path << endl;
    PsGames psGames;
    ableem::RetroArchPlaylistEntries entries;
    if (!ableem::RetroArchPlaylist::load(path, entries)) {
        cout << "Games found: 0" << endl;
        return psGames;
    }

    const string usbRoot = Env::getPathToUSBRoot();
    int id = 0;
    for (const auto &entry : entries) {
        PsGamePtr game{new PsGame};
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

        // RetroArch writes absolute /media paths. On the console the USB root is /media, so this is the
        // identity; on a dev host it is what makes the playlist point into the fake USB tree.
        if (game->image_path.substr(0, 6) == "/media")
            game->image_path.replace(0, 6, usbRoot);
        if (game->core_path.substr(0, 6) == "/media")
            game->core_path.replace(0, 6, usbRoot);

        if ((game->core_path == "DETECT") || (game->core_name == "DETECT")) {
            autoDetectCorePath(*game, game->core_name, game->core_path);
        }
        if (!DirEntry::exists(game->core_path)) {
            autoDetectCorePath(*game, game->core_name, game->core_path);
        }
        if (isGameValid(*game)) {
            psGames.emplace_back(game);
        } else {
            cout << "Game invalid: title = '" << game->title << "'" << endl;
        }
    }
    cout << "Games found: " << psGames.size() << endl;
    return psGames;
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
        for (auto const &info : playlistInfos_) {
            if (info.displayName != displayName) {
                auto it = find_if(begin(info.psGames), end(info.psGames),
                                  [&](const PsGamePtr &original) { return original->image_path == game->image_path; });
                if (it != end(info.psGames)) {
                    if (copyTitle)
                        game->title = (*it)->title;
                    game->db_name = (*it)->db_name;
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
    cout << "Checking playlists path" << path << endl;

    if (!DirEntry::exists(path))
        return;

    vector<DirEntry> entries = DirEntry::diru_FilesOnly(path);
    cout << "Total Playlists:" << entries.size() << endl;
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
        cout << "Playlist: " << playlistName << endl;
        string playlistPath = Env::getPathToRetroarchPlaylistsDir() + sep + playlistName;
        if (isValidPlaylist(playlistPath)) {
            PsGames games = readGamesFromPlaylistFile(playlistPath);
            string nameOnly = DirEntry::getFileNameWithoutExtension(playlistName);
            if (games.size() > 0)
                playlistInfos_.emplace_back(nameOnly, playlistPath, games);
            else
                cout << "Playlist has no games: " << playlistName << endl;
        } else
            cout << "Invalid Playlist: " << playlistName << endl;
    }
    reloadFavorites();  // since it isn't already in the list, reloadFavorites() will add favorites at the end
    reloadHistory();    // since it isn't already in the list, reloadHistory() will add history at the end
}

//********************
// RetroArchService::findOverrideCore
//********************
bool RetroArchService::findOverrideCore(const PsGame &game, string &core_name, string &core_path) const {
    string dbName = DirEntry::getFileNameWithoutExtension(game.db_name);

    lcase(dbName);
    trim(dbName);
    auto pos = overrideCores_.find(dbName);
    if (pos == overrideCores_.end()) {
        core_name = "DETECT";
        core_path = "DETECT";
        return false;
    }
    core_name = pos->second->name;
    core_path = pos->second->core_path;
    return true;
}

//********************
// RetroArchService::autoDetectCorePath
//********************
bool RetroArchService::autoDetectCorePath(const PsGame &game, string &core_name, string &core_path) const {
    if (findOverrideCore(game, core_name, core_path)) {
        return true;
    }
    string dbName = DirEntry::getFileNameWithoutExtension(game.db_name);
    auto pos = defaultCores_.find(dbName);
    if (pos == defaultCores_.end()) {
        core_name = "DETECT";
        core_path = "DETECT";
        return false;
    }
    core_name = pos->second->name;
    core_path = pos->second->core_path;
    return true;
}

//********************
// RetroArchService::loadCores
//********************
void RetroArchService::loadCores() {
    cout << "Building core list" << endl;
    if (!DirEntry::exists(Env::getPathToRetroarchDir())) {
        cout << "Retroarch Not Found" << endl;
        return;
    }
    cores_.clear();
    databases_.clear();
    defaultCores_.clear();
    string infoFolder = Env::getPathToRetroarchDir() + sep + "info/";
    cout << "Scanning: " << infoFolder << endl;
    vector<DirEntry> entries = DirEntry::diru_FilesOnly(infoFolder);
    cout << "Found files:" << entries.size() << endl;
    for (const DirEntry &entry : entries) {
        if (DirEntry::getFileExtension(entry.name) == "info") {
            string fullPath = infoFolder + sep + entry.name;
            cores_.push_back(parseCoreInfo(fullPath, entry.name));
        }
    }
    sort(cores_.begin(), cores_.end(), sortByMaxExtensions); // why not

    // each database gets the first core (most extensions first) whose .info lists it
    for (const string &dbname : databases_) {
        bool nextDb = false;

        for (CoreInfoPtr ciPtr : cores_) {
            for (const string &db : ciPtr->databases) {
                if (dbname == db) {
                    defaultCores_.insert(std::pair<string, CoreInfoPtr>(db, ciPtr));
                    nextDb = true;
                }
                if (nextDb) continue;
            }
            if (nextDb) continue;
        }

        auto pos = defaultCores_.find(dbname);
        if (pos == defaultCores_.end()) {
            continue;
        }
        cout << "Mapping DB: " << dbname << "  Core: " << pos->second->name << endl;
    }

    // resources/coreOverride.cfg: "<database name>=<part of a core's display name>", one per line
    overrideCores_.clear();
    ifstream in(Env::getWorkingPath() + sep + "coreOverride.cfg");
    string line;
    while (getline(in, line)) {
        string db_name = line.substr(0, line.find("="));
        string value = line.substr(line.find("=") + 1);
        cout << "Custom Core Override: " << db_name << "    core: " << value << endl;

        for (CoreInfoPtr ciPtr : cores_) {
            if (ciPtr->name.find(value) != string::npos) {
                lcase(db_name);
                trim(db_name);
                overrideCores_.insert(std::pair<string, CoreInfoPtr>(db_name, ciPtr));
                cout << "Found: " << db_name << "    core: " << ciPtr->name << " " << ciPtr->core_path << endl;
            }
        }
    }
    in.close();
}

//********************
// RetroArchService::parseCoreInfo
//********************
CoreInfoPtr RetroArchService::parseCoreInfo(const string &file, const string &entry) {
    ifstream in(file);
    string line;

    cout << "Parsing " << endl;
    CoreInfoPtr coreInfoPtr{new CoreInfo};
    coreInfoPtr->core_path = Env::getPathToRetroarchDir() + sep + "cores/" + DirEntry::getFileNameWithoutExtension(entry) + ".so";
    coreInfoPtr->extensions.clear();
    cout << "CorePath: " << coreInfoPtr->core_path << endl;
    while (getline(in, line)) {
        string lcaseline = line;
        lcase(lcaseline);

        if (lcaseline.rfind("display_name", 0) == 0) {
            string value = line.substr(lcaseline.find("=") + 1);
            value.erase(remove(value.begin(), value.end(), '\"'), value.end());
            trim(value);
            coreInfoPtr->name = value;
            cout << "CoreName: " << coreInfoPtr->name << endl;
        }
        if (lcaseline.rfind("supported_extensions", 0) == 0) {
            string value = line.substr(lcaseline.find("=") + 1);
            value.erase(remove(value.begin(), value.end(), '\"'), value.end());
            trim(value);

            coreInfoPtr->extensions.clear();
            std::stringstream check1(value);
            string intermediate;
            while (getline(check1, intermediate, '|')) {
                coreInfoPtr->extensions.push_back(intermediate);
            }
        }
        if (lcaseline.rfind("database", 0) == 0) {
            string value = line.substr(lcaseline.find("=") + 1);
            value.erase(remove(value.begin(), value.end(), '\"'), value.end());
            trim(value);

            coreInfoPtr->databases.clear();
            std::stringstream check1(value);
            string intermediate;
            while (getline(check1, intermediate, '|')) {
                coreInfoPtr->databases.push_back(intermediate);
                databases_.insert(intermediate);
            }
        }
    }
    in.close();
    return coreInfoPtr;
}
