//
// What the RetroArch program being installed (Env::retroArchInstalled) decides in the launcher, as pure functions:
// which emulator a PS1 game starts in, and whether the Lightgun set exists. A game's "Play using RA" and lightgun
// flags stay saved either way; they apply again when RetroArch is back.
//
#pragma once

#include "core/model/game_set.h"

// a PS1 game starts in RetroArch when RetroArch is installed and the game is from the Lightgun set, flagged "Play
// using RA" (its ini, or the internal db), or Options' "Play all PSX games with RA" is on; otherwise in PCSX
inline bool startPs1InRetroArch(bool retroArchInstalled, bool lightgunSet, bool playUsingRa, bool playAllWithRa) {
    return retroArchInstalled && (lightgunSet || playUsingRa || playAllWithRa);
}

// the Lightgun set (light-gun games run only in RetroArch) is offered only with RetroArch
inline bool lightgunSetAvailable(bool retroArchInstalled) {
    return retroArchInstalled;
}

// a remembered set that is not offered any more falls back to the PlayStation set
inline GameSet setOrFallback(GameSet set, bool retroArchInstalled) {
    return set == GameSet::Lightgun && !lightgunSetAvailable(retroArchInstalled) ? GameSet::PS1 : set;
}
