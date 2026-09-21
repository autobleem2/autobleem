// InstallerJob: a stick from a package and a fake download repository, fresh and as an update
#include <doctest/doctest.h>

#include "core/installer_job.h"

#include <ableem/engine/filesystem.h>
#include <ableem/engine/sha256.h>
#include <ableem/engine/zip_writer.h>

#include "support/tar_builder.h"
#include "support/temp_dir.h"

#include <algorithm>
#include <cstring>
#include <map>
#include <string>
#include <vector>

using namespace std;
using ableem::DirEntry;
using ableem::Sha256;
using test_support::TarBuilder;

namespace {

const char *const Site = "http://site";

//******************
// FakeSite
//******************
// a Downloader over files in a temp tree: url -> path; a url not there is a 404
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

// the package: what autobleem-psc-<v>.tar.gz carries, in miniature
string makePackage(TempDir &tmp, const string &version) {
    TarBuilder b;
    b.dir("./Autobleem").dir("./Autobleem/bin").dir("./Autobleem/bin/autobleem");
    b.file("./Autobleem/bin/autobleem/autobleem-gui", "ELF " + version, 0755);
    b.file("./Autobleem/bin/autobleem/config.ini", "theme=ab2\nlanguage=English\n");
    b.file("./Autobleem/bin/emu/pcsx-ab", "ELF pcsx", 0755);
    b.file("./Autobleem/rc/launch.sh", "#!/bin/sh\n", 0755);
    b.file("./Autobleem/lib/libs.tar.gz", "gz");
    b.file("./Autobleem/start.sh", "#!/bin/sh\n", 0755);
    b.dir("./Apps").file("./Apps/pscbios/pscbios", "ELF", 0755).file("./Apps/pscbios/app.ini", "Title=BIOS\n");
    b.dir("./Themes")
        .dir("./Themes/ab2")
        .file("./Themes/ab2/theme.json", "{}")
        .dir("./Themes/default")
        .file("./Themes/default/theme.json", "{}");
    b.dir("./Games").dir("./System").dir("./Docs").file("./Docs/readme.txt", "hi");
    b.file("./VERSION", version + "\n");
    string path = tmp.at("pkg/autobleem-psc-" + version + ".tar.gz");
    DirEntry::createDirs(tmp.at("pkg"));
    REQUIRE(b.writeTarGz(path));
    return path;
}

string json(const string &name, const string &path, const string &extra = "") {
    return "{\"name\": \"" + name + "\", \"size\": " + to_string(DirEntry::fileSize(path)) + ", \"sha256\": \"" +
           Sha256::ofFile(path) + "\", \"url\": \"" + Site + "/" + name + "\"" + extra + "}";
}

//******************
// Fixture
//******************
// a stick, a package, and the site's files for every option
struct Fixture {
    TempDir tmp{"installer"};
    FakeSite site;
    Recorder out;
    InstallOptions options;
    string root;

