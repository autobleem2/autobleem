//
// GameSettingsService: what the game editor edits - a game's Game.ini flags and its pcsx.cfg values.
//
#pragma once

#include "../model/ps_game.h"

#include <ableem/engine/game_library.h>
#include <ableem/engine/ini_file.h>

#include <string>

//******************
// PcsxSettings
//******************
// The per-game emulator values the editor shows, as read back from the game's pcsx.cfg. They are ints
// because the editor steps them with the d-pad; how each one is encoded in the file is the service's
// business (see the setters below).
struct PcsxSettings {
    int highres = 0;
    int speedhack = 0;
    int clock = 0;
    int frameskip = 0;
    int dither = 0; // read like the others but never shown or written; kept as it was
    int scanlines = 0;
    int scanlineLevel = 0;
    int interpolation = 0;
    int bootLogo = 1;  // pcsx.cfg SlowBoot: the BIOS boot logo shown before the game; no line = shown
    int smoothing = 0; // pcsx.cfg soft_filter, pcsx-abnxt's Smoothing: 0 none, 1 scale2x, 2 eagle2x, 3 hq2x, 4 hq3x
    std::string gpu;
};

//******************
// GameSettings
//******************
// Everything the editor holds for one game while it is open. For a USB game `ini` is the game's Game.ini,
// loaded from its folder, with `ini.entry` set to the folder's name (the key ConfigFileEditor uses to find
// the !SaveStates copies). For an internal game there is no Game.ini: `ini` is filled from the database
// record so the screen can show the same fields, its `path` stays empty and it is never saved - the flags
// go to internal.db instead.
struct GameSettings {
    PsGamePtr game;
    bool internal = false;
    ableem::IniFile ini;
    PcsxSettings pcsx;
};

//******************
// GameSettingsService
//******************
// GuiEditor used to do all of this inline, over an IniFile member that both of its callers seeded by hand.
// It is here so it can be tested against a temp tree and so the editor is only a screen.
//
// Owned by App (App::gameSettings()).
class GameSettingsService {
public:
    explicit GameSettingsService(ableem::GameLibrary &library) : library_(library) {}

    // Loads the game's Game.ini (or, for an internal game, fills one in from the record) and reads its
    // pcsx.cfg. A memory-card set the ini names but which no longer exists is shown as the console's own
    // card - in memory only; the file is not touched until something else is saved.
    GameSettings open(PsGamePtr game) const;

    // re-reads the pcsx.cfg values; the setters below do this themselves after writing
    void refreshPcsx(GameSettings &s) const;

    // --- Game.ini for a USB game, internal.db for an internal one ---
    void setFavorite(GameSettings &s, bool on);
    void setPlayUsingRa(GameSettings &s, bool on);
    // A light-gun game plays in RetroArch's pcsx_rearmed (guncon), so switching it on also switches Play
    // using RA on; off leaves Play using RA as it is. The editor keeps the Play using RA row locked while
    // the flag is on. RetroArch games are flagged elsewhere (LightgunService) - they have no Game.ini.
    void setLightgun(GameSettings &s, bool on);

    // --- Game.ini only; a no-op for an internal game ---
    // "Locked" is Automation=0: the user edited the ini, the scanner must not rewrite it. Only flips the
    // flag from its opposite value - an ini with no Automation key at all is left as it is (the scanner
    // always writes one, so this is not reached in practice, but it is what the editor did).
    void setLocked(GameSettings &s, bool on);
    // The ini's Memcard value only. MemcardService::setCardForGame also writes regional.db; the editor
    // never did, and a launch reads the ini, so nothing depends on the column being current.
    void setMemcard(GameSettings &s, const std::string &name);
    // A rename also unlocks the ini (Automation=0) so the scanner keeps the new title. Internal games are
    // renamed in the database by the caller, after the editor closes - see GuiLauncher's use of lastName.
    void rename(GameSettings &s, const std::string &title);

    // --- pcsx.cfg: the game folder's copy plus every copy under !SaveStates (ConfigFileEditor::replace) ---
    // Each one rewrites the line, then re-reads all the values, so what the caller sees is what the file
    // says (nothing, if the game has no pcsx.cfg). The 0/1 flags are written in decimal, the levels in
    // hex - that is what PCSX reads. Levels are clamped to their ranges here.
    void setHighres(GameSettings &s, bool on); // also remembered in the Game.ini as Highres
    void setSpeedhack(GameSettings &s, bool on);
    void setScanlines(GameSettings &s, bool on);
    void setScanlineLevel(GameSettings &s, int level); // 0..100
    void setClock(GameSettings &s, int clock);         // 0..100
    void setFrameskip(GameSettings &s, int frames);    // 0..3
    void setInterpolation(GameSettings &s, int mode);  // 0..3
    // SlowBoot: off skips the BIOS shell (a homebrew's custom logo can crash pcsx-ab's boot); RetroArch
    // gets it as pcsx_rearmed_show_bios_bootlogo (LaunchService)
    void setBootLogo(GameSettings &s, bool on);
    // soft_filter: pcsx-abnxt's software scaler on the PSX frame (its menu's "Smoothing"); the classic
    // pcsx-ab ignores the key, so the editor shows the row only with pcsx-abnxt selected
    void setSmoothing(GameSettings &s, int mode);                  // 0..4, see SmoothingNames
    void setGpuPlugin(GameSettings &s, const std::string &plugin); // USB only: "builtin_gpu" or "gpu_peops.so"

    static const char *const BuiltinGpu;
    static const char *const PeopsGpu;
    static const char *const SmoothingNames[5]; // "None", "Scale2x", "Eagle2x", "HQ2x", "HQ3x"

private:
    // where the pcsx.cfg is: the game's folder, or the !SaveStates folder for an internal game
    static std::string cfgFolder(const GameSettings &s);
    static void fallBackToSonyCardIfSetIsGone(GameSettings &s);
    void saveIni(GameSettings &s) const;
    void replaceCfgLine(GameSettings &s, const std::string &property, const std::string &value);

    ableem::GameLibrary &library_;
};
