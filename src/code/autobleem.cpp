//
// AutoBleem: the program - App plus the loop that runs it.
//
#include "autobleem.h"
#include "core/services/system.h"
#include "gui/screens/gui_classic_menu.h"

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
// AutoBleem::rescan
// a Re/Scan menu selection: repair/verify every game, rebuild regional.db + autobleem.list, and the
// EmulationStation gamelist.xml that mirrors it.
//*******************************
void AutoBleem::rescan(GamesHierarchy &gamesHierarchy, const string &prevPath) {
    // write the prev file now. if it's written after the scan, games that failed to verify will already
    // have been removed from the hierarchy, forcing a rescan on every boot.
    gamesHierarchy.writeAutobleemPrev(prevPath);

    if (!gameLibrary.usbGames().createSchema()) {
        cout << "Error creating db structure" << endl;
        return;
    }

    if (!gameLibrary.usbGames().clearAllTables()) {
        gui_->drawText("ERROR IN DB");
        sleep(1);
        return;
    }

    scanner_->scanGamesDirectory(gamesHierarchy, gameLibrary.covers());
    scanner_->writeRegionalDatabase(gamesHierarchy, gameLibrary.usbGames());

    gui_->drawText(_("Total:") + " " + to_string(scanner_->gamesToAddToDB.size()) + " " + _("games scanned") + ".");
    sleep(1);
    scanner_->gamesToAddToDB.clear();

    gameLibrary.writeEmulationStationGamelist();
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

    session_.forceScan = false;
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

    string pathToGamesDir = Env::getPathToGamesDir();

    MemcardManager memcardOperation(pathToGamesDir);
    memcardOperation.restoreAll(Env::getPathToSaveStatesDir());

    string prevPath = Env::getWorkingPath() + sep + "autobleem.prev";
    bool prevFileExists = DirEntry::exists(prevPath);
    bool gamelistXmlExists = DirEntry::exists(Env::getPathToRetroarchDir() + sep +
            "retroboot/emulationstation/.emulationstation/gamelists/psx/gamelist.xml");

    GamesHierarchy gamesHierarchy;
    gamesHierarchy.getHierarchy(pathToGamesDir);

    bool autobleemPrevOutOfDate = gamesHierarchy.gamesDoNotMatchAutobleemPrev(prevPath);
    bool thereAreRawGameFilesInGamesDir = Scanner::hasLooseGameFiles(pathToGamesDir);

    if (!prevFileExists || !gamelistXmlExists || thereAreRawGameFilesInGamesDir || autobleemPrevOutOfDate) {
        session_.forceScan = true;
    }

    gui_->display(false);

    if (thereAreRawGameFilesInGamesDir)
        scanner_->moveLooseGameFilesIntoSubDirs(pathToGamesDir);   // gui_->display() needs to be up first

    while (session_.menuOption == MENU_OPTION_SCAN || session_.menuOption == MENU_OPTION_START) {
        {
            ClassicMenuScreen menu(*gui_);
            menu.show();
        }
        launcher_.writeSelectionScript();

        if (session_.menuOption == MENU_OPTION_SCAN) {
            gamesHierarchy.getHierarchy(pathToGamesDir);
            rescan(gamesHierarchy, prevPath);
            session_.forceScan = false;
        }

        if (session_.menuOption == MENU_OPTION_START) {
            launchGame();
        }
    }

    // close the databases before the gui goes away.
    gameLibrary.close();

    Gui::splash(_("Loading ... Please Wait ..."));
    gui_->finish();

    return EXIT_SUCCESS;
}
