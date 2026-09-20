//
// AutoBleem: the program - App plus the loop that runs it.
//
#include "autobleem.h"
#include "core/services/system.h"
#include <ctime>
#include "evoui/screens/evoui_launcher.h"

#include <cstdlib>
#include <iostream>
#include <unistd.h>
#include <ableem/engine/log.h>
#include <ableem/engine/update_catalog.h>
#include "core/version.h"

using namespace std;

//*******************************
// AutoBleem::AutoBleem
//*******************************
AutoBleem::AutoBleem() : App(makeProcessRunner()) {}

#ifdef AB_DEBUG_HOST
namespace {
// On a PC there is no rc/launch.sh to run and no emulator behind it: show what would have happened and
// come straight back, which is what the interceptors' #ifdef used to do.
class SplashProcessRunner : public ProcessRunner {
public:
    void run(const LaunchPlan &plan) override {
        PLOG_INFO << "would run " << plan.toString();
        Gui::splash("I'm sorry Dave.  I'm afraid I can't do that.");
    }
    bool needsExclusiveDisplay() const override { return false; } // it draws on the launcher's own window
};
} // namespace
#endif

//*******************************
// AutoBleem::makeProcessRunner
//*******************************
unique_ptr<ProcessRunner> AutoBleem::makeProcessRunner() {
#if defined(AB_DEBUG_HOST)
    return unique_ptr<ProcessRunner>(new SplashProcessRunner());
#elif defined(AB_PLATFORM_WIN)
    return unique_ptr<ProcessRunner>(new WinProcessRunner());
#else
    return unique_ptr<ProcessRunner>(new ForkProcessRunner());
#endif
}

//*******************************
// AutoBleem::openLibrary
//*******************************
bool AutoBleem::openLibrary() {
    if (!gameLibrary.openCoversAndUsbGames()) {
        return false;
    }

#ifdef AB_PLATFORM_PSC
    // if the /System/Databases/internal.db doesn't exist make a copy from the PSC
    PLOG_INFO << "Importing internal games from PSC to USB";
    System::execUnixCommand((Env::getPathToRCDir() + sep + "backup_internal.sh").c_str());
#endif

    // off the console this opens (and so creates) an empty internal.db: nothing ever queries it - the internal
    // sets are unreachable there - but GameCatalogService and GameSettingsService still expect the handle to exist.
    return gameLibrary.openInternalGames();
}

//*******************************
// AutoBleem::launchGame
//*******************************
void AutoBleem::launchGame() {
    PLOG_INFO << "Starting game";
    gui_->finish(); // fades the music out and closes the mixer

    gui_->input().flushPads();
    // the emulator needs the whole machine: audio (closed above), the pads (flushed above) and the display.
    // Without a compositor - a Raspberry Pi on KMS/DRM - our window is the DRM master and pcsx-ab's
    // SDL_Init(VIDEO) fails while it exists, so the window goes too; display(true) below rebuilds it.
    if (runner_->needsExclusiveDisplay()) {
        gui_->releaseDisplay();
    }
    // on a desktop the emulator opens its own window over ours: ours goes out of the way for the run
    if (runner_->minimisesLauncherWindow()) {
        gui_->minimizeWindow();
    }

    launcher_.launch(session_.runningGame, session_.emuMode, session_.resumePoint);

    if (runner_->minimisesLauncherWindow()) {
        gui_->restoreWindow();
    }

    bool reloadFavHist{false};
    if (session_.runningGame->foreign)
        reloadFavHist = true;
    else if (session_.emuMode != EmuMode::Pcsx)
        reloadFavHist = true;

    if (reloadFavHist) {
        retroArch_.reloadFavoritesAndHistory(); // they could have changed
    }

    // a moment for the machine to settle before the window comes back: on the PSC the GPU frees the
    // emulator's memory a few seconds after the process is gone, and a RetroArch 1.22.2 session (its XMB
    // alone holds hundreds of icon textures) leaves a lot to free - the launcher's own uploads failed at
    // 300 ms and at 1 s (twice, after Quake; the third rebuild at ~4 s held), so 2 s here and the
    // rebuild-on-loss in run() for the rest
    bool wasRetroArch = (session_.runningGame && session_.runningGame->foreign) || session_.emuMode != EmuMode::Pcsx;
    usleep((wasRetroArch ? 2000 : 300) * 1000);

    gui_->input().probePads();
    session_.runningGame.reset(); // replace with shared_ptr pointing to nullptr
    session_.startingGame = false;
    // remove all events if something left
    gui_->input().flushEvents();

    // RetroArch may have saved a screenshot or an auto save state just now: forget the listings
    thumbnails().clearCache();

    gui_->display(true);
    session_.resumingGui = true; // the launcher fades back in over the game that just ended

    // RetroBoot's return splash (abimage, the AutoBleem 2 emblem) waits for this file to go; it used to
    // be rc/launch_rb.sh that removed it, before our window existed - a black gap between the two
    unlink("/tmp/.abload");
}

