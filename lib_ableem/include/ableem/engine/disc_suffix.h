// lib_ableem - engine: the disc marker at the end of a multi-disc game's folder name.
#pragma once

#include <string>

namespace ableem {

//******************
// DiscSuffix
//******************
// What ROM managers (Redump, No-Intro, libretro playlists) put after a multi-disc game's name:
//   "Game (Disc 1)"  "Game (Disk 2)"  "Game (CD 3)"  "Game (CD3)"  "Game - Disc 1"  "Game - Disk 2"  "Game - CD 3"
// The keyword is matched case-insensitively and trailing whitespace is tolerated; base is the name
// without the marker (and without the whitespace before it). Ported from AutoBleem-NG (engine/disc_suffix).
struct DiscSuffix {
    std::string base; // the game's name without the disc marker
    int disc = 0;     // the disc number, 0 when there is no marker

    bool matched() const { return disc != 0; }

    static DiscSuffix parse(const std::string &name);
};

} // namespace ableem
