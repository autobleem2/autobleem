//
// Which games the carousel is showing, and where in them the cursor is.
//
#pragma once

#include <string>

// the carousel's "set". if you add one, also update GuiLauncher::showSetName's setNames and GameSetLast.
enum class GameSet : int { PS1 = 0, RetroArch = 1, Apps = 2 };
constexpr GameSet GameSetLast = GameSet::Apps;

// cycles PS1 -> RetroArch -> Apps -> PS1, the order Select steps through the sets in.
inline GameSet nextGameSet(GameSet set) {
    int next = static_cast<int>(set) + 1;
    return next > static_cast<int>(GameSetLast) ? GameSet::PS1 : static_cast<GameSet>(next);
}

// GameSet::PS1 sub-states. keep GamesSubdir last: it is left off the L2+Select menu.
enum class Ps1SelectState : int { AllGames = 0, InternalOnly, Favorites, History, GamesSubdir };

//******************
// GameSetSelection
//******************
// Everything that says "the carousel is showing these games, positioned here". GuiLauncher works on its own
// copy while it runs and hands it to the Session when a game starts, so pressing Start later reopens the
// carousel in the same place. Before this was six loose fields mirrored on both sides and copied across by
// hand at four call sites.
struct GameSetSelection {
    GameSet set = GameSet::PS1;
    Ps1SelectState ps1SelectState = Ps1SelectState::AllGames;
    int gameIndex = 0;                  // index into GuiLauncher::carouselGames
    int usbGameDirIndex = 0;            // row 0 of the game-dir menu is /Games itself
    std::string usbGameDirName;
    int raPlaylistIndex = 0;            // row 0 is the first playlist name
    std::string raPlaylistName;
};
