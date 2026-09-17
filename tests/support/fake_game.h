//
// makeFakeGame: a minimal real PS1 game on disk, for tests that need GameScanner::scanGamesDirectory (or
// anything built on it) to actually verify() a game rather than working from database rows alone.
//
#pragma once

#include <string>

namespace test_support {

// Writes gamesDir/title/title.bin + title.cue: a 24-sector MODE2/2352 raw-sector ISO9660 image whose root
// directory holds a SYSTEM.CNF and a file named serialFile (default "SLUS_012.34", which SerialScanner
// reads back as serial "SLUS-01234") - enough for the scanner to find a serial and verify() to pass. A
// C++ port of tools/make_usb.py's make_iso()/make_fake_game(); keep the two in sync if either changes.
// Pass a different serialFile ("SLUS_012.35", ...) to give two fake games distinct serials.
void makeFakeGame(const std::string &gamesDir, const std::string &title, const std::string &serialFile = "SLUS_012.34");

} // namespace test_support
