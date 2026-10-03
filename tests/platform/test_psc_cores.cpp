//
// The PlayStation Classic's core picks and ROM folders against the console's real core set: tests/data/psc-info in
// autobleem-core is the info/ folder of the cores tarball the PSC installs, with an empty fake .so beside each .info.
// What is held: every line of platform/psc.cores.cfg names an installed core that plays that system (a stem is
// the one exact name), the picks the plan chose are the ones the table gives, and the scan's folder pass makes
// exactly the expected folders - Amiga and Atari ST among them, nothing for FFmpeg, MAME 2010 or PlayStation.
//
#include "doctest/doctest.h"

#include "support/psc_info_tree.h"

#include <ableem/engine/retroarch_cores.h>
#include <ableem/engine/retroarch_scanner.h>

#include <algorithm>
#include <fstream>
#include <map>
#include <set>
#include <string>
#include <vector>

using ableem::CoreInfoTable;
using ableem::DirEntry;
using ableem::RetroArchScanner;
using std::string;
using std::vector;

namespace {

const string platformDir = AB_PLATFORM_DIR;

// "<database>=<core>" lines of a cores.cfg, in file order
vector<std::pair<string, string>> cfgLines(const string &file) {
    vector<std::pair<string, string>> out;
    std::ifstream in(file);
    string line;
    while (std::getline(in, line)) {
        if (line.empty() || line[0] == '#' || line.find('=') == string::npos)
            continue;
        out.emplace_back(line.substr(0, line.find('=')), line.substr(line.find('=') + 1));
    }
    return out;
}

// the folders a scan of the whole pack makes, with the shipped skip list and aliases
const char *const expectedFolders[] = {
    "Amstrad - CPC",
    "Arduboy",
    "Atari - 2600",
    "Atari - 5200",
    "Atari - 7800",
    "Atari - Jaguar",
    "Atari - Lynx",
    "Atari - ST",
    "Bandai - WonderSwan",
    "Bandai - WonderSwan Color",
    "Cannonball",
    "Cave Story",
    "ChaiLove",
    "Coleco - ColecoVision",
    "Commodore - 64",
    "Commodore - Amiga",
    "Commodore - C128",
    "Commodore - VIC20",
    "DOOM",
    "DOS",
    "Daphne",
    "FB Alpha - Arcade Games",
    "FBA - Arcade Games",
    "Flashback",
    "FreeChaF",
    "GCE - Vectrex",
    "Handheld Electronic Game",
    "Jump 'n Bump",
    "LowRes NX",
    "Lutro",
    "MAME",
    "Magnavox - Odyssey2",
    "Mattel - Intellivision",
    "Mega Duck",
    "Microsoft - MSX",
    "Microsoft - MSX2",
    "MrBoom",
    "NEC - PC Engine - TurboGrafx 16",
    "NEC - PC Engine CD - TurboGrafx-CD",
    "NEC - PC Engine SuperGrafx",
    "NEC - PC-8001 - PC-8801",
    "NEC - PC-FX",
    "Nintendo - Family Computer Disk System",
    "Nintendo - Game Boy",
    "Nintendo - Game Boy Advance",
    "Nintendo - Game Boy Advance (e-Cards)",
    "Nintendo - Game Boy Color",
    "Nintendo - Nintendo 64",
    "Nintendo - Nintendo 64DD",
    "Nintendo - Nintendo DS",
    "Nintendo - Nintendo DS (Download Play)",
    "Nintendo - Nintendo DS Decrypted",
    "Nintendo - Nintendo Entertainment System",
    "Nintendo - Pokemon Mini",
    "Nintendo - Satellaview",
    "Nintendo - Sufami Turbo",
    "Nintendo - Super Nintendo Entertainment System",
    "Nintendo - Super Nintendo Entertainment System Hacks",
    "Nintendo - Virtual Boy",
    "PC-98",
    "PICO8",
    "Phillips - Videopac+",
    "Quake",
    "RPG Maker",
    "RPG Maker 2000",
    "RPG Maker 2003",
    "Rick Dangerous",
    "SNK - Neo Geo CD",
    "SNK - Neo Geo Pocket",
    "SNK - Neo Geo Pocket Color",
    "ScummVM",
    "Sega - 32X",
    "Sega - Dreamcast",
    "Sega - Game Gear",
    "Sega - Master System - Mark III",
    "Sega - Mega Drive - Genesis",
    "Sega - Mega-CD - Sega CD",
    "Sega - NAOMI",
    "Sega - PICO",
    "Sega - SG-1000",
    "Sega - Saturn",
    "Sharp - X68000",
    "Sharp X1",
    "Sinclair - ZX 81",
    "Sinclair - ZX Spectrum",
    "Sinclair - ZX Spectrum +3",
    "Sony - PlayStation Portable",
    "Super Bros War",
    "TIC-80",
    "The 3DO Company - 3DO",
    "Thomson - MOTO",
    "Uzebox",
    "WASM-4",
    "Watara - Supervision",
    "Wolfenstein 3D",
};

} // namespace

