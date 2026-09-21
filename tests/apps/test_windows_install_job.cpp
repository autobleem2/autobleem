// WindowsInstallJob: the Windows product's data tree from a fake download repository, fresh and as an update
#include <doctest/doctest.h>

#include "core/windows_install_job.h"

#include <ableem/engine/filesystem.h>
#include <ableem/engine/sha256.h>
#include <ableem/engine/zip_writer.h>

#include "support/tar_builder.h"
#include "support/temp_dir.h"

#include <map>
#include <string>
#include <vector>

using namespace std;
using ableem::DirEntry;
using ableem::Sha256;
using test_support::TarBuilder;

namespace {

const char *const Site = "http://site";
const char *const Buildbot = "http://buildbot";
const string dataDir = AB_TEST_DATA_DIR;

//******************
// FakeSite
//******************
class FakeSite : public Downloader {
public:
    map<string, string> files;
    vector<string> fetched;
    bool fetch(const string &url, const string &destFile, const Progress &progress, string &error) override {
        fetched.push_back(url);
        auto it = files.find(url);
        if (it == files.end()) {
            error = url + ": HTTP 404";
            return false;
        }
        if (!DirEntry::copyFile(it->second, destFile)) {
            error = "cannot copy " + it->second;
            return false;
        }
        if (progress)
            progress(1, 1);
        return true;
    }
    int count(const string &urlPart) const {
        int n = 0;
        for (const string &u : fetched)
            if (u.find(urlPart) != string::npos)
                n++;
        return n;
    }
};

//******************
// Recorder
//******************
class Recorder : public InstallListener {
public:
    vector<string> phases, lines;
    void onPhase(int, int, const string &title) override { phases.push_back(title); }
    void onProgress(uint64_t, uint64_t) override {}
    void onLine(const string &line) override { lines.push_back(line); }
    bool said(const string &part) const {
        for (const string &l : lines)
            if (l.find(part) != string::npos)
                return true;
        return false;
    }
};

string json(const string &name, const string &path, const string &extra = "") {
    return "{\"name\": \"" + name + "\", \"size\": " + to_string(DirEntry::fileSize(path)) + ", \"sha256\": \"" +
           Sha256::ofFile(path) + "\", \"url\": \"" + Site + "/" + name + "\"" + extra + "}";
}

//******************
// Fixture
//******************
// a program folder with shipped themes, an empty data root, and the site's files for every option
struct Fixture {
    TempDir tmp{"win_install"};
    FakeSite site;
    Recorder out;
    WindowsInstallOptions options;
    string root;

