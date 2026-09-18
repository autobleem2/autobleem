//
// RetroArchScanner: the offline scan of RetroArch's ROM folders into playlists, and the CoreInfoTable it
// takes its system table from. Every tree is built in a TempDir; archives through ZipWriter.
//
#include "doctest/doctest.h"

#include "../support/env_fixture.h"
#include "../support/rdb_builder.h"
#include "../support/string_maker.h"
#include "../support/temp_dir.h"

#include <ableem/engine/crc32.h>
#include <ableem/engine/filesystem.h>
#include <ableem/engine/game_scanner.h>
#include <ableem/engine/retroarch_cores.h>
#include <ableem/engine/retroarch_playlist.h>
#include <ableem/engine/retroarch_scanner.h>
#include <ableem/engine/zip_archive.h>
#include <ableem/engine/zip_writer.h>

#include <algorithm>
#include <fstream>
#include <string>
#include <vector>

using ableem::CoreInfoPtr;
using ableem::CoreInfoTable;
using ableem::Crc32;
using ableem::DirEntry;
using ableem::RetroArchPlaylist;
using ableem::RetroArchPlaylistEntries;
using ableem::RetroArchPlaylistEntry;
using ableem::RetroArchPlaylistHeader;
using ableem::RetroArchScanner;
using ableem::RetroArchScanResult;
using ableem::RetroArchSystem;
using ableem::RetroArchSystems;
using ableem::ScannedRom;
using ableem::ScannedRoms;
using ableem::ScanStage;
using ableem::ZipWriter;
using std::string;
using std::vector;

namespace {

// a .zip at `path` holding the given entries (name -> bytes)
void writeZip(const string &path, const vector<std::pair<string, string>> &entries) {
    ZipWriter zip;
    REQUIRE(zip.open(path));
    for (const auto &e : entries)
        REQUIRE(zip.addBytes(e.first, e.second));
    REQUIRE(zip.close());
}

RetroArchSystem nesSystem() {
    RetroArchSystem nes;
    nes.name = "Nintendo - Nintendo Entertainment System";
    nes.coreName = "Nintendo - NES / Famicom (Nestopia UE)";
    nes.corePath = "/media/retroarch/cores/nestopia_libretro.so";
    nes.extensions = {"nes", "fds", "unf", "unif"};
    return nes;
}

RetroArchSystem cdSystem() {
    RetroArchSystem sega;
    sega.name = "Sega - Mega-CD - Sega CD";
    sega.coreName = "Sega - MS/GG/MD/CD (Genesis Plus GX)";
    sega.corePath = "/media/retroarch/cores/genesis_plus_gx_libretro.so";
    sega.extensions = {"bin", "cue", "iso", "chd", "m3u", "ccd"};
    return sega;
}

// as fbneo's .info really reads: "zip" among the extensions, no block_extract line
RetroArchSystem arcadeSystem() {
    RetroArchSystem fbneo;
    fbneo.name = "FBNeo - Arcade Games";
    fbneo.coreName = "Arcade (FinalBurn Neo)";
    fbneo.corePath = "/media/retroarch/cores/fbneo_libretro.so";
    fbneo.extensions = {"zip", "7z", "cue", "ccd"};
    return fbneo;
}

const RetroArchPlaylistEntry *findByLabel(const RetroArchPlaylistEntries &entries, const string &label) {
    for (const auto &e : entries) {
        if (e.label == label)
            return &e;
    }
    return nullptr;
}

// a path as a JSON string body: the temp dir may hold backslashes (C:\msys64\tmp on the MSYS2 host)
string jsonPath(const string &path) {
    string out;
    for (char c : path) {
        if (c == '\\' || c == '"')
            out += '\\';
        out += c;
    }
    return out;
}

// the playlist entries of a folder scan, and the reverse for a merge() test that starts from entries
RetroArchPlaylistEntries entriesOf(const ableem::ScannedRoms &roms) {
    RetroArchPlaylistEntries out;
    for (const auto &r : roms)
        out.push_back(r.entry);
    return out;
}
ableem::ScannedRoms scanned(const RetroArchPlaylistEntries &entries, bool identified = false) {
    ableem::ScannedRoms out;
    for (const auto &e : entries) {
        ableem::ScannedRom r;
        r.entry = e;
        r.identified = identified;
        out.push_back(r);
    }
    return out;
}

vector<string> labelsOf(const RetroArchPlaylistEntries &entries) {
    vector<string> out;
    for (const auto &e : entries)
        out.push_back(e.label);
    return out;
}

struct RecordingListener : ableem::ScanProgressListener {
    void onScanProgress(ScanStage stage, const string &detail, int done, int total) override {
        stages.push_back(stage);
        details.push_back(detail);
        dones.push_back(done);
        totals.push_back(total);
    }
    vector<ScanStage> stages;
    vector<string> details;
    vector<int> dones, totals;
};

// a RetroArch tree with an info/ + cores/ pair per core and a roms/ dir, the scanner pointed at them
struct RomsTree {
    RomsTree() : tmp("rascan") {
        tmp.makeSubDir("retroarch/info");
        tmp.makeSubDir("retroarch/cores");
        tmp.makeSubDir("retroarch/playlists");
        tmp.makeSubDir("roms");
        options.romsDir = tmp.at("roms");
        options.playlistsDir = tmp.at("retroarch/playlists");
    }

    void addCore(const string &file, const string &displayName, const string &extensions, const string &database,
                 bool blockExtract = false, bool installed = true) {
        string info = "display_name = \"" + displayName + "\"\nsupported_extensions = \"" + extensions +
                      "\"\ndatabase = \"" + database + "\"\n";
        if (blockExtract)
            info += "block_extract = \"true\"\n";
        tmp.writeFile("retroarch/info/" + file + ".info", info);
        if (installed)
            tmp.writeFile("retroarch/cores/" + file + ".so", "core");
    }