    Fixture() {
        root = tmp.makeSubDir("stick");
        options.root = root;
        options.repoUrl = Site;
        options.buildbotUrl = string(Site) + "/buildbot";
        options.scratchDir = tmp.makeSubDir("scratch");
        options.packageFile = makePackage(tmp, "v2.0.0-pre0-abc1234");
        // the cover databases
        for (const char *r : {"J", "U", "P"}) {
            string name = string("covers") + r + ".db";
            tmp.writeFile("site/" + name, "sqlite " + name);
            site.files[string(Site) + "/db/" + name] = tmp.at("site/" + name);
            tmp.writeFile("site/" + name + ".sha256", Sha256::ofFile(tmp.at("site/" + name)) + "  " + name + "\n");
            site.files[string(Site) + "/db/" + name + ".sha256"] = tmp.at("site/" + name + ".sha256");
        }
        // RetroArch: the zip, the cores, the libs, the apps, the bundles
        {
            ableem::ZipWriter zip;
            REQUIRE(zip.open(tmp.at("site/retroarch-psc-v1.22.2-4.zip")));
            zip.addBytes("retroarch", "ELF retroarch");
            zip.addBytes("VERSION", "v1.22.2-4\n");
            zip.addBytes("theme/Autobleem2.png", "png");
            zip.addBytes("theme/selawik-light.ttf", "ttf");
            zip.addBytes("theme/retroarch-psc.cfg", "# the keys\nxmb_theme = \"7\"\nquit_on_close_content = \"2\"\n");
            REQUIRE(zip.close());
            string zipPath = tmp.at("site/retroarch-psc-v1.22.2-4.zip");
            tmp.writeFile("site/retroarch.json",
                          "{\"version\": \"v1.22.2-4\", \"zip\": " + json("retroarch-psc-v1.22.2-4.zip", zipPath) +
                              ", \"manifest\": \"http://site/m.json\"}");
            site.files[string(Site) + "/psc/retroarch/latest.json"] = tmp.at("site/retroarch.json");
            site.files[string(Site) + "/retroarch-psc-v1.22.2-4.zip"] = zipPath;
        }
        pack("cores",
             TarBuilder()
                 .file("cores/snes9x_libretro.so", "core")
                 .file("info/snes9x_libretro.info", "info")
                 .file("cores.json", "{}"),
             ", \"count\": 1");
        pack("libs",
             TarBuilder()
                 .file("apps/libSDL2_image-2.0.so.0.0.1", "so")
                 .symlink("apps/libSDL2_image-2.0.so.0", "libSDL2_image-2.0.so.0.0.1")
                 .file("modules/xpad.ko", "ko")
                 .file("libs.json", "{}"),
             ", \"count\": 2");
        pack("apps",
             TarBuilder()
                 .file("Apps/doom/run.sh", "run", 0755)
                 .file("Apps/doom/app.ini", "Title=Doom\n")
                 .file("apps.json", "{}"),
             ", \"count\": 1");
        for (const char *b :
             {"assets", "autoconfig", "database-rdb", "database-cursors", "cheats", "overlays", "shaders_glsl"}) {
            ableem::ZipWriter zip;
            string path = tmp.at(string("site/") + b + ".zip");
            REQUIRE(zip.open(path));
            zip.addBytes(string(b) + "-file.txt", b);
            REQUIRE(zip.close());
            site.files[string(Site) + "/buildbot/" + b + ".zip"] = path;
        }
        // the BIOS list: two files, one of them not on the "server"
        tmp.writeFile("site/scph5501.bin", "bios!");
        site.files[string(Site) + "/bios/scph5501.bin"] = tmp.at("site/scph5501.bin");
        string list = "# the list\n" + Sha256::ofFile(tmp.at("site/scph5501.bin")) + " 5 " + Site +
                      "/bios/scph5501.bin scph5501.bin\n" + string(64, 'a') + " 3 " + Site +
                      "/bios/gone.bin sub dir/gone.bin\n";
        tmp.writeFile("site/biospack.txt", list);
        site.files[string(Site) + "/psc/bios/biospack.txt"] = tmp.at("site/biospack.txt");
        // (the url json() writes is Site/<name>; the list's is psc/bios/biospack.txt)
        {
            string j = json("biospack.txt", tmp.at("site/biospack.txt"), ", \"count\": 2, \"total_bytes\": 8");
            size_t at = j.find("http://site/biospack.txt");
            j.replace(at, strlen("http://site/biospack.txt"), string(Site) + "/psc/bios/biospack.txt");
            tmp.writeFile("site/bios.json", j);
        }
        site.files[string(Site) + "/psc/bios/latest.json"] = tmp.at("site/bios.json");
        // the releases: the pre-release this package is from (with UpdateRoms), a stable one without it
        {
            ableem::ZipWriter zip;
            REQUIRE(zip.open(tmp.at("site/UpdateRoms-v2.0.0-pre0-abc1234.zip")));
            zip.addBytes("UpdateRoms/UpdateRoms.exe", "MZ updateroms");
            zip.addBytes("UpdateRoms/README.txt", "readme");
            REQUIRE(zip.close());
            string ur = json("UpdateRoms-v2.0.0-pre0-abc1234.zip", tmp.at("site/UpdateRoms-v2.0.0-pre0-abc1234.zip"));
            string fs = json("autobleem-psc-v2.0.0-pre0-abc1234.tar.gz", options.packageFile);
            tmp.writeFile("site/unstable.json",
                          "{\"version\": \"v2.0.0-pre0-abc1234\", \"prerelease\": true, \"files\": {\"psc-fs\": " + fs +
                              ", \"updateroms\": " + ur + "}}");
            tmp.writeFile("site/latest.json", "{\"version\": \"v1.9.9\", \"files\": {\"psc\": " + fs + "}}");
            site.files[string(Site) + "/releases/unstable.json"] = tmp.at("site/unstable.json");
            site.files[string(Site) + "/releases/latest.json"] = tmp.at("site/latest.json");
            site.files[string(Site) + "/UpdateRoms-v2.0.0-pre0-abc1234.zip"] =
                tmp.at("site/UpdateRoms-v2.0.0-pre0-abc1234.zip");
        }
        // the samples: a PS1 game, a NES ROM with its box art
        {
            TarBuilder b;
            b.file("Games/Tetrade/Tetrade.cue", "cue")
                .file("Games/Tetrade/Game.ini", "Title=Tetrade\n")
                .file("SAMPLES.md", "# samples\n");
            b.file("RetroArch/roms/Nintendo - Nintendo Entertainment System/Nova.nes", "nes");
            b.file("RetroArch/thumbnails/Nintendo - Nintendo Entertainment System/Named_Boxarts/Nova.png", "png");
            REQUIRE(b.writeTarGz(tmp.at("site/samples-20260920.tar.gz")));
            tmp.writeFile("site/samples.json", json("samples-20260920.tar.gz", tmp.at("site/samples-20260920.tar.gz"),
                                                    ", \"date\": \"20260920\""));
            site.files[string(Site) + "/samples/latest.json"] = tmp.at("site/samples.json");
            site.files[string(Site) + "/samples-20260920.tar.gz"] = tmp.at("site/samples-20260920.tar.gz");
        }
    }

