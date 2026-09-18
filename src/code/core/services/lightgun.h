//
// LightgunService: which games are light-gun games, and where that is remembered.
//
#pragma once

#include "../model/ps_game.h"

#include <ableem/engine/game_library.h>

#include <set>
#include <string>
#include <vector>

//******************
// LightgunService
//******************
// A light-gun game (Time Crisis, Point Blank...) needs RetroArch's pcsx_rearmed with its guncon support,
// so a flagged PS1 game always launches through RetroArch, and the launcher has a "Lightgun Games" set
// (GameSet::Lightgun) of every flagged game across PS1 and RetroArch. From AutoBleem-NG (Axanar, 2022).
//
// Where the flag lives depends on the kind of game, the same way favorite does: a USB PS1 game keeps it
// in its Game.ini (Lightgun=1, read onto GameRecord::lightgun by the loaders, kept across scans), an
// internal game in internal.db's LIGHTGUN column, and a RetroArch game - which has no ini of its own -
// as a line in System/lightguns.txt, one image path per line. (The fork kept every path in a file next to
// the binary; that is lost on an upgrade, and a PS1 game's ini is where its other flags already are.)
//
// PS1 flags are written by GameSettingsService::setLightgun, which also forces Play using RA on; this
// service owns the RetroArch list and answers isLightgun() for any game. Owned by App (App::lightguns()).
class LightgunService {
public:
    explicit LightgunService(ableem::GameLibrary &library);

    // a PS1 game's record flag, a RetroArch game's presence in the list; an App is never one
    bool isLightgun(const PsGame &game) const;

    // --- the RetroArch list (System/lightguns.txt) ---
    void setRetroArchLightgun(const PsGame &game, bool on); // by its image path; saves the file
    bool anyRetroArchLightguns() const { return !raPaths_.empty(); }
    // re-reads the file, dropping every path that is no longer on disk (a removed game), and saves it
    // when something was dropped
    void reload();

    static std::string lightgunsFile(); // System/lightguns.txt under the USB root

private:
    void save() const;

    ableem::GameLibrary &library_;
    std::set<std::string> raPaths_;
};