    CoreInfoTable cores(const string &coresCfg = "") {
        CoreInfoTable table;
        table.load(tmp.at("retroarch"), coresCfg);
        return table;
    }

    string playlist(const string &system) const { return tmp.at("retroarch/playlists/" + system + ".lpl"); }

    RetroArchPlaylistEntries loadPlaylist(const string &system, RetroArchPlaylistHeader *header = nullptr) const {
        RetroArchPlaylistEntries entries;
        RetroArchPlaylist::load(playlist(system), entries, header);
        return entries;
    }

    string readFile(const string &relative) const { return tmp.readFile(relative); }

    TempDir tmp;
    RetroArchScanner::Options options;
};

const char *const NES = "Nintendo - Nintendo Entertainment System";

} // namespace

//*******************************
// CoreInfoTable
//*******************************
TEST_CASE("CoreInfoTable: an .info's fields, block_extract included; only installed cores count") {
    RomsTree t;
    t.addCore("nestopia_libretro", "Nintendo - NES / Famicom (Nestopia UE)", "nes|fds|unf|unif", NES);
    t.addCore("fbneo_libretro", "Arcade (FinalBurn Neo)", "zip|7z", "FBNeo - Arcade Games", true);
    t.addCore("mesen_libretro", "Nintendo - NES / Famicom (Mesen)", "nes|fds", NES, false, false);

    CoreInfoTable cores = t.cores();
    REQUIRE(cores.cores().size() == 2);
    CHECK(cores.databases().size() == 2);

    CoreInfoPtr nes = cores.coreForDatabase(NES);
    REQUIRE(nes);
    CHECK(nes->name == "Nintendo - NES / Famicom (Nestopia UE)");
    CHECK(nes->core_path == t.tmp.at("retroarch/cores/nestopia_libretro.so"));
    CHECK(nes->extensions == vector<string>{"nes", "fds", "unf", "unif"});
    CHECK_FALSE(nes->block_extract);

    CoreInfoPtr fbneo = cores.coreForDatabase("FBNeo - Arcade Games.lpl"); // the .lpl suffix is ignored
    REQUIRE(fbneo);
    CHECK(fbneo->block_extract);

    CHECK(cores.coreForDatabase("Sony - PlayStation") == nullptr);
}

TEST_CASE("CoreInfoTable: the core with the most extensions is the default, a cores.cfg overrides it") {
    RomsTree t;
    t.addCore("bsnes_libretro", "Nintendo - SNES / SFC (bsnes)", "sfc",
              "Nintendo - Super Nintendo Entertainment System");
    t.addCore("snes9x_libretro", "Nintendo - SNES / SFC (Snes9x)", "sfc|smc|fig",
              "Nintendo - Super Nintendo Entertainment System");

    CHECK(t.cores().coreForDatabase("Nintendo - Super Nintendo Entertainment System")->name ==
          "Nintendo - SNES / SFC (Snes9x)");

    t.tmp.writeFile("cores.cfg", "# comment\n\nNintendo - Super Nintendo Entertainment System = bsnes\n");
    CoreInfoTable cores = t.cores(t.tmp.at("cores.cfg"));
    CHECK(cores.coreForDatabase("Nintendo - Super Nintendo Entertainment System")->name ==
          "Nintendo - SNES / SFC (bsnes)");
    CHECK(cores.overrideCoreFor("nintendo - super nintendo entertainment system")->name ==
          "Nintendo - SNES / SFC (bsnes)");
    CHECK(cores.defaultCoreFor("Nintendo - Super Nintendo Entertainment System")->name ==
          "Nintendo - SNES / SFC (Snes9x)");
}

TEST_CASE("systemsFrom: one system per database an installed core plays") {
    RomsTree t;
    t.addCore("nestopia_libretro", "Nintendo - NES / Famicom (Nestopia UE)", "nes|fds", NES);
    t.addCore("fbneo_libretro", "Arcade (FinalBurn Neo)", "zip|7z", "FBNeo - Arcade Games|FBNeo - Arcade Games (Other)",
              true);

    RetroArchSystems systems = RetroArchScanner::systemsFrom(t.cores());
    REQUIRE(systems.size() == 3);
    auto find = [&](const string &name) -> const RetroArchSystem * {
        for (const auto &s : systems)
            if (s.name == name)
                return &s;
        return nullptr;
    };
    REQUIRE(find(NES));
    CHECK(find(NES)->coreName == "Nintendo - NES / Famicom (Nestopia UE)");
    CHECK(find(NES)->corePath == t.tmp.at("retroarch/cores/nestopia_libretro.so"));
    CHECK(find(NES)->extensions == vector<string>{"nes", "fds"});
    CHECK_FALSE(find(NES)->blockExtract);
    REQUIRE(find("FBNeo - Arcade Games"));
    CHECK(find("FBNeo - Arcade Games")->blockExtract);
}

//*******************************
// ZipArchive::listEntries
//*******************************
TEST_CASE("ZipArchive::listEntries gives each entry's CRC and size from the central directory") {
    TempDir tmp("zipcrc");
    writeZip(tmp.at("a.zip"), {{"game.nes", "rom"}, {"readme.txt", "other rom"}});

    vector<ableem::ZipEntry> entries;
    REQUIRE(ableem::ZipArchive::listEntries(tmp.at("a.zip"), entries));
    REQUIRE(entries.size() == 2);
    CHECK(entries[0].name == "game.nes");
    CHECK(entries[0].crc == 0x79520FA1u); // crc32("rom")
    CHECK(entries[0].size == 3);
    CHECK(entries[1].crc == 0x089A93F8u); // crc32("other rom")
    CHECK_FALSE(ableem::ZipArchive::listEntries(tmp.at("missing.zip"), entries));
}