    Fixture() {
        root = tmp.at("Documents/AutoBleem");
        options.dataRoot = root;
        options.programDir = tmp.makeSubDir("Program Files/AutoBleem");
        options.repoUrl = Site;
        options.buildbotUrl = Buildbot;
        options.scratchDir = tmp.makeSubDir("scratch");
        tmp.writeFile("Program Files/AutoBleem/Themes/ab2/theme.json", "{ab2}");
        tmp.writeFile("Program Files/AutoBleem/Themes/ab2/images/bg.png", "png");
        tmp.writeFile("Program Files/AutoBleem/Themes/default/theme.json", "{default}");
        // the cover databases
        for (const char *r : {"J", "U", "P"}) {
            string name = string("covers") + r + ".db";
            tmp.writeFile("site/" + name, "sqlite " + name);
            site.files[string(Site) + "/db/" + name] = tmp.at("site/" + name);
            tmp.writeFile("site/" + name + ".sha256", Sha256::ofFile(tmp.at("site/" + name)) + "  " + name + "\n");
            site.files[string(Site) + "/db/" + name + ".sha256"] = tmp.at("site/" + name + ".sha256");
        }
        // the BIOS list: the two PlayStation files and one of a core's
        string list = "# the list\n";
        for (const char *name : {"scph5501.bin", "scph5500.bin", "disksys.rom"}) {
            tmp.writeFile(string("site/") + name, string("bios ") + name);
            site.files[string(Site) + "/bios/" + name] = tmp.at(string("site/") + name);
            list += Sha256::ofFile(tmp.at(string("site/") + name)) + " " +
                    to_string(DirEntry::fileSize(tmp.at(string("site/") + name))) + " " + Site + "/bios/" + name + " " +
                    (string(name) == "disksys.rom" ? "Nintendo - Famicom Disk System/" : "") + name + "\n";
        }
        tmp.writeFile("site/biospack.txt", list);
        site.files[string(Site) + "/win/bios/biospack.txt"] = tmp.at("site/biospack.txt");
        {
            string j = json("biospack.txt", tmp.at("site/biospack.txt"), ", \"count\": 3, \"total_bytes\": 48");
            size_t at = j.find("http://site/biospack.txt");
            j.replace(at, strlen("http://site/biospack.txt"), string(Site) + "/win/bios/biospack.txt");
            tmp.writeFile("site/bios.json", j);
        }
        site.files[string(Site) + "/win/bios/latest.json"] = tmp.at("site/bios.json");
        // the samples
        {
            TarBuilder b;
            b.file("Games/Tetrade/Tetrade.cue", "cue").file("SAMPLES.md", "# samples\n");
            b.file("RetroArch/roms/Nintendo - Nintendo Entertainment System/Nova.nes", "nes");
            b.file("RetroArch/thumbnails/Nintendo - Nintendo Entertainment System/Named_Boxarts/Nova.png", "png");
            REQUIRE(b.writeTarGz(tmp.at("site/samples-20260920.tar.gz")));
            tmp.writeFile("site/samples.json", json("samples-20260920.tar.gz", tmp.at("site/samples-20260920.tar.gz"),
                                                    ", \"date\": \"20260920\""));
            site.files[string(Site) + "/samples/latest.json"] = tmp.at("site/samples.json");
            site.files[string(Site) + "/samples-20260920.tar.gz"] = tmp.at("site/samples-20260920.tar.gz");
        }
    }

    // the site's own RetroArch build and cores pack
    void siteRetroArch() {
        REQUIRE(TarBuilder()
                    .file("retroarch.exe", "MZ retroarch")
                    .file("assets/xmb/x.png", "png")
                    .file("info/snes9x_libretro.info", "info")
                    .writeTarGz(tmp.at("site/retroarch-win64-1.22.2.tar.gz")));
        tmp.writeFile("site/retroarch.json",
                      json("retroarch-win64-1.22.2.tar.gz", tmp.at("site/retroarch-win64-1.22.2.tar.gz"),
                           ", \"version\": \"1.22.2\""));
        site.files[string(Site) + "/win/retroarch/latest.json"] = tmp.at("site/retroarch.json");
        site.files[string(Site) + "/retroarch-win64-1.22.2.tar.gz"] = tmp.at("site/retroarch-win64-1.22.2.tar.gz");
        REQUIRE(TarBuilder()
                    .file("cores/snes9x_libretro.dll", "core")
                    .file("info/snes9x_libretro.info", "info")
                    .writeTarGz(tmp.at("site/cores-win64-20260920.tar.gz")));
        tmp.writeFile("site/cores.json", json("cores-win64-20260920.tar.gz", tmp.at("site/cores-win64-20260920.tar.gz"),
                                              ", \"date\": \"20260920\", \"count\": 1"));
        site.files[string(Site) + "/win/cores/latest.json"] = tmp.at("site/cores.json");
        site.files[string(Site) + "/cores-win64-20260920.tar.gz"] = tmp.at("site/cores-win64-20260920.tar.gz");
    }

    // libretro's own: the official archive (tests/data/test_bcj2.7z stands in - a real 7z, but with none of
    // RetroArch-Win64/retroarch.exe in it, which the job must notice) and the per-core index
    void buildbotRetroArch() {
        site.files[string(Buildbot) + "/stable/1.22.2/windows/x86_64/RetroArch.7z"] = dataDir + "/test_bcj2.7z";
        tmp.writeFile("site/index",
                      "2026-09-20 abcdef01 snes9x_libretro.dll.zip\n2026-09-20 abcdef02 gone_libretro.dll.zip\n");
        site.files[string(Buildbot) + "/nightly/windows/x86_64/latest/.index-extended"] = tmp.at("site/index");
        ableem::ZipWriter zip;
        REQUIRE(zip.open(tmp.at("site/snes9x_libretro.dll.zip")));
        zip.addBytes("snes9x_libretro.dll", "core");
        REQUIRE(zip.close());
        site.files[string(Buildbot) + "/nightly/windows/x86_64/latest/snes9x_libretro.dll.zip"] =
            tmp.at("site/snes9x_libretro.dll.zip");
    }

