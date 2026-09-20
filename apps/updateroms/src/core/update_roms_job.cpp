//
// UpdateRomsJob - see the header.
//
#include "update_roms_job.h"

#include "core/main.h"
#include "core/services/environment.h"
#include "core/services/environment_setup.h"
#include "core/services/platform_config.h"

#include <ableem/engine/log.h>
#include <ableem/engine/retroarch_cores.h>
#include <ableem/engine/retroarch_scanner.h>

using namespace std;

namespace {

// a relative path is taken from `base`; an absolute one stands (as PlatformConfig resolves its keys)
string under(const string &base, const string &path) {
    if (path.empty())
        return base;
    if (path[0] == '/' || (path.size() > 1 && path[1] == ':'))
        return path;
    return base + sep + path;
}

string normalized(string path) {
    for (char &c : path) {
        if (c == '\\')
            c = '/';
    }
    return DirEntry::removeSeparatorFromEndOfPath(path);
}

} // namespace

//*******************************
// UpdateRomsJob::rootFromProgramPath
//*******************************
string UpdateRomsJob::rootFromProgramPath(const string &programPath) {
    // the program's own folder, then each parent, up to the drive or "/": "E:/UpdateRoms/UpdateRoms.exe"
    // -> "E:/UpdateRoms" -> "E:"
    string dir = normalized(programPath);
    while (true) {
        size_t slash = dir.find_last_of('/');
        if (slash == string::npos)
            return "";
        dir = dir.substr(0, slash);
        if (DirEntry::isDirectory(dir + "/Autobleem/bin/autobleem"))
            return dir.empty() ? "/" : dir;
        if (dir.empty() || (dir.size() == 2 && dir[1] == ':'))
            return ""; // that was the root
    }
}

//*******************************
// UpdateRomsJob::detect
//*******************************
bool UpdateRomsJob::detect(const string &root, Setup &setup, string &error, const string &target) {
    setup = Setup();
    setup.root = normalized(root);
    setup.resourcesDir = setup.root + sep + "Autobleem/bin/autobleem";
    if (!DirEntry::isDirectory(setup.resourcesDir)) {
        error = "Not an AutoBleem stick: no Autobleem/bin/autobleem under " + setup.root;
        return false;
    }
    // which platform the stick is for: both keep RetroArch under RetroArch/, but the console's tree is one
    // level down (RetroArch/bin, with bios/ and roms/ beside it) and the Pi's is RetroArch's own standard
    // tree, with the retroarch.cfg the Pi installer generates at its top. The platform ini says the rest.
    if (!target.empty())
        setup.target = target;
    else if (DirEntry::isDirectory(setup.root + sep + "RetroArch/bin"))
        setup.target = "psc";
    else if (DirEntry::exists(setup.root + sep + "RetroArch/retroarch.cfg"))
        setup.target = "rpi";
    else
        setup.target = "psc";

    // the engine's roots as the launcher would have them on this stick, then the target's RetroArch
    // layout over the PC's, and the PC's way of downloading
    EnvironmentSetup::fromRoot(setup.root);
    PlatformConfig targetCfg = PlatformConfig::load(PlatformConfig::pathFor(setup.resourcesDir, setup.target));
    targetCfg.apply();
    PlatformConfig pc = PlatformConfig::load(PlatformConfig::pathFor(setup.resourcesDir, "pc"));
    Env::setDownloadCommand(pc.downloadCommand);

    setup.targetRoot = targetCfg.usbRoot.empty() ? "/media" : targetCfg.usbRoot;
    setup.retroarchDir = Env::getPathToRetroarchDir();
    setup.romsDir = Env::getPathToRetroarchRomsDir();
    setup.playlistsDir = Env::getPathToRetroarchPlaylistsDir();
    setup.rdbDir = Env::getPathToRetroarchRdbDir();
    setup.thumbnailsDir = Env::getPathToRetroarchThumbnailsDir();
    setup.targetRetroarchDir = under(setup.targetRoot, targetCfg.retroarchDir);
    setup.targetRomsDir = under(setup.targetRoot, targetCfg.retroarchRomsDir);
    setup.coresCfg = setup.resourcesDir + sep + "platform" + sep + setup.target + ".cores.cfg";
    setup.aliasesCfg = setup.resourcesDir + sep + "platform" + sep + "roms_folders.cfg";
    setup.downloadCommand = pc.downloadCommand;

    if (!DirEntry::isDirectory(setup.retroarchDir)) {
        error = "No RetroArch on this stick (" + setup.retroarchDir + ") - nothing to do";
        return false;
    }
    return true;
}