//*******************************
// scanFolder
//*******************************
TEST_CASE("scanFolder: a zip with one ROM is 'zip#rom' named after the zip, with the ROM's CRC") {
    TempDir tmp("scanfolder");
    tmp.makeSubDir("nes");
    writeZip(tmp.at("nes/Adventures of Lolo (USA).zip"), {{"Adventures of Lolo (USA).nes", "rom"}});

    RetroArchPlaylistEntries entries =
        entriesOf(RetroArchScanner::scanFolder(tmp.at("nes"), "/media/roms/nes", nesSystem()));
    REQUIRE(entries.size() == 1);
    CHECK(entries[0].path == "/media/roms/nes/Adventures of Lolo (USA).zip#Adventures of Lolo (USA).nes");
    CHECK(entries[0].label == "Adventures of Lolo (USA)");
    CHECK(entries[0].crc32 == "79520FA1|crc");
    CHECK(entries[0].core_name == "Nintendo - NES / Famicom (Nestopia UE)");
    CHECK(entries[0].core_path == "/media/retroarch/cores/nestopia_libretro.so");
    CHECK(entries[0].db_name == "Nintendo - Nintendo Entertainment System.lpl");
}

TEST_CASE("scanFolder: a pack of several ROMs is several games named after each; a zip with none is skipped") {
    TempDir tmp("scanfolder");
    tmp.makeSubDir("nes");
    writeZip(tmp.at("nes/pack.zip"),
             {{"Alpha (USA).nes", "rom"}, {"notes.txt", "x"}, {"Beta (Europe).nes", "other rom"}});
    writeZip(tmp.at("nes/bios.zip"), {{"disksys.rom", "rom"}}); // .rom is not an NES extension
    tmp.writeFile("nes/broken.zip", "this is not a zip");

    RetroArchPlaylistEntries entries =
        entriesOf(RetroArchScanner::scanFolder(tmp.at("nes"), tmp.at("nes"), nesSystem()));
    REQUIRE(entries.size() == 2);
    CHECK(entries[0].path == tmp.at("nes/pack.zip") + "#Alpha (USA).nes");
    CHECK(entries[0].label == "Alpha (USA)");
    CHECK(entries[1].path == tmp.at("nes/pack.zip") + "#Beta (Europe).nes");
    CHECK(entries[1].label == "Beta (Europe)");
    CHECK(entries[1].crc32 == "089A93F8|crc");
}

TEST_CASE("scanFolder: a loose ROM is a plain entry; other files are ignored; sub-folders are walked") {
    TempDir tmp("scanfolder");
    tmp.makeSubDir("nes/Homebrew");
    tmp.writeFile("nes/Zelda.NES", "rom"); // any case
    tmp.writeFile("nes/Zelda.sav", "save");
    tmp.writeFile("nes/readme.txt", "text");
    tmp.writeFile("nes/.hidden.nes", "rom");
    tmp.writeFile("nes/Homebrew/Micro Mages.nes", "rom");

    RetroArchPlaylistEntries entries =
        entriesOf(RetroArchScanner::scanFolder(tmp.at("nes"), "/media/roms/nes", nesSystem()));
    REQUIRE(entries.size() == 2);
    CHECK(entries[0].path == "/media/roms/nes/Homebrew/Micro Mages.nes");
    CHECK(entries[0].label == "Micro Mages");
    CHECK(entries[0].crc32 == "00000000|crc");
    CHECK(entries[1].path == "/media/roms/nes/Zelda.NES");
    CHECK(entries[1].label == "Zelda");
}

TEST_CASE("scanFolder: a cue hides its bins, an m3u hides its discs, a ccd hides its img") {
    TempDir tmp("scanfolder");
    tmp.makeSubDir("cd");
    tmp.writeFile("cd/Sonic CD (USA).cue", "FILE \"Sonic CD (USA) (Track 1).bin\" BINARY\n  TRACK 01 MODE1/2352\n"
                                           "FILE \"Sonic CD (USA) (Track 2).bin\" BINARY\n  TRACK 02 AUDIO\n");
    tmp.writeFile("cd/Sonic CD (USA) (Track 1).bin", "data");
    tmp.writeFile("cd/Sonic CD (USA) (Track 2).bin", "audio");
    tmp.writeFile("cd/Lunar (USA).m3u", "Lunar (USA) (Disc 1).cue\r\nLunar (USA) (Disc 2).cue\r\n");
    tmp.writeFile("cd/Lunar (USA) (Disc 1).cue", "FILE \"Lunar (USA) (Disc 1).bin\" BINARY\n");
    tmp.writeFile("cd/Lunar (USA) (Disc 1).bin", "d1");
    tmp.writeFile("cd/Lunar (USA) (Disc 2).cue", "FILE \"lunar (usa) (disc 2).bin\" BINARY\n"); // case differs
    tmp.writeFile("cd/Lunar (USA) (Disc 2).bin", "d2");
    tmp.writeFile("cd/Popful Mail (USA).ccd", "[CloneCD]\nVersion=3\n");
    tmp.writeFile("cd/Popful Mail (USA).img", "img");
    tmp.writeFile("cd/Popful Mail (USA).sub", "sub");
    tmp.writeFile("cd/Loose Track.bin", "a lone bin is a game of its own");

    RetroArchPlaylistEntries entries = entriesOf(RetroArchScanner::scanFolder(tmp.at("cd"), "/r/cd", cdSystem()));
    CHECK(labelsOf(entries) == vector<string>{"Loose Track", "Lunar (USA)", "Popful Mail (USA)", "Sonic CD (USA)"});
    CHECK(findByLabel(entries, "Lunar (USA)")->path == "/r/cd/Lunar (USA).m3u");
    CHECK(findByLabel(entries, "Sonic CD (USA)")->path == "/r/cd/Sonic CD (USA).cue");
    CHECK(findByLabel(entries, "Popful Mail (USA)")->path == "/r/cd/Popful Mail (USA).ccd");
}

