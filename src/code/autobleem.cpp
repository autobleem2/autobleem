//
// AutoBleem: the program - App plus the loop that runs it.
//
#include "autobleem.h"
#include <ableem/ui/debug_driver.h>
#include "core/services/system.h"
#include <ctime>
#include "evoui/screens/evoui_launcher.h"

#include <cstdlib>
#include <iostream>
#include <unistd.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif
#include <ableem/engine/log.h>
#include <ableem/engine/update_catalog.h>
#include "core/version.h"

using namespace std;

#ifdef AB_APPLIANCE
// How long the waiting picture is held before the display is handed to the game. It is not a delay
// for its own sake: without it the picture is drawn and taken away in the same instant and the
// hand-over looks like a flicker. A game takes seconds to come up, so this is not felt.
constexpr int WaitingPictureDuration = 400; // ms
#endif

//*******************************
// AutoBleem::AutoBleem
//*******************************
AutoBleem::AutoBleem() : App(makeProcessRunner()) {
#ifdef AB_PLATFORM_PSC
    // the console's power button: AppBase wired it to a halt; the launcher's power off is the standby
    // (App::requestPowerOff) - the tools under apps/ keep the halt
    gui_->platform().setPowerOffHandler([this]() {
        gui_->drawText(_("POWERING OFF... PLEASE WAIT"));
        requestPowerOff();
    });
#endif
}

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
    // the window stays up behind the emulator's: its events are pumped (and dropped) through the run
    return unique_ptr<ProcessRunner>(new WinProcessRunner([]() { Gui::getInstance()->input().flushEvents(); }));
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
// AutoBleem::runOutside
//*******************************
void AutoBleem::runOutside(bool retroArch, const std::function<void()> &body) {
    gui_->finish(); // fades the music out and closes the mixer

    gui_->input().flushPads();
    // the emulator needs the whole machine: audio (closed above), the pads (flushed above) and the display.
    // Without a compositor - a Raspberry Pi on KMS/DRM - our window is the DRM master and pcsx-ab's
    // SDL_Init(VIDEO) fails while it exists, so the window goes too; display(true) below rebuilds it.
    if (runner_->needsExclusiveDisplay()) {
#ifdef AB_APPLIANCE
        // An appliance - a Pi or the PC stick - is the whole session: bare KMS, no compositor, and a
        // text console on the tty underneath. Two things follow, and neither applies to the console
        // (Weston mediates there, which is what lets absplash hold a picture over a running game).
        //
        // First, the waiting picture has to be shown *before* the display goes, because there is only
        // one DRM master: a separate splash process holding it would stop the emulator starting, and
        // waiting for the launcher's window to come back would deadlock against it. So the last thing
        // we scan out is the picture, and it is what stands there while the game loads.
        //
        // Second, giving the display up puts the tty back into text mode, and whatever was last
        // printed on it comes back - a login prompt, a systemd line, the tail of a script. Blanked,
        // the gap reads as black instead of as somebody else's terminal.
        gui_->showSplashPicture(retroArch ? "retroarch.jpg" : "autobleem.jpg");
        usleep(WaitingPictureDuration * 1000); // long enough to be seen rather than flicker
        gui_->releaseDisplay();
        System::blankConsole();
#else
        gui_->releaseDisplay();
#endif
    }
    // on a desktop the emulator opens its own window over ours, which stays: the picture the console's
    // absplash shows around a RetroArch run is what is under the emulator's window, and what shows the
    // moment that window goes - not the desktop, and not a carousel that is not ready to be used yet
    if (runner_->keepsLauncherWindow()) {
        gui_->showSplashPicture(retroArch ? "retroarch.jpg" : "autobleem.jpg");
    }

    body();

    if (runner_->keepsLauncherWindow()) {
        gui_->showSplashPicture("autobleem.jpg");
        gui_->raiseWindow(); // Windows hands the focus back to us by itself; this makes sure of it
    }

    // a moment for the machine to settle before the window comes back: on the PSC the GPU frees the
    // emulator's memory a few seconds after the process is gone, and a RetroArch 1.22.2 session (its XMB
    // alone holds hundreds of icon textures) leaves a lot to free - the launcher's own uploads failed at
    // 300 ms and at 1 s (twice, after Quake; the third rebuild at ~4 s held), so 2 s here and the
    // rebuild-on-loss in run() for the rest. A desktop keeps its window and needs none of that.
    if (runner_->needsExclusiveDisplay()) {
#ifdef AB_APPLIANCE
        // the game has just put the tty back into text mode on its way out, so blank it again: this
        // is the other half of the hand-over, and the wait below would otherwise be spent looking at
        // a console rather than at black
        System::blankConsole();
#endif
        usleep((retroArch ? 2000 : 300) * 1000);
    }

    gui_->input().probePads();
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
// AutoBleem::launchGame
//*******************************
void AutoBleem::launchGame() {
    PLOG_INFO << "Starting game";
    const bool retroArch = (session_.runningGame && session_.runningGame->foreign) || session_.emuMode != EmuMode::Pcsx;
    runOutside(retroArch, [this]() { launcher_.launch(session_.runningGame, session_.emuMode, session_.resumePoint); });

    bool reloadFavHist{false};
    if (session_.runningGame->foreign)
        reloadFavHist = true;
    else if (session_.emuMode != EmuMode::Pcsx)
        reloadFavHist = true;

    if (reloadFavHist) {
        retroArch_.reloadFavoritesAndHistory(); // they could have changed
    }

    session_.runningGame.reset(); // replace with shared_ptr pointing to nullptr
    session_.startingGame = false;
}

//*******************************
// AutoBleem::runRetroArchMenu
//*******************************
// The system menu's RetroArch item where there is no rc/retroarch.sh to leave to (the Windows product):
// RetroArch's own menu in front of the launcher, and its playlists re-read after - the user may have
// scanned content in there.
void AutoBleem::runRetroArchMenu() {
    PLOG_INFO << "Starting RetroArch's menu";
    runOutside(true, [this]() { launcher_.launchRetroArchMenu(); });
    retroArch_.reloadPlaylists();
    retroArch_.reloadFavoritesAndHistory();
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
    unlink("/tmp/.abload"); // the console's wake-up picture (rc/selection.sh's standby) waits for this

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

    // PROOF (proof/plugin branch): load the plugin AB_PLUGIN_PROOF names and let it show its dialog
    if (const char *proof = getenv("AB_PLUGIN_PROOF")) {
        typedef int (*ProofRun)(ableem::GuiBase *, plog::IAppender *);
#ifdef _WIN32
        HMODULE module =
            LoadLibraryExA(proof, nullptr, LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
        ProofRun run = module ? reinterpret_cast<ProofRun>(GetProcAddress(module, "ab_proof_run")) : nullptr;
        if (!module) {
            PLOG_ERROR << "[proof] LoadLibrary(" << proof << ") failed: " << GetLastError();
        }
#else
        void *module = dlopen(proof, RTLD_NOW | RTLD_LOCAL);
        ProofRun run = module ? reinterpret_cast<ProofRun>(dlsym(module, "ab_proof_run")) : nullptr;
        if (!module) {
            PLOG_ERROR << "[proof] dlopen(" << proof << ") failed: " << dlerror();
        }
#endif
        if (run) {
            PLOG_INFO << "[proof] calling ab_proof_run";
            int answer = run(gui_.get(), plog::get());
            PLOG_INFO << "[proof] the plugin returned " << answer;
        }
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
        if (session_.menuOption == MENU_OPTION_POWEROFF) {
            launcher_.writeSelectionScript(); // the console's rc/selection.sh does the standby
            break;
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

        // the launcher closed asking to exit to RetroArch/EmulationStation (the system menu's item): on a
        // desktop that is RetroArch run in front of us and the launcher again after; on the console and the
        // Pi the process leaves and rc/retroarch.sh takes over
        if (session_.menuOption == MENU_OPTION_RETRO && Env::directLaunch()) {
            scans().setWatching(false);
            runRetroArchMenu();
            scans().setWatching(true);
            session_.menuOption = MENU_OPTION_IDLE;
            continue;
        }
        // Circle alone in the launcher is a no-op - there is nothing else to show - so any other return
        // from show() is unexpected and the safest thing is to just show it again
        if (session_.menuOption == MENU_OPTION_RETRO || session_.menuOption == MENU_OPTION_UPDATE) {
            break;
        }
    }

    scans().stop();

    // close the databases before the gui goes away.
    gameLibrary.close();

    if (session_.menuOption == MENU_OPTION_POWEROFF) {
        gui_->drawText(_("POWERING OFF... PLEASE WAIT")); // the standby follows within a second
    } else {
        Gui::splash(_("Loading ... Please Wait ..."));
    }
    gui_->finish();

    return EXIT_SUCCESS;
}
