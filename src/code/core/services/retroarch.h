//
// RetroArchService: RetroArch's playlists as sets of games, and which core plays each entry.
//
#pragma once

#include "../model/ps_game.h"
#include "game_query.h"

#include <ableem/engine/retroarch_cores.h>

#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

//********************
// RAPlaylistInfo
//********************
struct RAPlaylistInfo {
    std::string displayName; // the .lpl file name without its extension
    std::string path;
    PsGames psGames;
    bool metadataLoaded = false; // see RetroArchService::ensureMetadata

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
// .so exists, else the one resources/platform/<platform>.cores.cfg names for its database, else the first core
// whose .info lists that database (ableem::CoreInfoTable holds that mapping). Entries whose core or image
// cannot be found are dropped. A game's publisher, year and player count come from the system's .rdb
// (<retroarch>/database/rdb/<playlist name>.rdb) by its label, read the first time the playlist is asked
// for and then dropped again - the launcher shows one playlist at a time and a database is megabytes.
//
// Owned by App (App::retroArch()).
class RetroArchService : public RetroArchGames {
public:
    RetroArchService() = default;

    // RetroArchGames, for GameQueryService
    PsGames gamesInPlaylist(const std::string &playlistName) override;
    std::string historyPlaylistName() override { return historyDisplayName_; }
    PsGames allGames() override;

    std::string favoritesPlaylistName() const { return favoritesDisplayName_; }

    // in the order the launcher shows them: the playlists sorted by name, then Favorites, then History
    std::vector<std::string> playlistNames();
    int gameCount(const std::string &playlistName);

    // RetroArch may have added or removed favorites and history entries while it ran
    void reloadFavoritesAndHistory();
    // the background scan rewrote playlists: read them all again (the cores stay as loaded)
    void reloadPlaylists();

    // the platform's cores.cfg, next to the resources: resources/platform/<platform>.cores.cfg
    static std::string coresCfgPath();

    // a playlist title as RetroArch names the boxart file for it
    static std::string escapeName(const std::string &title);

    // a path from a playlist, as this machine sees it: a console playlist says /media/..., which is the
    // USB root there; on a dev host that prefix is mapped onto the fake USB tree, and a path that already
    // starts with the USB root (a Pi writes its real mount point, /media/autobleem/...) is left alone
    static std::string mapPlaylistPath(const std::string &path, const std::string &usbRoot);

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
    std::string specialPlaylistPath(const std::string &fileName) const; // "" when RetroArch has none

    bool findPlaylist(const std::string &displayName, int *index) const;
    void ensureMetadata(RAPlaylistInfo &info);
    bool isValidPlaylist(const std::string &path) const;
    PsGames readGamesFromPlaylistFile(const std::string &path);
    bool isGameValid(const PsGame &game) const;

    bool autoDetectCorePath(const PsGame &game, std::string &core_name, std::string &core_path) const;

    bool loaded_ = false;
    ableem::CoreInfoTable cores_;
    std::vector<RAPlaylistInfo> playlistInfos_;
    std::string favoritesDisplayName_{"Favorites"};
    std::string historyDisplayName_{"History"};
};
