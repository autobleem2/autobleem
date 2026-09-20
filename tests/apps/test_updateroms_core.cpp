//
// UpdateRomsJob: a console stick (or a Pi card) in a PC, scanned with the PC's network - a fake stick in a
// TempDir, a fake network as OnlineAssets's CommandRunner.
//
#include "doctest/doctest.h"

#include "../support/env_fixture.h"
#include "../support/rdb_builder.h"
#include "../support/string_maker.h"
#include "../support/temp_dir.h"

#include "core/update_roms_job.h"

#include <ableem/engine/filesystem.h>
#include <ableem/engine/retroarch_playlist.h>
#include <ableem/engine/zip_writer.h>

#include <fstream>
#include <map>
#include <string>
#include <vector>

using ableem::DirEntry;
using std::string;
using std::vector;

namespace {

const char *const NES = "Nintendo - Nintendo Entertainment System";

// the job spells every path with forward slashes; the temp dir may not (C:\Users\... on the MSYS2 host)
string fwd(string path) {
    for (char &c : path)
        c = c == '\\' ? '/' : c;
    return path;
}

// a stick as the payload lays it out, with one installed core and one ROM; a console's (RetroBoot's
// folder) unless `pi`
struct FakeStick {
    explicit FakeStick(bool pi = false) : tmp("stick") {
        env.setWorkingPath(tmp.path());
        tmp.makeSubDir("Autobleem/bin/autobleem/platform");
        tmp.writeFile(
            "Autobleem/bin/autobleem/platform/psc.ini",
            "retroarch_dir=RetroArch/bin\nretroarch_roms_dir=RetroArch/roms\ndownload_command=\nusb_root=/media\n");
        tmp.writeFile("Autobleem/bin/autobleem/platform/rpi.ini",
                      "retroarch_dir=RetroArch\nretroarch_roms_dir=RetroArch/roms\nusb_root=/media/autobleem\n");
        tmp.writeFile("Autobleem/bin/autobleem/platform/pc.ini", "download_command=fetch %u %o\n");
        tmp.writeFile("Autobleem/bin/autobleem/platform/roms_folders.cfg", "Arcade=FBNeo - Arcade Games\n");
        tmp.makeSubDir("System/Logs");
        ra = pi ? "RetroArch" : "RetroArch/bin";
        if (pi)
            tmp.writeFile("RetroArch/retroarch.cfg", "# generated\n");
        else
            tmp.makeSubDir("RetroArch/bin");
        tmp.writeFile(ra + "/info/nestopia_libretro.info", "display_name = \"Nintendo - NES (Nestopia)\"\n"
                                                           "supported_extensions = \"nes|fds\"\n"
                                                           "database = \"" +
                                                               string(NES) + "\"\n");
        tmp.writeFile(ra + "/cores/nestopia_libretro.so", "core");
        tmp.makeSubDir(ra + "/playlists");
        tmp.makeSubDir(ra + "/database/rdb");
        tmp.makeSubDir(ra + "/thumbnails");
        roms = "RetroArch/roms";
        tmp.makeSubDir(roms + "/" + NES);
    }

    void addRom(const string &name, const string &bytes = "rom") {
        tmp.writeFile(roms + "/" + NES + "/" + name, bytes);
    }

    // the NES database, Adventures of Lolo by crc32("rom")
    void addDatabase() {
        test_support::Bytes records;
        test_support::appendRomRecord(records, "Adventures of Lolo (USA)", "Adventures of Lolo (USA).nes", 0x79520FA1u,
                                      "HAL", 1989, 1);
        records.push_back(0xc0);
        test_support::writeRdb(tmp, ra + "/database/rdb/" + NES + ".rdb", test_support::makeRdb(0, records));
    }

    ableem::RetroArchPlaylistEntries playlist() const {
        ableem::RetroArchPlaylistEntries entries;
        ableem::RetroArchPlaylist::load(tmp.at(ra + "/playlists/" + NES + ".lpl"), entries);
        return entries;
    }

