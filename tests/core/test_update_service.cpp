//
// UpdateService: the online update check and download against a fake site - the catalog JSON as
// tools/repo_index.py writes it, served by a runner that "fetches" URLs into the file the command names.
// Plus the engine pieces it stands on: Sha256 and the catalog/state files.
//
#include "doctest/doctest.h"

#include "../support/string_maker.h"
#include "../support/temp_dir.h"

#include "core/services/update_service.h"

#include <ableem/engine/filesystem.h>
#include <ableem/engine/sha256.h>
#include <ableem/engine/update_catalog.h>

#include <chrono>
#include <fstream>
#include <map>
#include <string>
#include <thread>
#include <vector>

using ableem::DirEntry;
using ableem::ReleaseCatalog;
using ableem::RetroArchCatalog;
using ableem::Sha256;
using std::string;
using std::vector;

namespace {

const char *ReleaseJson = R"({
  "version": "v2.0.0-pre0-df68521",
  "prerelease": true,
  "date": "2026-09-20",
  "files": {
    "rpi64": {"name": "autobleem-rpi-arm64.tar.gz", "size": 5, "sha256": "SHA64",
              "url": "http://site/releases/v2.0.0-pre0-df68521/autobleem-rpi-arm64.tar.gz"},
    "rpi": {"name": "autobleem-rpi.tar.gz", "size": 5, "sha256": "SHA32",
            "url": "http://site/releases/v2.0.0-pre0-df68521/autobleem-rpi.tar.gz"}
  },
  "other_files": []
})";

const char *RetroArchJson = R"({
  "version": "v1.22.2",
  "arm64": {"name": "retroarch-v1.22.2-arm64.tar.gz", "size": 3, "sha256": "RA64",
            "url": "http://site/rpi/retroarch/v1.22.2/retroarch-v1.22.2-arm64.tar.gz"},
  "armhf": {"name": "retroarch-v1.22.2-armhf.tar.gz", "size": 3, "sha256": "RA32",
            "url": "http://site/rpi/retroarch/v1.22.2/retroarch-v1.22.2-armhf.tar.gz"}
})";

// a fake site: URL -> body; the runner parses `fetch <url> <out>` and writes the body
struct FakeSite {
    std::map<string, string> files;
    vector<string> commands;
    bool down = false;

    // how many fetches of this URL the service made
    int fetched(const string &url) const {
        int n = 0;
        for (const string &c : commands)
            if (c.find(" " + url + " ") != string::npos)
                n++;
        return n;
    }

    UpdateService::CommandRunner runner() {
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
                return 22;
            std::ofstream o(out, std::ios::binary);
            o << it->second;
            return 0;
        };
    }
};

UpdateService::Config config(const TempDir &tmp, const string &channel = "latest") {
    UpdateService::Config c;
    c.repoUrl = "http://site";
    c.channel = channel;
    c.platformKey = "rpi";
    c.arch = "armhf";
    c.retroarchCatalog = "rpi/retroarch/latest.json";
    c.installedStable = "v2.0.0-pre0";
    c.installedVersion = "v2.0.0-pre0-83fe6d1";
    c.installedRetroArch = "v1.22.1";
    c.fetchCommand = "fetch %u %o";
    c.downloadCommand = "fetch %u %o";
    c.stateFile = tmp.path() + "/System/update.json";
    c.updatesDir = tmp.path() + "/System/Updates";
    return c;
}

// the worker is a thread: poll until it lands
UpdateService::Status waitFor(UpdateService &service) {
    for (int i = 0; i < 500; i++) {
        UpdateService::Status s = service.poll();
        if (s.phase != UpdateService::Phase::Checking && s.phase != UpdateService::Phase::Downloading)
            return s;
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    return service.poll();
}

} // namespace

