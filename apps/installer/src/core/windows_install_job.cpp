#include "core/windows_install_job.h"
#include "core/install_job_base.h"

#include <ableem/engine/filesystem.h>
#include <ableem/engine/log.h>
#include <ableem/engine/seven_zip_archive.h>
#include <ableem/engine/sha256.h>
#include <ableem/engine/tar_archive.h>
#include <ableem/engine/update_catalog.h>
#include <ableem/engine/zip_archive.h>

#include <sstream>

using namespace std;
using ableem::DirEntry;
using ableem::PackCatalog;
using ableem::SevenZipArchive;
using ableem::SevenZipEntry;
using ableem::Sha256;
using ableem::TarArchive;
using ableem::UpdateFile;
using ableem::ZipArchive;

namespace {

const char *const ConfigIni = "System/config.ini";
const char *const RetroArchExe = "RetroArch/bin/retroarch.exe";
const char *const RetroArchVersionFile = "RetroArch/bin/VERSION";
const char *const RetroArchCfg = "RetroArch/bin/retroarch.cfg";
const char *const SamplesMarker = "System/samples.txt";
const char *const SevenZipTopFolder = "RetroArch-Win64/"; // what libretro's archives put everything under

struct CoverDb {
    const char *name;
    const char *file;
    bool WindowsInstallOptions::*selected;
};
const CoverDb Covers[] = {
    {"Japan", "coversJ.db", &WindowsInstallOptions::coversJapan},
    {"USA", "coversU.db", &WindowsInstallOptions::coversUsa},
    {"PAL", "coversP.db", &WindowsInstallOptions::coversPal},
};

// the data tree's folders, made on every run (a user may have deleted one)
const char *const DataFolders[] = {"Games",          "System/Databases", "System/Logs",    "System/Bios",
                                   "System/Updates", "Themes",           "RetroArch/roms", "Apps"};

// a shallow-recursive copy of a shipped theme folder, files already there kept
void copyTree(const string &from, const string &to) {
    DirEntry::createDirs(to);
    for (const DirEntry &e : DirEntry::diru(from)) {
        const string src = from + "/" + e.name, dst = to + "/" + e.name;
        if (e.isDir)
            copyTree(src, dst);
        else if (!DirEntry::exists(dst))
            DirEntry::copyFile(src, dst);
    }
}

//******************
// Run
//******************
class Run : public InstallJobBase {
public:
    Run(const WindowsInstallOptions &options, const WindowsInstallInfo &info, Downloader &downloader,
        InstallListener &listener, const InstallerJob::ShouldStop &shouldStop)
        : InstallJobBase(downloader, listener, shouldStop, options.repoUrl,
                         options.scratchDir.empty() ? options.dataRoot + "/System/Install" : options.scratchDir),
          opt(options), info(info), root(options.dataRoot) {}

    bool go(string &error) {
        phases = WindowsInstallJob::phasesFor(opt, info);
        DirEntry::createDirs(scratch);
        bool ok = dataTree(error) && covers(error);
        if (ok && opt.retroarch)
            ok = retroarch(error) && cores(error);
        if (ok && opt.bios)
            ok = bios(error);
        if (ok && opt.samples)
            ok = samples(error);
        if (ok)
            finish();
        if (DirEntry::isDirectory(scratch) && opt.scratchDir.empty())
            DirEntry::removeDirAndContents(scratch);
        return ok;
    }

private:
    string at(const string &rel) const { return root + "/" + rel; }

    //******************
    // 1. the data tree
    //******************
    bool dataTree(string &error) {
        phase("Data folder");
        for (const char *d : DataFolders) {
            if (!DirEntry::createDirs(at(d))) {
                error = "cannot create " + at(d);
                return false;
            }
        }
        // the shipped themes, copied in once: the launcher reads them from the data tree (a user drops
        // their own next to these) - a theme already there is the user's, edited or not
        const string shipped = opt.programDir + "/Themes";
        int copied = 0;
        if (DirEntry::isDirectory(shipped)) {
            for (const DirEntry &t : DirEntry::diru_DirsOnly(shipped)) {
                if (!DirEntry::exists(at("Themes/" + t.name))) {
                    copyTree(shipped + "/" + t.name, at("Themes/" + t.name));
                    copied++;
                }
            }
        }
        say("  " + root + (info.installed ? " (AutoBleem has run here before - the settings are kept)" : "") +
            (copied ? ", " + to_string(copied) + " themes copied in" : ""));
        if (opt.update) {
            // the launcher's scan then goes over everything once, as it does after a Pi update
            DirEntry::removeFile(at("System/games.fingerprint"));
            DirEntry::removeFile(at("System/roms.fingerprint"));
        }
        // the PS1 emulator every install lands on (the owner's rule, 2026-09-21): pcsx-abnxt, whatever a
        // config.ini already here said
        if (info.installed && setIniValue(at(ConfigIni), "emulator", "pcsx-abnxt"))
            say("  PS1 emulator set to pcsx-abnxt");
        return true;
    }