    EnvFixture env;
    TempDir tmp;
    string ra, roms;
};

// the fake network: URL -> bytes; the command template is "fetch %u %o"
struct FakeServer {
    std::map<string, string> files;
    vector<string> urls;
    OnlineAssets::CommandRunner runner() {
        return [this](const string &commandLine) {
            size_t sp = commandLine.find(' ');
            size_t sp2 = commandLine.find(' ', sp + 1);
            string url = commandLine.substr(sp + 1, sp2 - sp - 1);
            string out = commandLine.substr(sp2 + 1);
            urls.push_back(url);
            auto it = files.find(url);
            if (it == files.end())
                return 22;
            std::ofstream o(out, std::ios::binary);
            o << it->second;
            return 0;
        };
    }
};

} // namespace

TEST_CASE("rootFromProgramPath: the first parent with Autobleem/bin/autobleem, else nothing") {
    TempDir tmp("root");
    tmp.makeSubDir("stick/Autobleem/bin/autobleem");
    tmp.makeSubDir("stick/UpdateRoms");
    CHECK(UpdateRomsJob::rootFromProgramPath(tmp.at("stick/UpdateRoms/UpdateRoms.exe")) == fwd(tmp.at("stick")));
    CHECK(UpdateRomsJob::rootFromProgramPath(tmp.at("stick/UpdateRoms.exe")) == fwd(tmp.at("stick")));
    CHECK(UpdateRomsJob::rootFromProgramPath(tmp.at("stick\\UpdateRoms\\UpdateRoms.exe")) == fwd(tmp.at("stick")));
    CHECK(UpdateRomsJob::rootFromProgramPath(tmp.at("elsewhere/UpdateRoms.exe")) == "");
    CHECK(UpdateRomsJob::rootFromProgramPath("UpdateRoms.exe") == "");
}

TEST_CASE("detect: a console stick by its RetroArch/bin, a Pi card by its retroarch.cfg, --target over both") {
    FakeStick console;
    UpdateRomsJob::Setup setup;
    string error;
    REQUIRE(UpdateRomsJob::detect(console.tmp.path(), setup, error));
    CHECK(setup.target == "psc");
    CHECK(setup.targetRoot == "/media");
    CHECK(setup.romsDir == fwd(console.tmp.at("RetroArch/roms")));
    CHECK(setup.playlistsDir == fwd(console.tmp.at("RetroArch/bin/playlists")));
    CHECK(setup.targetRomsDir == "/media/RetroArch/roms");
    CHECK(setup.targetRetroarchDir == "/media/RetroArch/bin");
    CHECK(setup.downloadCommand == "fetch %u %o"); // the PC's, never the console's empty one
    CHECK(setup.coresCfg == fwd(console.tmp.at("Autobleem/bin/autobleem/platform/psc.cores.cfg")));

    FakeStick pi(true);
    REQUIRE(UpdateRomsJob::detect(pi.tmp.path(), setup, error));
    CHECK(setup.target == "rpi");
    CHECK(setup.targetRoot == "/media/autobleem");
    CHECK(setup.romsDir == fwd(pi.tmp.at("RetroArch/roms")));
    CHECK(setup.targetRomsDir == "/media/autobleem/RetroArch/roms");

    // --target psc on a Pi card: the console layout says RetroArch/bin, the card has no bin/ - detect
    // rightly finds no RetroArch there
    CHECK_FALSE(UpdateRomsJob::detect(pi.tmp.path(), setup, error, "psc"));
    CHECK(error.find("No RetroArch") != string::npos);

    TempDir notAStick("nope");
    CHECK_FALSE(UpdateRomsJob::detect(notAStick.path(), setup, error));
    CHECK(error.find("Not an AutoBleem stick") != string::npos);
}

TEST_CASE("run: the playlists name the target's paths - ROMs and cores - and the databases name the games") {
    FakeStick stick;
    stick.addRom("lolo.nes");
    stick.addDatabase();
    UpdateRomsJob::Setup setup;
    string error;
    REQUIRE(UpdateRomsJob::detect(stick.tmp.path(), setup, error));

    FakeServer server; // nothing on it: offline
    vector<string> said;
    UpdateRomsJob::Report report =
        UpdateRomsJob::run(setup, nullptr, nullptr, [&said](const string &l) { said.push_back(l); }, server.runner());
    CHECK_FALSE(report.online);
    CHECK(report.databases == 1);
    CHECK(report.games == 1);
    CHECK(report.identified == 1);
    CHECK(report.playlistsWritten == vector<string>{string(NES) + ".lpl"});
    CHECK(report.boxArtFetched == 0);
    CHECK(said.size() == report.lines.size());
    CHECK(said[0].find("PlayStation Classic") != string::npos);

    ableem::RetroArchPlaylistEntries entries = stick.playlist();
    REQUIRE(entries.size() == 1);
    CHECK(entries[0].path == string("/media/RetroArch/roms/") + NES + "/lolo.nes");
    CHECK(entries[0].label == "Adventures of Lolo (USA)");
    CHECK(entries[0].core_path == "/media/RetroArch/bin/cores/nestopia_libretro.so");
    CHECK(entries[0].crc32 == "79520FA1|crc");
}