TEST_CASE("scanFolder: a core that reads archives itself gets the zip whole") {
    TempDir tmp("scanfolder");
    tmp.makeSubDir("arcade");
    writeZip(tmp.at("arcade/mslug.zip"), {{"201-c1.c1", "rom"}, {"201-c2.c2", "rom"}});
    tmp.writeFile("arcade/neogeo.zip", "not even a zip - the core is the one to complain");

    RetroArchPlaylistEntries entries =
        entriesOf(RetroArchScanner::scanFolder(tmp.at("arcade"), "/r/arcade", arcadeSystem()));
    REQUIRE(entries.size() == 2);
    CHECK(entries[0].path == "/r/arcade/mslug.zip");
    CHECK(entries[0].label == "mslug");
    CHECK(entries[0].crc32 == "00000000|crc");
    CHECK(entries[1].path == "/r/arcade/neogeo.zip");

    // block_extract alone says the same
    RetroArchSystem flagged = nesSystem();
    flagged.blockExtract = true;
    CHECK(flagged.readsArchives());
    CHECK_FALSE(nesSystem().readsArchives());
    CHECK(arcadeSystem().readsArchives());
}

TEST_CASE("scan: an aliased folder feeds its database's playlist; two folders may share one") {
    RomsTree t;
    t.addCore("fbneo_libretro", "Arcade (FinalBurn Neo)", "zip|7z|cue|ccd", "FBNeo - Arcade Games");
    t.tmp.writeFile("roms/Arcade/mslug.zip", "set");
    t.tmp.writeFile("roms/SNK - Neo Geo/aof.zip", "set");
    t.tmp.writeFile("aliases.cfg", "# folders\nArcade = FBNeo - Arcade Games\nSNK - Neo Geo=FBNeo - Arcade Games\n");
    t.options.folderAliases = RetroArchScanner::loadFolderAliases(t.tmp.at("aliases.cfg"));
    REQUIRE(t.options.folderAliases.size() == 2);
    CHECK(RetroArchScanner::loadFolderAliases(t.tmp.at("missing.cfg")).empty());
    RetroArchSystems systems = RetroArchScanner::systemsFrom(t.cores());

    RetroArchScanner scanner;
    RetroArchScanResult result = scanner.scan(t.options, systems);
    CHECK(result.systemsScanned == 2);
    CHECK(result.gamesFound == 2);
    CHECK(result.playlistsWritten == vector<string>{"FBNeo - Arcade Games.lpl"});
    CHECK(result.unknownFolders.empty());
    RetroArchPlaylistEntries entries = t.loadPlaylist("FBNeo - Arcade Games");
    CHECK(labelsOf(entries) == vector<string>{"aof", "mslug"});
    CHECK(entries[0].path == t.tmp.at("roms/SNK - Neo Geo/aof.zip"));
    CHECK(entries[1].path == t.tmp.at("roms/Arcade/mslug.zip"));
    CHECK(scanner.scan(t.options, systems).playlistsWritten.empty());

    // a set gone from one folder is dropped by that folder's pass, the other folder's entries untouched
    DirEntry::removeFile(t.tmp.at("roms/Arcade/mslug.zip"));
    CHECK(scanner.scan(t.options, systems).playlistsWritten.size() == 1);
    CHECK(labelsOf(t.loadPlaylist("FBNeo - Arcade Games")) == vector<string>{"aof"});
}

//*******************************
// merge
//*******************************
TEST_CASE("merge: foreign entries stay, a vanished file's entry goes, an existing entry beats ours, new ones join") {
    TempDir tmp("merge");
    tmp.makeSubDir("roms/nes");
    tmp.writeFile("roms/nes/Kept.nes", "rom");
    tmp.writeFile("roms/nes/New.nes", "rom");
    writeZip(tmp.at("roms/nes/Identified.zip"), {{"Identified.nes", "rom"}});

    auto entry = [](const string &path, const string &label, const string &crc = "00000000|crc") {
        RetroArchPlaylistEntry e;
        e.path = path;
        e.label = label;
        e.core_path = "DETECT";
        e.core_name = "DETECT";
        e.crc32 = crc;
        e.db_name = "Nintendo - Nintendo Entertainment System.lpl";
        return e;
    };
    const string source = tmp.at("roms/nes");
    const string target = "/media/roms/nes";

    RetroArchPlaylistEntries existing = {
        entry("/home/user/Downloads/Elsewhere.nes", "Elsewhere"),       // not under our folder: the user's
        entry(target + "/Gone.nes", "Gone"),                            // file no longer there
        entry(target + "/Kept.nes", "Kept As Written", "AAAAAAAA|crc"), // still there: kept exactly
        entry(source + "/Identified.zip#Identified.nes", "Identified (USA)", "79520FA1|crc"), // by the source path
    };
    RetroArchPlaylistEntries fresh = {
        entry(target + "/Identified.zip#Identified.nes", "Identified", "79520FA1|crc"),
        entry(target + "/Kept.nes", "Kept"),
        entry(target + "/New.nes", "New"),
    };

    RetroArchPlaylistEntries merged = RetroArchScanner::merge(existing, scanned(fresh), source, target);
    CHECK(labelsOf(merged) == vector<string>{"Elsewhere", "Identified (USA)", "Kept As Written", "New"});
    CHECK(findByLabel(merged, "Kept As Written")->crc32 == "AAAAAAAA|crc");
}

