//
// Created by lifting the memcardIn/memcardOut halves the PCSX and RetroArch interceptors each had a copy
// of, and reuniting PsGame::setMemCard's two halves.
//

#include "memcard.h"
#include "../environment.h"
#include "../main.h"

#include <iostream>

using namespace std;

const char *const MemcardService::SonyCard = "SONY";

//*******************************
// MemcardService::manager
//*******************************
ableem::MemcardManager MemcardService::manager() const {
    return ableem::MemcardManager(Env::getPathToGamesDir());
}

//*******************************
// MemcardService::activeCardName
//*******************************
string MemcardService::activeCardName(const PsGame &game) const {
    if (game.internal)
        return SonyCard;        // the console's built-in games always use its own card

    IniFile ini;
    ini.load(game.folder + sep + GAME_INI);
    return ini.values["memcard"];
}

//*******************************
// MemcardService::setCardForGame
//*******************************
void MemcardService::setCardForGame(PsGame &game, const string &name) {
    if (game.foreign)
        return;     // a RetroArch or App entry has neither a Game.ini nor a database row

    game.memcard = name;

    IniFile ini;
    ini.load(game.folder + sep + GAME_INI);
    ini.values["memcard"] = name;
    ini.save(game.folder + sep + GAME_INI);

    library_.usbGames().updateMemcard(game.gameId, name);
}

//*******************************
// MemcardService::swapInForLaunch
//*******************************
void MemcardService::swapInForLaunch(PsGame &game) {
    if (activeCardName(game) == SonyCard)
        return;

    // The name checked and swapped in is the record's, not the Game.ini's just read. setCardForGame writes
    // both together so they agree; kept as it was rather than unified inside a structural move.
    //
    // KNOWN BUG, preserved from the interceptors this came from: the fallback below cannot run. swapIn()
    // returns false only when the set directory is missing, which this guard has already excluded, so a
    // game left pointing at a deleted set keeps pointing at it. Deleting this guard is the fix - it is a
    // behaviour change, so it is pinned by a test (tests/core/test_memcard.cpp) rather than done here.
    if (!DirEntry::exists(Env::getPathToMemCardsDir() + sep + game.memcard))
        return;

    if (!manager().swapIn(game.ssFolder, game.memcard)) {
        // the set is gone: fall back to the stock card rather than run on whatever is there
        cout << "Memory card set " << game.memcard << " could not be swapped in, falling back to SONY" << endl;
        setCardForGame(game, SonyCard);
    }
}

//*******************************
// MemcardService::swapOutAfterLaunch
//*******************************
void MemcardService::swapOutAfterLaunch(PsGame &game) {
    if (activeCardName(game) == SonyCard)
        return;
    manager().swapOut(game.ssFolder, game.memcard);
}

//*******************************
// MemcardService:: the sets
//*******************************
vector<string> MemcardService::listCards() const { return manager().list(); }
void MemcardService::createCard(const string &name) { manager().create(name); }
void MemcardService::removeCard(const string &name) { manager().remove(name); }

void MemcardService::renameCard(const string &oldName, const string &newName) {
    manager().rename(oldName, newName);    // also rewrites every Game.ini that named the old set
}

void MemcardService::storeGameCardsAsSet(const string &gameMemcardsPath, const string &name) {
    manager().storeToRepo(gameMemcardsPath, name);
}
