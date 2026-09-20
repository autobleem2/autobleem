#include "core/installer_job.h"
#include "core/legacy_layout.h"

#include <ableem/engine/filesystem.h>
#include <ableem/engine/log.h>
#include <ableem/engine/sha256.h>
#include <ableem/engine/strings.h>
#include <ableem/engine/tar_archive.h>
#include <ableem/engine/update_catalog.h>
#include <ableem/engine/zip_archive.h>

#include <algorithm>
#include <fstream>
#include <sstream>

using namespace std;
using ableem::DirEntry;
using ableem::PackCatalog;
using ableem::PscRetroArchCatalog;
using ableem::ReleaseCatalog;
using ableem::Sha256;
using ableem::TarArchive;
using ableem::TarEntry;
using ableem::UpdateFile;
using ableem::ZipArchive;

namespace {

const char *const VersionFile = "VERSION";
const char *const LauncherBinary = "Autobleem/bin/autobleem/autobleem-gui";
const char *const ConfigIni = "Autobleem/bin/autobleem/config.ini";
const char *const SamplesMarker = "System/samples.txt";

struct CoverDb {
    const char *name;
    const char *file;
    bool InstallOptions::*selected;
};
const CoverDb Covers[] = {
    {"Japan", "coversJ.db", &InstallOptions::coversJapan},
    {"USA", "coversU.db", &InstallOptions::coversUsa},
    {"PAL", "coversP.db", &InstallOptions::coversPal},
};

// libretro's bundles and where each unpacks under RetroArch/bin (the Pi installer's list, minus info -
// the cores pack carries the info files)
struct Bundle {
    const char *name;
    const char *dest;
};
const Bundle Bundles[] = {
    {"assets", "assets"},
    {"autoconfig", "autoconfig"},
    {"database-rdb", "database/rdb"},
    {"database-cursors", "database/cursors"},
    {"cheats", "cheats"},
    {"overlays", "overlays"},
    {"shaders_glsl", "shaders"},
};

string readText(const string &path) {
    ifstream in(path, ios::binary);
    if (!in)
        return "";
    stringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

bool writeText(const string &path, const string &text) {
    ofstream out(path, ios::binary | ios::trunc);
    out << text;
    return static_cast<bool>(out);
}

string trimmed(const string &s) {
    return ableem::Strings::trim(s);
}

string firstLine(const string &text) {
    size_t nl = text.find_first_of("\r\n");
    return trimmed(nl == string::npos ? text : text.substr(0, nl));
}

// "abc  name" -> "abc" (a sha256sum sidecar)
string sidecarHash(const string &text) {
    string line = firstLine(text);
    size_t sp = line.find(' ');
    return sp == string::npos ? line : line.substr(0, sp);
}

string humanSize(uint64_t bytes) {
    char buf[32];
    if (bytes >= 1024ull * 1024 * 1024)
        snprintf(buf, sizeof(buf), "%.1f GB", bytes / (1024.0 * 1024 * 1024));
    else if (bytes >= 1024ull * 1024)
        snprintf(buf, sizeof(buf), "%.1f MB", bytes / (1024.0 * 1024));
    else
        snprintf(buf, sizeof(buf), "%.0f KB", bytes / 1024.0);
    return buf;
}

//******************
// Run
//******************
// one install: the options, the stick, and the helpers every phase uses
class Run {
public:
    Run(const InstallOptions &options, const StickInfo &info, Downloader &downloader, InstallListener &listener,
        const InstallerJob::ShouldStop &shouldStop)
        : opt(options), info(info), dl(downloader), out(listener), stop(shouldStop), root(options.root),
          scratch(options.scratchDir.empty() ? root + "/System/Install" : options.scratchDir) {}

    bool go(string &error) {
        phases = InstallerJob::phasesFor(opt, info);
        DirEntry::createDirs(scratch);
        bool ok =
            package(error) && prepare(error) && legacy(error) && unpack(error) && updateRoms(error) && covers(error);
        if (ok && opt.retroarch)
            ok = retroarch(error);
        if (ok && opt.bios && (opt.retroarch || info.hasRetroArch))
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
    bool stopped(string &error) {
        if (stop && stop()) {
            error = "Stopped";
            return true;
        }
        return false;
    }
    void phase(const string &title) {
        phaseIndex++;
        out.onPhase(phaseIndex, static_cast<int>(phases.size()), title);
        out.onProgress(0, 0);
        say("== " + title);
    }
    void say(const string &line) {
        out.onLine(line);
        PLOG_INFO << line;
    }
    string at(const string &rel) const { return root + "/" + rel; }

    // a download with the progress on the phase's bar and the stop flag honoured
    bool download(const string &url, const string &dest, string &error) {
        DirEntry::createDirs(dest.substr(0, dest.find_last_of('/')));
        bool ok = dl.fetch(
            url, dest,
            [this](uint64_t done, uint64_t total) {
                out.onProgress(done, total);
                return !(stop && stop());
            },
            error);
        if (!ok && stop && stop())
            error = "Stopped";
        return ok;
    }
    // a file the site publishes with its sha256: kept when the stick has that very file already,
    // fetched to .part and checked otherwise
    bool downloadVerified(const UpdateFile &file, const string &dest, string &error) {
        if (DirEntry::exists(dest) && !file.sha256.empty() && Sha256::ofFile(dest) == file.sha256) {
            say("  " + file.name + " is already there");
            return true;
        }
        say("  " + file.name + (file.size ? " (" + humanSize(file.size) + ")" : ""));
        const string part = dest + ".part";
        if (!download(file.url, part, error))
            return false;
        if (!file.sha256.empty() && Sha256::ofFile(part) != file.sha256) {
            DirEntry::removeFile(part);
            error = file.name + ": the download does not match its sha256";
            return false;
        }
        DirEntry::removeFile(dest);
        if (!DirEntry::renameFile(part, dest)) {
            error = "cannot write " + dest;
            return false;
        }
        return true;
    }
    bool fetchCatalog(const string &rel, string &text, string &error) {
        return dl.fetchText(opt.repoUrl + "/" + rel, scratch + "/catalog.json", text, error);
    }
    // a tarball's contents under `dest`, the progress on the bar
    bool untar(const string &tarball, const string &dest, string &error,
               const TarArchive::Filter &filter = TarArchive::Filter(), const string &prefix = "") {
        return TarArchive::extract(
            tarball, dest, error, filter, [this](uint64_t done, uint64_t total) { out.onProgress(done, total); },
            prefix);
    }

    //******************
    // 1. the package
    //******************
    bool package(string &error) {
        phase("Reading the package");
        if (opt.packageFile.empty() || !DirEntry::exists(opt.packageFile)) {
            error = "No package: autobleem-psc-<version>.tar.gz should sit next to the installer";
            return false;
        }
        vector<TarEntry> entries;
        if (!TarArchive::list(opt.packageFile, entries, error))
            return false;
        bool launcher = false;
        for (const TarEntry &e : entries) {
            if (e.name == LauncherBinary)
                launcher = true;
            if (e.isDir && e.name.rfind("Themes/", 0) == 0 && e.name.find('/', 7) == string::npos)
                shippedThemes.push_back(e.name.substr(7));
        }
        if (!launcher) {
            error = opt.packageFile + " is not an AutoBleem package (no " + string(LauncherBinary) + ")";
            return false;
        }
        say("  " + opt.packageFile + ": " + info.packageVersion + ", " + to_string(entries.size()) + " entries");
        return true;
    }

    //******************
    // 2. the stick
    //******************
    // an update: what the package ships goes, everything of the user's stays
    bool prepare(string &error) {
        phase(info.installed ? "Preparing the update" : "Preparing the stick");
        if (!DirEntry::isDirectory(root)) {
            error = "No such drive: " + root;
            return false;
        }
        if (stopped(error))
            return false;
        if (info.installed) {
            say("  AutoBleem " + (info.installedVersion.empty() ? string("(unknown version)") : info.installedVersion) +
                " is on the stick - updating to " + info.packageVersion);
            savedConfig = readText(at(ConfigIni));
            for (const char *dir : {"Autobleem/bin/autobleem", "Autobleem/bin/emu", "Autobleem/bin/emunxt",
                                    "Autobleem/rc", "Apps/pscbios", "Apps/abflashkit", "Docs"}) {
                if (DirEntry::isDirectory(at(dir)))
                    DirEntry::removeDirAndContents(at(dir));
            }
            for (const string &theme : shippedThemes)
                if (DirEntry::isDirectory(at("Themes/" + theme)))
                    DirEntry::removeDirAndContents(at("Themes/" + theme));
            DirEntry::removeFile(at("Autobleem/lib/libs.tar.gz"));
            DirEntry::removeFile(at("Autobleem/start.sh"));
            DirEntry::removeFile(at(VersionFile));
        } else {
            say("  A fresh install of " + info.packageVersion + " onto " + root);
        }
        return true;
    }

    //******************
    // 2b. an old stick's layout
    //******************
    bool legacy(string &error) {
        if (!info.legacyLayout)
            return true;
        phase("Bringing the old layout up to date");
        if (stopped(error))
            return false;
        say("  an AutoBleem 1.0 / NG stick: RetroArch, the ROMs and the themes move to where the launcher looks now -");
        say("  games, save states, memory cards, ROMs and RetroArch's saves stay");
        if (!LegacyLayout::migrate(root, [this](const string &line) { say(line); }, error))
            return false;
        say("  the playlists are rebuilt by the launcher's first scan (or UpdateRoms, for box art from the PC)");
        return true;
    }

    //******************
    // 3. AutoBleem itself
    //******************
    bool unpack(string &error) {
        phase("Unpacking AutoBleem");
        if (stopped(error))
            return false;
        if (!untar(opt.packageFile, root, error))
            return false;
        if (!savedConfig.empty()) {
            writeText(at(ConfigIni), savedConfig);
            say("  config.ini kept as it was");
        }
        for (const char *dir : {"Games", "Games/!SaveStates", "Games/!MemCards", "System", "System/Databases",
                                "System/Logs", "Apps", "Themes"})
            DirEntry::createDirs(at(dir));
        say("  done");
        return true;
    }

    //******************
    // 3b. UpdateRoms
    //******************
    // the PC-side ROM scanner, into <stick>/UpdateRoms/ from the release this package belongs to - the
    // console has no network, so the ROMs' box art and names come from a PC run of it. Not having it is
    // no reason to stop.
    bool updateRoms(string &error) {
        phase("UpdateRoms");
        if (stopped(error))
            return false;
        const UpdateFile *file = nullptr;
        ReleaseCatalog unstable, stable;
        string text, why;
        if (fetchCatalog("releases/unstable.json", text, why))
            unstable.parse(text);
        if (fetchCatalog("releases/latest.json", text, why))
            stable.parse(text);
        // the release whose console package this is, else the pre-release, else the stable one
        const string mine = "autobleem-psc-" + info.packageVersion + ".tar.gz";
        for (const ReleaseCatalog *r : {&unstable, &stable}) {
            const UpdateFile *fs = r->fileFor("psc-fs");
            if (fs && fs->name == mine && r->fileFor("updateroms")) {
                file = r->fileFor("updateroms");
                break;
            }
        }
        if (!file)
            file = unstable.fileFor("updateroms") ? unstable.fileFor("updateroms") : stable.fileFor("updateroms");
        if (!file) {
            say("  no UpdateRoms package on the site - skipped (copy UpdateRoms/ onto the stick by hand for box art)");
            return true;
        }
        const string zip = scratch + "/" + file->name;
        if (!downloadVerified(*file, zip, error)) {
            say("  could not fetch " + file->name + ": " + error + " - going on without it");
            error.clear();
            return true;
        }
        if (DirEntry::isDirectory(at("UpdateRoms")))
            DirEntry::removeDirAndContents(at("UpdateRoms"));
        if (!ZipArchive::extract(zip, root))
            say("  could not unpack " + file->name + " - going on without it");
        DirEntry::removeFile(zip);
        return true;
    }

    //******************
    // 4. the cover databases
    //******************
    bool covers(string &error) {
        bool any = false;
        for (const CoverDb &c : Covers)
            any = any || opt.*c.selected;
        if (!any)
            return true;
        phase("Cover databases");
        DirEntry::createDirs(at("Autobleem/bin/db"));
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
            if (!downloadVerified(file, at(string("Autobleem/bin/db/") + c.file), error))
                return false;
        }
        return true;
    }

    //******************
    // 5. RetroArch
    //******************
    bool retroarch(string &error) {
        const string bin = at("RetroArch/bin");
        // RetroArch itself, and the theme it comes with
        phase("RetroArch");
        if (stopped(error))
            return false;
        string text;
        PscRetroArchCatalog ra;
        if (!fetchCatalog("psc/retroarch/latest.json", text, error) || !ra.parse(text)) {
            if (error.empty())
                error = "psc/retroarch/latest.json is not what was expected";
            return false;
        }
        if (info.hasRetroArch && info.retroarchVersion == ra.version) {
            say("  RetroArch " + ra.version + " is on the stick already");
        } else {
            say("  RetroArch " + ra.version);
            const string zip = scratch + "/" + ra.zip.name;
            if (!downloadVerified(ra.zip, zip, error))
                return false;
            const string unpacked = scratch + "/retroarch";
            DirEntry::removeDirAndContents(unpacked);
            if (!ZipArchive::extract(zip, unpacked)) {
                if (error.empty())
                    error = "cannot unpack " + ra.zip.name;
                return false;
            }
            for (const char *dir : {"", "Retroarch themes", "fonts", "playlists", "saves", "savestates", "screenshots",
                                    "config", "logs", "thumbnails", "downloads", "records", "cores", "info"})
                DirEntry::createDirs(bin + (*dir ? string("/") + dir : ""));
            DirEntry::createDirs(at("RetroArch/bios"));
            DirEntry::createDirs(at("RetroArch/roms"));
            struct Place {
                const char *from;
                const char *to;
            };
            for (const Place &p : {Place{"retroarch", "retroarch"}, Place{"VERSION", "VERSION"},
                                   Place{"theme/Autobleem2.png", "Retroarch themes/Autobleem2.png"},
                                   Place{"theme/selawik-light.ttf", "fonts/selawik-light.ttf"},
                                   Place{"theme/OFL.txt", "fonts/OFL.txt"}}) {
                if (!DirEntry::exists(unpacked + "/" + p.from))
                    continue;
                DirEntry::removeFile(bin + "/" + p.to);
                if (!DirEntry::copyFile(unpacked + "/" + p.from, bin + "/" + p.to)) {
                    error = "cannot write RetroArch/bin/" + string(p.to);
                    return false;
                }
            }
            themeCfg = readText(unpacked + "/theme/retroarch-psc.cfg");
            DirEntry::removeDirAndContents(unpacked);
            DirEntry::removeFile(zip);
            newBinary = true;
        }
        if (!writeRetroArchCfg(error))
            return false;

        // the cores, with their info files
        phase("RetroArch cores");
        if (!pack("psc/cores/latest.json", "cores", bin, error, TarArchive::Filter()))
            return false;
        // the libraries the apps need, and the xpad module
        phase("Runtime libraries");
        if (!pack("psc/libs/latest.json", "libs", at("Autobleem/lib"), error,
                  [](const TarEntry &e) { return e.name != "libs.json"; }))
            return false;
        // the apps
        phase("Apps");
        if (!pack("psc/apps/latest.json", "apps", root, error, [](const TarEntry &e) { return e.name != "apps.json"; }))
            return false;
        // libretro's bundles
        phase("RetroArch assets");
        for (const Bundle &b : Bundles) {
            if (stopped(error))
                return false;
            const string dest = bin + "/" + b.dest;
            if (DirEntry::isDirectory(dest) && !DirEntry::diru(dest).empty()) {
                say("  " + string(b.name) + ": already there");
                continue;
            }
            say("  " + string(b.name));
            const string zip = scratch + "/" + b.name + ".zip";
            if (!download(opt.buildbotUrl + "/" + b.name + ".zip", zip, error)) {
                say("  could not download " + string(b.name) + ".zip: " + error + " - going on without it");
                error.clear();
                continue;
            }
            if (!ZipArchive::extract(zip, dest))
                say("  could not unpack " + string(b.name) + ".zip - going on without it");
            error.clear();
            DirEntry::removeFile(zip);
        }
        return true;
    }

    // a dated pack from psc/<kind>/latest.json, unpacked under `dest`
    bool pack(const string &catalog, const string &what, const string &dest, string &error,
              const TarArchive::Filter &filter) {
        if (stopped(error))
            return false;
        string text;
        PackCatalog cat;
        if (!fetchCatalog(catalog, text, error) || !cat.parse(text)) {
            if (error.empty())
                error = catalog + " is not what was expected";
            return false;
        }
        say("  " + cat.file.name + (cat.count ? " (" + to_string(cat.count) + " " + what + ")" : ""));
        const string tarball = scratch + "/" + cat.file.name;
        if (!downloadVerified(cat.file, tarball, error))
            return false;
        DirEntry::createDirs(dest);
        bool ok = untar(tarball, dest, error, filter);
        DirEntry::removeFile(tarball);
        return ok;
    }

    // retroarch.cfg, only when there is none: RetroArch keeps it up to date itself and the launcher edits
    // a few keys around each launch - both must keep what the user has set since
    bool writeRetroArchCfg(string &error) {
        const string cfg = at("RetroArch/bin/retroarch.cfg");
        if (DirEntry::exists(cfg)) {
            // an existing cfg is the user's - but a new RetroArch build brings keys the old one gets wrong
            // (a RetroBoot-era cfg on 1.22.2: the XMB theme enum, quit_on_close_content, the front buttons):
            // the build's own keys go over it, everything else stays
            if (newBinary && !themeCfg.empty()) {
                string text = readText(cfg);
                int set = 0;
                istringstream in(themeCfg);
                string line;
                while (getline(in, line)) {
                    string t = trimmed(line);
                    if (t.empty() || t[0] == '#')
                        continue;
                    size_t eq = t.find('=');
                    if (eq == string::npos)
                        continue;
                    const string key = trimmed(t.substr(0, eq));
                    // the key's line, wherever it is, replaced; appended when there is none
                    size_t pos = 0;
                    bool found = false;
                    while (pos < text.size()) {
                        size_t end = text.find('\n', pos);
                        if (end == string::npos)
                            end = text.size();
                        string existing = text.substr(pos, end - pos);
                        string k = trimmed(existing.substr(0, existing.find('=')));
                        if (existing.find('=') != string::npos && k == key) {
                            text.replace(pos, end - pos, t);
                            found = true;
                            break;
                        }
                        pos = end + 1;
                    }
                    if (!found) {
                        if (!text.empty() && text.back() != '\n')
                            text += "\n";
                        text += t + "\n";
                    }
                    set++;
                }
                if (!writeText(cfg, text)) {
                    error = "cannot write " + cfg;
                    return false;
                }
                say("  retroarch.cfg kept, " + to_string(set) + " keys of this RetroArch build set in it");
            } else {
                say("  keeping the existing retroarch.cfg");
            }
            return true;
        }
        string text =
            "# Written by AutoBleem's installer. RetroArch keeps this file up to date itself; AutoBleem edits\n"
            "# a few display keys around each launch. Every directory lives under RetroArch/bin (\":/\" is\n"
            "# this file's folder), the BIOS files under RetroArch/bios, the games under RetroArch/roms.\n"
            "libretro_directory = \":/cores\"\n"
            "libretro_info_path = \":/info\"\n"
            "system_directory = \"/media/RetroArch/bios\"\n"
            "rgui_browser_directory = \"/media/RetroArch/roms/\"\n"
            "core_assets_directory = \":/downloads\"\n"
            "savefile_directory = \":/saves\"\n"
            "savestate_directory = \":/savestates\"\n"
            "playlist_directory = \":/playlists\"\n"
            "content_database_path = \":/database/rdb\"\n"
            "cursor_directory = \":/database/cursors\"\n"
            "cheat_database_path = \":/cheats\"\n"
            "assets_directory = \":/assets\"\n"
            "joypad_autoconfig_dir = \":/autoconfig\"\n"
            "overlay_directory = \":/overlays\"\n"
            "video_shader_dir = \":/shaders\"\n"
            "thumbnails_directory = \":/thumbnails\"\n"
            "screenshot_directory = \":/screenshots\"\n"
            "recording_output_directory = \":/records\"\n"
            "recording_config_directory = \":/records\"\n"
            "rgui_config_directory = \":/config\"\n"
            "core_options_path = \":/config/retroarch-core-options.cfg\"\n"
            "global_core_options = \"true\"\n"
            "log_dir = \":/logs\"\n"
            "cache_directory = \"/tmp/ra_cache\"\n"
            "video_fullscreen = \"true\"\n"
            "input_autodetect_enable = \"true\"\n"
            "menu_show_core_updater = \"false\"\n";
        // the build's own keys (the XMB theme, the front buttons, quit_on_close_content...), comments out
        istringstream in(themeCfg);
        string line;
        while (getline(in, line)) {
            string t = trimmed(line);
            if (!t.empty() && t[0] != '#')
                text += t + "\n";
        }
        if (!writeText(cfg, text)) {
            error = "cannot write " + cfg;
            return false;
        }
        say("  retroarch.cfg written");
        return true;
    }

    //******************
    // 6. the BIOS files
    //******************
    bool bios(string &error) {
        phase("BIOS files");
        if (stopped(error))
            return false;
        string text;
        PackCatalog cat;
        if (!fetchCatalog("psc/bios/latest.json", text, error) || !cat.parse(text)) {
            if (error.empty())
                error = "psc/bios/latest.json is not what was expected";
            return false;
        }
        string list;
        if (!dl.fetchText(cat.file.url, scratch + "/biospack.txt", list, error))
            return false;
        struct Item {
            string sha, url, path;
            uint64_t size;
        };
        vector<Item> items;
        istringstream in(list);
        string line;
        while (getline(in, line)) {
            if (line.empty() || line[0] == '#')
                continue;
            if (!line.empty() && line.back() == '\r')
                line.pop_back();
            // <sha256> <size> <url> <path> - the path may contain spaces
            size_t a = line.find(' '), b = line.find(' ', a + 1), c = line.find(' ', b + 1);
            if (a == string::npos || b == string::npos || c == string::npos)
                continue;
            Item it;
            it.sha = line.substr(0, a);
            it.size = strtoull(line.substr(a + 1, b - a - 1).c_str(), nullptr, 10);
            it.url = line.substr(b + 1, c - b - 1);
            it.path = line.substr(c + 1);
            if (TarArchive::isSafeName(it.path))
                items.push_back(it);
        }
        say("  " + to_string(items.size()) + " files, " + humanSize(cat.totalBytes) +
            " - what is there already is kept");
        const string dir = at("RetroArch/bios");
        int fetched = 0, kept = 0, failed = 0;
        for (size_t i = 0; i < items.size(); i++) {
            if (stopped(error))
                return false;
            const Item &it = items[i];
            out.onProgress(i, items.size());
            const string dest = dir + "/" + it.path;
            if (DirEntry::exists(dest) && static_cast<uint64_t>(DirEntry::fileSize(dest)) == it.size &&
                Sha256::ofFile(dest) == it.sha) {
                kept++;
                continue;
            }
            DirEntry::createDirs(dest.substr(0, dest.find_last_of('/')));
            const string part = dest + ".part";
            string why;
            bool ok = dl.fetch(it.url, part, [this](uint64_t, uint64_t) { return !(stop && stop()); }, why);
            if (ok && Sha256::ofFile(part) != it.sha) {
                ok = false;
                why = "checksum mismatch";
            }
            if (ok) {
                DirEntry::removeFile(dest);
                ok = DirEntry::renameFile(part, dest);
            }
            if (ok) {
                fetched++;
            } else {
                DirEntry::removeFile(part);
                failed++;
                if (failed <= 20)
                    say("  could not fetch " + it.path + ": " + why);
                if (stop && stop()) {
                    error = "Stopped";
                    return false;
                }
            }
            if ((fetched + failed) % 50 == 0 && fetched + failed > 0)
                say("  " + to_string(fetched) + " fetched, " + to_string(kept) + " kept, " + to_string(failed) +
                    " failed so far");
        }
        out.onProgress(items.size(), items.size());
        say("  " + to_string(fetched) + " fetched, " + to_string(kept) + " already there, " + to_string(failed) +
            " failed");
        return true;
    }

    //******************
    // 7. the sample games
    //******************
    bool samples(string &error) {
        phase("Sample games");
        if (stopped(error))
            return false;
        if (DirEntry::exists(at(SamplesMarker))) {
            say("  the samples were put on this stick before (System/samples.txt) - not again");
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
        // Games/ and SAMPLES.md as they are; the RetroArch part only with RetroArch on the stick, and laid
        // out the console's way: the pack's RetroArch/roms is RetroArch/roms, its RetroArch/thumbnails is
        // RetroArch/bin/thumbnails
        bool ok = untar(tarball, root, error, [](const TarEntry &e) { return e.name.rfind("RetroArch/", 0) != 0; });
        const bool withRetroArch = opt.retroarch || info.hasRetroArch;
        if (ok && withRetroArch)
            ok = untar(tarball, at("RetroArch/roms"), error, TarArchive::Filter(), "RetroArch/roms/") &&
                 untar(tarball, at("RetroArch/bin/thumbnails"), error, TarArchive::Filter(), "RetroArch/thumbnails/");
        DirEntry::removeFile(tarball);
        if (!ok)
            return false;
        writeText(at(SamplesMarker), cat.file.name + "\n");
        say(string("  done") + (withRetroArch ? "" : " (the PlayStation game; the other systems' need RetroArch)"));
        return true;
    }

    void finish() {
        phase("Finishing");
        out.onProgress(1, 1);
        say(info.installed ? "Updated. Put the stick into the console's second controller port and boot it."
                           : "Installed. Put the stick into the console's second controller port and boot it.");
    }

    const InstallOptions &opt;
    const StickInfo &info;
    Downloader &dl;
    InstallListener &out;
    InstallerJob::ShouldStop stop;
    string root, scratch;
    vector<string> phases;
    int phaseIndex = 0;
    vector<string> shippedThemes;
    string savedConfig;
    string themeCfg;
    bool newBinary = false;
};

} // namespace

//*******************************
// Downloader::fetchText
//*******************************
bool Downloader::fetchText(const string &url, const string &scratchFile, string &text, string &error) {
    DirEntry::createDirs(scratchFile.substr(0, scratchFile.find_last_of('/')));
    if (!fetch(url, scratchFile, Progress(), error))
        return false;
    text = readText(scratchFile);
    DirEntry::removeFile(scratchFile);
    return true;
}

//*******************************
// InstallerJob::normalizeRoot
//*******************************
string InstallerJob::normalizeRoot(const string &input) {
    string root = input;
    replace(root.begin(), root.end(), '\\', '/');
    while (root.size() > 1 && root.back() == '/' && !(root.size() == 3 && root[1] == ':'))
        root.pop_back();
    if (root.size() == 2 && root[1] == ':')
        root += "/";
    return root;
}

//*******************************
// InstallerJob::packageNextTo
//*******************************
string InstallerJob::packageNextTo(const string &programPath) {
    string dir = programPath;
    replace(dir.begin(), dir.end(), '\\', '/');
    size_t slash = dir.find_last_of('/');
    dir = slash == string::npos ? "." : dir.substr(0, slash);
    string best;
    for (const ableem::DirEntry &e : DirEntry::diru_FilesOnly(dir)) {
        if (e.name.rfind("autobleem-psc-", 0) == 0 && e.name.size() > 7 &&
            e.name.compare(e.name.size() - 7, 7, ".tar.gz") == 0 && e.name > best)
            best = e.name;
    }
    return best.empty() ? "" : dir + "/" + best;
}

//*******************************
// InstallerJob::inspect
//*******************************
StickInfo InstallerJob::inspect(const InstallOptions &options) {
    StickInfo info;
    const string root = normalizeRoot(options.root);
    info.isStick = !root.empty() && DirEntry::isDirectory(root);
    if (info.isStick) {
        info.installed = DirEntry::exists(root + "/" + LauncherBinary);
        info.installedVersion = firstLine(readText(root + "/" + VersionFile));
        info.legacyLayout = LegacyLayout::detect(root);
        info.hasRetroArch = DirEntry::exists(root + "/RetroArch/bin/retroarch") ||
                            (info.legacyLayout && DirEntry::exists(root + "/retroarch/retroarch"));
        info.retroarchVersion = firstLine(readText(root + "/RetroArch/bin/VERSION"));
        for (size_t i = 0; i < 3; i++)
            info.hasCovers[i] = DirEntry::exists(root + "/Autobleem/bin/db/" + Covers[i].file);
    }
    if (!options.packageFile.empty()) {
        string data, error;
        if (TarArchive::readEntry(options.packageFile, VersionFile, data, error))
            info.packageVersion = firstLine(data);
        else
            info.error = error;
    } else {
        info.error = "no package next to the installer";
    }
    return info;
}

//*******************************
// InstallerJob::phasesFor
//*******************************
vector<string> InstallerJob::phasesFor(const InstallOptions &options, const StickInfo &info) {
    vector<string> phases{"Reading the package", info.installed ? "Preparing the update" : "Preparing the stick"};
    if (info.legacyLayout)
        phases.push_back("Bringing the old layout up to date");
    phases.push_back("Unpacking AutoBleem");
    phases.push_back("UpdateRoms");
    if (options.coversJapan || options.coversUsa || options.coversPal)
        phases.push_back("Cover databases");
    if (options.retroarch)
        for (const char *p : {"RetroArch", "RetroArch cores", "Runtime libraries", "Apps", "RetroArch assets"})
            phases.push_back(p);
    if (options.bios && (options.retroarch || info.hasRetroArch))
        phases.push_back("BIOS files");
    if (options.samples)
        phases.push_back("Sample games");
    phases.push_back("Finishing");
    return phases;
}

//*******************************
// InstallerJob::run
//*******************************
bool InstallerJob::run(const InstallOptions &input, Downloader &downloader, InstallListener &listener,
                       const ShouldStop &shouldStop, string &error) {
    InstallOptions options = input;
    options.root = normalizeRoot(options.root);
    StickInfo info = inspect(options);
    if (!info.isStick) {
        error = "No such drive: " + options.root;
        return false;
    }
    if (info.packageVersion.empty()) {
        error = info.error.empty() ? "The package has no VERSION" : info.error;
        return false;
    }
    Run run(options, info, downloader, listener, shouldStop);
    return run.go(error);
}