    void pack(const string &kind, const TarBuilder &b, const string &extra) {
        string name = kind + "-psc-20260920.tar.gz";
        REQUIRE(b.writeTarGz(tmp.at("site/" + name)));
        tmp.writeFile("site/" + kind + ".json", json(name, tmp.at("site/" + name), ", \"date\": \"20260920\"" + extra));
        site.files[string(Site) + "/psc/" + kind + "/latest.json"] = tmp.at("site/" + kind + ".json");
        site.files[string(Site) + "/" + name] = tmp.at("site/" + name);
    }

    bool run(string &error) {
        return InstallerJob::run(options, site, out, []() { return false; }, error);
    }
    bool has(const string &rel) const { return DirEntry::exists(root + "/" + rel); }
};

} // namespace

TEST_CASE("packageNextTo finds the newest package beside the program") {
    TempDir tmp("installer");
    tmp.writeFile("bin/autobleem-psc-v2.0.0-pre0-aaa.tar.gz", "x");
    tmp.writeFile("bin/autobleem-psc-v2.0.1.tar.gz", "x");
    tmp.writeFile("bin/other.tar.gz", "x");
    CHECK(InstallerJob::packageNextTo(tmp.at("bin/AutoBleemInstaller.exe")) ==
          InstallerJob::normalizeRoot(tmp.at("bin/autobleem-psc-v2.0.1.tar.gz")));
    CHECK(InstallerJob::packageNextTo(tmp.at("nowhere/x.exe")).empty());
    CHECK(InstallerJob::normalizeRoot("F:") == "F:/");
    CHECK(InstallerJob::normalizeRoot("F:\\") == "F:/");
    CHECK(InstallerJob::normalizeRoot("/media/me/SONY/") == "/media/me/SONY");
}

