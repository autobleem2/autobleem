//
// MemcardService: which card set a game plays with, and the swap around a launch.
//
#include "doctest/doctest.h"

#include "../support/game_library_fixture.h"
#include "../support/string_maker.h"

#include "core/services/memcard.h"

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

using std::string;
using std::vector;

namespace {

// a library with one USB game, plus the !MemCards tree and the blank card templates a set is created from
struct Cards : GameLibraryFixture {
    Cards() {
        addUsbGame(1, "Tekken 3");
        addSubDirRow(0, "Games", 0, 1);
        putGameInSubDirRow(0, 1);

        // MemcardManager::create copies these two blanks into a new set
        tmp.makeSubDir("memcard");
        tmp.writeFile("memcard/card1.mcd", "blank one");
        tmp.writeFile("memcard/card2.mcd", "blank two");
        ableem::Environment::setWorkingPath(tmp.path());

        tmp.makeSubDir("Games/!MemCards");
        service.reset(new MemcardService(library));
    }

    // the game as the database hands it back, which is what the service is given in production
    PsGamePtr game() {
        PsGames games = PsGame::fromRecords(library.usbGames().loadUsbGames());
        REQUIRE(games.size() == 1);
        return games[0];
    }

    // the two cards a game plays with live here while it runs
    void giveGameItsOwnCards() {
        tmp.makeSubDir("Games/Tekken 3/sstates/memcards");
        tmp.writeFile("Games/Tekken 3/sstates/memcards/card1.mcd", "in play");
    }

    std::unique_ptr<MemcardService> service;
};

} // namespace

TEST_CASE("an internal game always plays with the console's own card") {
    Cards lib;
    lib.addInternalGame(10, "Jumping Flash");
    PsGames internal = PsGame::fromRecords(lib.library.internalGames().loadInternalGames());
    REQUIRE(internal.size() == 1);

    CHECK(lib.service->activeCardName(*internal[0]) == MemcardService::SonyCard);
}

TEST_CASE("a USB game's card set comes from its Game.ini") {
    Cards lib;
    lib.tmp.writeFile("Games/Tekken 3/" + string(ableem::GAME_INI), "[Game]\nMemcard=Fighting\n");

    CHECK(lib.service->activeCardName(*lib.game()) == "Fighting");
}

TEST_CASE("setCardForGame records the set in the Game.ini and in the database") {
    Cards lib;

    PsGamePtr game = lib.game();
    lib.service->setCardForGame(*game, "Fighting");

    CHECK(game->memcard == "Fighting");                        // the record in hand
    CHECK(lib.service->activeCardName(*game) == "Fighting");   // the Game.ini
    CHECK(lib.game()->memcard == "Fighting");                  // and the database, re-read
}

TEST_CASE("swapping in does nothing for a game on the stock card") {
    Cards lib;
    lib.service->setCardForGame(*lib.game(), MemcardService::SonyCard);
    lib.giveGameItsOwnCards();

    PsGamePtr game = lib.game();
    lib.service->swapInForLaunch(*game);

    // the game's own cards are untouched
    CHECK(lib.tmp.readFile("Games/Tekken 3/sstates/memcards/card1.mcd") == "in play");
}

TEST_CASE("swapping in puts the chosen set into the game's save-state folder") {
    Cards lib;
    lib.service->createCard("Fighting");
    REQUIRE(ableem::DirEntry::exists(lib.tmp.at("Games/!MemCards/Fighting")));
    lib.tmp.writeFile("Games/!MemCards/Fighting/card1.mcd", "fighting saves");

    PsGamePtr game = lib.game();
    lib.service->setCardForGame(*game, "Fighting");
    lib.giveGameItsOwnCards();

    lib.service->swapInForLaunch(*game);

    CHECK(lib.tmp.readFile("Games/Tekken 3/sstates/memcards/card1.mcd") == "fighting saves");
}

// KNOWN BUG, pinned here rather than fixed inside a structural move - see docs/refactor-plan.md step 8.
//
// swapInForLaunch means to drop a game back to the stock card when its set has gone missing, but the
// fallback cannot run: MemcardManager::swapIn returns false only when the set directory does not exist,
// and the DirEntry::exists guard just above the call has already ruled that out. So the branch is
// unreachable and a game left pointing at a deleted set stays pointing at it.
//
// The effect is mild - the game launches on its own last cards, not on someone else's - but the code reads
// as though it handles a case it does not. Deleting the redundant guard is the fix.
TEST_CASE("a card set that has gone missing leaves the game pointing at it (known bug)") {
    Cards lib;
    PsGamePtr game = lib.game();
    lib.service->setCardForGame(*game, "Fighting");
    // ...and then the set is deleted behind its back
    REQUIRE_FALSE(ableem::DirEntry::exists(lib.tmp.at("Games/!MemCards/Fighting")));

    lib.service->swapInForLaunch(*game);

    // what it should say is SonyCard. This asserts today's behaviour so that fixing it is a visible,
    // deliberate change to this test rather than a silent one.
    CHECK(lib.service->activeCardName(*lib.game()) == "Fighting");
    CHECK(lib.game()->memcard == "Fighting");
}

TEST_CASE("swapping out copies the game's cards back into its set") {
    Cards lib;
    lib.service->createCard("Fighting");
    PsGamePtr game = lib.game();
    lib.service->setCardForGame(*game, "Fighting");

    lib.service->swapInForLaunch(*game);
    // the game plays and writes to its own cards
    lib.tmp.writeFile("Games/Tekken 3/sstates/memcards/card1.mcd", "saved during play");

    lib.service->swapOutAfterLaunch(*game);

    CHECK(lib.tmp.readFile("Games/!MemCards/Fighting/card1.mcd") == "saved during play");
}

TEST_CASE("swapping out does nothing for a game on the stock card") {
    Cards lib;
    lib.service->createCard("Fighting");
    lib.tmp.writeFile("Games/!MemCards/Fighting/card1.mcd", "fighting saves");
    lib.service->setCardForGame(*lib.game(), MemcardService::SonyCard);
    lib.giveGameItsOwnCards();

    lib.service->swapOutAfterLaunch(*lib.game());

    // the set is not overwritten by a game that was never using it
    CHECK(lib.tmp.readFile("Games/!MemCards/Fighting/card1.mcd") == "fighting saves");
}

TEST_CASE("creating, listing, renaming and removing card sets") {
    Cards lib;
    CHECK(lib.service->listCards().empty());

    lib.service->createCard("Fighting");
    lib.service->createCard("Racing");
    vector<string> cards = lib.service->listCards();
    std::sort(cards.begin(), cards.end());
    CHECK(cards == vector<string>{"Fighting", "Racing"});

    // a new set starts from the blank templates
    CHECK(lib.tmp.readFile("Games/!MemCards/Fighting/card1.mcd") == "blank one");

    lib.service->renameCard("Fighting", "Beat Em Up");
    cards = lib.service->listCards();
    std::sort(cards.begin(), cards.end());
    CHECK(cards == vector<string>{"Beat Em Up", "Racing"});

    lib.service->removeCard("Racing");
    CHECK(lib.service->listCards() == vector<string>{"Beat Em Up"});
}

TEST_CASE("renaming a set repoints the games that used it") {
    Cards lib;
    lib.service->createCard("Fighting");
    PsGamePtr game = lib.game();
    lib.service->setCardForGame(*game, "Fighting");

    lib.service->renameCard("Fighting", "Beat Em Up");

    // the game's Game.ini follows the rename, or the game would launch on the stock card next time
    CHECK(lib.service->activeCardName(*lib.game()) == "Beat Em Up");
}
