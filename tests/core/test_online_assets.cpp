//
// OnlineAssets: what the scan fetches from libretro's servers when a network is there - through an external
// command, faked here by a runner that "serves" a few URLs into the output file the command names.
//
#include "doctest/doctest.h"

#include "../support/env_fixture.h"
#include "../support/string_maker.h"
#include "../support/temp_dir.h"

#include "core/services/online_assets.h"

#include <ableem/engine/filesystem.h>
#include <ableem/engine/thumbnail_lookup.h>
#include <ableem/engine/zip_writer.h>

#include <fstream>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

using ableem::DirEntry;
using std::string;
using std::vector;

namespace {

// a fake network: the URLs it has and what they hold; every command line it was asked to run
struct FakeServer {
    std::map<string, string> files;
    vector<string> commands;
    bool down = false;

    // the runner: parses `fetch <url> <out>` (the command template below), writes the file on a hit
    OnlineAssets::CommandRunner runner() {
        return [this](const string &commandLine) {
            commands.push_back(commandLine);
            if (down)
                return 7;
            size_t sp = commandLine.find(' ');
            size_t sp2 = commandLine.find(' ', sp + 1);
            string url = commandLine.substr(sp + 1, sp2 - sp - 1);
            string out = commandLine.substr(sp2 + 1);
            auto it = files.find(url);
            if (it == files.end())
                return 22; // curl's "HTTP error"
            std::ofstream o(out, std::ios::binary);
            o << it->second;
            return 0;
        };
    }
};

OnlineAssets::Config config() {
    OnlineAssets::Config c;
    c.downloadCommand = "fetch %u %o";
    c.thumbnailsBaseUrl = "http://thumbs";
    c.databasesUrl = "http://buildbot/database-rdb.zip";
    return c;
}

const char *const NES = "Nintendo - Nintendo Entertainment System";

} // namespace

TEST_CASE("urlEncode, boxArtUrl and boxArtPath: RetroArch's file name rule, then the URL's") {
    CHECK(OnlineAssets::urlEncode("Adventures of Lolo (USA).png") == "Adventures%20of%20Lolo%20%28USA%29.png");
    CHECK(OnlineAssets::urlEncode("a-b_c.d~e") == "a-b_c.d~e");
    CHECK(
        OnlineAssets::boxArtUrl("http://thumbs/", NES, "Ms. Pac-Man / Galaga") ==
        "http://thumbs/Nintendo%20-%20Nintendo%20Entertainment%20System/Named_Boxarts/Ms.%20Pac-Man%20_%20Galaga.png");
    CHECK(OnlineAssets::boxArtPath("/t", NES, "Ms. Pac-Man / Galaga") ==
          string("/t/") + NES + "/Named_Boxarts/Ms. Pac-Man _ Galaga.png");
    CHECK(OnlineAssets::missingListPath("/t", NES) == string("/t/") + NES + "/Named_Boxarts/.autobleem-missing.txt");
}

TEST_CASE("without a download command nothing is enabled, nothing runs") {
    OnlineAssets::Config c;
    FakeServer server;
    OnlineAssets online(c, server.runner());
    CHECK_FALSE(online.enabled());
    CHECK_FALSE(online.probe());
    CHECK_FALSE(online.fetch("http://x", "y"));
    CHECK(server.commands.empty());
}