TEST_CASE("a fresh install with the default options: the package and the three cover databases") {
    Fixture fx;
    StickInfo before = InstallerJob::inspect(fx.options);
    CHECK(before.isStick);
    CHECK_FALSE(before.installed);
    CHECK(before.packageVersion == "v2.0.0-pre0-abc1234");
    CHECK(InstallerJob::phasesFor(fx.options, before) == vector<string>{"Reading the package", "Preparing the stick",
                                                                        "Unpacking AutoBleem", "UpdateRoms",
                                                                        "Cover databases", "Finishing"});

    string error;
    REQUIRE_MESSAGE(fx.run(error), error);
    CHECK(fx.out.phases == vector<string>{"Reading the package", "Preparing the stick", "Unpacking AutoBleem",
                                          "UpdateRoms", "Cover databases", "Finishing"});
    // UpdateRoms from the release this package belongs to (the pre-release names it)
    CHECK(fx.tmp.readFile("stick/UpdateRoms/UpdateRoms.exe") == "MZ updateroms");
    CHECK(fx.site.count("UpdateRoms-v2.0.0-pre0-abc1234.zip") == 1);
    CHECK(fx.tmp.readFile("stick/Autobleem/bin/autobleem/autobleem-gui") == "ELF v2.0.0-pre0-abc1234");
    CHECK(fx.tmp.readFile("stick/VERSION") == "v2.0.0-pre0-abc1234\n");
    CHECK(fx.has("Games/!SaveStates"));
    CHECK(fx.has("Games/!MemCards"));
    CHECK(fx.has("System/Logs"));
    CHECK(fx.tmp.readFile("stick/Autobleem/bin/db/coversJ.db") == "sqlite coversJ.db");
    CHECK(fx.has("Autobleem/bin/db/coversU.db"));
    CHECK(fx.has("Autobleem/bin/db/coversP.db"));
    CHECK_FALSE(fx.has("RetroArch"));
    CHECK(fx.site.count("psc/") == 0);     // nothing of RetroArch's was asked for
    CHECK_FALSE(fx.has("System/Install")); // the scratch dir is elsewhere in the tests, and cleaned in any case

    StickInfo after = InstallerJob::inspect(fx.options);
    CHECK(after.installed);
    CHECK(after.installedVersion == "v2.0.0-pre0-abc1234");
    CHECK(after.hasCovers[0]);
    CHECK_FALSE(after.hasRetroArch);
}

TEST_CASE("only the chosen cover databases, and one already there is not fetched again") {
    Fixture fx;
    fx.options.coversJapan = false;
    fx.tmp.writeFile("stick/Autobleem/bin/db/coversU.db", "sqlite coversU.db"); // the very file
    string error;
    REQUIRE_MESSAGE(fx.run(error), error);
    CHECK_FALSE(fx.has("Autobleem/bin/db/coversJ.db"));
    CHECK(fx.has("Autobleem/bin/db/coversP.db"));
    CHECK(fx.site.count("/db/coversU.db.sha256") == 1);
    CHECK(fx.site.count("/db/coversU.db") == 1); // the sidecar only, not the database
    CHECK(fx.out.said("coversU.db is already there"));
}

TEST_CASE("an update replaces what the package ships and keeps the user's files and settings") {
    Fixture fx;
    string error;
    REQUIRE(fx.run(error));
    // the user's life on the stick
    fx.tmp.writeFile("stick/Autobleem/bin/autobleem/config.ini", "theme=aergb\nlanguage=Polish\n");
    fx.tmp.writeFile("stick/Games/Crash/Crash.cue", "cue");
    fx.tmp.writeFile("stick/Games/!MemCards/Crash/card1.mcd", "mcd");
    fx.tmp.writeFile("stick/System/Databases/regional.db", "db");
    fx.tmp.writeFile("stick/Themes/mine/theme.json", "{}");
    fx.tmp.writeFile("stick/Themes/ab2/stale.png", "old");                   // a shipped theme's stray file goes
    fx.tmp.writeFile("stick/Apps/doom/run.sh", "run");                       // a user's app stays
    fx.tmp.writeFile("stick/Autobleem/bin/autobleem/old-file.txt", "stale"); // the launcher's folder is replaced
    fx.tmp.writeFile("stick/Autobleem/rc/stale.sh", "old");
    fx.tmp.writeFile("stick/Docs/old.txt", "old");
    fx.tmp.writeFile("stick/UpdateRoms/stale.dll", "old");

    Fixture next; // a newer package, the same stick
    next.options = fx.options;
    next.options.packageFile = makePackage(next.tmp, "v2.0.1");
    StickInfo info = InstallerJob::inspect(next.options);
    CHECK(info.installed);
    CHECK(InstallerJob::phasesFor(next.options, info)[1] == "Preparing the update");
    REQUIRE_MESSAGE(InstallerJob::run(next.options, next.site, next.out, []() { return false; }, error), error);

    CHECK(fx.tmp.readFile("stick/Autobleem/bin/autobleem/autobleem-gui") == "ELF v2.0.1");
    CHECK(fx.tmp.readFile("stick/VERSION") == "v2.0.1\n");
    // the user's settings kept, and the PS1 emulator every install lands on set (the file rewritten as
    // the launcher writes it: capitalised keys)
    {
        const string cfg = fx.tmp.readFile("stick/Autobleem/bin/autobleem/config.ini");
        CHECK(cfg.find("Theme=aergb") != string::npos);
        CHECK(cfg.find("Language=Polish") != string::npos);
        CHECK(cfg.find("Emulator=pcsx-abnxt") != string::npos);
    }
    CHECK(fx.has("Games/Crash/Crash.cue"));
    CHECK(fx.has("Games/!MemCards/Crash/card1.mcd"));
    CHECK(fx.has("System/Databases/regional.db"));
    CHECK(fx.has("Themes/mine/theme.json"));
    CHECK(fx.has("Apps/doom/run.sh"));
    CHECK(fx.has("Autobleem/bin/db/coversJ.db"));
    CHECK_FALSE(fx.has("Themes/ab2/stale.png"));
    CHECK_FALSE(fx.has("Autobleem/bin/autobleem/old-file.txt"));
    CHECK_FALSE(fx.has("Autobleem/rc/stale.sh"));
    CHECK_FALSE(fx.has("Docs/old.txt"));
    CHECK_FALSE(fx.has("UpdateRoms/stale.dll"));
    CHECK(fx.has("UpdateRoms/UpdateRoms.exe"));
    CHECK(next.out.said("config.ini kept as it was"));
    CHECK(next.out.said("Updated."));
}