TEST_CASE("merge: sorted by label ignoring case, stable") {
    RetroArchPlaylistEntries fresh;
    for (const char *label : {"beta", "Alpha", "gamma", "alpha"}) {
        RetroArchPlaylistEntry e;
        e.path = string("/r/x/") + label + ".nes";
        e.label = label;
        fresh.push_back(e);
    }
    RetroArchPlaylistEntries merged = RetroArchScanner::merge({}, scanned(fresh), "/r/x", "/r/x");
    CHECK(labelsOf(merged) == vector<string>{"Alpha", "alpha", "beta", "gamma"});
}

//*******************************
// scan
//*******************************
TEST_CASE("scan: a playlist per known folder, an unknown folder reported, nothing written for an empty one") {
    RomsTree t;
    t.addCore("nestopia_libretro", "Nintendo - NES / Famicom (Nestopia UE)", "nes|fds", NES);
    t.addCore("fbneo_libretro", "Arcade (FinalBurn Neo)", "zip|7z", "FBNeo - Arcade Games");
    t.tmp.makeSubDir(string("roms/") + NES);
    t.tmp.makeSubDir("roms/FBNeo - Arcade Games");
    t.tmp.makeSubDir("roms/Sony - PlayStation"); // no core plays it
    t.tmp.makeSubDir("roms/AutoBleem");          // a reserved name, whatever is in it
    t.tmp.writeFile("roms/AutoBleem/x.nes", "rom");
    writeZip(t.tmp.at(string("roms/") + NES + "/Adventures of Lolo (USA).zip"),
             {{"Adventures of Lolo (USA).nes", "rom"}});
    t.tmp.writeFile(string("roms/") + NES + "/Battletoads (USA).nes", "rom");

    RecordingListener listener;
    RetroArchScanner scanner(&listener);
    RetroArchScanResult result = scanner.scan(t.options, RetroArchScanner::systemsFrom(t.cores()));

    CHECK(result.systemsScanned == 2);
    CHECK(result.gamesFound == 2);
    CHECK(result.playlistsWritten == vector<string>{string(NES) + ".lpl"});
    CHECK(result.unknownFolders == vector<string>{"AutoBleem", "Sony - PlayStation"});
    CHECK_FALSE(DirEntry::exists(t.playlist("FBNeo - Arcade Games"))); // empty folder, no playlist before
    CHECK_FALSE(DirEntry::exists(t.playlist("AutoBleem")));
    CHECK_FALSE(DirEntry::exists(t.playlist(NES) + ".tmp"));

    RetroArchPlaylistHeader header;
    RetroArchPlaylistEntries entries = t.loadPlaylist(NES, &header);
    REQUIRE(entries.size() == 2);
    CHECK(entries[0].label == "Adventures of Lolo (USA)");
    CHECK(entries[0].path ==
          t.tmp.at(string("roms/") + NES + "/Adventures of Lolo (USA).zip#Adventures of Lolo (USA).nes"));
    CHECK(entries[0].core_path == t.tmp.at("retroarch/cores/nestopia_libretro.so"));
    CHECK(entries[1].label == "Battletoads (USA)");
    REQUIRE(header.size() == 1);
    CHECK(header[0].first == "version");
    CHECK(header[0].second == "\"1.0\"");

    // one progress report per known folder
    REQUIRE(listener.stages.size() == 2);
    CHECK(listener.stages[0] == ScanStage::ScanningRoms);
    CHECK(listener.details == vector<string>{"FBNeo - Arcade Games", NES});
    CHECK(listener.dones == vector<int>{1, 2});
    CHECK(listener.totals == vector<int>{2, 2});
}

TEST_CASE("scan: a second run over the same tree writes nothing; a removed ROM rewrites the playlist") {
    RomsTree t;
    t.addCore("nestopia_libretro", "Nintendo - NES / Famicom (Nestopia UE)", "nes|fds", NES);
    t.tmp.writeFile(string("roms/") + NES + "/A.nes", "rom");
    t.tmp.writeFile(string("roms/") + NES + "/B.nes", "rom");
    RetroArchSystems systems = RetroArchScanner::systemsFrom(t.cores());

    RetroArchScanner scanner;
    CHECK(scanner.scan(t.options, systems).playlistsWritten.size() == 1);
    CHECK(scanner.scan(t.options, systems).playlistsWritten.empty());

    DirEntry::removeFile(t.tmp.at(string("roms/") + NES + "/B.nes"));
    RetroArchScanResult result = scanner.scan(t.options, systems);
    CHECK(result.playlistsWritten.size() == 1);
    CHECK(result.gamesFound == 1);
    CHECK(labelsOf(t.loadPlaylist(NES)) == vector<string>{"A"});
}

