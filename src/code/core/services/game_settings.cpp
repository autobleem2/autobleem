//
// GameSettingsService: what the game editor edits - a game's Game.ini flags and its pcsx.cfg values.
//
#include "game_settings.h"
#include "../main.h"
#include "../environment.h"

#include <ableem/engine/config_file_editor.h>

#include <cstdlib>
#include <sstream>

using namespace std;

const char *const GameSettingsService::BuiltinGpu = "builtin_gpu";
const char *const GameSettingsService::PeopsGpu = "gpu_peops.so";

namespace {

// the levels are stored in hex in pcsx.cfg (100 -> "64")
string toHex(int value) {
    stringstream ss;
    ss << hex << value;
    return ss.str();
}

int clampTo(int value, int lo, int hi) {
    return value < lo ? lo : (value > hi ? hi : value);
}

} // namespace

//*******************************
// GameSettingsService::cfgFolder
//*******************************
string GameSettingsService::cfgFolder(const GameSettings &s) {
    return s.internal ? s.game->ssFolder : s.game->folder;
}

//*******************************
// GameSettingsService::fallBackToSonyCardIfSetIsGone
//*******************************
void GameSettingsService::fallBackToSonyCardIfSetIsGone(GameSettings &s) {
    if (s.ini.values["memcard"] != "SONY") {
        string cardpath = Env::getPathToMemCardsDir() + sep + s.ini.values["memcard"];
        if (!DirEntry::exists(cardpath)) {
            s.ini.values["memcard"] = "SONY";
        }
    }
}

//*******************************
// GameSettingsService::open
//*******************************
GameSettings GameSettingsService::open(PsGamePtr game) const {
    GameSettings s;
    s.game = game;
    s.internal = game->internal;

    if (!s.internal) {
        s.ini.load(game->folder + sep + GAME_INI);
        // change "/media/Games/Racing/Driver 2" to "Driver 2"
        string folderNoLast = DirEntry::removeSeparatorFromEndOfPath(game->folder);
        s.ini.entry = DirEntry::getFileNameFromPath(folderNoLast);
    } else {
        // recover ini
        s.ini.values["title"] = game->title;
        s.ini.values["publisher"] = game->publisher;
        s.ini.values["year"] = to_string(game->year);
        s.ini.values["players"] = to_string(game->players);
        s.ini.values["memcard"] = game->memcard;
    }
    fallBackToSonyCardIfSetIsGone(s);

    refreshPcsx(s);
    return s;
}

//*******************************
// GameSettingsService::refreshPcsx
//*******************************
// frameskip3 is written in hex like the other levels but read in decimal; with a range of 0..3 the two
// agree, so it has never mattered. Left as it was.
void GameSettingsService::refreshPcsx(GameSettings &s) const {
    ConfigFileEditor processor;
    string path = cfgFolder(s);
    PcsxSettings &p = s.pcsx;
    p.highres       = atoi  (processor.getValue(path, "gpu_neon.enhancement_enable").c_str());
    p.speedhack     = atoi  (processor.getValue(path, "gpu_neon.enhancement_no_main").c_str());
    p.clock         = strtol(processor.getValue(path, "psx_clock").c_str(), NULL, 16);
    p.gpu           =        processor.getValue(path, "gpu3");
    p.frameskip     = atoi  (processor.getValue(path, "frameskip3").c_str());
    p.dither        = atoi  (processor.getValue(path, "gpu_peops.iUseDither").c_str());
    p.scanlines     = atoi  (processor.getValue(path, "scanlines").c_str());
    p.scanlineLevel = strtol(processor.getValue(path, "scanline_level").c_str(), NULL, 16);
    p.interpolation = strtol(processor.getValue(path, "spu_config.iUseInterpolation").c_str(), NULL, 16);
}

//*******************************
// GameSettingsService::saveIni
//*******************************
void GameSettingsService::saveIni(GameSettings &s) const {
    if (!s.internal) {
        s.ini.save(s.ini.path);
    }
}