TEST_CASE("RetroArch, its cores, libraries, apps and bundles, then the BIOS files and the samples") {
    Fixture fx;
    fx.options.retroarch = true;
    fx.options.bios = true;
    fx.options.samples = true;
    StickInfo before = InstallerJob::inspect(fx.options);
    vector<string> phases = InstallerJob::phasesFor(fx.options, before);
    CHECK(phases.size() == 13);
    CHECK(phases[5] == "RetroArch");
    CHECK(phases[10] == "BIOS files");
    CHECK(phases[11] == "Sample games");

    string error;
    REQUIRE_MESSAGE(fx.run(error), error);
    CHECK(fx.out.phases == phases);
    // RetroArch/bin: the binary, the theme where the cfg keys look, the cfg with the directories
    CHECK(fx.tmp.readFile("stick/RetroArch/bin/retroarch") == "ELF retroarch");
    CHECK(fx.tmp.readFile("stick/RetroArch/bin/VERSION") == "v1.22.2-4\n");
    CHECK(fx.has("RetroArch/bin/Retroarch themes/Autobleem2.png"));
    CHECK(fx.has("RetroArch/bin/fonts/selawik-light.ttf"));
    string cfg = fx.tmp.readFile("stick/RetroArch/bin/retroarch.cfg");
    CHECK(cfg.find("system_directory = \"/media/RetroArch/bios\"") != string::npos);
    CHECK(cfg.find("rgui_browser_directory = \"/media/RetroArch/roms/\"") != string::npos);
    CHECK(cfg.find("libretro_directory = \":/cores\"") != string::npos);
    CHECK(cfg.find("xmb_theme = \"7\"") != string::npos);
    CHECK(cfg.find("# the keys") == string::npos);
    CHECK(fx.has("RetroArch/bin/playlists"));
    CHECK(fx.has("RetroArch/roms"));
    // the packs
    CHECK(fx.has("RetroArch/bin/cores/snes9x_libretro.so"));
    CHECK(fx.has("RetroArch/bin/info/snes9x_libretro.info"));
    CHECK(fx.has("Autobleem/lib/apps/libSDL2_image-2.0.so.0.0.1"));
    CHECK_FALSE(fx.has("Autobleem/lib/apps/libSDL2_image-2.0.so.0")); // a symlink: not on FAT
    CHECK(fx.has("Autobleem/lib/modules/xpad.ko"));
    CHECK(fx.has("Apps/doom/run.sh"));
    CHECK_FALSE(fx.has("apps.json"));
    // the bundles
    CHECK(fx.has("RetroArch/bin/assets/assets-file.txt"));
    CHECK(fx.has("RetroArch/bin/database/rdb/database-rdb-file.txt"));
    CHECK(fx.has("RetroArch/bin/shaders/shaders_glsl-file.txt"));
    // the BIOS files: one fetched, the missing one reported, the run goes on
    CHECK(fx.tmp.readFile("stick/RetroArch/bios/scph5501.bin") == "bios!");
    CHECK_FALSE(fx.has("RetroArch/bios/sub dir/gone.bin"));
    CHECK(fx.out.said("could not fetch sub dir/gone.bin"));
    CHECK(fx.out.said("1 fetched, 0 already there, 1 failed"));
    // the samples, the console's way round
    CHECK(fx.has("Games/Tetrade/Tetrade.cue"));
    CHECK(fx.has("SAMPLES.md"));
    CHECK(fx.has("RetroArch/roms/Nintendo - Nintendo Entertainment System/Nova.nes"));
    CHECK(fx.has("RetroArch/bin/thumbnails/Nintendo - Nintendo Entertainment System/Named_Boxarts/Nova.png"));
    CHECK_FALSE(fx.has("RetroArch/thumbnails"));
    CHECK(fx.tmp.readFile("stick/System/samples.txt") == "samples-20260920.tar.gz\n");

    // again: RetroArch is there at that version, the BIOS file is kept, the samples are not put back
    Fixture again;
    again.options = fx.options;
    fx.tmp.writeFile("stick/RetroArch/bin/retroarch.cfg", cfg + "video_smooth = \"true\"\n");
    DirEntry::removeDirAndContents(fx.root + "/Games/Tetrade");
    REQUIRE_MESSAGE(InstallerJob::run(again.options, again.site, again.out, []() { return false; }, error), error);
    CHECK(again.out.said("RetroArch v1.22.2-4 is on the stick already"));
    CHECK(again.out.said("keeping the existing retroarch.cfg"));
    CHECK(again.out.said("0 fetched, 1 already there, 1 failed"));
    CHECK(again.out.said("not again"));
    CHECK_FALSE(fx.has("Games/Tetrade"));
    CHECK(again.site.count("retroarch-psc-v1.22.2-4.zip") == 0);
}