    bool run(string &error) {
        return WindowsInstallJob::run(options, site, out, []() { return false; }, error);
    }
    bool has(const string &rel) const { return DirEntry::exists(root + "/" + rel); }
};

} // namespace

TEST_CASE("a fresh install: the data tree, the shipped themes and the three cover databases") {
    Fixture fx;
    WindowsInstallInfo before = WindowsInstallJob::inspect(fx.options);
    CHECK(before.exists);
    CHECK_FALSE(before.installed);
    CHECK(WindowsInstallJob::phasesFor(fx.options, before) ==
          vector<string>{"Data folder", "Cover databases", "Finishing"});

    string error;
    REQUIRE_MESSAGE(fx.run(error), error);
    CHECK(fx.out.phases == vector<string>{"Data folder", "Cover databases", "Finishing"});
    for (const char *d :
         {"Games", "System/Databases", "System/Logs", "System/Bios", "Themes", "RetroArch/roms", "Apps"})
        CHECK(DirEntry::isDirectory(fx.root + "/" + d));
    CHECK(fx.tmp.readFile("Documents/AutoBleem/Themes/ab2/theme.json") == "{ab2}");
    CHECK(fx.has("Themes/ab2/images/bg.png"));
    CHECK(fx.has("Themes/default/theme.json"));
    CHECK(fx.out.said("2 themes copied in"));
    for (const char *r : {"J", "U", "P"})
        CHECK(fx.tmp.readFile(string("Documents/AutoBleem/System/Databases/covers") + r + ".db") ==
              string("sqlite covers") + r + ".db");
    CHECK_FALSE(fx.has("System/Install"));
    CHECK(fx.out.said("Installed."));
}

TEST_CASE("an update keeps the user's settings and themes, removes the scan fingerprints") {
    Fixture fx;
    fx.tmp.writeFile("Documents/AutoBleem/System/config.ini", "theme=aergb\n");
    fx.tmp.writeFile("Documents/AutoBleem/System/games.fingerprint", "old");
    fx.tmp.writeFile("Documents/AutoBleem/System/roms.fingerprint", "old");
    fx.tmp.writeFile("Documents/AutoBleem/Themes/ab2/theme.json", "{edited}");
    fx.tmp.writeFile("Documents/AutoBleem/Games/Tekken 3/Tekken 3.cue", "cue");
    fx.tmp.writeFile("Documents/AutoBleem/System/Databases/coversU.db", "sqlite coversU.db");
    fx.options.update = true;
    fx.options.coversJapan = fx.options.coversPal = false;

    WindowsInstallInfo before = WindowsInstallJob::inspect(fx.options);
    CHECK(before.installed);
    CHECK(before.hasCovers[1]);
    string error;
    REQUIRE_MESSAGE(fx.run(error), error);
    {
        // the settings kept, the PS1 emulator every install lands on set (capitalised: the launcher's writer)
        const string cfg = fx.tmp.readFile("Documents/AutoBleem/System/config.ini");
        CHECK(cfg.find("Theme=aergb") != string::npos);
        CHECK(cfg.find("Emulator=pcsx-abnxt") != string::npos);
    }
    CHECK(fx.tmp.readFile("Documents/AutoBleem/Themes/ab2/theme.json") == "{edited}");
    CHECK(fx.has("Themes/default/theme.json")); // a shipped theme not there yet still comes in
    CHECK(fx.has("Games/Tekken 3/Tekken 3.cue"));
    CHECK_FALSE(fx.has("System/games.fingerprint"));
    CHECK_FALSE(fx.has("System/roms.fingerprint"));
    CHECK(fx.out.said("coversU.db is already there"));
    CHECK(fx.site.count("coversJ.db") == 0);
    CHECK(fx.out.said("Updated."));
}

