//
// ResumePointService: the save-state slots PCSX leaves in a game's !SaveStates folder.
//
#pragma once

#include "../model/ps_game.h"

#include <string>

//******************
// ResumePointService
//******************
// When PCSX exits it leaves a save state and a screenshot in the game's save-state folder. AutoBleem keeps
// up to SlotCount of those per game as numbered "resume points", so the player can pick one to continue
// from. The files involved, all under game.ssFolder:
//
//   filename.txt            written by PCSX on exit; its second line is the base name of the state
//   filename.txt.res        AutoBleem's kept copy of that
//   filename.<n>.txt.res    the same, per slot
//   sstates/<name>.000      the state PCSX just wrote
//   sstates/<name>.00<n>.res  the state kept for slot n
//   screenshots/<name>.png  the screenshot PCSX just wrote
//   screenshots/<name>.png.res      the picture for slot 0
//   screenshots/<name>.<n>.png.res  the picture for slot n
//   lastcdimg.txt / lastcdimg.<n>.txt   which disc image the slot was playing
//
// A foreign (RetroArch or App) entry has none of this, so every call is a no-op for one.
//
// Owned by App (App::resumePoints()).
class ResumePointService {
public:
    static const int SlotCount = 4;

    bool slotIsActive(const PsGame &game, int slot) const;
    std::string pictureForSlot(const PsGame &game, int slot) const;
    // the picture for whichever slot the game last stopped in, "" when there is none
    std::string lastPicture(const PsGame &game) const;

    void storePictureForSlot(const PsGame &game, int slot);
    void removeSlot(const PsGame &game, int slot);

    // true when PCSX wrote its filename.txt, i.e. the game was quit rather than killed
    bool exitedCleanly(const PsGame &game) const;

    // Around a PCSX launch: clear out what the last run left, and set up the slot being resumed from
    // (slot -1 means "start from the beginning"). Then keep whatever the run wrote as that slot.
    void prepareForLaunch(const PsGame &game, int slot);
    void saveAfterLaunch(const PsGame &game, int slot);
};