TEST_CASE("the BIOS files without RetroArch on the stick are not a phase; samples without RetroArch skip its part") {
    Fixture fx;
    fx.options.bios = true;
    fx.options.samples = true;
    StickInfo info = InstallerJob::inspect(fx.options);
    vector<string> phases = InstallerJob::phasesFor(fx.options, info);
    CHECK(find(phases.begin(), phases.end(), "BIOS files") == phases.end());
    string error;
    REQUIRE_MESSAGE(fx.run(error), error);
    CHECK(fx.has("Games/Tetrade/Tetrade.cue"));
    CHECK_FALSE(fx.has("RetroArch/roms"));
    CHECK(fx.out.said("need RetroArch"));
}

TEST_CASE("a stop request, a bad package and a missing drive are reported") {
    Fixture fx;
    string error;
    CHECK_FALSE(InstallerJob::run(fx.options, fx.site, fx.out, []() { return true; }, error));
    CHECK(error == "Stopped");
    CHECK_FALSE(fx.has("Autobleem/bin/autobleem/autobleem-gui"));

    fx.tmp.writeFile("pkg/bad.tar.gz", "not a tarball");
    fx.options.packageFile = fx.tmp.at("pkg/bad.tar.gz");
    CHECK_FALSE(fx.run(error));
    CHECK_FALSE(error.empty());

    Fixture other;
    other.options.root = other.tmp.at("nope");
    CHECK_FALSE(InstallerJob::run(other.options, other.site, other.out, []() { return false; }, error));
    CHECK(error.find("No such drive") != string::npos);

    // the site down: the covers phase fails with the reason
    Fixture down;
    down.site.files.clear();
    CHECK_FALSE(down.run(error));
    CHECK(error.find("coversJ.db") != string::npos);
}