TEST_CASE("RetroArch and its cores from the download repository, then the BIOS files and the samples") {
    Fixture fx;
    fx.siteRetroArch();
    fx.options.retroarch = true;
    fx.options.bios = true;
    fx.options.samples = true;
    WindowsInstallInfo before = WindowsInstallJob::inspect(fx.options);
    CHECK(WindowsInstallJob::phasesFor(fx.options, before) ==
          vector<string>{"Data folder", "Cover databases", "RetroArch", "RetroArch cores", "BIOS files", "Sample games",
                         "Finishing"});

    string error;
    REQUIRE_MESSAGE(fx.run(error), error);
    CHECK(fx.tmp.readFile("Documents/AutoBleem/RetroArch/bin/retroarch.exe") == "MZ retroarch");
    CHECK(fx.tmp.readFile("Documents/AutoBleem/RetroArch/bin/VERSION") == "1.22.2\n");
    CHECK(fx.has("RetroArch/bin/assets/xmb/x.png"));
    CHECK(fx.has("RetroArch/bin/cores/snes9x_libretro.dll"));
    CHECK(fx.has("RetroArch/bin/info/snes9x_libretro.info"));
    for (const char *d : {"playlists", "saves", "states", "system", "thumbnails"})
        CHECK(DirEntry::isDirectory(fx.root + "/RetroArch/bin/" + d));
    string cfg = fx.tmp.readFile("Documents/AutoBleem/RetroArch/bin/retroarch.cfg");
    CHECK(cfg.find("video_fullscreen = \"true\"") != string::npos);
    CHECK(cfg.find("quit_on_close_content = \"2\"") != string::npos);
    CHECK(cfg.find("rgui_browser_directory = \"") != string::npos);
    CHECK(cfg.find("RetroArch\\roms\"") != string::npos);
    // the whole pack into RetroArch's own system dir, and the PlayStation pair under the emulator's names
    CHECK(fx.tmp.readFile("Documents/AutoBleem/RetroArch/bin/system/scph5501.bin") == "bios scph5501.bin");
    CHECK(fx.has("RetroArch/bin/system/Nintendo - Famicom Disk System/disksys.rom"));
    CHECK(fx.out.said("3 fetched, 0 already there, 0 failed"));
    CHECK(fx.tmp.readFile("Documents/AutoBleem/System/Bios/romw.bin") == "bios scph5501.bin");
    CHECK(fx.tmp.readFile("Documents/AutoBleem/System/Bios/romJP.bin") == "bios scph5500.bin");
    // the samples, RetroArch's part laid out for the Windows tree
    CHECK(fx.has("Games/Tetrade/Tetrade.cue"));
    CHECK(fx.has("SAMPLES.md"));
    CHECK(fx.has("RetroArch/roms/Nintendo - Nintendo Entertainment System/Nova.nes"));
    CHECK(fx.has("RetroArch/bin/thumbnails/Nintendo - Nintendo Entertainment System/Named_Boxarts/Nova.png"));
    CHECK_FALSE(fx.has("RetroArch/thumbnails"));
    CHECK(fx.tmp.readFile("Documents/AutoBleem/System/samples.txt") == "samples-20260920.tar.gz\n");
    CHECK(fx.site.count("buildbot") == 0);

    // again: RetroArch is there at that version, the cfg is kept, the samples are not put back
    Fixture again;
    again.siteRetroArch();
    again.options = fx.options;
    fx.tmp.writeFile("Documents/AutoBleem/RetroArch/bin/retroarch.cfg", cfg + "video_smooth = \"true\"\n");
    DirEntry::removeDirAndContents(fx.root + "/Games/Tetrade");
    REQUIRE_MESSAGE(WindowsInstallJob::run(again.options, again.site, again.out, []() { return false; }, error), error);
    CHECK(again.out.said("RetroArch 1.22.2 is installed already"));
    CHECK(again.out.said("keeping the existing retroarch.cfg"));
    CHECK(again.out.said("not again"));
    CHECK_FALSE(fx.has("Games/Tetrade"));
    CHECK(again.site.count("retroarch-win64") == 0);
    CHECK(fx.tmp.readFile("Documents/AutoBleem/RetroArch/bin/retroarch.cfg").find("video_smooth") != string::npos);
}