TEST_CASE("scan: a playlist RetroArch wrote keeps its header, its identified entries and the user's own") {
    RomsTree t;
    t.addCore("nestopia_libretro", "Nintendo - NES / Famicom (Nestopia UE)", "nes|fds", NES);
    const string folder = string("roms/") + NES;
    t.tmp.makeSubDir(folder);
    writeZip(t.tmp.at(folder + "/lolo.zip"), {{"lolo.nes", "rom"}});
    t.tmp.writeFile(folder + "/Battletoads (USA).nes", "rom");
    t.tmp.writeFile("elsewhere.nes", "rom");

    // what RetroArch 1.22's own scanner leaves behind: a 1.5 header, the archive entry identified by CRC
    t.tmp.writeFile("retroarch/playlists/" + string(NES) + ".lpl",
                    "{\n"
                    "  \"version\": \"1.5\",\n"
                    "  \"default_core_path\": \"\",\n"
                    "  \"default_core_name\": \"\",\n"
                    "  \"label_display_mode\": 0,\n"
                    "  \"right_thumbnail_mode\": 0,\n"
                    "  \"left_thumbnail_mode\": 0,\n"
                    "  \"thumbnail_match_mode\": 0,\n"
                    "  \"sort_mode\": 0,\n"
                    "  \"scan_content_dir\": \"" +
                        jsonPath(t.tmp.at(folder)) +
                        "\",\n"
                        "  \"scan_file_exts\": \"\",\n"
                        "  \"scan_dat_file_path\": \"\",\n"
                        "  \"scan_search_recursively\": true,\n"
                        "  \"scan_search_archives\": false,\n"
                        "  \"scan_filter_dat_content\": false,\n"
                        "  \"scan_overwrite_playlist\": false,\n"
                        "  \"items\": [\n"
                        "    {\n"
                        "      \"path\": \"" +
                        jsonPath(t.tmp.at(folder)) +
                        "/lolo.zip#lolo.nes\",\n"
                        "      \"label\": \"Adventures of Lolo (USA)\",\n"
                        "      \"core_path\": \"DETECT\",\n"
                        "      \"core_name\": \"DETECT\",\n"
                        "      \"crc32\": \"79520FA1|crc\",\n"
                        "      \"db_name\": \"Nintendo - Nintendo Entertainment System.lpl\"\n"
                        "    },\n"
                        "    {\n"
                        "      \"path\": \"" +
                        jsonPath(t.tmp.at("elsewhere.nes")) +
                        "\",\n"
                        "      \"label\": \"Elsewhere\",\n"
                        "      \"core_path\": \"DETECT\",\n"
                        "      \"core_name\": \"DETECT\",\n"
                        "      \"crc32\": \"00000000|crc\",\n"
                        "      \"db_name\": \"Nintendo - Nintendo Entertainment System.lpl\"\n"
                        "    }\n"
                        "  ]\n"
                        "}\n");

    RetroArchScanner scanner;
    RetroArchScanResult result = scanner.scan(t.options, RetroArchScanner::systemsFrom(t.cores()));
    CHECK(result.playlistsWritten.size() == 1);
    CHECK(result.gamesFound == 2);

    RetroArchPlaylistHeader header;
    RetroArchPlaylistEntries entries = t.loadPlaylist(NES, &header);
    CHECK(labelsOf(entries) == vector<string>{"Adventures of Lolo (USA)", "Battletoads (USA)", "Elsewhere"});
    CHECK(findByLabel(entries, "Adventures of Lolo (USA)")->core_path == "DETECT"); // as RetroArch left it
    CHECK(findByLabel(entries, "Battletoads (USA)")->core_path == t.tmp.at("retroarch/cores/nestopia_libretro.so"));

    REQUIRE(header.size() == 15);
    CHECK(header[0].first == "version");
    CHECK(header[0].second == "\"1.5\"");
    CHECK(header[8].first == "scan_content_dir");
    CHECK(header[11].first == "scan_search_recursively");
    CHECK(header[11].second == "true");
    // and the file itself reads as RetroArch wrote it
    string text = t.readFile("retroarch/playlists/" + string(NES) + ".lpl");
    CHECK(text.find("\"version\": \"1.5\"") == text.find("\"version\""));
    CHECK(text.find("\"scan_search_recursively\": true") != string::npos);
}

TEST_CASE("scan: the playlists name the target root, not where the ROMs are on this machine") {
    RomsTree t;
    t.addCore("nestopia_libretro", "Nintendo - NES / Famicom (Nestopia UE)", "nes|fds", NES);
    t.tmp.writeFile(string("roms/") + NES + "/A.nes", "rom");
    t.options.targetRomsDir = "/media/roms";
    RetroArchSystems systems = RetroArchScanner::systemsFrom(t.cores());

    RetroArchScanner scanner;
    CHECK(scanner.scan(t.options, systems).playlistsWritten.size() == 1);
    RetroArchPlaylistEntries entries = t.loadPlaylist(NES);
    REQUIRE(entries.size() == 1);
    CHECK(entries[0].path == string("/media/roms/") + NES + "/A.nes");

    // and a second run recognises its own entries through that root
    CHECK(scanner.scan(t.options, systems).playlistsWritten.empty());
    DirEntry::removeFile(t.tmp.at(string("roms/") + NES + "/A.nes"));
    CHECK(scanner.scan(t.options, systems).playlistsWritten.size() == 1);
    CHECK(t.loadPlaylist(NES).empty());
}

TEST_CASE("scan: without a roms dir or without systems nothing happens") {
    RomsTree t;
    RetroArchScanner scanner;
    DirEntry::rmDir(t.tmp.at("roms"));
    RetroArchScanResult none = scanner.scan(t.options, {nesSystem()});
    CHECK(none.systemsScanned == 0);

    t.tmp.makeSubDir(string("roms/") + NES);
    t.tmp.writeFile(string("roms/") + NES + "/A.nes", "rom");
    RetroArchScanResult noSystems = scanner.scan(t.options, {});
    CHECK(noSystems.systemsScanned == 0);
    CHECK(noSystems.unknownFolders == vector<string>{NES});
    CHECK_FALSE(DirEntry::exists(t.playlist(NES)));
}

//*******************************
// RetroArchPlaylist header round trip
//*******************************
TEST_CASE("RetroArchPlaylist: save() puts the loaded header back, version first; a six-line file has none") {
    TempDir tmp("lplheader");
    tmp.writeFile("a.lpl", "{\"sort_mode\": 2, \"version\": \"1.5\", \"items\": [], \"default_core_name\": \"x\"}");
    RetroArchPlaylistEntries entries;
    RetroArchPlaylistHeader header;
    REQUIRE(RetroArchPlaylist::load(tmp.at("a.lpl"), entries, &header));
    CHECK(entries.empty());
    REQUIRE(header.size() == 3);
    CHECK(header[0].first == "sort_mode");
    CHECK(header[0].second == "2");
    CHECK(header[2].first == "default_core_name");

    RetroArchPlaylistEntry e;
    e.path = "/r/x.nes";
    e.label = "x";
    REQUIRE(RetroArchPlaylist::save(tmp.at("b.lpl"), {e}, header));
    string text = tmp.readFile("b.lpl");
    CHECK(text.find("\"version\": \"1.5\"") < text.find("\"sort_mode\": 2"));
    CHECK(text.find("\"sort_mode\": 2") < text.find("\"items\""));

    RetroArchPlaylistHeader again;
    REQUIRE(RetroArchPlaylist::load(tmp.at("b.lpl"), entries, &again));
    REQUIRE(entries.size() == 1);
    CHECK(entries[0].core_path == ""); // written as given
    REQUIRE(again.size() == 3);
    CHECK(again[0].first == "version");

    tmp.writeFile("six.lpl", "/r/y.nes\ny\nDETECT\nDETECT\n00000000|crc\nx.lpl\n");
    REQUIRE(RetroArchPlaylist::load(tmp.at("six.lpl"), entries, &again));
    CHECK(entries.size() == 1);
    CHECK(again.empty());
}