TEST_CASE("Sha256: the known vectors, and a file") {
    CHECK(Sha256::ofString("") == "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
    CHECK(Sha256::ofString("abc") == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
    CHECK(Sha256::ofString("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq") ==
          "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1");
    // a million 'a': crosses many blocks, exercises the buffering
    string million(1000000, 'a');
    CHECK(Sha256::ofString(million) == "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0");
    TempDir tmp("sha256");
    tmp.writeFile("f.bin", "abc");
    CHECK(Sha256::ofFile(tmp.path() + "/f.bin") == Sha256::ofString("abc"));
    CHECK(Sha256::ofFile(tmp.path() + "/missing") == "");
}

TEST_CASE("ReleaseCatalog and RetroArchCatalog read what repo_index.py writes") {
    ReleaseCatalog release;
    REQUIRE(release.parse(ReleaseJson));
    CHECK(release.version == "v2.0.0-pre0-df68521");
    CHECK(release.prerelease);
    CHECK(release.date == "2026-09-20");
    REQUIRE(release.fileFor("rpi") != nullptr);
    CHECK(release.fileFor("rpi")->name == "autobleem-rpi.tar.gz");
    CHECK(release.fileFor("rpi")->size == 5);
    CHECK(release.fileFor("rpi")->sha256 == "SHA32");
    CHECK(release.fileFor("psc") == nullptr);
    CHECK_FALSE(release.parse("not json"));
    CHECK_FALSE(release.parse("{\"files\": {}}")); // no version

    RetroArchCatalog ra;
    REQUIRE(ra.parse(RetroArchJson));
    CHECK(ra.version == "v1.22.2");
    REQUIRE(ra.fileFor("arm64") != nullptr);
    CHECK(ra.fileFor("arm64")->url == "http://site/rpi/retroarch/v1.22.2/retroarch-v1.22.2-arm64.tar.gz");
    CHECK(ra.fileFor("x86") == nullptr);
}

TEST_CASE("UpdateState and PendingUpdate round-trip through their files") {
    TempDir tmp("update-state");
    ableem::UpdateState state;
    state.lastCheck = 1700000000;
    state.postponedUntil = 1700086400;
    state.skippedVersion = "v2.0.0-pre0-abc1234";
    REQUIRE(state.save(tmp.path() + "/update.json"));
    ableem::UpdateState back;
    REQUIRE(back.load(tmp.path() + "/update.json"));
    CHECK(back.lastCheck == 1700000000);
    CHECK(back.postponedUntil == 1700086400);
    CHECK(back.skippedVersion == "v2.0.0-pre0-abc1234");
    CHECK(back.skippedRetroArch == "");
    CHECK_FALSE(DirEntry::exists(tmp.path() + "/update.json.tmp"));
    CHECK_FALSE(ableem::UpdateState().load(tmp.path() + "/none.json"));

    ableem::PendingUpdate pending;
    pending.autobleemVersion = "v2";
    pending.autobleemFile = "autobleem-rpi.tar.gz";
    REQUIRE(pending.save(tmp.path() + "/pending.json"));
    ableem::PendingUpdate p2;
    REQUIRE(p2.load(tmp.path() + "/pending.json"));
    CHECK(p2.autobleemFile == "autobleem-rpi.tar.gz");
    CHECK(p2.retroarchFile == "");
}

TEST_CASE("UpdateService::compare: what counts as newer") {
    TempDir tmp("update-compare");
    ReleaseCatalog release;
    REQUIRE(release.parse(ReleaseJson));
    RetroArchCatalog ra;
    REQUIRE(ra.parse(RetroArchJson));

    SUBCASE("latest channel: the tag-hash differs -> AutoBleem update; RetroArch stamp differs -> update") {
        UpdateInfo info = UpdateService::compare(config(tmp), &release, &ra);
        CHECK(info.autobleemVersion == "v2.0.0-pre0-df68521");
        CHECK(info.autobleem.name == "autobleem-rpi.tar.gz");
        CHECK(info.retroarchVersion == "v1.22.2");
        CHECK(info.retroarch.sha256 == "RA32");
        CHECK(info.any());
    }
    SUBCASE("the installed build is the site's -> nothing") {
        UpdateService::Config c = config(tmp);
        c.installedVersion = "v2.0.0-pre0-df68521";
        c.installedRetroArch = "v1.22.2";
        CHECK_FALSE(UpdateService::compare(c, &release, &ra).any());
    }
    SUBCASE("stable channel compares the tag alone") {
        UpdateService::Config c = config(tmp, "stable");
        c.installedStable = "v2.0.0-pre0-df68521"; // what the stable list would say
        UpdateInfo info = UpdateService::compare(c, &release, nullptr);
        CHECK(info.autobleemVersion == "");
        c.installedStable = "v1.9.0";
        CHECK(UpdateService::compare(c, &release, nullptr).autobleemVersion == "v2.0.0-pre0-df68521");
    }
    SUBCASE("no package for this platform -> no AutoBleem update") {
        UpdateService::Config c = config(tmp);
        c.platformKey = "psc";
        CHECK(UpdateService::compare(c, &release, &ra).autobleemVersion == "");
    }
    SUBCASE("no RetroArch installed, or no build for the architecture -> no RetroArch update") {
        UpdateService::Config c = config(tmp);
        c.installedRetroArch = "";
        CHECK(UpdateService::compare(c, &release, &ra).retroarchVersion == "");
        c = config(tmp);
        c.arch = "x86";
        CHECK(UpdateService::compare(c, &release, &ra).retroarchVersion == "");
    }
    CHECK(UpdateService::channelFile("latest") == "releases/unstable.json");
    CHECK(UpdateService::channelFile("stable") == "releases/latest.json");
    CHECK(UpdateService::commandFor("curl -o \"%o\" \"%u\"", "http://x/y", "/tmp/out") ==
          "curl -o \"/tmp/out\" \"http://x/y\"");
}

TEST_CASE("UpdateService: RetroArch's catalog is wherever the platform says, or nowhere") {
    TempDir tmp("update-catalog");
    FakeSite site;
    site.files["http://site/releases/unstable.json"] = ReleaseJson;
    // the PC stick's builds are listed under pc/, keyed by its architecture
    site.files["http://site/pc/retroarch/latest.json"] =
        "{\"version\": \"v1.22.2\", \"i386\": {\"name\": \"retroarch-v1.22.2-i386.tar.gz\", \"size\": 3, "
        "\"sha256\": \"RAX86\", \"url\": \"http://site/pc/retroarch/v1.22.2/retroarch-v1.22.2-i386.tar.gz\"}}";

    SUBCASE("a pcusb build reads pc/retroarch/latest.json") {
        UpdateService::Config c = config(tmp);
        c.platformKey = "pcusb";
        c.arch = "i386";
        c.retroarchCatalog = "pc/retroarch/latest.json";
        UpdateService service(site.runner());
        service.configure(c);
        service.startCheck(1000);
        UpdateService::Status s = waitFor(service);
        CHECK(s.phase == UpdateService::Phase::Checked);
        CHECK(s.info.retroarchVersion == "v1.22.2");
        CHECK(s.info.retroarch.sha256 == "RAX86");
        CHECK(site.fetched("http://site/pc/retroarch/latest.json") == 1);
        CHECK(site.fetched("http://site/rpi/retroarch/latest.json") == 0);
    }

    SUBCASE("no catalog (Windows: RetroArch updates itself) - no RetroArch check, AutoBleem's still made") {
        UpdateService::Config c = config(tmp);
        c.retroarchCatalog.clear();
        UpdateService service(site.runner());
        service.configure(c);
        service.startCheck(1000);
        UpdateService::Status s = waitFor(service);
        CHECK(s.phase == UpdateService::Phase::Checked);
        CHECK(s.info.autobleemVersion == "v2.0.0-pre0-df68521");
        CHECK(s.info.retroarchVersion.empty());
        CHECK(site.fetched("http://site/rpi/retroarch/latest.json") == 0);
    }
}

TEST_CASE("UpdateService: the check against the site, and what the user's answers do") {
    TempDir tmp("update-check");
    FakeSite site;
    site.files["http://site/releases/unstable.json"] = ReleaseJson;
    site.files["http://site/rpi/retroarch/latest.json"] = RetroArchJson;
    UpdateService service(site.runner());
    service.configure(config(tmp));
    REQUIRE(service.enabled());

    SUBCASE("due when never checked; the check finds both updates and asks; a day later it is due again") {
        CHECK(service.checkDue(1000));
        service.startCheck(1000);
        CHECK_FALSE(service.checkDue(1000)); // in flight
        UpdateService::Status s = waitFor(service);
        CHECK(s.phase == UpdateService::Phase::Checked);
        CHECK(s.checkedThisPoll);
        CHECK(s.info.autobleemVersion == "v2.0.0-pre0-df68521");
        CHECK(s.info.retroarchVersion == "v1.22.2");
        CHECK(service.shouldPrompt(1000));
        CHECK_FALSE(service.poll().checkedThisPoll); // reported once
        CHECK_FALSE(service.checkDue(1000 + 3600));
        CHECK(service.checkDue(1000 + UpdateService::CheckInterval));
        // the state file remembers the check
        ableem::UpdateState state;
        REQUIRE(state.load(tmp.path() + "/System/update.json"));
        CHECK(state.lastCheck == 1000);
    }
    SUBCASE("remind me tomorrow: no prompt for a day") {
        service.startCheck(1000);
        waitFor(service);
        service.postpone(1000);
        CHECK_FALSE(service.shouldPrompt(1000 + 3600));
        CHECK(service.shouldPrompt(1000 + UpdateService::CheckInterval));
    }
    SUBCASE("skip this version: no prompt for these versions, a prompt for a newer one") {
        service.startCheck(1000);
        waitFor(service);
        service.skip(1000);
        CHECK_FALSE(service.shouldPrompt(1000 + UpdateService::CheckInterval * 2));
        // a fresh service reads the skip back from the file
        UpdateService again(site.runner());
        again.configure(config(tmp));
        CHECK(again.state().skippedVersion == "v2.0.0-pre0-df68521");
        CHECK(again.state().skippedRetroArch == "v1.22.2");
        // the site moves on: a new pre-release is asked about
        site.files["http://site/releases/unstable.json"] =
            string(ReleaseJson).replace(string(ReleaseJson).find("df68521"), 7, "1234567");
        again.startCheck(2000);
        UpdateService::Status s = waitFor(again);
        CHECK(s.info.autobleemVersion == "v2.0.0-pre0-1234567");
        CHECK(again.shouldPrompt(2000));
    }
    SUBCASE("the site is down: Failed, nothing to ask") {
        site.down = true;
        service.startCheck(1000);
        UpdateService::Status s = waitFor(service);
        CHECK(s.phase == UpdateService::Phase::Failed);
        CHECK_FALSE(s.info.any());
        CHECK_FALSE(service.shouldPrompt(1000));
    }
    SUBCASE("latest channel with no pre-release on the site falls back to the stable list") {
        site.files.erase("http://site/releases/unstable.json");
        site.files["http://site/releases/latest.json"] =
            string(ReleaseJson).replace(string(ReleaseJson).find("v2.0.0-pre0-df68521"), 19, "v2.1.0");
        service.startCheck(1000);
        UpdateService::Status s = waitFor(service);
        CHECK(s.phase == UpdateService::Phase::Checked);
        CHECK(s.info.autobleemVersion == "v2.1.0");
    }
    SUBCASE("off: nothing is ever due") {
        service.configure(config(tmp, "off"));
        CHECK_FALSE(service.enabled());
        CHECK_FALSE(service.checkDue(1000));
    }
}

TEST_CASE("UpdateService: the download lands in Updates/ verified, with pending.json for the installer") {
    TempDir tmp("update-download");
    FakeSite site;
    // the bodies, with the real sums in the catalog
    const string abBody = "ABPKG", raBody = "RA!";
    string releaseJson = ReleaseJson, raJson = RetroArchJson;
    releaseJson.replace(releaseJson.find("SHA32"), 5, Sha256::ofString(abBody));
    raJson.replace(raJson.find("RA32"), 4, Sha256::ofString(raBody));
    site.files["http://site/releases/unstable.json"] = releaseJson;
    site.files["http://site/rpi/retroarch/latest.json"] = raJson;
    site.files["http://site/releases/v2.0.0-pre0-df68521/autobleem-rpi.tar.gz"] = abBody;
    site.files["http://site/rpi/retroarch/v1.22.2/retroarch-v1.22.2-armhf.tar.gz"] = raBody;
    UpdateService service(site.runner());
    service.configure(config(tmp));
    service.startCheck(1000);
    REQUIRE(waitFor(service).info.any());

    SUBCASE("both files, checked, and the pending file") {
        service.startDownload();
        UpdateService::Status s = waitFor(service);
        CHECK(s.phase == UpdateService::Phase::Downloaded);
        CHECK(s.bytesTotal == 8);
        CHECK(s.bytesDone == 8);
        CHECK(tmp.readFile("System/Updates/autobleem-rpi.tar.gz") == abBody);
        CHECK(tmp.readFile("System/Updates/retroarch-v1.22.2-armhf.tar.gz") == raBody);
        ableem::PendingUpdate pending;
        REQUIRE(pending.load(tmp.path() + "/System/Updates/pending.json"));
        CHECK(pending.autobleemVersion == "v2.0.0-pre0-df68521");
        CHECK(pending.autobleemFile == "autobleem-rpi.tar.gz");
        CHECK(pending.retroarchVersion == "v1.22.2");
        CHECK(pending.retroarchFile == "retroarch-v1.22.2-armhf.tar.gz");
        CHECK_FALSE(DirEntry::exists(tmp.path() + "/System/Updates/autobleem-rpi.tar.gz.part"));
    }
    SUBCASE("a wrong sum is a failure, and nothing is left behind") {
        site.files["http://site/rpi/retroarch/v1.22.2/retroarch-v1.22.2-armhf.tar.gz"] = "tampered";
        service.startDownload();
        UpdateService::Status s = waitFor(service);
        CHECK(s.phase == UpdateService::Phase::Failed);
        CHECK(s.error == "the RetroArch build could not be downloaded");
        CHECK_FALSE(DirEntry::exists(tmp.path() + "/System/Updates/retroarch-v1.22.2-armhf.tar.gz"));
        CHECK_FALSE(DirEntry::exists(tmp.path() + "/System/Updates/pending.json"));
    }
    SUBCASE("a file already downloaded and right is not fetched again") {
        tmp.makeSubDir("System/Updates");
        tmp.writeFile("System/Updates/autobleem-rpi.tar.gz", abBody);
        size_t before = site.commands.size();
        service.startDownload();
        REQUIRE(waitFor(service).phase == UpdateService::Phase::Downloaded);
        CHECK(site.commands.size() == before + 1); // only RetroArch
    }
}
