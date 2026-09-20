//
// WindowsInstallJob: the Windows product's data tree, filled in from the download repository - the job
// behind AutoBleemWinSetup.exe, which the NSIS installer runs after it has put the program in place (and
// which a user can run again from the Start Menu to add RetroArch or the BIOS files later). The program
// folder is the installer's business; this job owns everything under the data root
// (Documents\AutoBleem by default - see EnvironmentSetup::fromWindowsInstall for how the launcher finds
// it): the folders, the shipped themes copied in once, the cover databases, RetroArch (libretro's own
// Windows build - the site's tarball of it, else the official .7z), its cores (the site's pack, else one
// zip per core from buildbot), the BIOS files by the site's list, the sample games.
//
// An update is the same run over a data tree that has AutoBleem already: nothing of the user's is
// touched (games, saves, RetroArch's saves, playlists and thumbnails, config.ini), the two scan
// fingerprints are removed so the launcher rescans once, and what was chosen before is refreshed.
//
// No SDL and nothing of the launcher's UI: ab_core only, a Downloader the program provides (WinINet on
// Windows, a fake in the tests), a listener for the phases, the progress and the lines.
//
#pragma once

#include "core/installer_job.h" // Downloader, InstallListener, InstallerJob::ShouldStop

#include <string>
#include <vector>

//******************
// WindowsInstallOptions
//******************
struct WindowsInstallOptions {
    std::string programDir; // where the launcher was put: <programDir>/Themes are the shipped themes
    std::string dataRoot;   // the data tree: Documents\AutoBleem
    std::string repoUrl = "https://autobleem.retromenele.pl";
    std::string buildbotUrl = "https://buildbot.libretro.com"; // libretro's: the fallbacks come from here
    std::string retroarchFallbackVersion = "1.22.2";           // the official .7z taken when the site has no build
    std::string scratchDir;                                    // "" = <dataRoot>/System/Install
    bool coversJapan = true;
    bool coversUsa = true;
    bool coversPal = true;
    bool retroarch = false;
    bool bios = false; // needs retroarch (or RetroArch already in the data tree)
    bool samples = false;
    bool update = false; // an update of the program: the fingerprints go, so the launcher rescans once
};

//******************
// WindowsInstallInfo
//******************
struct WindowsInstallInfo {
    bool exists = false;                       // the data root is there and can be written to
    bool installed = false;                    // System/config.ini: the launcher has run here
    bool hasRetroArch = false;                 // RetroArch/bin/retroarch.exe
    std::string retroarchVersion;              // RetroArch/bin/VERSION, "" when unknown
    bool hasCovers[3] = {false, false, false}; // J, U, P
    std::string error;
};

//******************
// WindowsInstallJob
//******************
class WindowsInstallJob {
public:
    static WindowsInstallInfo inspect(const WindowsInstallOptions &options);
    // the phases the options ask for, for a preview (the listener gets the same titles)
    static std::vector<std::string> phasesFor(const WindowsInstallOptions &options, const WindowsInstallInfo &info);

    // the install; false with `error` on the first failure that cannot be gone past
    static bool run(const WindowsInstallOptions &options, Downloader &downloader, InstallListener &listener,
                    const InstallerJob::ShouldStop &shouldStop, std::string &error);

    // the retroarch.cfg the job writes into RetroArch/bin when there is none: full screen, back to the
    // launcher when content closes, the roms folder as the browser's start
    static std::string retroArchConfigText(const std::string &dataRoot);
};
