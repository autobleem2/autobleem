#include "app.h"
#include "lang.h"
#include "util.h"
#include "launcher/emu_interceptor.h"
#include "launcher/pcsx_interceptor.h"
#include "launcher/retboot_interceptor.h"
#include "launcher/launch_interceptor.h"
#include "launcher/ra_integrator.h"

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <unistd.h>

using namespace std;

App *App::instance = nullptr;

//*******************************
// App::App
//*******************************
App::App() {
    instance = this;

    gui_ = Gui::getInstance();
    scanner_ = Scanner::getInstance();

    gui_->platform().setPowerOffHandler([this]() {
        gui_->drawText(_("POWERING OFF... PLEASE WAIT"));
        Util::powerOff();
    });

    shared_ptr<Lang> lang(Lang::getInstance());
    lang->load(gui_->cfg.inifile.values["language"]);
}

//*******************************
// App::~App
//*******************************
App::~App() {
    instance = nullptr;
}

//*******************************
// App::get
//*******************************
App &App::get() {
    return *instance;
}

//*******************************
// App::openLibrary
//*******************************
bool App::openLibrary() {
    if (!gameLibrary.openCoversAndUsbGames()) {
        return false;
    }

    // if the /System/Databases/internal.db doesn't exist make a copy from the PSC
    cout << "Importing internal games from PSC to USB" << endl;
    Util::execUnixCommand("/media/Autobleem/rc/backup_internal.sh");

    return gameLibrary.openInternalGames();
}

//*******************************
// App::writeSelectionScript
//*******************************
void App::writeSelectionScript() {
    ofstream os;
    string path = gui_->cfg.inifile.values["cfg"];
    os.open(path);
    if (!DirEntry::checkWritable(os, path)) return;   // the rc scripts then keep the previous selection
    os << "#!/bin/sh" << endl << endl;
    os << "AB_SELECTION=" << session_.menuOption << endl;
    os << "AB_THEME=" << gui_->cfg.inifile.values["theme"] << endl;
    os << "AB_PCSX=" << gui_->cfg.inifile.values["pcsx"] << endl;
    os << "AB_MIP=" << gui_->cfg.inifile.values["mip"] << endl;

    os.flush();
    os.close();
}

//*******************************
// App::rescan
// a Re/Scan menu selection: repair/verify every game, rebuild regional.db + autobleem.list, and the
// EmulationStation gamelist.xml that mirrors it.
//*******************************
void App::rescan(GamesHierarchy &gamesHierarchy, const string &prevPath) {
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
// App::launchGame
//*******************************
void App::launchGame() {
    cout << "Starting game" << endl;
    gui_->finish();

    gui_->audio().close();
    gui_->input().flushPads();

    writeSelectionScript();
    unique_ptr<EmuInterceptor> interceptor;
    if (session_.runningGame->foreign) {
        if (!session_.runningGame->app) {
            interceptor.reset(new RetroArchInterceptor());
        } else {
            interceptor.reset(new LaunchInterceptor());
        }
    } else {
        if (session_.emuMode == EmuMode::Pcsx) {
            interceptor.reset(new PcsxInterceptor());
        } else {
            interceptor.reset(new RetroArchInterceptor());
        }
    }

    interceptor->memcardIn(session_.runningGame);
    interceptor->prepareResumePoint(session_.runningGame, session_.resumePoint);
    interceptor->execute(session_.runningGame, session_.resumePoint);
    interceptor->memcardOut(session_.runningGame);
    interceptor.reset();

    bool reloadFavHist{false};
    if (session_.runningGame->foreign)
        reloadFavHist = true;
    else if (session_.emuMode != EmuMode::Pcsx)
        reloadFavHist = true;

    if (reloadFavHist) {
        auto ra = RAIntegrator::getInstance();
        ra->reloadFavorites();  // they could have changed
        ra->reloadHistory();    // they could have changed
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
// App::run
// the whole program from here on. main() only wraps this so a stray exception is logged instead of a silent abort.
//*******************************
int App::run() {
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
        gui_->menuSelection();
        writeSelectionScript();

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
