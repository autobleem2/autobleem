//
// MemcardService: the memory-card sets under <games>/!MemCards, and which one a game plays with.
//
#pragma once

#include "../model/ps_game.h"

#include <ableem/engine/game_library.h>
#include <ableem/engine/memcard_manager.h>

#include <string>
#include <vector>

//******************
// MemcardService
//******************
// ableem::MemcardManager already does the file work on a set of cards. What was missing was the layer above
// it: which set a given game plays with, and the swap in/out around a launch, which both the PCSX and the
// RetroArch interceptor had their own copy of.
//
// Owned by App (App::memcards()).
class MemcardService {
public:
    explicit MemcardService(ableem::GameLibrary &library) : library_(library) {}

    // The console's own card. A game set to this has no custom set of its own.
    static const char *const SonyCard;

    // Which set the game plays with. Internal games always use the console's own; for a USB game the
    // game's Game.ini is what says. Note this can be "" for a game with no Game.ini at all - see
    // swapInForLaunch, and the foreign-game guards in the interceptors.
    std::string activeCardName(const PsGame &game) const;

    // Records the set a game should play with, in both places that remember it: the game's Game.ini and its
    // regional.db row. One call again - step 6 had to split this to get PsGame into ab_core.
    void setCardForGame(PsGame &game, const std::string &name);

    // Around a launch: swap the game's chosen set in beforehand and back out after. A set that has gone
    // missing drops the game back to the stock card rather than letting it run on another game's saves.
    void swapInForLaunch(PsGame &game);
    void swapOutAfterLaunch(PsGame &game);

    // the sets themselves, for the memory-card screens
    std::vector<std::string> listCards() const;
    void createCard(const std::string &name);
    void removeCard(const std::string &name);
    void renameCard(const std::string &oldName, const std::string &newName);
    // copies a game's own cards into a (new) set, so it can be shared with other games
    void storeGameCardsAsSet(const std::string &gameMemcardsPath, const std::string &name);

private:
    // built per call rather than held: Environment's paths are configured after App is constructed
    ableem::MemcardManager manager() const;

    ableem::GameLibrary &library_;
};
