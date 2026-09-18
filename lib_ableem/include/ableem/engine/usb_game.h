// lib_ableem - engine: a game folder on the USB stick as the scanner sees it: its Game.ini, its discs, and
// which of the files a launchable game needs are present.
#pragma once

#include <algorithm>
#include <ctime>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "game_types.h"
#include "strings.h"

namespace ableem {

class MetadataLookup;

//******************
// Disc
//******************
class Disc {
public:
    std::string diskName;
    std::string cueName;
    // verifications

    bool cueFound = false;
    bool binVerified = false;
};

using UsbGamePtr = std::shared_ptr<class UsbGame>;
using UsbGames = std::vector<UsbGamePtr>;

//******************
// UsbGame
//******************
class UsbGame {
public:
    int folder_id = 0;
    std::string fullPath;       // "/Games/Sports/Football/NFL Blitz"
    std::string gameDirName;    // "NFL Blitz"
    std::string saveStatePath;
    int gameId = 0;

    std::string title;
    std::string publisher;
    int year = 0;
    std::string serial;
    std::string region;
    int players = 0;
    std::vector<Disc> discs;
    std::string favorite;
    std::string play_using_ra;
    time_t last_played = 0;

    std::string memcard;

    bool gameDataFound = false;
    bool pcsxCfgFound = false;
    bool gameIniFound = false;
    bool gameIniValid = false;
    bool coverImageFound = false;
    bool automationUsed = false;    // some value was filled in by the scanner rather than read from Game.ini
    ImageType imageType = IMAGE_BIN;
    bool highRes = false;
    std::string firstBinPath;

    void loadGameIni(const std::string &path);   // parse + applyIniValues
    void saveGameIni(const std::string &path);
    void applyIniValues();                      // iniValues -> members (defaults where missing) and the disc list

    // creates whatever is missing (cover .png, pcsx.cfg, the disc list) using the defaults in
    // Environment::getWorkingPath() and the cover database
    void recoverMissingFiles(MetadataLookup &metadata);
    // every file a launchable game needs is present. the reasons are plain (untranslated) English.
    bool verify(std::vector<std::string> *failureReasons = nullptr);
    bool print();
    bool validateCue(std::string cuePath, std::string path);   // every FILE in the cue exists; records firstBinPath

    std::map<std::string, std::string> iniValues;

    static void sortByTitle(UsbGames &games) { std::sort(games.begin(), games.end(),
                                                         [] (const UsbGamePtr &g1, const UsbGamePtr &g2) { return lessCaseInsensitive(g1->title, g2->title); }); }
    static void sortByFullPath(UsbGames &games) { std::sort(begin(games), end(games),
                                                            [] (const UsbGamePtr &g1, const UsbGamePtr &g2) { return lessCaseInsensitive(g1->fullPath, g2->fullPath); }); }
    static void sortByGameDirName(UsbGames &games) { std::sort(begin(games), end(games),
                                                            [] (const UsbGamePtr &g1, const UsbGamePtr &g2) { return lessCaseInsensitive(g1->gameDirName, g2->gameDirName); }); }
    static void sortBySerial(UsbGames &games) { std::sort(begin(games), end(games),
                                                          [] (const UsbGamePtr &g1, const UsbGamePtr &g2) { return lessCaseInsensitive(g1->serial, g2->serial); }); }

private:
    void parseIni(const std::string &path);
    std::string valueOrDefault(std::string name, std::string def, bool setAutomationIfDefaultUsed = true);
};

void operator += (UsbGames &dest, const UsbGames &src);

} // namespace ableem