TEST_CASE("without a build on the site, libretro's own archive and one zip per core") {
    Fixture fx;
    fx.buildbotRetroArch();
    fx.options.retroarch = true;
    string error;
    // the stand-in archive has no RetroArch-Win64/retroarch.exe: the job says so instead of going on
    CHECK_FALSE(fx.run(error));
    CHECK(error.find("no retroarch.exe") != string::npos);
    CHECK(fx.out.said("taking libretro's RetroArch.7z"));
    CHECK(fx.site.count("stable/1.22.2/windows/x86_64/RetroArch.7z") == 1);

    // the cores' fallback on its own, with RetroArch "installed" by hand
    Fixture cores;
    cores.buildbotRetroArch();
    cores.tmp.writeFile("Documents/AutoBleem/RetroArch/bin/retroarch.exe", "MZ");
    cores.tmp.writeFile("Documents/AutoBleem/RetroArch/bin/VERSION", "1.22.2\n");
    cores.options.retroarch = true;
    REQUIRE_MESSAGE(cores.run(error), error);
    CHECK(cores.out.said("RetroArch 1.22.2 is installed already"));
    CHECK(cores.out.said("taking them one by one from libretro"));
    CHECK(cores.tmp.readFile("Documents/AutoBleem/RetroArch/bin/cores/snes9x_libretro.dll") == "core");
    CHECK(cores.out.said("could not fetch gone_libretro.dll.zip"));
    CHECK(cores.out.said("1 fetched, 1 failed"));
}

TEST_CASE("without RetroArch the BIOS step fetches the PlayStation files alone; the samples skip the other systems") {
    Fixture fx;
    fx.options.bios = true;
    fx.options.samples = true;
    fx.options.coversJapan = fx.options.coversUsa = fx.options.coversPal = false;
    // the user's own NTSC-U BIOS is there already: kept, the Japanese one filled in
    fx.tmp.writeFile("Documents/AutoBleem/System/Bios/romw.bin", "mine");
    WindowsInstallInfo before = WindowsInstallJob::inspect(fx.options);
    CHECK(WindowsInstallJob::phasesFor(fx.options, before) ==
          vector<string>{"Data folder", "BIOS files", "Sample games", "Finishing"});
    string error;
    REQUIRE_MESSAGE(fx.run(error), error);
    CHECK(fx.out.said("PlayStation only"));
    CHECK(fx.has("RetroArch/bin/system/scph5501.bin"));
    CHECK(fx.has("RetroArch/bin/system/scph5500.bin"));
    CHECK_FALSE(fx.has("RetroArch/bin/system/Nintendo - Famicom Disk System/disksys.rom"));
    CHECK(fx.out.said("2 fetched, 0 already there, 0 failed"));
    CHECK(fx.tmp.readFile("Documents/AutoBleem/System/Bios/romw.bin") == "mine");
    CHECK(fx.out.said("keeping the existing romw.bin"));
    CHECK(fx.tmp.readFile("Documents/AutoBleem/System/Bios/romJP.bin") == "bios scph5500.bin");
    CHECK(fx.has("Games/Tetrade/Tetrade.cue"));
    CHECK_FALSE(fx.has("RetroArch/roms/Nintendo - Nintendo Entertainment System/Nova.nes"));
    CHECK(fx.out.said("need RetroArch"));
}

TEST_CASE("a stop request and a missing site are reported") {
    Fixture fx;
    string error;
    bool stopNow = true;
    CHECK_FALSE(WindowsInstallJob::run(fx.options, fx.site, fx.out, [&]() { return stopNow; }, error));
    CHECK(error == "Stopped");

    Fixture down;
    down.site.files.clear();
    CHECK_FALSE(down.run(error));
    CHECK(error.find("404") != string::npos);
}

TEST_CASE("retroArchConfigText names the roms folder the Windows way") {
    string cfg = WindowsInstallJob::retroArchConfigText("C:/Users/me/Documents/AutoBleem");
    CHECK(cfg.find("rgui_browser_directory = \"C:\\Users\\me\\Documents\\AutoBleem\\RetroArch\\roms\"") !=
          string::npos);
}