//*******************************
// UpdateRomsJob::run
//*******************************
UpdateRomsJob::Report UpdateRomsJob::run(const Setup &setup, ableem::ScanProgressListener *listener,
                                         const function<bool()> &shouldStop, const LineSink &sink,
                                         OnlineAssets::CommandRunner runner) {
    Report report;
    auto say = [&](const string &line) {
        report.lines.push_back(line);
        PLOG_INFO << line;
        if (sink)
            sink(line);
    };
    say("Stick at " + setup.root + " for the " + (setup.target == "rpi" ? "Raspberry Pi" : "PlayStation Classic") +
        " (playlists will say " + setup.targetRoot + ")");

    OnlineAssets::Config onlineConfig;
    onlineConfig.downloadCommand = setup.downloadCommand;
    OnlineAssets online(onlineConfig, std::move(runner));
    report.online = online.enabled() && online.probe();
    say(report.online ? "Online: libretro's servers answer" : "Offline: file names only, no box art");
    if (shouldStop && shouldStop())
        return report;

    // the databases, so the scan can name the games
    if (report.online)
        report.databases = online.ensureDatabases(setup.rdbDir);
    else {
        for (const DirEntry &e : DirEntry::diru_FilesOnly(setup.rdbDir))
            report.databases += DirEntry::matchExtension(e.name, "rdb") ? 1 : 0;
    }
    say(to_string(report.databases) + " databases in " + setup.rdbDir);

    // the scan, exactly the launcher's, with the target's paths in the playlists
    ableem::CoreInfoTable cores;
    cores.load(setup.retroarchDir, setup.coresCfg);
    say(to_string(cores.cores().size()) + " cores installed, " + to_string(cores.databases().size()) + " systems");

    ableem::RetroArchScanner::Options options;
    options.romsDir = setup.romsDir;
    options.playlistsDir = setup.playlistsDir;
    options.targetRomsDir = setup.targetRomsDir;
    options.folderAliases = ableem::RetroArchScanner::loadFolderAliases(setup.aliasesCfg);
    options.rdbDir = report.databases > 0 ? setup.rdbDir : "";
    // the cores as the target will find them, not as this PC sees the stick
    ableem::RetroArchSystems systems = ableem::RetroArchScanner::systemsFrom(cores);
    const string coresHere = setup.retroarchDir + "/";
    for (ableem::RetroArchSystem &system : systems) {
        if (system.corePath.compare(0, coresHere.size(), coresHere) == 0)
            system.corePath = setup.targetRetroarchDir + "/" + system.corePath.substr(coresHere.size());
    }
    ableem::RetroArchScanner scanner(listener);
    ableem::RetroArchScanResult result = scanner.scan(options, systems);
    report.systems = result.systemsScanned;
    report.games = result.gamesFound;
    report.identified = result.gamesIdentified;
    report.playlistsWritten = result.playlistsWritten;
    report.unknownFolders = result.unknownFolders;
    say(to_string(result.gamesFound) + " games in " + to_string(result.systemsScanned) + " systems, " +
        to_string(result.gamesIdentified) + " named by the databases, " + to_string(result.playlistsWritten.size()) +
        " playlists written");
    for (const string &folder : result.unknownFolders)
        say("No core plays " + folder + " - skipped");
    if (shouldStop && shouldStop())
        return report;

    if (report.online && !result.games.empty()) {
        report.boxArtFetched =
            online.fetchMissingBoxArt(result.games, setup.thumbnailsDir, listener, shouldStop, &report.boxArtMissing);
        say(to_string(report.boxArtFetched) + " covers fetched, " + to_string(report.boxArtMissing) +
            " not on the server");
    }
    return report;
}