TEST_CASE("an AutoBleem 1.0 / NG stick is brought to the new layout before the update, nothing of the user's lost") {
    Fixture fx;
    // the old stick: RetroBoot's tree at the root, roms/ and themes/ beside it, an old launcher
    fx.tmp.writeFile("stick/Autobleem/bin/autobleem/autobleem-gui", "ELF 1.0");
    fx.tmp.writeFile("stick/Autobleem/bin/autobleem/config.ini", "theme=aergb\n");
    fx.tmp.writeFile("stick/Games/Crash/Crash.cue", "cue");
    fx.tmp.writeFile("stick/Games/!MemCards/Crash/card1.mcd", "mcd");
    fx.tmp.writeFile("stick/themes/mine/theme.ini", "Background=bg.png\n");
    fx.tmp.writeFile("stick/retroarch/retroarch", "ELF 1.9.0");
    fx.tmp.writeFile(
        "stick/retroarch/retroarch.cfg",
        "libretro_directory = \":/cores\"\nsystem_directory = \":/system\"\nrgui_browser_directory = \"/media/roms/\"\n"
        "xmb_theme = \"8\"\ncontent_favorites_directory = \"default\"\n");
    fx.tmp.writeFile("stick/retroarch/cores/snes9x_libretro.so", "core");
    fx.tmp.writeFile("stick/retroarch/system/scph5501.bin", "bios");
    fx.tmp.writeFile("stick/retroarch/saves/Crash.srm", "srm");
    fx.tmp.writeFile(
        "stick/retroarch/playlists/Nintendo - Super Nintendo Entertainment System.lpl",
        "{\"items\": [{\"path\": \"/media/roms/Nintendo - Super Nintendo Entertainment System/Zelda.sfc\", "
        "\"core_path\": \"/media/retroarch/cores/snes9x_libretro.so\"}]}");
    fx.tmp.writeFile("stick/retroarch/playlists/Applications.lpl", "{}");
    fx.tmp.writeFile("stick/retroarch/playlists/Sony - PlayStation.lpl", "{}");
    fx.tmp.writeFile("stick/retroarch/playlists/AutoBleem.lpl", "{}");
    fx.tmp.writeFile("stick/retroarch/retroboot/assets/lib/libSDL2_image-2.0.so.0.0.1", "so");
    fx.tmp.writeFile("stick/retroarch/retroboot/modules/xpad.ko", "ko");
    fx.tmp.writeFile("stick/retroarch/retroboot/bin/launch_rfa.sh", "rb");
    fx.tmp.writeFile("stick/retroarch/apps/eduke32/eduke32", "ELF");
    fx.tmp.writeFile("stick/retroarch/apps/eduke32/launcheduke32.sh",
                     "export LD_LIBRARY_PATH=\"${RB_LIBRARY_PATH}\"\ncd /media/retroarch/apps/eduke32\n./eduke32 > "
                     "/media/retroarch/logs/duke3d.log\n");
    fx.tmp.writeFile("stick/Apps/eduke32/app.ini", "Title=Duke\n");
    fx.tmp.writeFile(
        "stick/Apps/eduke32/run.sh",
        "#! /bin/sh\n# Retroboot App Launcher\n\nif [ -z \"${RB_LIBRARY_PATH}\" ]; then\n\tsource "
        "/media/retroarch/retroboot/bin/loadconfig.sh\nfi\n"
        "if [ ! -d \"${RB_LIBRARY_PATH}\" ]; then\n\tsh /media/retroarch/retroboot/bin/init_libs.sh\nfi\nsh "
        "\"/media/retroarch/apps/eduke32/launcheduke32.sh\"\n");
    fx.tmp.writeFile("stick/Apps/retroboot/run.sh", "sh /media/retroarch/retroboot/bin/launch_rfa.sh\n");
    fx.tmp.writeFile("stick/Apps/retroboot/app.ini", "Title=RetroBoot\n");
    fx.tmp.writeFile("stick/roms/Nintendo - Super Nintendo Entertainment System/Zelda.sfc", "rom");

    StickInfo info = InstallerJob::inspect(fx.options);
    CHECK(info.installed);
    CHECK(info.legacyLayout);
    CHECK(info.hasRetroArch);
    vector<string> phases = InstallerJob::phasesFor(fx.options, info);
    CHECK(phases[1] == "Preparing the update");
    CHECK(phases[2] == "Bringing the old layout up to date");
    CHECK(phases[4] == "UpdateRoms");

    string error;
    REQUIRE_MESSAGE(fx.run(error), error);
    // moved
    CHECK(fx.tmp.readFile("stick/RetroArch/bin/retroarch") == "ELF 1.9.0");
    CHECK(fx.has("RetroArch/bin/cores/snes9x_libretro.so"));
    CHECK(fx.has("RetroArch/bin/saves/Crash.srm"));
    CHECK(fx.tmp.readFile("stick/RetroArch/bios/scph5501.bin") == "bios");
    CHECK_FALSE(fx.has("RetroArch/bin/system"));
    CHECK(fx.has("RetroArch/roms/Nintendo - Super Nintendo Entertainment System/Zelda.sfc"));
    CHECK_FALSE(fx.has("roms"));
    CHECK(fx.has("Themes/mine/theme.ini"));
    CHECK(fx.has("Games/Crash/Crash.cue"));
    CHECK(fx.has("Games/!MemCards/Crash/card1.mcd"));
    // rewritten
    string cfg = fx.tmp.readFile("stick/RetroArch/bin/retroarch.cfg");
    CHECK(cfg.find("system_directory = \"/media/RetroArch/bios\"") != string::npos);
    CHECK(cfg.find("rgui_browser_directory = \"/media/RetroArch/roms/\"") != string::npos);
    CHECK(cfg.find("xmb_theme = \"8\"") != string::npos); // RetroBoot's RetroArch stays, so does its theme
    string lpl = fx.tmp.readFile("stick/RetroArch/bin/playlists/Nintendo - Super Nintendo Entertainment System.lpl");
    CHECK(lpl.find("/media/RetroArch/roms/Nintendo - Super Nintendo Entertainment System/Zelda.sfc") != string::npos);
    CHECK(lpl.find("/media/RetroArch/bin/cores/snes9x_libretro.so") != string::npos);
    CHECK_FALSE(fx.has("RetroArch/bin/playlists/Applications.lpl"));
    CHECK_FALSE(fx.has("RetroArch/bin/playlists/Sony - PlayStation.lpl"));
    CHECK_FALSE(fx.has("RetroArch/bin/playlists/AutoBleem.lpl"));
    // the libraries, the apps
    CHECK(fx.has("Autobleem/lib/apps/libSDL2_image-2.0.so.0.0.1"));
    CHECK(fx.has("Autobleem/lib/modules/xpad.ko"));
    CHECK(fx.has("Apps/eduke32/eduke32"));
    string run = fx.tmp.readFile("stick/Apps/eduke32/run.sh");
    CHECK(run == "#! /bin/sh\n# Retroboot App Launcher\n. /media/Autobleem/rc/app_env.sh\n\nsh "
                 "\"/media/Apps/eduke32/launcheduke32.sh\"\n");
    string launch = fx.tmp.readFile("stick/Apps/eduke32/launcheduke32.sh");
    CHECK(
        launch ==
        "export LD_LIBRARY_PATH=\"/tmp/applib\"\ncd /media/Apps/eduke32\n./eduke32 > /media/System/Logs/duke3d.log\n");
    CHECK_FALSE(fx.has("Apps/retroboot"));
    // and the update itself went through, the old settings kept
    CHECK(fx.tmp.readFile("stick/Autobleem/bin/autobleem/autobleem-gui") == "ELF v2.0.0-pre0-abc1234");
    {
        const string cfg = fx.tmp.readFile("stick/Autobleem/bin/autobleem/config.ini");
        CHECK(cfg.find("Theme=aergb") != string::npos);
        CHECK(cfg.find("Emulator=pcsx-abnxt") != string::npos);
    }
    CHECK(fx.out.said("retroarch -> RetroArch/bin"));

    // a second run finds the new layout and does not migrate again
    Fixture again;
    again.options = fx.options;
    CHECK_FALSE(InstallerJob::inspect(again.options).legacyLayout);
}

TEST_CASE("a new RetroArch build over a RetroBoot-era retroarch.cfg sets its own keys, keeps the rest") {
    Fixture fx;
    fx.options.retroarch = true;
    fx.tmp.writeFile("stick/RetroArch/bin/retroarch", "ELF 1.9.0");
    fx.tmp.writeFile("stick/RetroArch/bin/retroarch.cfg",
                     "video_smooth = \"true\"\nxmb_theme = \"8\"\nmenu_driver = \"xmb\"\n");
    string error;
    REQUIRE_MESSAGE(fx.run(error), error);
    string cfg = fx.tmp.readFile("stick/RetroArch/bin/retroarch.cfg");
    CHECK(cfg == "video_smooth = \"true\"\nxmb_theme = \"7\"\nmenu_driver = \"xmb\"\nquit_on_close_content = \"2\"\n");
    CHECK(fx.out.said("2 keys of this RetroArch build set in it"));
}