TEST_CASE("probe: one request, its answer remembered; a fetch writes the file whole or not at all") {
    TempDir tmp("online");
    EnvFixture env;
    env.setWorkingPath(tmp.path());
    FakeServer server;
    server.files["http://thumbs/"] = "<html>";
    server.files["http://thumbs/a.png"] = "png bytes";
    OnlineAssets online(config(), server.runner());

    CHECK(online.probe());
    CHECK(online.probe());
    CHECK(server.commands.size() == 1); // the second answer came from memory
    CHECK(server.commands[0] == "fetch http://thumbs/ " + tmp.at(".online-probe.part"));
    CHECK_FALSE(DirEntry::exists(tmp.at(".online-probe")));

    CHECK(online.fetch("http://thumbs/a.png", tmp.at("deep/er/a.png")));
    CHECK(tmp.readFile("deep/er/a.png") == "png bytes");
    CHECK_FALSE(online.fetch("http://thumbs/nope.png", tmp.at("nope.png")));
    CHECK_FALSE(DirEntry::exists(tmp.at("nope.png")));
    CHECK_FALSE(DirEntry::exists(tmp.at("nope.png.part")));

    server.down = true;
    CHECK(online.probe());           // still the remembered answer
    CHECK_FALSE(online.probe(true)); // asked afresh
    CHECK_FALSE(online.online());
}

TEST_CASE("fetchBoxArt: fetched once, then already there; a server miss is remembered and never asked again") {
    TempDir tmp("online");
    EnvFixture env;
    env.setWorkingPath(tmp.path());
    FakeServer server;
    server.files["http://thumbs/"] = "<html>";
    server.files[OnlineAssets::boxArtUrl("http://thumbs", NES, "Adventures of Lolo (USA)")] = "png";
    OnlineAssets online(config(), server.runner());
    const string thumbs = tmp.at("thumbnails");

    CHECK(online.fetchBoxArt(thumbs, NES, "Adventures of Lolo (USA)") == OnlineAssets::BoxArt::Fetched);
    CHECK(tmp.readFile(string("thumbnails/") + NES + "/Named_Boxarts/Adventures of Lolo (USA).png") == "png");
    CHECK(online.fetchBoxArt(thumbs, NES, "Adventures of Lolo (USA)") == OnlineAssets::BoxArt::AlreadyThere);

    size_t before = server.commands.size();
    CHECK(online.fetchBoxArt(thumbs, NES, "Homebrew Thing (World)") == OnlineAssets::BoxArt::Missing);
    CHECK(server.commands.size() == before + 2); // the fetch, and the probe that said the server is up
    CHECK(online.fetchBoxArt(thumbs, NES, "Homebrew Thing (World)") == OnlineAssets::BoxArt::Missing);
    CHECK(server.commands.size() == before + 2); // remembered
    std::set<string> missing = OnlineAssets::loadMissingList(OnlineAssets::missingListPath(thumbs, NES));
    CHECK(missing == std::set<string>{"Homebrew Thing (World)"});

    // the network goes away mid-pass: Failed, and nothing marked missing
    server.down = true;
    CHECK(online.fetchBoxArt(thumbs, NES, "Battletoads (USA)") == OnlineAssets::BoxArt::Failed);
    CHECK_FALSE(online.online());
    CHECK(OnlineAssets::loadMissingList(OnlineAssets::missingListPath(thumbs, NES)).size() == 1);
}

TEST_CASE("ensureDatabases: nothing to do with databases there or no network; else the bundle is unpacked") {
    TempDir tmp("online");
    EnvFixture env;
    env.setWorkingPath(tmp.path());
    tmp.makeSubDir("retroarch/database/rdb");

    // the bundle as buildbot packs it: rdb/<system>.rdb
    ableem::ZipWriter zip;
    REQUIRE(zip.open(tmp.at("bundle.zip")));
    REQUIRE(zip.addBytes(string("rdb/") + NES + ".rdb", "RARCHDB"));
    REQUIRE(zip.addBytes("rdb/Atari - 2600.rdb", "RARCHDB"));
    REQUIRE(zip.close());
    FakeServer server;
    server.files["http://thumbs/"] = "<html>";
    server.files["http://buildbot/database-rdb.zip"] = tmp.readFile("bundle.zip");

    {
        FakeServer offline;
        OnlineAssets online(config(), offline.runner());
        CHECK(online.ensureDatabases(tmp.at("retroarch/database/rdb")) == 0);
        CHECK(offline.commands.size() == 1); // the probe only
    }
    OnlineAssets online(config(), server.runner());
    CHECK(online.ensureDatabases(tmp.at("retroarch/database/rdb")) == 2);
    CHECK(DirEntry::exists(tmp.at(string("retroarch/database/rdb/") + NES + ".rdb")));
    CHECK_FALSE(DirEntry::exists(tmp.at("retroarch/database/rdb/database-rdb.zip")));

    size_t before = server.commands.size();
    CHECK(online.ensureDatabases(tmp.at("retroarch/database/rdb")) == 2);
    CHECK(server.commands.size() == before); // they are there: not even a probe
}

