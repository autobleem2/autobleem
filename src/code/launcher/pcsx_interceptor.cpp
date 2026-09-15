//
// Created by screemer on 2/13/19.
//

#include "pcsx_interceptor.h"
#include "../util.h"
#include "../util_time.h"
#include "../gui/gui.h"
#include "../lang.h"
#include "../main.h"
#include "../app.h"
#include <fstream>
#include <iostream>
#include <unistd.h>
#include "../environment.h"

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
    if (gui->cfg.inifile.values["aspect"] == "true") {
        aspect = "1";
    }

    string filter = "0";
    if (gui->cfg.inifile.values["mip"] == "true") {
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
    string memcard = "SONY";
    if (!game->internal) {
        IniFile gameini;
        gameini.load(game->folder + sep + GAME_INI);
        memcard = gameini.values["memcard"];
    }
    if (memcard != "SONY") {
        if (DirEntry::exists(Env::getPathToMemCardsDir() + sep + game->memcard)) {
            MemcardManager card(Env::getPathToGamesDir() + sep);
            if (!card.swapIn(game->ssFolder, game->memcard)) {
                game->setMemCard("SONY");
            };
        }
    }
}

//*******************************
// PcsxInterceptor::memcardOut
//*******************************
void PcsxInterceptor::memcardOut(PsGamePtr & game) {
    string memcard = "SONY";
    if (!game->internal) {
        IniFile gameini;
        gameini.load(game->folder + sep + GAME_INI);
        memcard = gameini.values["memcard"];
    }
    if (memcard != "SONY") {
        MemcardManager card(Env::getPathToGamesDir() + sep);
        card.swapOut(game->ssFolder, game->memcard);
    }
}

//*******************************
// PcsxInterceptor::saveResumePoint
//*******************************
void PcsxInterceptor::saveResumePoint(PsGamePtr & game, int pointId) {
    string filenamefile = game->ssFolder + sep + "filename.txt";
    string filenamefileX = game->ssFolder + sep + "filename.txt.res";
    string filenamepoint = game->ssFolder + sep + "filename."+to_string(pointId)+".txt.res";
    string lastCDpoint = game->ssFolder + sep + "lastcdimg.txt";
    string lastCDpointX = game->ssFolder + sep + "lastcdimg."+to_string(pointId)+".txt";
    if (DirEntry::exists(filenamefile)) {
        ifstream is(filenamefile.c_str());
        if (is.is_open()) {

            std::string line;
            std::getline(is, line);
            std::getline(is, line);

            // last line is our filename
            string ssfile = game->ssFolder + sep + "sstates/" + line + ".00" + to_string(pointId) + ".res";
            string newName = game->ssFolder + sep + "sstates/" + line + ".000";

            DirEntry::removeFile(ssfile);
            DirEntry::copy(newName.c_str(), ssfile.c_str());
            DirEntry::removeFile(newName);

            // update image

            is.close();
        }
        DirEntry::removeFile(filenamefileX);
        DirEntry::removeFile(filenamepoint);
        DirEntry::renameFile(filenamefile, filenamefileX);
        DirEntry::copy(filenamefileX,filenamepoint);
        if (DirEntry::exists(lastCDpoint))
        {
            DirEntry::removeFile(lastCDpointX);
            DirEntry::copy(lastCDpoint, lastCDpointX);
            DirEntry::removeFile(lastCDpoint);
        }
    }
}

//*******************************
// PcsxInterceptor::prepareResumePoint
//*******************************
void PcsxInterceptor::prepareResumePoint(PsGamePtr & game, int pointId) {

    // cleanup after previous crash as pcsx doest not want to save
    string filenameTrash = game->ssFolder + sep + "filename.txt";
    if (DirEntry::exists(filenameTrash)) {
        DirEntry::removeFile(filenameTrash);
    }

    string ssfile = game->ssFolder + sep + "sstates";
    for (const DirEntry & sstate:DirEntry::diru(ssfile)) {
        if (DirEntry::getFileExtension(sstate.name) == "000") {
            string toDelete = ssfile + sep + sstate.name;
            DirEntry::removeFile(toDelete);
        }
    }

    ssfile = game->ssFolder + sep + "screenshots";
    for (const DirEntry & sstate:DirEntry::diru(ssfile)) {
        if (DirEntry::getFileExtension(sstate.name) == "png") {
            string toDelete = ssfile + sep + sstate.name;
            DirEntry::removeFile(toDelete);
        }
    }

    if (pointId == -1)
        return;
    string filenamefile = game->ssFolder + sep + "filename.txt.res";
    string filenamefileX = game->ssFolder + sep + "filename.txt";
    string filenamepoint = game->ssFolder + sep + "filename."+to_string(pointId)+".txt.res";
    DirEntry::removeFile(filenamefileX);
    if (DirEntry::exists(filenamepoint))
    {
        filenamefile = filenamepoint;
    }
    if (DirEntry::exists(filenamefile)) {
        ifstream is(filenamefile.c_str());
        if (is.is_open()) {

            std::string line;
            std::getline(is, line);
            string lastImageInfo = line;

            // fix lastcdpoint
            string lastCDpointX = game->ssFolder + sep + "lastcdimg."+to_string(pointId)+".txt";
            DirEntry::removeFile(lastCDpointX);
            string file = DirEntry::getFileNameFromPath(lastImageInfo);
            string imageToLoad = game->folder + sep + file;

            ofstream os;
            os.open(lastCDpointX);
            if (DirEntry::checkWritable(os, lastCDpointX)) {
                os << imageToLoad << endl;
            }
            os.close();

            std::getline(is, line);

            // last line is our filename
            string ssfile = game->ssFolder + sep + "sstates/" + line + ".00" + to_string(pointId) + ".res";
            string newName = game->ssFolder + sep + "sstates/" + line + ".000";
            if (DirEntry::exists(ssfile)) {
                DirEntry::removeFile(newName);
                DirEntry::copy(ssfile.c_str(), newName.c_str());
            }
            is.close();
        }
    }
}