//*******************************
// identification by database
//*******************************
namespace {

// crc32("rom") and crc32("other rom"), the bytes writeZip and the loose files below hold
const uint32_t CrcRom = 0x79520FA1u;
const uint32_t CrcOtherRom = 0x089A93F8u;

// the NES and arcade databases a test writes into <dir>: Lolo by CRC, Metal Slug by rom_name
string writeNesRdb(const TempDir &tmp, const string &dir) {
    test_support::Bytes records;
    test_support::appendRomRecord(records, "Adventures of Lolo (USA)", "Adventures of Lolo (USA).nes", CrcRom,
                                  "HAL Laboratory", 1989, 1);
    test_support::appendRomRecord(records, "Battletoads (USA)", "Battletoads (USA).nes", CrcOtherRom, "Tradewest", 1991,
                                  2);
    records.push_back(0xc0);
    tmp.makeSubDir(dir);
    return test_support::writeRdb(tmp, dir + "/" + NES + ".rdb", test_support::makeRdb(0, records));
}
string writeArcadeRdb(const TempDir &tmp, const string &dir) {
    test_support::Bytes records;
    test_support::appendRomRecord(records, "Metal Slug (NGM-2510)", "mslug.zip", 0x0AC09D00u, "Nazca", 1996, 2);
    records.push_back(0xc0);
    tmp.makeSubDir(dir);
    return test_support::writeRdb(tmp, dir + "/FBNeo - Arcade Games.rdb", test_support::makeRdb(0, records));
}

} // namespace

TEST_CASE("Crc32::ofFile streams a file, refuses one over the limit, spells a CRC as a playlist does") {
    TempDir tmp("crc");
    tmp.writeFile("a.bin", "rom");
    uint32_t crc = 1;
    CHECK(Crc32::ofFile(tmp.at("a.bin"), crc));
    CHECK(crc == CrcRom);
    CHECK(Crc32::ofFile(tmp.at("a.bin"), crc, 3));
    CHECK_FALSE(Crc32::ofFile(tmp.at("a.bin"), crc, 2));
    CHECK(crc == 0);
    CHECK_FALSE(Crc32::ofFile(tmp.at("missing.bin"), crc));
    CHECK(Crc32::ofBytes("other rom") == CrcOtherRom);
    CHECK(Crc32::playlistText(CrcRom) == "79520FA1|crc");
    CHECK(Crc32::playlistText(0) == "00000000|crc");
}

TEST_CASE("identify: a zipped ROM by the archive's CRC, a loose one by hashing it, a miss keeps the file's name") {
    TempDir tmp("identify");
    tmp.makeSubDir("nes");
    writeZip(tmp.at("nes/lolo.zip"), {{"lolo.nes", "rom"}});     // the database knows this CRC
    tmp.writeFile("nes/toads.nes", "other rom");                 // and this one, hashed from the file
    tmp.writeFile("nes/Homebrew Thing (World).nes", "unknown!"); // not in the database
    ableem::RdbReader rdb;
    REQUIRE(rdb.open(writeNesRdb(tmp, "rdb")));

    ScannedRoms roms = RetroArchScanner::scanFolder(tmp.at("nes"), "/r/nes", nesSystem());
    REQUIRE(roms.size() == 3);
    CHECK(RetroArchScanner::identify(roms, rdb, 0) == 2);

    const ScannedRom &homebrew = roms[0];
    CHECK_FALSE(homebrew.identified);
    CHECK(homebrew.entry.label == "Homebrew Thing (World)");
    CHECK(homebrew.entry.crc32 == Crc32::playlistText(Crc32::ofBytes("unknown!"))); // hashed all the same

    const ScannedRom &lolo = roms[1];
    CHECK(lolo.identified);
    CHECK(lolo.entry.label == "Adventures of Lolo (USA)");
    CHECK(lolo.entry.path == "/r/nes/lolo.zip#lolo.nes");
    CHECK(lolo.entry.crc32 == "79520FA1|crc");

    const ScannedRom &toads = roms[2];
    CHECK(toads.identified);
    CHECK(toads.entry.label == "Battletoads (USA)");
    CHECK(toads.entry.crc32 == "089A93F8|crc");
}

TEST_CASE("identify: a loose file over the size limit is not hashed and keeps its name") {
    TempDir tmp("identify");
    tmp.makeSubDir("nes");
    tmp.writeFile("nes/toads.nes", "other rom"); // 9 bytes
    ableem::RdbReader rdb;
    REQUIRE(rdb.open(writeNesRdb(tmp, "rdb")));

    ScannedRoms roms = RetroArchScanner::scanFolder(tmp.at("nes"), "/r/nes", nesSystem());
    CHECK(RetroArchScanner::identify(roms, rdb, 8) == 0);
    CHECK(roms[0].entry.label == "toads");
    CHECK(roms[0].entry.crc32 == "00000000|crc");
}