TEST_CASE("fetchMissingBoxArt by request: only the covers not there, each arrival reported with its file") {
    TempDir tmp("online");
    EnvFixture env;
    env.setWorkingPath(tmp.path());
    FakeServer server;
    server.files["http://thumbs/"] = "<html>";
    const char *const PSX = ableem::ThumbnailLookup::PlayStationDbName;
    server.files[OnlineAssets::boxArtUrl("http://thumbs", PSX, "Crash Bandicoot (USA)")] = "crash";
    server.files[OnlineAssets::boxArtUrl("http://thumbs", PSX, "Tekken 2 (USA)")] = "tekken";
    OnlineAssets online(config(), server.runner());
    const string thumbs = tmp.at("thumbnails");
    // one is there already, one the server has, one it does not
    tmp.writeFile(string("thumbnails/") + PSX + "/Named_Boxarts/Tekken 2 (USA).png", "old");

    vector<std::pair<string, string>> arrived;
    int missing = -1;
    int fetched = online.fetchMissingBoxArt(
        {{PSX, "Crash Bandicoot (USA)"}, {PSX, "Tekken 2 (USA)"}, {PSX, "Nobody (Nowhere)"}}, thumbs, nullptr, nullptr,
        &missing,
        [&](const OnlineAssets::BoxArtRequest &r, const string &path) { arrived.push_back({r.label, path}); });

    CHECK(fetched == 1);
    CHECK(missing == 1);
    REQUIRE(arrived.size() == 1);
    CHECK(arrived[0].first == "Crash Bandicoot (USA)");
    CHECK(arrived[0].second == OnlineAssets::boxArtPath(thumbs, PSX, "Crash Bandicoot (USA)"));
    CHECK(tmp.readFile(string("thumbnails/") + PSX + "/Named_Boxarts/Crash Bandicoot (USA).png") == "crash");
    CHECK(tmp.readFile(string("thumbnails/") + PSX + "/Named_Boxarts/Tekken 2 (USA).png") == "old"); // untouched
}

TEST_CASE("ps1Requests: the games without any cover, named by the rdb record, else the title") {
    auto game = [](const string &title, const string &record, const string &coverPath, bool pngNextToIt) {
        auto g = std::make_shared<ableem::UsbGame>();
        g->title = title;
        g->recordName = record;
        g->coverPath = coverPath;
        g->coverImageFound = pngNextToIt;
        return g;
    };
    vector<ableem::UsbGamePtr> games = {
        game("Crash Bandicoot", "Crash Bandicoot (USA)", "", false),             // wanted, by record name
        game("Homebrew", "", "", false),                                         // wanted, by title (no rdb match)
        game("Tekken 2", "Tekken 2 (USA)", "/thumbs/Tekken 2 (USA).png", false), // the tree has it
        game("Ridge Racer", "Ridge Racer (USA)", "", true),                      // a PNG next to the game
        game("", "", "", false),                                                 // nothing to name it by
        nullptr,
    };
    vector<OnlineAssets::BoxArtRequest> requests = OnlineAssets::ps1Requests(games);
    REQUIRE(requests.size() == 2);
    CHECK(requests[0].database == string(ableem::ThumbnailLookup::PlayStationDbName));
    CHECK(requests[0].label == "Crash Bandicoot (USA)");
    CHECK(requests[1].label == "Homebrew");
}
