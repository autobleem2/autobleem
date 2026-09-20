//
// InstallerJob: AutoBleem onto a PlayStation Classic stick from a PC - the job behind the installer window
// and its --quiet line. The stick's file system comes from the package next to the program
// (autobleem-psc-<version>.tar.gz, the release's tarball: the launcher, pcsx-ab, the scripts, the themes,
// the console tools); everything else from the download repository, as asked: the cover databases (db/),
// RetroArch with its cores, libraries, apps and libretro's asset bundles (psc/*, buildbot.libretro.com),
// the BIOS files by the site's list (psc/bios -> RetroBIOS), the sample games (samples/).
//
// An update is the same run over a stick that has AutoBleem already: what the package ships is replaced
// (Autobleem/, the two console tools under Apps/, the shipped themes, Docs/), everything the user has -
// games, save states, memory cards, System/, RetroArch's saves, playlists and thumbnails, other apps and
// themes - is left as it is, and config.ini keeps the user's settings.
//
// No SDL and nothing of the launcher's UI: ab_core only, a Downloader the program provides (WinINet on
// Windows, a fake in the tests), a listener for the phases, the progress and the lines.
//
#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

//******************
// InstallOptions
//******************
struct InstallOptions {
    std::string root;        // the stick's root: "F:" / "F:/" / "/media/me/SONY"
    std::string packageFile; // autobleem-psc-<version>.tar.gz
    std::string repoUrl = "https://autobleem.retromenele.pl";
    std::string buildbotUrl = "https://buildbot.libretro.com/assets/frontend"; // libretro's bundles
    std::string scratchDir; // where downloads land before they are unpacked; "" = <root>/System/Install
    bool coversJapan = true;
    bool coversUsa = true;
    bool coversPal = true;
    bool retroarch = false;
    bool bios = false; // needs retroarch (or RetroArch already on the stick)
    bool samples = false;
};

//******************
// StickInfo
//******************
// what InstallerJob::inspect finds: is this an update, of what
struct StickInfo {
    bool isStick = false;                      // the root exists and can be written to
    bool installed = false;                    // Autobleem/bin/autobleem/autobleem-gui is there
    bool legacyLayout = false;                 // an AutoBleem 1.0 / NG / RetroBoot stick: retroarch/, roms/ at the root
    std::string installedVersion;              // the stick's VERSION file, "" when none
    bool hasRetroArch = false;                 // RetroArch/bin/retroarch
    std::string retroarchVersion;              // RetroArch/bin/VERSION
    bool hasCovers[3] = {false, false, false}; // J, U, P
    std::string packageVersion;                // the package's VERSION entry
    std::string error;
};

//******************
// Downloader
//******************
// fetches one URL into a file; the progress callback (bytes so far, total or 0 when unknown) returns
// false to abort. `error` says why on a failure. Never throws.
class Downloader {
public:
    using Progress = std::function<bool(uint64_t done, uint64_t total)>;
    virtual ~Downloader() = default;
    virtual bool fetch(const std::string &url, const std::string &destFile, const Progress &progress,
                       std::string &error) = 0;
    // a small text file (a latest.json, a .sha256 sidecar)
    bool fetchText(const std::string &url, const std::string &scratchFile, std::string &text, std::string &error);
};

//******************
// InstallListener
//******************
class InstallListener {
public:
    virtual ~InstallListener() = default;
    virtual void onPhase(int index, int total, const std::string &title) = 0; // 1-based
    virtual void onProgress(uint64_t done, uint64_t total) = 0;               // within the phase; 0/0 = unknown
    virtual void onLine(const std::string &line) = 0;
};

//******************
// InstallerJob
//******************
class InstallerJob {
public:
    using ShouldStop = std::function<bool()>;

    // the package next to the program: the newest autobleem-psc-*.tar.gz in argv[0]'s directory, "" if none
    static std::string packageNextTo(const std::string &programPath);
    // the stick and the package as they are
    static StickInfo inspect(const InstallOptions &options);
    // the phases the options ask for, for a preview (the listener gets the same titles)
    static std::vector<std::string> phasesFor(const InstallOptions &options, const StickInfo &info);

    // the install; false with `error` on the first failure that cannot be gone past (a lost download of
    // one BIOS file is reported and skipped, a bad package is not)
    static bool run(const InstallOptions &options, Downloader &downloader, InstallListener &listener,
                    const ShouldStop &shouldStop, std::string &error);

    // "F:" -> "F:/", "/x/" -> "/x", backslashes forward
    static std::string normalizeRoot(const std::string &root);
};
