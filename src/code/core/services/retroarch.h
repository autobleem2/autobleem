//
// RetroArchService: RetroArch's playlists as sets of games, and which core plays each entry.
//
#pragma once

#include "../model/ps_game.h"
#include "game_query.h"

#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

//********************
// CoreInfo
//********************
// one retroarch/info/<core>.info file: what the core is called, what it plays, and the .so it lives in
struct CoreInfo {
    std::string name;
    std::vector<std::string> extensions;
    std::vector<std::string> databases;
    std::string core_path;
};

using CoreInfoPtr = std::shared_ptr<CoreInfo>;
using CoreInfos = std::vector<CoreInfoPtr>;

//********************
// RAPlaylistInfo
//********************
struct RAPlaylistInfo {
    std::string displayName;    // the .lpl file name without its extension
    std::string path;
    PsGames psGames;

    RAPlaylistInfo(const std::string &_displayName, const std::string &_path, const PsGames &games)
        : displayName(_displayName), path(_path), psGames(games) {}
};

//********************
// RetroArchService
//********************
// Was the RAIntegrator singleton. Reads retroarch/info/*.info (the cores) and retroarch/playlists/*.lpl
// on first use - not before, because it needs Environment's paths - and answers the launcher's questions
// from that: which playlists there are, which games each holds, how many. It is the RetroArchGames that
// GameQueryService consumes, so the RetroArch and Apps sets come through the same door as the PS1 ones.
//
// Every playlist entry becomes a "foreign" PsGame whose core is resolved here: the entry's own core if the
// .so exists, else the one resources/coreOverride.cfg names for the entry's database, else the first core
// whose .info lists that database. Entries whose core or image cannot be found are dropped.
//
// Owned by App (App::retroArch()).
class RetroArchService : public RetroArchGames {
public:
    RetroArchService() {}

    // RetroArchGames, for GameQueryService
    PsGames gamesInPlaylist(const std::string &playlistName) override;
    std::string historyPlaylistName() override { return historyDisplayName_; }

    std::string favoritesPlaylistName() const { return favoritesDisplayName_; }

    // in the order the launcher shows them: the playlists sorted by name, then Favorites, then History
    std::vector<std::string> playlistNames();
    int gameCount(const std::string &playlistName);

    // RetroArch may have added or removed favorites and history entries while it ran
    void reloadFavoritesAndHistory();

    // a playlist title as RetroArch names the boxart file for it
    static std::string escapeName(const std::string &title);

private:
    void ensureLoaded();
    void loadCores();
    void loadPlaylists();
    void reloadFavorites();
    void reloadHistory();
    // the Favorites/History playlist rebuilt from RetroArch's own file, with each entry's title and db_name
    // filled in from the playlist it came from (RetroArch leaves them empty), and entries whose game is gone
    // dropped
    void reloadSpecialPlaylist(const std::string &displayName, const std::string &path, bool copyTitle);
    std::string specialPlaylistPath(const std::string &fileName) const;   // "" when RetroArch has none

    bool findPlaylist(const std::string &displayName, int *index) const;
    bool isValidPlaylist(const std::string &path) const;
    PsGames readGamesFromPlaylistFile(const std::string &path);
    bool isGameValid(const PsGame &game) const;

    bool autoDetectCorePath(const PsGame &game, std::string &core_name, std::string &core_path) const;
    bool findOverrideCore(const PsGame &game, std::string &core_name, std::string &core_path) const;
    CoreInfoPtr parseCoreInfo(const std::string &file, const std::string &entry);

    bool loaded_ = false;
    CoreInfos cores_;
    std::map<std::string, CoreInfoPtr> defaultCores_;    // database name -> core
    std::map<std::string, CoreInfoPtr> overrideCores_;   // lower-cased database name -> core, from coreOverride.cfg
    std::set<std::string> databases_;                    // every database any core's .info lists
    std::vector<RAPlaylistInfo> playlistInfos_;
    std::string favoritesDisplayName_{"Favorites"};
    std::string historyDisplayName_{"History"};
};
