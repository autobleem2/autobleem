//
// Created by screemer on 2/13/19.
//

#include "pcsx_interceptor.h"
#include "../core/util.h"
#include "../util_time.h"
#include "../gui/gui.h"
#include "../core/lang.h"
#include "../core/main.h"
#include "../app.h"
#include <fstream>
#include <iostream>
#include <unistd.h>
#include "../core/environment.h"

using namespace std;

void PcsxInterceptor::cleanupConfig(PsGamePtr &game)
{
    // copy back config to its place
    ConfigFileEditor processor;
    string newConfig = game->ssFolder + sep + "autobleem.cfg";
    if (DirEntry::exists(newConfig)) {
        // fix bios
        processor.replaceInFile(newConfig,"Bios","Bios = SET_BY_PCSX");



        if (!game->internal) {
            DirEntry::copy(newConfig, game->ssFolder + sep + PCSX_CFG);
            DirEntry::copy(newConfig, game->folder + sep + PCSX_CFG);
        } else {
            DirEntry::copy(newConfig, game->ssFolder + sep + PCSX_CFG);
        }
        DirEntry::removeFile(newConfig);
    }
}
//*******************************
// PcsxInterceptor::execute
//*******************************
bool PcsxInterceptor::execute(PsGamePtr & game, int resumepoint) {
    cout << "calling PcsxInterceptor::execute()" << endl;

    shared_ptr<Gui> gui(Gui::getInstance());

    App::get().library().updateDatePlayed(*game, UtilTime::getCurrentTime());

    string lastCDpoint = game->ssFolder + sep + "lastcdimg.txt";
    string lastCDpointX = game->ssFolder + sep + "lastcdimg." + to_string(resumepoint)+".txt";
    App::get().writeSelectionScript();
    std::vector<string> args;
    string gameFile = "";

    string region = "2"; // need to find out if console is jap to switch to 2 - later on
    string link = "/media/Autobleem/rc/launch.sh";
    string aspect = "0";
    if (App::get().config().inifile.values["aspect"] == "true") {
        aspect = "1";
    }

    string filter = "0";
    if (App::get().config().inifile.values["mip"] == "true") {
        filter = "1";
    } else {
        filter = "0";
    }

    trim(game->ssFolder);
    game->ssFolder = DirEntry::removeSeparatorFromEndOfPath(game->ssFolder);

    args.push_back(game->ssFolder);

    remove (lastCDpoint.c_str());

    if (DirEntry::exists(lastCDpointX))
    {
        DirEntry::copy(lastCDpointX,lastCDpoint);
        ifstream is(lastCDpointX.c_str());
        if (is.is_open()) {
            std::string line;
            std::getline(is, line);

            // last line is our filename
            gameFile = line;
            is.close();
        }
    } else {
        gameFile += (game->folder + sep + game->base);
        if (! (DirEntry::matchExtension(game->base, ".pbp") || DirEntry::matchExtension(game->base, ".chd"))){
            gameFile += ".cue";
        }
    }

    args.push_back(gameFile);
    // hack to get language from lang file
    string langStr = _("|@lang|");
    if (langStr == "|@lang|") {
        langStr = "2";
    }
    args.push_back(langStr); // lang by language file hack
    args.push_back(region);
    args.push_back(game->folder);
    args.push_back(resumepoint != -1 ? "1" : "0");
    args.push_back(aspect);
    args.push_back(filter);
    args.push_back("NA");   // pad mapping per-game was never wired up; this was always the fallback

#ifdef AB_DEBUG_HOST
    Gui::splash("I'm sorry Dave.  I'm afraid I can't do that.");
#else
    Util::runAndWait(link, args);
#endif
    cleanupConfig(game);

    usleep(3 * 1000);
    return true;
}

//*******************************
// PcsxInterceptor::memcardIn
//*******************************
void PcsxInterceptor::memcardIn(PsGamePtr & game) {
    App::get().memcards().swapInForLaunch(*game);
}

//*******************************
// PcsxInterceptor::memcardOut
//*******************************
void PcsxInterceptor::memcardOut(PsGamePtr & game) {
    App::get().memcards().swapOutAfterLaunch(*game);
}

//*******************************
// PcsxInterceptor::saveResumePoint
//*******************************
void PcsxInterceptor::saveResumePoint(PsGamePtr & game, int pointId) {
    App::get().resumePoints().saveAfterLaunch(*game, pointId);
}

//*******************************
// PcsxInterceptor::prepareResumePoint
//*******************************
void PcsxInterceptor::prepareResumePoint(PsGamePtr & game, int pointId) {
    App::get().resumePoints().prepareForLaunch(*game, pointId);
}
