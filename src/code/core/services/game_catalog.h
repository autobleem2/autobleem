//
// GameCatalogService: the changes to the library, as opposed to the questions about it.
//
#pragma once

#include "../model/ps_game.h"

#include <ableem/engine/game_library.h>

#include <string>

class GameQueryService;

//******************
// GameCatalogService
//******************
// Where GameQueryService reads the library, this one writes it: what "most recently played" means, and
// removing a game and the files behind it. Both used to sit inside the screens that happened to need them
// first - the history renumbering in GuiLauncher, the delete in GuiManager.
//
// Owned by App (App::gameCatalog()).
class GameCatalogService {
public:
    GameCatalogService(ableem::GameLibrary &library, GameQueryService &query)
            : library_(library), query_(query) {}

    // The history is a ranking, not a timestamp: 1 is the game just played, 2 the one before it, and so on.
    // Anything that would rank past HistoryLimit drops out of the history altogether.
    static const int HistoryLimit = 100;    // the same limit RetroArch's own history uses
    void recordGamePlayed(const PsGamePtr &game);

    //******************
    // deleting a game
    //******************
    struct DeleteResult {
        bool removed = false;                   // the database row and the game folder are both gone
        bool saveStateFolderIsNowUnused = false;// no remaining game shares the folder, so it may be deleted
        std::string saveStateFolder;
    };
    // Removes the game from regional.db and deletes its folder. The !SaveStates folder is deliberately left
    // alone: more than one game can share one, so the caller checks saveStateFolderIsNowUnused, asks the
    // user, and then calls removeSaveStateFolder().
    DeleteResult deleteUsbGame(const PsGame &game);
    bool removeSaveStateFolder(const std::string &ssFolder);

    // Deletes every cover .png under the Games tree so the next scan fetches them again. Returns how many
    // files were removed.
    int flushAllCovers();

private:
    ableem::GameLibrary &library_;
    GameQueryService &query_;
};
