// lib_ableem - engine: the few constants every PS1-game-handling routine shares.
#pragma once

namespace ableem {

// how a game is stored on disk. the numeric values are written into Game.ini ("Imagetype=") and must not change.
enum ImageType {
    IMAGE_NO_GAME_FOUND = -1,
    IMAGE_BIN = 0, // .cue + .bin
    IMAGE_PBP = 1, // PSP eboot
    IMAGE_IMG = 2, // .cue + .img
    IMAGE_CHD = 3  // MAME compressed hunks of data
    //  IMAGE_ISO           // not supported yet
};

const char GAME_DATA[] = "GameData"; // pre-0.5 layout: game files were one level down, in this sub-dir
const char GAME_INI[] = "Game.ini";
const char PCSX_CFG[] = "pcsx.cfg";
const char EXT_PNG[] = ".png";
const char EXT_PBP[] = ".pbp";
const char EXT_ECM[] = ".ecm";
const char EXT_BIN[] = ".bin";
const char EXT_IMG[] = ".img";
const char EXT_CHD[] = ".chd";
// const char EXT_ISO[] = ".iso";
const char EXT_CUE[] = ".cue";

// special sub-directories of the games dir that never hold a game
const char SAVESTATES_DIR_NAME[] = "!SaveStates";
const char MEMCARDS_DIR_NAME[] = "!MemCards";

} // namespace ableem