    //******************
    // 2. the cover databases
    //******************
    bool covers(string &error) {
        bool any = false;
        for (const CoverDb &c : Covers)
            any = any || opt.*c.selected;
        if (!any)
            return true;
        phase("Cover databases");
        for (const CoverDb &c : Covers) {
            if (!(opt.*c.selected))
                continue;
            if (stopped(error))
                return false;
            string sidecar;
            UpdateFile file;
            file.name = c.file;
            file.url = opt.repoUrl + "/db/" + c.file;
            if (dl.fetchText(file.url + ".sha256", scratch + "/sidecar.txt", sidecar, error))
                file.sha256 = sidecarHash(sidecar);
            else
                say("  (no checksum published for " + string(c.file) + " - " + error + ")");
            say("  " + string(c.name));
            if (!downloadVerified(file, at(string("System/Databases/") + c.file), error))
                return false;
        }
        return true;
    }

    //******************
    // 3. RetroArch
    //******************
    // libretro's own Windows build: the site's tarball of it (win/retroarch/latest.json - what the CI
    // repacks from the official archive), else the official RetroArch.7z unpacked here. Its setup exe is
    // not an option: it asks for administrator rights, which a per-user install does not have.
    bool retroarch(string &error) {
        phase("RetroArch");
        if (stopped(error))
            return false;
        const string bin = at("RetroArch/bin");
        string text;
        PackCatalog cat;
        bool fromSite = fetchCatalog("win/retroarch/latest.json", text, error) && cat.parse(text);
        const string version = fromSite ? cat.version : opt.retroarchFallbackVersion;
        if (info.hasRetroArch && !version.empty() && info.retroarchVersion == version) {
            say("  RetroArch " + version + " is installed already");
            return writeRetroArchCfg(error);
        }
        bool ok;
        if (fromSite) {
            const string tarball = scratch + "/" + cat.file.name;
            say("  RetroArch " + version + " from the download repository");
            ok = downloadVerified(cat.file, tarball, error) && untar(tarball, bin, error);
            DirEntry::removeFile(tarball);
        } else {
            say("  (the download repository lists no Windows build - taking libretro's RetroArch.7z)");
            const string archive = scratch + "/RetroArch.7z";
            const string url = opt.buildbotUrl + "/stable/" + version + "/windows/x86_64/RetroArch.7z";
            say("  RetroArch " + version + " from " + url);
            ok = download(url, archive, error);
            if (ok) {
                say("  unpacking (this takes a while)");
                ok = SevenZipArchive::extract(
                    archive, bin, error, SevenZipArchive::Filter(),
                    [this](uint64_t done, uint64_t total) { out.onProgress(done, total); }, SevenZipTopFolder);
            }
            DirEntry::removeFile(archive);
        }
        if (!ok)
            return false;
        if (!DirEntry::exists(at(RetroArchExe))) {
            error = "no retroarch.exe came out of the RetroArch download";
            return false;
        }
        writeText(at(RetroArchVersionFile), version + "\n");
        for (const char *d : {"cores", "playlists", "saves", "states", "system", "thumbnails", "screenshots"})
            DirEntry::createDirs(bin + "/" + d);
        // shared with payload_linux/install.sh: <program>/platform/roms_systems.cfg (the product ships it)
        createRomFolders(opt.programDir + "/platform/roms_systems.cfg", at("RetroArch/roms"));
        return writeRetroArchCfg(error);
    }

    // retroarch.cfg, only when there is none: RetroArch keeps it up to date itself and the launcher edits
    // a few keys around each launch - both must keep what the user has set since
    bool writeRetroArchCfg(string &error) {
        const string cfg = at(RetroArchCfg);
        if (DirEntry::exists(cfg)) {
            say("  keeping the existing retroarch.cfg");
            return true;
        }
        if (!writeText(cfg, WindowsInstallJob::retroArchConfigText(root))) {
            error = "cannot write " + cfg;
            return false;
        }
        say("  retroarch.cfg written (full screen, the roms folder as the browser's start)");
        return true;
    }