TEST_CASE("run: online, the databases bundle comes first and the box art after the scan") {
    FakeStick stick(true);
    stick.addRom("lolo.nes");
    UpdateRomsJob::Setup setup;
    string error;
    REQUIRE(UpdateRomsJob::detect(stick.tmp.path(), setup, error));

    // the bundle: rdb/<system>.rdb, as buildbot packs it
    test_support::Bytes records;
    test_support::appendRomRecord(records, "Adventures of Lolo (USA)", "Adventures of Lolo (USA).nes", 0x79520FA1u);
    records.push_back(0xc0);
    test_support::Bytes rdb = test_support::makeRdb(0, records);
    ableem::ZipWriter zip;
    REQUIRE(zip.open(stick.tmp.at("bundle.zip")));
    REQUIRE(zip.addBytes(string("rdb/") + NES + ".rdb", string(rdb.begin(), rdb.end())));
    REQUIRE(zip.close());

    FakeServer server;
    server.files["https://thumbnails.libretro.com/"] = "<html>";
    server.files["https://buildbot.libretro.com/assets/frontend/database-rdb.zip"] = stick.tmp.readFile("bundle.zip");
    server.files[OnlineAssets::boxArtUrl("https://thumbnails.libretro.com", NES, "Adventures of Lolo (USA)")] = "png";

    UpdateRomsJob::Report report = UpdateRomsJob::run(setup, nullptr, nullptr, nullptr, server.runner());
    CHECK(report.online);
    CHECK(report.databases == 1);
    CHECK(report.identified == 1);
    CHECK(report.boxArtFetched == 1);
    CHECK(report.boxArtMissing == 0);
    CHECK(DirEntry::exists(
        stick.tmp.at(string("RetroArch/thumbnails/") + NES + "/Named_Boxarts/Adventures of Lolo (USA).png")));
    ableem::RetroArchPlaylistEntries entries = stick.playlist();
    REQUIRE(entries.size() == 1);
    CHECK(entries[0].path == string("/media/autobleem/RetroArch/roms/") + NES + "/lolo.nes");
    CHECK(entries[0].core_path == "/media/autobleem/RetroArch/cores/nestopia_libretro.so");

    // a second run: the databases are there, the cover is there - no request beyond the probe
    server.urls.clear();
    report = UpdateRomsJob::run(setup, nullptr, nullptr, nullptr, server.runner());
    CHECK(report.playlistsWritten.empty());
    CHECK(server.urls == vector<string>{"https://thumbnails.libretro.com/"});
}

TEST_CASE("run: an entry a scan on this PC wrote with this PC's path is made to name the target's") {
    FakeStick stick;
    stick.addRom("lolo.nes");
    UpdateRomsJob::Setup setup;
    string error;
    REQUIRE(UpdateRomsJob::detect(stick.tmp.path(), setup, error));
    ableem::RetroArchPlaylistEntry old;
    old.path = stick.tmp.at(string("RetroArch/roms/") + NES + "/lolo.nes");
    old.label = "lolo";
    old.core_path = "DETECT";
    old.core_name = "DETECT";
    old.crc32 = "00000000|crc";
    old.db_name = string(NES) + ".lpl";
    REQUIRE(ableem::RetroArchPlaylist::save(stick.tmp.at(string("RetroArch/bin/playlists/") + NES + ".lpl"), {old}));

    FakeServer server;
    UpdateRomsJob::Report report = UpdateRomsJob::run(setup, nullptr, nullptr, nullptr, server.runner());
    CHECK(report.playlistsWritten.size() == 1);
    ableem::RetroArchPlaylistEntries entries = stick.playlist();
    REQUIRE(entries.size() == 1);
    CHECK(entries[0].path == string("/media/RetroArch/roms/") + NES + "/lolo.nes");
    CHECK(entries[0].label == "lolo"); // kept: no database to say better
}
