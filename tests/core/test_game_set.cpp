//
// GameSet / Ps1SelectState / GameSetSelection - the carousel's position, as ab_core sees it.
//
#include "doctest/doctest.h"

#include "core/model/game_set.h"

TEST_CASE("nextGameSet cycles the four sets and wraps") {
    CHECK(nextGameSet(GameSet::PS1) == GameSet::RetroArch);
    CHECK(nextGameSet(GameSet::RetroArch) == GameSet::Lightgun);
    CHECK(nextGameSet(GameSet::Lightgun) == GameSet::Apps);
    CHECK(nextGameSet(GameSet::Apps) == GameSet::PS1);
}

TEST_CASE("nextGameSet visits every set and returns to the start") {
    GameSet set = GameSet::PS1;
    for (int step = 0; step <= static_cast<int>(GameSetLast); ++step) {
        set = nextGameSet(set);
    }
    CHECK(set == GameSet::PS1);
}

TEST_CASE("the enum values are the indexes the name tables are keyed by") {
    // GuiLauncher::showSetName indexes its setNames / setPS1SubStateNames vectors with these, and asserts
    // the tables are the right length - so the numbering is part of the contract, not an accident.
    CHECK(static_cast<int>(GameSet::PS1) == 0);
    CHECK(static_cast<int>(GameSet::RetroArch) == 1);
    CHECK(static_cast<int>(GameSet::Lightgun) == 2);
    CHECK(static_cast<int>(GameSet::Apps) == 3);
    CHECK(GameSetLast == GameSet::Apps);

    CHECK(static_cast<int>(Ps1SelectState::AllGames) == 0);
    CHECK(static_cast<int>(Ps1SelectState::InternalOnly) == 1);
    CHECK(static_cast<int>(Ps1SelectState::Favorites) == 2);
    CHECK(static_cast<int>(Ps1SelectState::History) == 3);
    CHECK(static_cast<int>(Ps1SelectState::GamesSubdir) == 4);   // must stay last: left off the L2+Select menu
}

TEST_CASE("a default GameSetSelection opens on all PS1 games") {
    GameSetSelection selection;

    CHECK(selection.set == GameSet::PS1);
    CHECK(selection.ps1SelectState == Ps1SelectState::AllGames);
    CHECK(selection.gameIndex == 0);
    CHECK(selection.usbGameDirIndex == 0);
    CHECK(selection.raPlaylistIndex == 0);
    CHECK(selection.usbGameDirName.empty());
    CHECK(selection.raPlaylistName.empty());
}

TEST_CASE("GameSetSelection copies whole, which is what the save/restore relies on") {
    GameSetSelection saved;
    saved.set = GameSet::RetroArch;
    saved.ps1SelectState = Ps1SelectState::Favorites;
    saved.gameIndex = 17;
    saved.usbGameDirIndex = 3;
    saved.usbGameDirName = "Racing";
    saved.raPlaylistIndex = 2;
    saved.raPlaylistName = "Sony - PlayStation.lpl";

    GameSetSelection restored = saved;

    CHECK(restored.set == GameSet::RetroArch);
    CHECK(restored.ps1SelectState == Ps1SelectState::Favorites);
    CHECK(restored.gameIndex == 17);
    CHECK(restored.usbGameDirIndex == 3);
    CHECK(restored.usbGameDirName == "Racing");
    CHECK(restored.raPlaylistIndex == 2);
    CHECK(restored.raPlaylistName == "Sony - PlayStation.lpl");
}