    //******************
    // 4. the cores
    //******************
    // the site's pack (win/cores/latest.json - every core buildbot has, in one tarball), else one zip per
    // core straight from buildbot's nightly index - the same cores, ~200 requests
    bool cores(string &error) {
        phase("RetroArch cores");
        if (stopped(error))
            return false;
        const string bin = at("RetroArch/bin");
        string text;
        PackCatalog cat;
        if (fetchCatalog("win/cores/latest.json", text, error) && cat.parse(text)) {
            say("  " + cat.file.name + (cat.count ? " (" + to_string(cat.count) + " cores)" : ""));
            const string tarball = scratch + "/" + cat.file.name;
            bool ok = downloadVerified(cat.file, tarball, error) && untar(tarball, bin, error);
            DirEntry::removeFile(tarball);
            return ok;
        }
        say("  (the download repository has no cores pack - taking them one by one from libretro)");
        const string base = opt.buildbotUrl + "/nightly/windows/x86_64/latest";
        string index;
        if (!dl.fetchText(base + "/.index-extended", scratch + "/index.txt", index, error))
            return false;
        // "<date> <crc> <name>_libretro.dll.zip" per line
        vector<string> zips;
        istringstream in(index);
        string line;
        while (getline(in, line)) {
            size_t sp = line.find_last_of(" \t");
            string zip = trimmed(sp == string::npos ? line : line.substr(sp + 1));
            if (zip.size() > 4 && zip.compare(zip.size() - 4, 4, ".zip") == 0 && TarArchive::isSafeName(zip))
                zips.push_back(zip);
        }
        say("  " + to_string(zips.size()) + " cores");
        DirEntry::createDirs(bin + "/cores");
        int fetched = 0, failed = 0;
        for (size_t i = 0; i < zips.size(); i++) {
            if (stopped(error))
                return false;
            out.onProgress(i, zips.size());
            const string zipFile = scratch + "/" + zips[i];
            string why;
            bool ok =
                dl.fetch(
                    base + "/" + zips[i], zipFile, [this](uint64_t, uint64_t) { return !(stop && stop()); }, why) &&
                ZipArchive::extract(zipFile, bin + "/cores");
            DirEntry::removeFile(zipFile);
            if (ok) {
                fetched++;
            } else {
                failed++;
                if (failed <= 20)
                    say("  could not fetch " + zips[i] + (why.empty() ? "" : ": " + why));
            }
        }
        out.onProgress(zips.size(), zips.size());
        say("  " + to_string(fetched) + " fetched, " + to_string(failed) + " failed");
        return true;
    }

    //******************
    // 5. the BIOS files
    //******************
    bool bios(string &error) {
        phase("BIOS files");
        if (stopped(error))
            return false;
        // RetroArch's system directory is its own tree's: the cores look there. Without RetroArch nothing
        // reads it but the PlayStation emulator's two files (installPs1Bios), so the rest of the pack is
        // not fetched
        const bool withRetroArch = opt.retroarch || info.hasRetroArch;
        if (!withRetroArch)
            say("  PlayStation only (no RetroArch): just the two files the emulator needs");
        const string systemDir = at("RetroArch/bin/system");
        if (!fetchBiosPack("win/bios/latest.json", systemDir, error,
                           withRetroArch ? BiosFilter() : BiosFilter(isPs1BiosFile)))
            return false;
        installPs1Bios(systemDir, at("System/Bios"));
        return true;
    }

