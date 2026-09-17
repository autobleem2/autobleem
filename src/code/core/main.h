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

// in-place string helpers (trim(s) modifies s; the copying versions are Strings::trim(s))
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
using ableem::ThemeSpec;
using ableem::ThemeColor;
using ableem::ClassicTheme;
using ableem::LauncherTheme;
using ableem::ZipArchive;
using ableem::ConfigFileEditor;
using ableem::MemcardManager;
using ableem::SerialScanner;
using ableem::IsoDirectory;
using ableem::IsoDirectoryReader;
using ableem::EcmDecoder;
using ableem::GameRecord;
using ableem::GameRecords;
using ableem::GameMetadata;
using ableem::GameDatabase;
using ableem::SubDirRowInfo;
using ableem::SubDirRowInfos;
using ableem::SubDirRowGame;
using ableem::SubDirRowGames;
using ableem::CoverDatabase;
using ableem::Disc;
using ableem::UsbGame;
using ableem::UsbGamePtr;
using ableem::UsbGames;
using ableem::GameSubDir;
using ableem::GameSubDirPtr;
using ableem::GameSubDirRows;
using ableem::GamesHierarchy;
using ableem::GamesFingerprint;
using ableem::GameScanner;
using ableem::ScanStage;
using ableem::ScanProgressListener;

// Translate one UI string: `_("Re/Scan")`. English is the source language, so it is returned unchanged.
// Strings may contain emoji markers such as |@X| or |@L1|, which TextRenderer replaces with the theme's
// button textures - keep them in the translation. Goes through the ableem::Lang the App registered; with
// none (a unit test) every string is its own translation.
using ableem::Lang;
using ableem::MemcardImage;
using ableem::Strings;   // the copying string helpers: Strings::trim, Strings::toInt, ... (the old System::)
inline std::string _(const std::string &input) { return ableem::translate(input); }