//*******************************
// GameSettingsService::replaceCfgLine
//*******************************
void GameSettingsService::replaceCfgLine(GameSettings &s, const string &property, const string &value) {
    ConfigFileEditor processor;
    processor.replace(s.ini.entry, cfgFolder(s), property, property + " = " + value, s.internal);
    refreshPcsx(s);
}

//*******************************
// GameSettingsService::setFavorite
//*******************************
void GameSettingsService::setFavorite(GameSettings &s, bool on) {
    if (s.internal) {
        s.game->favorite = on;
        library_.internalGames().updateFavorite(s.game->gameId, s.game->favorite);
    } else {
        s.ini.values["favorite"] = on ? "1" : "0";
        saveIni(s);
    }
}

//*******************************
// GameSettingsService::setPlayUsingRa
//*******************************
void GameSettingsService::setPlayUsingRa(GameSettings &s, bool on) {
    if (s.internal) {
        s.game->play_using_ra = on;
        library_.internalGames().updatePlayUsingRA(s.game->gameId, s.game->play_using_ra);
    } else {
        s.ini.values["play_using_ra"] = on ? "true" : "false";
        saveIni(s);
    }
}

//*******************************
// GameSettingsService::setLocked
//*******************************
void GameSettingsService::setLocked(GameSettings &s, bool on) {
    if (s.internal) {
        return;
    }
    string &automation = s.ini.values["automation"];
    if (on) {
        if (automation == "1") automation = "0";
    } else {
        if (automation == "0") automation = "1";
    }
    saveIni(s);
}

//*******************************
// GameSettingsService::setMemcard
//*******************************
void GameSettingsService::setMemcard(GameSettings &s, const string &name) {
    if (s.internal) {
        return;
    }
    s.ini.values["memcard"] = name;
    saveIni(s);
}

//*******************************
// GameSettingsService::rename
//*******************************
void GameSettingsService::rename(GameSettings &s, const string &title) {
    if (s.internal) {
        return;
    }
    s.ini.values["title"] = title;
    s.ini.values["automation"] = "0";
    saveIni(s);
}

//*******************************
// GameSettingsService::setHighres
//*******************************
void GameSettingsService::setHighres(GameSettings &s, bool on) {
    s.ini.values["highres"] = to_string(on ? 1 : 0);
    replaceCfgLine(s, "gpu_neon.enhancement_enable", s.ini.values["highres"]);
    saveIni(s);
}

//*******************************
// GameSettingsService::setSpeedhack
//*******************************
void GameSettingsService::setSpeedhack(GameSettings &s, bool on) {
    replaceCfgLine(s, "gpu_neon.enhancement_no_main", to_string(on ? 1 : 0));
}

//*******************************
// GameSettingsService::setScanlines
//*******************************
void GameSettingsService::setScanlines(GameSettings &s, bool on) {
    replaceCfgLine(s, "scanlines", to_string(on ? 1 : 0));
}

//*******************************
// GameSettingsService::setScanlineLevel
//*******************************
void GameSettingsService::setScanlineLevel(GameSettings &s, int level) {
    replaceCfgLine(s, "scanline_level", toHex(clampTo(level, 0, 100)));
}

//*******************************
// GameSettingsService::setClock
//*******************************
void GameSettingsService::setClock(GameSettings &s, int clock) {
    replaceCfgLine(s, "psx_clock", toHex(clampTo(clock, 0, 100)));
}

//*******************************
// GameSettingsService::setFrameskip
//*******************************
void GameSettingsService::setFrameskip(GameSettings &s, int frames) {
    replaceCfgLine(s, "frameskip3", toHex(clampTo(frames, 0, 3)));
}

//*******************************
// GameSettingsService::setInterpolation
//*******************************
void GameSettingsService::setInterpolation(GameSettings &s, int mode) {
    replaceCfgLine(s, "spu_config.iUseInterpolation", toHex(clampTo(mode, 0, 3)));
}

//*******************************
// GameSettingsService::setGpuPlugin
//*******************************
void GameSettingsService::setGpuPlugin(GameSettings &s, const string &plugin) {
    if (s.internal) {
        return;
    }
    replaceCfgLine(s, "Gpu3", plugin);
}
