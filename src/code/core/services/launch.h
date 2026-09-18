//
// LaunchService: starting a game - PCSX, RetroArch or a launchable App - and everything around it.
//
#pragma once

#include "../model/ps_game.h"
#include "../model/session.h"
#include "config.h"
#include "memcard.h"
#include "process_runner.h"
#include "resume_point.h"

#include <ableem/engine/game_library.h>

#include <string>
#include <vector>

//******************
// LaunchService
//******************
// Was App::launchGame plus the three EmuInterceptors (PcsxInterceptor, RetroArchInterceptor,
// LaunchInterceptor) - the argv for rc/launch.sh and rc/launch_rb.sh, the memory-card and resume-point
// work around a run, the RetroArch config transfer. What was an #ifdef AB_DEBUG_HOST in each interceptor is
// now which ProcessRunner the composition root passes in, which is also what makes the argv assertable.
//
// Owned by App (App::launcher()).
class LaunchService {
public:
    LaunchService(Config &config, Session &session, ableem::GameLibrary &library, MemcardService &memcards,
                  ResumePointService &resumePoints, ProcessRunner &runner)
        : config_(config), session_(session), library_(library), memcards_(memcards), resumePoints_(resumePoints),
          runner_(runner) {}

    // rc/autobleem_cfg.sh: AB_SELECTION/AB_THEME/AB_PCSX/AB_MIP, so the shell launch scripts see the menu
    // choice and the theme/emulator settings after the GUI exits or before a game starts.
    void writeSelectionScript();

    // The launch itself, start to finish: the emulator is chosen from the game and the mode, the game's
    // memory cards go in and its resume point is prepared, the launcher script runs and is waited for, the
    // cards come back out. `game` is the record the launcher handed over; PCSX's path normalises its
    // ssFolder in place, as it always has.
    void launch(PsGamePtr &game, EmuMode mode, int resumePoint);

    // where the launcher scripts are
    static std::string pcsxLauncherScript();      // rc/launch.sh
    static std::string retroArchLauncherScript(); // rc/launch_rb.sh

private:
    enum class Path { Pcsx, RetroArch, App };
    static Path pathFor(const PsGame &game, EmuMode mode);

    // the disc image handed to an emulator: <folder>/<base>, with .cue appended unless it is a .pbp or .chd
    static std::string discImageFor(const PsGame &game);
    // the image's base name with a .pbp extension dropped - what RetroArch names its saves after
    static std::string raBaseNameFor(const PsGame &game);

    // --- PCSX ---
    void launchPcsx(PsGame &game, int resumePoint);
    // PCSX writes its edited config next to the save states as autobleem.cfg; copy it back where it is read from
    void cleanupPcsxConfig(PsGame &game);

    // --- RetroArch ---
    void launchRetroArch(PsGame &game);
    // the game's card1.mcd goes to RetroArch's saves dir as <base>.srm for the run and comes back after
    void raMemcardIn(PsGame &game);
    void raMemcardOut(PsGame &game);
    // with config.ini raconfig=true, the game's pcsx.cfg settings are written into RetroArch's own config
    // for the run, from a backup that is put back afterwards
    void backupRaConfig();
    void restoreRaConfig();
    void transferRaConfig(PsGame &game);
    static std::string raSavesDir();
    static std::string raConfigFile();      // retroarch.cfg
    static std::string raCoreOptionsFile(); // config/retroarch-core-options.cfg

    // --- Apps ---
    void launchApp(PsGame &game);

    Config &config_;
    Session &session_;
    ableem::GameLibrary &library_;
    MemcardService &memcards_;
    ResumePointService &resumePoints_;
    ProcessRunner &runner_;
};
