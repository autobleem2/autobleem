#include "ableem/engine/game_library.h"
#include "ableem/engine/environment.h"
#include "ableem/engine/filesystem.h"
#include "ableem/engine/retroarch_playlist.h"
#include "ableem/engine/strings.h"

#include <algorithm>
#include <fstream>
#include <iostream>

using namespace std;

namespace ableem {

namespace {
const char RA_PLAYLIST[] = "AutoBleem.lpl";
}

//*******************************
// GameLibrary::~GameLibrary
//*******************************
GameLibrary::~GameLibrary() {
    close();
}

//*******************************
// GameLibrary::openCoversAndUsbGames
//*******************************
bool GameLibrary::openCoversAndUsbGames() {
    metadata_.reset(new MetadataLookup(Environment::getPathToCoversDBDir(), Environment::getPathToPlayStationRdbFile()));

    regionalDb.reset(new GameDatabase());
    if (!regionalDb->open(Environment::getPathToRegionalDBFile())) {
        return false;
    }
    regionalDb->createSchema();
    return true;
}

//*******************************
// GameLibrary::openInternalGames
//*******************************
bool GameLibrary::openInternalGames() {
    internalDb.reset(new GameDatabase());
    if (!internalDb->open(Environment::getPathToInternalDBFile())) {
        return false;
    }
    internalDb->addFavoriteColumnIfMissing();
    internalDb->addHistoryColumnIfMissing();
    internalDb->addLastPlayedColumnIfMissing();
    internalDb->addPlayUsingRAColumnIfMissing();
    return true;
}

//*******************************
// GameLibrary::close
//*******************************
void GameLibrary::close() {
    internalDb.reset();
    regionalDb.reset();
    metadata_.reset();
}

//*******************************
// GameLibrary::exportToRetroArchPlaylist
//*******************************
bool GameLibrary::exportToRetroArchPlaylist() {
    GameRecords games = usbGames().loadUsbGames();
    sort(games.begin(), games.end(), [](const GameRecord &a, const GameRecord &b) {
        return lessCaseInsensitive(a.title, b.title);
    });

    RetroArchPlaylistEntries entries;
    for (const GameRecord &game : games) {
        // a disc's base is the cue name without ".cue", but a PBP's or a CHD's is the whole file name -
        // only a cue game needs the extension put back (a CHD used to come out as "foo.chd.cue", NG's 8ee4fbea)
        bool singleFileImage = DirEntry::matchExtension(game.base, EXT_PBP) || DirEntry::matchExtension(game.base, EXT_CHD);
        string gameFile = game.folder + sep + game.base;
        if (!singleFileImage) {
            gameFile += EXT_CUE;
        }

        string base = singleFileImage ? game.base.substr(0, game.base.length() - 4) : game.base;
        if (DirEntry::exists(game.folder + sep + base + ".m3u")) {
            gameFile = game.folder + sep + base + ".m3u";
        }

        RetroArchPlaylistEntry entry;
        entry.path = gameFile;
        entry.label = game.title;
        entry.core_path = Environment::getPathToRetroarchCoreFile();
        entry.core_name = "DETECT";
        entry.crc32 = "00000000|crc";
        entry.db_name = RA_PLAYLIST;
        entries.push_back(entry);
    }

    return RetroArchPlaylist::save(Environment::getPathToRetroarchPlaylistsDir() + sep + RA_PLAYLIST, entries);
}

//*******************************
// GameLibrary::writeEmulationStationGamelist
// /Games/gamelist.xml is for EmulationStation to find the PS1 cover files
//*******************************
bool GameLibrary::writeEmulationStationGamelist() {
    // EmulationStation comes with RetroBoot, the console's RetroArch bundle. Without it (a Raspberry Pi, or
    // a stick without RetroBoot) there is nobody to read the list, and nowhere to put it.
    if (!Environment::hasRetroBoot()) return true;

    // this file was used during 0.9.0 testing. it must be removed or ES will use it by mistake.
    DirEntry::removeFile(Environment::getPathToGamesDir() + sep + "gamelist.xml");

    string path = Environment::getPathToRetroarchDir() + sep + "retroboot/emulationstation/.emulationstation/gamelists/psx";
    DirEntry::createDir(path);
    string filePath = path + sep + "gamelist.xml";
    DirEntry::removeFile(filePath);

    GameRecords games = usbGames().loadUsbGames();

    ofstream xml;
    xml.open(filePath.c_str(), ios::binary);
    if (!DirEntry::checkWritable(xml, filePath)) return false;

    xml << "<?xml version=\"1.0\"?>" << endl;
    xml << "<gameList>" << endl;

    auto makeGamesPathRelative = [](const string &oldPath) -> string {
        string newPath = oldPath;
        size_t pos = newPath.find("/Games");
        if (pos != string::npos) {
            newPath.erase(0, pos - 1 + sizeof("/Games"));
            newPath = "." + newPath;
        }
        return newPath;
    };

    for (const auto &game : games) {
        xml << "\t<game>" << endl;

        xml << "\t\t<path>" << makeGamesPathRelative(game.folder) << "</path>" << endl;
        xml << "\t\t<name>" << game.title << "</name>" << endl;
        xml << "\t\t<desc>" << game.title << "</desc>" << endl;
        string imagePath = game.folder + sep + game.base + ".png";
        xml << "\t\t<image>" << makeGamesPathRelative(imagePath) << "</image>" << endl;

        xml << "\t</game>" << endl;
    }
    xml << "</gameList>" << endl;

    xml.close();
    return true;
}

} // namespace ableem