    //******************
    // 6. the sample games
    //******************
    bool samples(string &error) {
        phase("Sample games");
        if (stopped(error))
            return false;
        if (DirEntry::exists(at(SamplesMarker))) {
            say("  the samples were put here before (System/samples.txt) - not again");
            return true;
        }
        string text;
        PackCatalog cat;
        if (!fetchCatalog("samples/latest.json", text, error) || !cat.parse(text)) {
            if (error.empty())
                error = "samples/latest.json is not what was expected";
            return false;
        }
        const string tarball = scratch + "/" + cat.file.name;
        if (!downloadVerified(cat.file, tarball, error))
            return false;
        // Games/ and SAMPLES.md as they are; the RetroArch part only with RetroArch here: the pack's
        // RetroArch/roms is RetroArch/roms, its RetroArch/thumbnails is RetroArch/bin/thumbnails
        bool ok =
            untar(tarball, root, error, [](const ableem::TarEntry &e) { return e.name.rfind("RetroArch/", 0) != 0; });
        const bool withRetroArch = opt.retroarch || info.hasRetroArch;
        if (ok && withRetroArch)
            ok = untar(tarball, at("RetroArch/roms"), error, TarArchive::Filter(), "RetroArch/roms/") &&
                 untar(tarball, at("RetroArch/bin/thumbnails"), error, TarArchive::Filter(), "RetroArch/thumbnails/");
        DirEntry::removeFile(tarball);
        if (!ok)
            return false;
        writeText(at(SamplesMarker), cat.file.name + "\n");
        say(string("  done") + (withRetroArch ? "" : " (the other systems' games need RetroArch)"));
        return true;
    }

    void finish() {
        phase("Finishing");
        out.onProgress(1, 1);
        say(opt.update ? "Updated." : "Installed. AutoBleem is in the Start Menu.");
    }

    const WindowsInstallOptions &opt;
    const WindowsInstallInfo &info;
    string root;
};

} // namespace

//*******************************
// WindowsInstallJob::inspect
//*******************************
WindowsInstallInfo WindowsInstallJob::inspect(const WindowsInstallOptions &options) {
    WindowsInstallInfo info;
    const string root = options.dataRoot;
    if (root.empty()) {
        info.error = "no data folder given";
        return info;
    }
    if (!DirEntry::isDirectory(root) && !DirEntry::createDirs(root)) {
        info.error = "cannot create " + root;
        return info;
    }
    info.exists = true;
    info.installed = DirEntry::exists(root + "/" + ConfigIni);
    info.hasRetroArch = DirEntry::exists(root + "/" + RetroArchExe);
    if (info.hasRetroArch)
        info.retroarchVersion = firstLine(readText(root + "/" + RetroArchVersionFile));
    for (int i = 0; i < 3; i++)
        info.hasCovers[i] = DirEntry::exists(root + "/System/Databases/" + Covers[i].file);
    return info;
}

//*******************************
// WindowsInstallJob::phasesFor
//*******************************
vector<string> WindowsInstallJob::phasesFor(const WindowsInstallOptions &options, const WindowsInstallInfo &info) {
    vector<string> phases{"Data folder"};
    if (options.coversJapan || options.coversUsa || options.coversPal)
        phases.push_back("Cover databases");
    if (options.retroarch) {
        phases.push_back("RetroArch");
        phases.push_back("RetroArch cores");
    }
    if (options.bios)
        phases.push_back("BIOS files");
    if (options.samples)
        phases.push_back("Sample games");
    phases.push_back("Finishing");
    return phases;
}

//*******************************
// WindowsInstallJob::run
//*******************************
bool WindowsInstallJob::run(const WindowsInstallOptions &input, Downloader &downloader, InstallListener &listener,
                            const InstallerJob::ShouldStop &shouldStop, string &error) {
    WindowsInstallOptions options = input;
    options.dataRoot = InstallerJob::normalizeRoot(options.dataRoot);
    options.programDir = InstallerJob::normalizeRoot(options.programDir);
    WindowsInstallInfo info = inspect(options);
    if (!info.exists) {
        error = info.error;
        return false;
    }
    Run run(options, info, downloader, listener, shouldStop);
    return run.go(error);
}

//*******************************
// WindowsInstallJob::retroArchConfigText
//*******************************
// RetroArch next to its retroarch.cfg is portable: every directory defaults to its own tree, so only what
// differs from a stock install is written - the launcher starts it with --fullscreen too, this is for a
// RetroArch started on its own
string WindowsInstallJob::retroArchConfigText(const string &dataRoot) {
    string roms = dataRoot + "/RetroArch/roms";
    for (char &c : roms)
        if (c == '/')
            c = '\\';
    return "# written by the AutoBleem installer; RetroArch keeps this file up to date itself\n"
           "video_fullscreen = \"true\"\n"
           "video_windowed_fullscreen = \"true\"\n"
           "quit_on_close_content = \"2\"\n"
           "menu_swap_ok_cancel_buttons = \"true\"\n"
           "rgui_browser_directory = \"" +
           roms + "\"\n";
}
