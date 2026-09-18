//
// AutoBleem: the program - App plus the loop that runs it.
//
#include "autobleem.h"
#include "core/services/system.h"
#include "evoui/screens/evoui_launcher.h"

#include <cstdlib>
#include <iostream>
#include <unistd.h>
#include <ableem/engine/log.h>

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
    void run(const string &exe, const vector<string> &args) override {
        string line = "would run " + exe;
        for (const string &arg : args)
            line += " '" + arg + "'";
        PLOG_INFO << line;
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
#ifdef AB_DEBUG_HOST
    return unique_ptr<ProcessRunner>(new SplashProcessRunner());
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

#ifndef AB_PLATFORM_RPI
    // if the /System/Databases/internal.db doesn't exist make a copy from the PSC
    PLOG_INFO << "Importing internal games from PSC to USB";
    System::execUnixCommand((Env::getPathToRCDir() + sep + "backup_internal.sh").c_str());
#endif

    // on a Pi this opens (and so creates) an empty internal.db: nothing ever queries it - the internal sets
    // are unreachable there - but GameCatalogService and GameSettingsService still expect the handle to exist.
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

    launcher_.launch(session_.runningGame, session_.emuMode, session_.resumePoint);

    bool reloadFavHist{false};
    if (session_.runningGame->foreign)
        reloadFavHist = true;
    else if (session_.emuMode != EmuMode::Pcsx)
        reloadFavHist = true;

    if (reloadFavHist) {
        retroArch_.reloadFavoritesAndHistory(); // they could have changed
    }

    usleep(300 * 1000);

    gui_->input().probePads();
    session_.runningGame.reset(); // replace with shared_ptr pointing to nullptr
    session_.startingGame = false;
    // remove all events if something left
    gui_->input().flushEvents();

    // RetroArch may have saved a screenshot or an auto save state just now: forget the listings
    thumbnails().clearCache();

    gui_->display(true);
}

//*******************************
// AutoBleem::run
// the whole program from here on. main() only wraps this so a stray exception is logged instead of a silent abort.
//*******************************
int AutoBleem::run() {
    if (!openLibrary()) {
        return EXIT_FAILURE;
    }

    if (!gameLibrary.metadata().hasRdb() && !gameLibrary.covers().hasAnyRegion()) {
        // was ClassicMenuScreen::init()'s check; still worth stopping for before anything else runs, since
        // every game would otherwise scan in with no title/cover. RetroArch's "Sony - PlayStation.rdb"
        // is the other source, so a stick with that tree but no covers*.db is fine.
        gui_->criticalException(_("WARNING: NO COVER DB FOUND. PRESS ANY BUTTON."));
    }

    string pathToGamesDir = Env::getPathToGamesDir();

    MemcardManager memcardOperation(pathToGamesDir);
    memcardOperation.restoreAll(Env::getPathToSaveStatesDir());

    // the same triggers the classic menu's forceScan prompt used to check, minus autobleem.prev (a
    // GamesFingerprint now stands in for it - see ScanService); moving loose game files into their own
    // sub-directories is ScanService's worker's job now, the first thing runScan() does.
    GamesFingerprint storedFingerprint;
    bool fingerprintOnDiskMatches = storedFingerprint.load(scans().fingerprintFilePath()) &&
                                    storedFingerprint == GamesFingerprint::take(pathToGamesDir);
    // RetroBoot's EmulationStation reads this list; without RetroBoot nobody does, and its absence must not
    // cost a full scan on every boot (it did, on the Pi)
    bool gamelistXmlExists =
        !Env::hasRetroBoot() ||
        DirEntry::exists(Env::getPathToRetroarchDir() + sep +
                         "retroboot/emulationstation/.emulationstation/gamelists/psx/gamelist.xml");
    bool thereAreRawGameFilesInGamesDir = GameScanner::hasLooseGameFiles(pathToGamesDir);

    gui_->display(false);

    scans().start();
    if (!fingerprintOnDiskMatches || !gamelistXmlExists || thereAreRawGameFilesInGamesDir) {
        scans().requestScan();
    }

    while (true) {
        bool quitRequested = false;
        {
            GuiLauncher launcherScreen(*gui_);
            launcherScreen.show();
            quitRequested = launcherScreen.quitRequested;
        }
        if (quitRequested) { // the window's own close button - see GuiLauncher::loop()'s comment
            break;
        }

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
        if (session_.menuOption == MENU_OPTION_RETRO) {
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
