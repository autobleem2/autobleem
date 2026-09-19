//
// UpdateRomsJob: what UpdateRoms does to a stick (or a Pi's SD card) sitting in a PC - the ROM scan the
// target's own launcher would do, with the network the PC has and the target may not.
//
#pragma once

#include "core/services/online_assets.h"

#include <ableem/engine/game_scanner.h>

#include <functional>
#include <string>
#include <vector>

//******************
// UpdateRomsJob
//******************
// detect() reads the stick: which platform it is for (RetroBoot's folder is the console's mark, the
// Pi installer's retroarch.cfg the Pi's), that platform's resources/platform/<target>.ini for where RetroArch and the
// ROM folders are and what the target calls its root (usb_root - the prefix the playlists must carry,
// never this PC's drive letter), and the PC's own pc.ini for how to download. It configures Env for the
// engine on the way, so the same CoreInfoTable, RetroArchScanner and OnlineAssets the launcher's scan
// uses run unchanged; run() is that scan: the databases bundle if there is none, the folders into
// playlists named as the target sees them, box art for what lacks one. Nothing of the console's own -
// PS1 games, its databases, themes - is touched.
class UpdateRomsJob {
public:
    struct Setup {
        std::string root;         // the stick's root as this machine sees it ("E:/", "/media/usb0")
        std::string target;       // "psc" or "rpi"
        std::string targetRoot;   // usb_root from the target's ini
        std::string resourcesDir; // <root>/Autobleem/bin/autobleem
        std::string retroarchDir, romsDir, playlistsDir, rdbDir, thumbnailsDir;
        std::string targetRetroarchDir, targetRomsDir; // the same two as the target names them
        std::string coresCfg, aliasesCfg;
        std::string downloadCommand; // the PC's
    };

    struct Report {
        bool online = false;
        int databases = 0; // .rdb files in place after the run
        int systems = 0, games = 0, identified = 0;
        std::vector<std::string> playlistsWritten;
        std::vector<std::string> unknownFolders;
        int boxArtFetched = 0, boxArtMissing = 0;
        std::vector<std::string> lines; // the log the window shows, one per system and per pass
    };

    // false with `error` set when `root` is not an AutoBleem stick (no Autobleem/bin/autobleem) or has no
    // RetroArch; target ("psc"/"rpi") overrides the detection, "" detects
    static bool detect(const std::string &root, Setup &setup, std::string &error, const std::string &target = "");

    // the stick's root from where the program is: the first ancestor with Autobleem/bin/autobleem in it,
    // "" when there is none (a copy of the exe run from elsewhere)
    static std::string rootFromProgramPath(const std::string &programPath);

    using LineSink = std::function<void(const std::string &)>;

    // the whole run; progress through the listener (the scanner's stages, FetchingBoxArt), shouldStop()
    // polled between the passes and per box art, each report line handed to `say` as it happens (they
    // are in Report::lines too). runner is the tests' fake network.
    static Report run(const Setup &setup, ableem::ScanProgressListener *listener,
                      const std::function<bool()> &shouldStop, const LineSink &say = LineSink(),
                      OnlineAssets::CommandRunner runner = OnlineAssets::CommandRunner());
};
