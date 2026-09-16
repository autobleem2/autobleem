//
// GameQueryService: which games a carousel set shows, and in what order.
//
#pragma once

#include "../model/game_set.h"
#include "../model/ps_game.h"

#include <ableem/engine/game_library.h>
#include <ableem/engine/strings.h>

#include <string>

class Config;

//******************
// RetroArchGames
//******************
// The RetroArch half of the query. It stays outside ab_core until RAIntegrator becomes RetroArchService
// (plan step 11) - RAIntegrator implements this today, and the tests pass a stub.
struct RetroArchGames {
    virtual ~RetroArchGames() {}
    virtual PsGames gamesInPlaylist(const std::string &playlistName) = 0;
    // the playlist that is already in most-recently-played order, so it must not be re-sorted by title
    virtual std::string historyPlaylistName() = 0;
};

//******************
// GameQueryService
//******************
// Everything GuiLauncher::switchSet used to work out for itself about *which* games to show. No screen and
// no carousel here: gamesFor() answers with a sorted PsGames and nothing else, which is what makes the
// answer checkable against a fixture database instead of against a screenshot.
//
// Owned by App (App::gameQuery()).
class GameQueryService {
public:
    GameQueryService(ableem::GameLibrary &library, Config &config) : library_(library), config_(config) {}

    // RAIntegrator is a launcher singleton, so the composition root hands it over rather than core reaching
    // for it. Null until then: the RetroArch set is simply empty, which is what an unconfigured RetroArch is.
    void setRetroArchGames(RetroArchGames *source) { retroArch_ = source; }

    // The whole query for one selection, sorted the way that set is displayed.
    //
    // The selection is taken by reference because two of the sets legitimately write back to it: the PS1
    // sub-set is forced off the internal-games views when config.ini says origames=false, and the sub-dir
    // view fills in the name of the row it landed on (the "Showing: USB Games Directory: X" line reads it).
    PsGames gamesFor(GameSetSelection &selection);

    // the individual sets. The game-dir menu uses these directly, to count the games on each row before it
    // has a selection to ask about.
    PsGames ps1GamesInSubDirRow(int rowIndex, std::string *rowName = nullptr);
    PsGames internalGames();
    PsGames allPs1Games(bool includeUSB, bool includeInternal);
    PsGames favorites();
    PsGames history();
    PsGames retroArchGames(const std::string &playlistName);
    PsGames apps();

    bool showInternalGames() const;     // config.ini "origames"

    static bool byTitle(const PsGamePtr &l, const PsGamePtr &r) { return ableem::lessCaseInsensitive(l->title, r->title); }
    // history is numbered 1..N with 1 the most recently played
    static bool byHistory(const PsGamePtr &l, const PsGamePtr &r) { return l->history < r->history; }

private:
    ableem::GameLibrary &library_;
    Config &config_;
    RetroArchGames *retroArch_ = nullptr;
};