TEST_CASE("psc.cores.cfg: every line names an installed core by its file stem, and that core plays the system") {
    PscInfoTree ra;
    const auto lines = cfgLines(platformDir + "/psc.cores.cfg");
    REQUIRE(lines.size() > 30);
    CoreInfoTable cores;
    cores.load(ra.retroarch(), platformDir + "/psc.cores.cfg");

    // the two lines that name a core whose own .info does not list the system, on purpose: FinalBurn Neo plays
    // FB Alpha's sets, and MAME 2003 Plus is the pack's "MAME" (its .info lists "MAME 2003-Plus")
    const std::map<string, string> byDesign{{"FB Alpha - Arcade Games", "km_fbneo"}, {"MAME", "km_mame2003_plus"}};

    std::set<string> seen;
    for (const auto &line : lines) {
        INFO(line.first << " = " << line.second);
        CHECK(seen.insert(line.first).second);           // a system once
        CHECK(cores.databases().count(line.first) == 1); // some core in the pack plays it
        ableem::CoreInfoPtr core = cores.overrideCoreFor(line.first);
        REQUIRE(core);
        CHECK(core->stem == line.second); // the stem, not a fragment that landed on another build
        const bool listed =
            std::find(core->databases.begin(), core->databases.end(), line.first) != core->databases.end();
        auto exception = byDesign.find(line.first);
        if (exception == byDesign.end()) {
            CHECK(listed);
        } else {
            CHECK(exception->second == core->stem);
        }
    }
}

TEST_CASE("psc.cores.cfg: the picks of the plan, by system") {
    PscInfoTree ra;
    CoreInfoTable cores;
    cores.load(ra.retroarch(), platformDir + "/psc.cores.cfg");
    auto stemFor = [&](const string &db) {
        ableem::CoreInfoPtr core = cores.coreForDatabase(db);
        return core ? core->stem : string("<none>");
    };
    CHECK(stemFor("Nintendo - Nintendo Entertainment System") == "km_fceumm");
    CHECK(stemFor("Nintendo - Family Computer Disk System") == "km_fceumm");
    for (const char *snes : {"Nintendo - Super Nintendo Entertainment System",
                             "Nintendo - Super Nintendo Entertainment System Hacks", "Nintendo - Sufami Turbo"})
        CHECK(stemFor(snes) == "km_snes9x2010");
    CHECK(stemFor("Nintendo - Satellaview") == "km_snes9x"); // km_snes9x2010 does not list it
    CHECK(stemFor("Nintendo - Nintendo 64") == "km_glupen64");
    CHECK(stemFor("Nintendo - Game Boy") == "km_gambatte");
    CHECK(stemFor("Nintendo - Game Boy Color") == "km_gambatte");
    CHECK(stemFor("Nintendo - Game Boy Advance") == "km_mgba");
    for (const char *sega : {"Sega - Mega Drive - Genesis", "Sega - Master System - Mark III", "Sega - Game Gear",
                             "Sega - SG-1000", "Sega - Mega-CD - Sega CD"})
        CHECK(stemFor(sega) == "km_genesis_plus_gx");
    CHECK(stemFor("Sega - 32X") == "km_picodrive");
    CHECK(stemFor("NEC - PC Engine - TurboGrafx 16") == "km_mednafen_pce_fast");
    CHECK(stemFor("NEC - PC Engine CD - TurboGrafx-CD") == "km_mednafen_pce_fast");
    CHECK(stemFor("FBA - Arcade Games") == "km_fbneo");
    CHECK(stemFor("FB Alpha - Arcade Games") == "km_fbneo");
    CHECK(stemFor("FBNeo - Arcade Games") == "fbneo");
    CHECK(stemFor("MAME") == "km_mame2003_plus");
    CHECK(stemFor("Sega - Dreamcast") == "km_flycast_xtreme");
    CHECK(stemFor("Sega - NAOMI") == "km_flycast_xtreme");
    CHECK(stemFor("DOS") == "km_dosbox_pure");
    CHECK(stemFor("Commodore - Amiga") == "km_puae_xtreme");
    // a system no line names still gets one core, the same every time
    CHECK(stemFor("Atari - ST") == "km_hatari");
    CHECK(stemFor("Nintendo - Nintendo 64DD") == "km_mupen64_plus");
}

TEST_CASE("the folder pass on the whole pack makes the expected folders and no others") {
    PscInfoTree ra;
    CoreInfoTable cores;
    cores.load(ra.retroarch(), platformDir + "/psc.cores.cfg");
    const auto aliases = RetroArchScanner::loadFolderAliases(platformDir + "/roms_folders.cfg");
    const auto skip = RetroArchScanner::loadSkipList(platformDir + "/roms_skip.cfg");
    REQUIRE(skip.size() >= 16);

    vector<string> created = RetroArchScanner::createMissingFolders(ra.roms(), cores, aliases, skip);
    std::sort(created.begin(), created.end());
    vector<string> expected(std::begin(expectedFolders), std::end(expectedFolders));
    std::sort(expected.begin(), expected.end());
    CHECK(created == expected);

    // the spot checks the owner asked about
    CHECK(DirEntry::isDirectory(ra.roms() + "/Commodore - Amiga"));
    CHECK(DirEntry::isDirectory(ra.roms() + "/Atari - ST"));
    CHECK_FALSE(DirEntry::isDirectory(ra.roms() + "/FFmpeg"));
    CHECK_FALSE(DirEntry::isDirectory(ra.roms() + "/MAME 2010"));
    CHECK_FALSE(DirEntry::isDirectory(ra.roms() + "/MAME 2003"));
    CHECK_FALSE(DirEntry::isDirectory(ra.roms() + "/Sony - PlayStation"));
    CHECK(DirEntry::isDirectory(ra.roms() + "/MAME"));
    CHECK_FALSE(DirEntry::isDirectory(ra.roms() + "/FBNeo - Arcade Games")); // "Arcade" is its folder

    // a stick that already has some: what is there is left alone, only the rest is made
    PscInfoTree second;
    second.tmp.makeSubDir("roms/Commodore - Amiga");
    second.tmp.writeFile("roms/Commodore - Amiga/Turrican.adf", "disk");
    vector<string> rest = RetroArchScanner::createMissingFolders(second.roms(), cores, aliases, skip);
    CHECK(rest.size() == expected.size() - 1);
    CHECK(second.tmp.readFile("roms/Commodore - Amiga/Turrican.adf") == "disk");
}
