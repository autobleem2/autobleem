#pragma once

// The engine half of lib_ableem provides the types every part of the app uses (DirEntry, sep, ImageType, the
// string helpers, ...). They are pulled into the global namespace here, once, so the rest of the app keeps
// spelling them the way it always has. `using namespace ableem` is deliberately NOT used: the app's own
// GuiScreen (gui/gui_screen.h) shares its name with ableem::GuiScreen.
#include <ableem/engine.h>

using ableem::ImageType;
using ableem::IMAGE_NO_GAME_FOUND;
using ableem::IMAGE_BIN;
using ableem::IMAGE_PBP;
using ableem::IMAGE_IMG;
using ableem::IMAGE_CHD;

using ableem::GAME_DATA;
using ableem::GAME_INI;
using ableem::PCSX_CFG;
using ableem::EXT_PNG;
using ableem::EXT_PBP;
using ableem::EXT_ECM;
using ableem::EXT_BIN;
using ableem::EXT_IMG;
using ableem::EXT_CHD;
using ableem::EXT_CUE;
using ableem::EXT_LIC;

// in-place string helpers (trim(s) modifies s; the copying versions are Util::trim(s))
using ableem::ltrim;
using ableem::rtrim;
using ableem::trim;
using ableem::lcase;
using ableem::ucase;
using ableem::toLowerCopy;
using ableem::toUpperCopy;
using ableem::lessCaseInsensitive;

using ableem::DirEntry;
using ableem::DirEntries;
using ableem::Sep;
using ableem::sep;
using ableem::separator;

using ableem::IniFile;
using ableem::ConfigFileEditor;
using ableem::MemcardManager;
using ableem::SerialScanner;
using ableem::IsoDirectory;
using ableem::IsoDirectoryReader;
using ableem::EcmDecoder;