TEST_CASE("identify: an arcade set by its archive's name, whatever its bytes") {
    TempDir tmp("identify");
    tmp.makeSubDir("arcade");
    tmp.writeFile("arcade/mslug.zip", "a repacked set, not the reference bytes");
    tmp.writeFile("arcade/neogeo.zip", "bios");
    ableem::RdbReader rdb;
    REQUIRE(rdb.open(writeArcadeRdb(tmp, "rdb")));

    ScannedRoms roms = RetroArchScanner::scanFolder(tmp.at("arcade"), "/r/arcade", arcadeSystem());
    REQUIRE(roms.size() == 2);
    CHECK(roms[0].wholeArchive);
    CHECK(RetroArchScanner::identify(roms, rdb, 0) == 1);
    CHECK(roms[0].entry.label == "Metal Slug (NGM-2510)");
    CHECK(roms[0].entry.crc32 == "00000000|crc"); // the archive is not hashed
    CHECK(roms[1].entry.label == "neogeo");
}

TEST_CASE("merge: a database name replaces an existing unidentified label for the same ROM, and only that") {
    TempDir tmp("merge");
    tmp.makeSubDir("roms/nes");
    writeZip(tmp.at("roms/nes/pack.zip"), {{"a.nes", "rom"}, {"b.nes", "other rom"}});
    tmp.writeFile("roms/nes/Kept.nes", "rom");
    const string source = tmp.at("roms/nes");
    const string target = "/media/roms/nes";

    auto entry = [](const string &path, const string &label, const string &crc = "00000000|crc") {
        RetroArchPlaylistEntry e;
        e.path = path;
        e.label = label;
        e.core_path = "DETECT";
        e.core_name = "DETECT";
        e.crc32 = crc;
        e.db_name = "Nintendo - Nintendo Entertainment System.lpl";
        return e;
    };
    RetroArchPlaylistEntries existing = {
        entry(target + "/pack.zip#a.nes", "a"),                          // an earlier file-name scan
        entry(target + "/pack.zip#b.nes", "Battletoads (USA)", "X|crc"), // RetroArch's own, already named
        entry(target + "/Kept.nes", "Kept"),                             // ours: nothing to say about it
    };
    ScannedRoms fresh = scanned(
        {
            entry(target + "/pack.zip#a.nes", "Adventures of Lolo (USA)", "79520FA1|crc"),
            entry(target + "/pack.zip#b.nes", "Battletoads (USA)", "089A93F8|crc"),
        },
        true);
    fresh.push_back(scanned({entry(target + "/Kept.nes", "Kept")})[0]);

    RetroArchPlaylistEntries merged = RetroArchScanner::merge(existing, fresh, source, target);
    CHECK(labelsOf(merged) == vector<string>{"Adventures of Lolo (USA)", "Battletoads (USA)", "Kept"});
    CHECK(merged[0].crc32 == "79520FA1|crc"); // the replacement is ours, whole
    CHECK(merged[1].crc32 == "X|crc");        // the same name: RetroArch's entry stays as it was
    CHECK(merged[1].core_path == "DETECT");
}

TEST_CASE("scan: with the databases in place the playlists carry their names; a system without one keeps the "
          "file names, and a name an earlier scan wrote is corrected") {
    RomsTree t;
    t.addCore("nestopia_libretro", "Nintendo - NES / Famicom (Nestopia UE)", "nes|fds", NES);
    t.addCore("snes9x_libretro", "Nintendo - SNES / SFC (Snes9x)", "sfc|smc",
              "Nintendo - Super Nintendo Entertainment System");
    t.addCore("fbneo_libretro", "Arcade (FinalBurn Neo)", "zip|7z", "FBNeo - Arcade Games");
    writeNesRdb(t.tmp, "retroarch/database/rdb");
    writeArcadeRdb(t.tmp, "retroarch/database/rdb"); // no SNES database
    t.options.rdbDir = t.tmp.at("retroarch/database/rdb");
    t.options.folderAliases = {{"Arcade", "FBNeo - Arcade Games"}};
    const string nes = string("roms/") + NES;
    t.tmp.makeSubDir(nes);
    writeZip(t.tmp.at(nes + "/lolo.zip"), {{"lolo.nes", "rom"}});
    t.tmp.writeFile("roms/Nintendo - Super Nintendo Entertainment System/Chrono Trigger (USA).sfc", "rom");
    t.tmp.writeFile("roms/Arcade/mslug.zip", "set");
    RetroArchSystems systems = RetroArchScanner::systemsFrom(t.cores());

    // a first scan without databases, as a stick before its rdb pack would have it
    RetroArchScanner::Options blind = t.options;
    blind.rdbDir = "";
    RetroArchScanner scanner;
    RetroArchScanResult first = scanner.scan(blind, systems);
    CHECK(first.gamesFound == 3);
    CHECK(first.gamesIdentified == 0);
    CHECK(labelsOf(t.loadPlaylist(NES)) == vector<string>{"lolo"});

    RetroArchScanResult second = scanner.scan(t.options, systems);
    CHECK(second.gamesFound == 3);
    CHECK(second.gamesIdentified == 2);
    CHECK(second.playlistsWritten == vector<string>{"FBNeo - Arcade Games.lpl", string(NES) + ".lpl"});
    CHECK(labelsOf(t.loadPlaylist(NES)) == vector<string>{"Adventures of Lolo (USA)"});
    CHECK(labelsOf(t.loadPlaylist("FBNeo - Arcade Games")) == vector<string>{"Metal Slug (NGM-2510)"});
    // the SNES folder comes after the NES one: its games must not be looked up in the NES database
    CHECK(labelsOf(t.loadPlaylist("Nintendo - Super Nintendo Entertainment System")) ==
          vector<string>{"Chrono Trigger (USA)"});

    CHECK(scanner.scan(t.options, systems).playlistsWritten.empty());
}