//*******************************
// AutoBleem::run
// the whole program from here on. main() only wraps this so a stray exception is logged instead of a silent abort.
//*******************************
int AutoBleem::run() {
    if (!openLibrary()) {
        return EXIT_FAILURE;
    }

    string pathToGamesDir = Env::getPathToGamesDir();

    MemcardManager memcardOperation(pathToGamesDir);
    memcardOperation.restoreAll(Env::getPathToSaveStatesDir());

    // the same triggers the classic menu's forceScan prompt used to check, minus autobleem.prev (a
    // GamesFingerprint now stands in for it - see ScanService); moving loose game files into their own
    // sub-directories is ScanService's worker's job now, the first thing runScan() does.
    bool fingerprintOnDiskMatches = ScanService::fingerprintsMatchDisk();
    // RetroBoot's EmulationStation reads this list; without RetroBoot nobody does, and its absence must not
    // cost a full scan on every boot (it did, on the Pi)
    bool gamelistXmlExists =
        !Env::hasRetroBoot() ||
        DirEntry::exists(Env::getPathToRetroarchDir() + sep +
                         "retroboot/emulationstation/.emulationstation/gamelists/psx/gamelist.xml");
    bool thereAreRawGameFilesInGamesDir = GameScanner::hasLooseGameFiles(pathToGamesDir);

    gui_->display(false);

    if (!gameLibrary.metadata().hasRdb() && !gameLibrary.covers().hasAnyRegion()) {
        // was ClassicMenuScreen::init()'s check; still worth stopping for before anything else runs, since
        // every game would otherwise scan in with no title/cover. RetroArch's "Sony - PlayStation.rdb"
        // is the other source, so a stick with that tree but no covers*.db is fine. After display(): the
        // theme's font and background are loaded there, and the message drawn before it was a black screen.
        gui_->criticalException(_("WARNING: NO COVER DB FOUND. PRESS ANY BUTTON."));
    }

    applyOnlineSetting();
#ifdef AB_ONLINE_UPDATE
    applyUpdateSetting();
#ifdef AB_PLATFORM_WIN
    // the installer the last update downloaded: this build is what it installed, so it and pending.json
    // go (the Pi's autobleem-update helper clears the folder itself)
    {
        const string updatesDir = Env::getPathToSystemDir() + sep + "Updates";
        ableem::PendingUpdate pending;
        if (pending.load(updatesDir + sep + "pending.json") &&
            pending.autobleemVersion == string(Version::VERSION) + "-" + Version::GIT_HASH) {
            PLOG_INFO << "Update " << pending.autobleemVersion << " is what runs now - removing its installer";
            DirEntry::removeFile(updatesDir + sep + pending.autobleemFile);
            DirEntry::removeFile(updatesDir + sep + "pending.json");
        }
    }
#endif
    if (updates().checkDue(time(nullptr)))
        updates().startCheck(time(nullptr)); // once a day, and at every start - the launcher asks when it lands
#endif
    scans().start();
    if (!fingerprintOnDiskMatches || !gamelistXmlExists || thereAreRawGameFilesInGamesDir) {
        scans().requestScan();
    }

    // On the console a Quit event is never a window's close button: it is SDL giving up on the display -
    // seen on the PSC on 2026-09-20 coming back from RetroArch 1.22.2 (Doom): the GPU had not returned the
    // emulator's memory yet, the launcher's first buffer uploads failed ("PVR: glBufferSubData: No memory
    // for object data"), Weston then dropped the client ("wl_display@1: error 0: invalid object 16") and
    // the Quit that followed took the whole program out through selection.sh's reboot. So the display is
    // rebuilt a few times, a second apart, before that is accepted.
    int displayLost = 0;
    while (true) {
        bool quitRequested = false;
        {
            GuiLauncher launcherScreen(*gui_);
            launcherScreen.show();
            quitRequested = launcherScreen.quitRequested;
        }
        if (quitRequested) {
            if (gui_->platform().isDevHost() || ++displayLost > 3) {
                break; // the window's own close button - see GuiLauncher::loop()'s comment - or hopeless
            }
            PLOG_WARNING << "The display went away (attempt " << displayLost << " of 3) - rebuilding it";
            gui_->releaseDisplay();
            usleep(1000 * 1000);
            gui_->input().flushEvents();
            gui_->display(true);
            session_.resumingGui = true;
            continue;
        }
        displayLost = 0;

        session_.resumingGui = false;

        // the launcher asks for a game with session().startingGame + runningGame (three places in
        // evoui_launcher_actions/input.cpp); turning that into MENU_OPTION_START used to be the classic
        // menu screen's job, and went missing with it - nothing launched anywhere until this line
        if (session_.startingGame && session_.runningGame) {
            session_.menuOption = MENU_OPTION_START;
        }

        launcher_.writeSelectionScript();

        if (session_.menuOption == MENU_OPTION_START) {
            scans().setWatching(false); // the emulator gets the CPU, not the scanner
            launchGame();
            scans().setWatching(true);
            session_.menuOption = MENU_OPTION_IDLE;
            continue;
        }

        // the launcher closed asking to exit to RetroArch/EmulationStation (the system menu's item, or a
        // future one like it); Circle alone in the launcher is a no-op - there is nothing else to show -
        // so any other return from show() is unexpected and the safest thing is to just show it again
        if (session_.menuOption == MENU_OPTION_RETRO || session_.menuOption == MENU_OPTION_UPDATE) {
            break;
        }
    }

    scans().stop();

    // close the databases before the gui goes away.
    gameLibrary.close();

    Gui::splash(_("Loading ... Please Wait ..."));
    gui_->finish();

    return EXIT_SUCCESS;
}
