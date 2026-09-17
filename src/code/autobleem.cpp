//
// AutoBleem: the program - App plus the loop that runs it.
//
#include "autobleem.h"
#include "core/services/system.h"
#include "evoui/screens/evoui_launcher.h"

#include <cstdlib>
#include <iostream>
#include <unistd.h>

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
        cout << "would run " << exe;
        for (const string &arg : args) cout << " '" << arg << "'";
        cout << endl;
        Gui::splash("I'm sorry Dave.  I'm afraid I can't do that.");
    }
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

    // if the /System/Databases/internal.db doesn't exist make a copy from the PSC
    cout << "Importing internal games from PSC to USB" << endl;
    System::execUnixCommand("/media/Autobleem/rc/backup_internal.sh");

    return gameLibrary.openInternalGames();
}

//*******************************
// AutoBleem::launchGame
//*******************************
void AutoBleem::launchGame() {
    cout << "Starting game" << endl;
    gui_->finish();          // fades the music out and closes the mixer

    gui_->input().flushPads();

    launcher_.launch(session_.runningGame, session_.emuMode, session_.resumePoint);

    bool reloadFavHist{false};
    if (session_.runningGame->foreign)
        reloadFavHist = true;
    else if (session_.emuMode != EmuMode::Pcsx)
        reloadFavHist = true;

    if (reloadFavHist) {
        retroArch_.reloadFavoritesAndHistory();   // they could have changed
    }

    usleep(300 * 1000);

    gui_->input().probePads();
    session_.runningGame.reset();    // replace with shared_ptr pointing to nullptr
    session_.startingGame = false;
    // remove all events if something left
    gui_->input().flushEvents();

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

    if (!gameLibrary.covers().hasAnyRegion()) {
        // was ClassicMenuScreen::init()'s check; still worth stopping for before anything else runs, since
        // every game would otherwise scan in with no title/cover
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
    bool gamelistXmlExists = DirEntry::exists(Env::getPathToRetroarchDir() + sep +
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
        if (quitRequested) {   // the window's own close button - see GuiLauncher::loop()'s comment
            break;
        }

        session_.resumingGui = false;

        launcher_.writeSelectionScript();

        if (session_.menuOption == MENU_OPTION_START) {
            scans().setWatching(false);   // the emulator gets the CPU, not the scanner
            launchGame();
            scans().setWatching(true);
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
