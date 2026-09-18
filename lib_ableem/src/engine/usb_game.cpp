#include "ableem/engine/usb_game.h"
#include "ableem/engine/config_file_editor.h"
#include "ableem/engine/metadata_lookup.h"
#include "ableem/engine/environment.h"
#include "ableem/engine/filesystem.h"
#include "ableem/engine/game_metadata.h"
#include "ableem/engine/ini_file.h"
#include "ableem/engine/serial_scanner.h"
#include "ableem/engine/strings.h"

#include <sstream>
#include <fstream>
#include <iostream>
#include "ableem/engine/log.h"

using namespace std;

namespace ableem {


//*******************************
// UsbGame::validateCue
//*******************************
bool UsbGame::validateCue(string cuePath, string path) {
    vector<string> binFiles;
    string line;
    ifstream cueStream;
    bool result = true;

    cueStream.open(cuePath);
    while (getline(cueStream, line)) {
        line = trim(line);
        if (line.empty()) continue;
        if (line.substr(0, 4) == "FILE") {
            line = line.substr(6, string::npos);
            line = line.substr(0, line.find('"'));
            binFiles.push_back(line);
        }
    }
    for (int i = 0; i < binFiles.size(); i++) {
        string binPath = path + sep + binFiles[i];
        if (!DirEntry::exists(binPath)) {
            result = false;
        } else {
            if (i == 0) {
                if (firstBinPath.empty()) {
                    firstBinPath = binPath;
                }
            }
        }
    }
    cueStream.close();
    return result;
}

//*******************************
// UsbGame::valueOrDefault
//*******************************
string UsbGame::valueOrDefault(string name, string def, bool setAutomationIfDefaultUsed) {
    string value;
    if (iniValues.find(name) != iniValues.end()) {
        value = trim(iniValues.find(name)->second);
        if (value.length() == 0) {
            if (setAutomationIfDefaultUsed)
                automationUsed = true;
            return def;
        }
    } else {
        if (setAutomationIfDefaultUsed)
            automationUsed = true;
        value = def;
    }
    return value;
}

//*******************************
// UsbGame::verify
//*******************************
bool UsbGame::verify(std::vector<std::string> *failureReasons) {
    bool result = true;

    if (discs.size() == 0) {
        if (failureReasons)
            failureReasons->emplace_back("No discs");
        result = false;
    }

    for (int i = 0; i < discs.size(); i++) {
        if (discs[i].diskName.length() == 0) {
            if (failureReasons)
                failureReasons->emplace_back("No disc name");
            result = false;
        }
        if (!discs[i].cueFound) {
            PLOG_INFO << i << discs[i].diskName << discs[i].cueFound;
            if (failureReasons)
                failureReasons->emplace_back("Cue file not found");
            result = false;
        }
        if (!discs[i].binVerified) {
            if (failureReasons)
                failureReasons->emplace_back("Bin file failed to verify");
            result = false;
        }
    }

    if (!gameDataFound) {
        if (failureReasons)
            failureReasons->emplace_back("Game file not found");
        result = false;
    }
    if (!gameIniFound) {
        if (failureReasons)
            failureReasons->emplace_back("Game.ini file not found");
        result = false;
    }
    if (!gameIniValid) {
        if (failureReasons)
            failureReasons->emplace_back("Game.ini file not valid");
        result = false;
    }
    if (!pcsxCfgFound) {
        if (failureReasons)
            failureReasons->emplace_back("pcsx.cfg file not found");
        result = false;
    }

    if (!result) {
        PLOG_ERROR << "Game: " << title << " Validation Failed";
    }

    return result;
}

//*******************************
// UsbGame::print
//*******************************
bool UsbGame::print() {
    PLOG_INFO << "-------------------";
    PLOG_INFO << "Printing game data:";
    PLOG_INFO << "-----------------";
    PLOG_INFO << "AUTOMATION: " << automationUsed;
    PLOG_INFO << "Game folder id: " << folder_id;
    PLOG_INFO << "Game: " << title;
    PLOG_INFO << "Players: " << players;
    PLOG_INFO << "Publisher: " << publisher;
    PLOG_INFO << "Year: " << year;
    PLOG_INFO << "Serial: " << serial;
    PLOG_INFO << "Region: " << region;
    PLOG_INFO << "GameData found: " << gameDataFound;
    PLOG_INFO << "Game.ini found: " << gameIniFound;
    PLOG_INFO << "Game.ini valid: " << gameIniValid;
    PLOG_INFO << "PNG found:" << coverImageFound;
    PLOG_INFO << "pcsx.cfg found: " << pcsxCfgFound;
    PLOG_INFO << "TotalDiscs: " << discs.size();
    PLOG_INFO << "Favorite: " << favorite;
    PLOG_INFO << "Play Using RA: " << play_using_ra;
    PLOG_INFO << "Last Played: " << last_played;

    for (int i = 0; i < discs.size(); i++) {
        PLOG_INFO << "  Disc:" << i + 1 << "  " << discs[i].diskName;
        PLOG_INFO << "  CUE found: " << discs[i].cueFound;
        PLOG_INFO << "  BIN correct: " << discs[i].binVerified;
    }

    vector<string> failureReasons;
    bool result = verify(&failureReasons);
    if (result) {
        PLOG_INFO << "-------Game Verify OK-------";
    } else {
        PLOG_WARNING << "------Game Verify FAIL------";
        for (const auto & reason : failureReasons)
            PLOG_INFO << "Reason: " << reason;
    }

    return result;
}

//*******************************
// UsbGame::recoverMissingFiles
//*******************************
void UsbGame::recoverMissingFiles(MetadataLookup &metadata) {
    string workingPath = Environment::getWorkingPath();

    GameMetadata md;
    bool metadataLoaded = false;

    if (this->imageType == IMAGE_PBP) {
        // disc link
        string destinationDir = fullPath ;
        string pbpFileName = DirEntry::findFirstFile(EXT_PBP, destinationDir);
        if (pbpFileName != "") {
            if (discs.size() == 0) {
                automationUsed = false;
                Disc disc;
                disc.diskName = pbpFileName;    // the full filename including the .PBP
                disc.cueFound = true;
                disc.cueName = pbpFileName;
                disc.binVerified = true;
                discs.push_back(disc);
            }
        } else
        {
            automationUsed = true;
            PLOG_INFO << "Switching automation in PBP";
        }
    } else if (this->imageType == IMAGE_CHD) {
        // disc link
        string destinationDir = fullPath ;
        string chdFileName = DirEntry::findFirstFile(EXT_CHD, destinationDir);
        if (chdFileName != "") {
            firstBinPath = destinationDir +  sep +chdFileName;
            if (discs.size() == 0) {
                vector<string> extensions;
                extensions.push_back("chd");
                DirEntries allFiles = DirEntry::diru(destinationDir);
                DirEntries fileList = DirEntry::getFilesWithExtension(destinationDir, allFiles, extensions);
                automationUsed = false;
                for (DirEntry dirEntry:fileList)
                {
                    Disc disc;
                    disc.diskName = dirEntry.name;    // the full filename including the .CHD
                    disc.cueFound = true;
                    disc.cueName = dirEntry.name;
                    disc.binVerified = true;
                    discs.push_back(disc);
                }


            }
            if (this->imageType==IMAGE_CHD) imageType = IMAGE_CHD;
        } else
        {
            automationUsed = true;
            PLOG_INFO << "Switching automation in CHD";
        }
    }
    if (DirEntry::imageTypeUsesACueFile(this->imageType)) {
        if (discs.size() == 0) {
            automationUsed = true;
            PLOG_INFO << "Switching automation no discs";
            // find cue files
            string destination = fullPath ;
            for (const DirEntry & entry: DirEntry::diru(destination)) {
                if (DirEntry::matchExtension(entry.name, EXT_CUE)) {
                    Disc disc;
                    string discEntry = entry.name.substr(0, entry.name.size() - 4); // remove .CUE
                    disc.diskName = discEntry;  // the CUE filename without the .CUE
                    disc.cueFound = true;
                    disc.cueName = discEntry;   // the CUE filename without the .CUE
                    disc.binVerified = validateCue(destination + sep + entry.name, fullPath );
                    discs.push_back(disc);
                }
            }
        }
    }

    // The cover: the covers db's PNG written next to the game when it has one. Nothing is written
    // otherwise - the carousel finds the cover in RetroArch's thumbnails tree, or draws default.png -
    // where a copy of default.png used to be made, which then hid the thumbnail for good.
    if (discs.size() > 0 && !coverImageFound) {
        automationUsed = true;
        PLOG_INFO << "Switching automation no image";
        string destination = fullPath + sep + discs[0].diskName + ".png";
        string serial = SerialScanner::readSerial(imageType, fullPath, firstBinPath);
        if (serial != "") {
            if (metadata.findBySerial(serial, md)) {
                metadataLoaded = true;
                if (!md.bytes.empty()) {
                    PLOG_WARNING << "Updating cover in recoverMissingFiles()" << destination;
                    ofstream pngFile;
                    pngFile.open(destination, ios::binary);
                    if (DirEntry::checkWritable(pngFile, destination)) {
                        pngFile.write(md.bytes.data(), md.bytes.size());
                        pngFile.flush();
                        pngFile.close();
                        automationUsed = false;
                        coverImageFound = true;
                    }
                }
            }
            md.clearCover();
        }
    }

    if (!pcsxCfgFound) {
        automationUsed = true;
        PLOG_INFO << "Switching automation no pcsx";
        string source = workingPath + sep + PCSX_CFG;
        string destination = fullPath + sep + PCSX_CFG;
        PLOG_ERROR << "SRC:" << source << " DST:" << destination;

        int region = 0;
        bool japan = false;

        if (!metadataLoaded) {
            string serial = SerialScanner::readSerial(imageType, fullPath, firstBinPath);
            if (serial != "") {
                metadataLoaded = metadata.findBySerial(serial, md);
            }
        }

        if (metadataLoaded) {
            if (md.lastRegion == "U") {
                japan = false;
                region = 1;
            }
            if (md.lastRegion == "J") {
                japan = true;
                region = 1;
            }
            if (md.lastRegion == "P") {
                japan = false;
                region = 2;
            }
        }
        md.clearCover();
        DirEntry::copy(source, destination);

        ConfigFileEditor processor;
        processor.replaceUsb(gameDirName, fullPath, "region", "region = " + to_string(region));
        pcsxCfgFound = true;
    }
}

//*******************************
// UsbGame::applyIniValues
//*******************************
void UsbGame::applyIniValues() {
    string tmp;
    discs.clear();
    title = valueOrDefault("title", gameDirName);
    memcard = valueOrDefault("memcard", "");

    publisher = valueOrDefault("publisher", "Other");
    string automation = valueOrDefault("automation", "0");
    automationUsed = atoi(automation.c_str());
    tmp = valueOrDefault("players", "1");
    if (Strings::isInteger(tmp.c_str())) players = atoi(tmp.c_str()); else players = 1;
    tmp = valueOrDefault("year", "2018");

    if (Strings::isInteger(tmp.c_str())) year = atoi(tmp.c_str()); else year = 2018;
    tmp = valueOrDefault("highres","0");
    if (Strings::isInteger(tmp.c_str())) highRes = atoi(tmp.c_str()); else highRes = 0;
    // what the scanner (or the user, by hand) wrote last time; the scanner decides whether to trust it -
    // a missing one is not an automation event, the image is simply read again
    serial = valueOrDefault("serial", "", false);
    region = valueOrDefault("region", "", false);
    recordName = valueOrDefault("thumbnail_record_name", "", false);
    coverPath = valueOrDefault("cached_cover_path", "", false);
    snapPath = valueOrDefault("cached_snap_path", "", false);
    // favorite and play_using_ra are newer fields that older Game.ini files lack, so a missing one must
    // not set automationUsed. (The key was misspelt "play_us_ra" here until 2026-09 - the fork's d80f67b7 -
    // which reset every game's "Play using RA" to false on each scan.)
    favorite = valueOrDefault("favorite", "0", false);
    play_using_ra = valueOrDefault("play_using_ra", "false", false);
    lightgun = valueOrDefault("lightgun", "0", false);

    tmp = valueOrDefault("discs", "");
    if (!tmp.empty()) {
        vector<string> strings;
        istringstream f(tmp);
        string s;
        while (getline(f, s, ',')) {
            s = Strings::unescapeCommas(s);
            strings.push_back(s);
        }
        for (int i = 0; i < strings.size(); i++) {
            Disc disc;
            disc.diskName = strings[i];
            if (DirEntry::imageTypeUsesACueFile(imageType)) {
                string cueFile = fullPath + sep + disc.diskName + EXT_CUE;
                bool discCueExists = DirEntry::exists(cueFile);
                if (discCueExists) {
                    disc.binVerified = validateCue(cueFile, fullPath );
                    disc.cueFound = true;
                    disc.cueName = disc.diskName;
                }
                discs.push_back(disc);
            }
            if (imageType == IMAGE_PBP) {
                string pbpName = DirEntry::findFirstFile(EXT_PBP, fullPath );
                if (pbpName == disc.diskName) {
                    disc.cueFound = true;
                } else {
                    disc.cueFound = false;
                }

                disc.binVerified = true;
                disc.cueName = disc.diskName;
                discs.push_back(disc);
            }
            if (imageType == IMAGE_CHD) {
                string chdName = DirEntry::findFirstFile(EXT_CHD, fullPath );
                disc.cueFound = true;
                disc.binVerified = true;
                disc.cueName = disc.diskName;
                discs.push_back(disc);
            }
        }
    }
    gameIniValid = true;
}

//*******************************
// UsbGame::saveGameIni
//*******************************
void UsbGame::saveGameIni(const string &path) {
    IniFile ini;
    ini.section = "Game";
    ini.values["title"] = title;
    ini.values["publisher"] = publisher;
    ini.values["year"] = to_string(year);
    ini.values["serial"] = serial;
    ini.values["region"] = region;
    ini.values["players"] = to_string(players);
    ini.values["automation"] = to_string(automationUsed);
    ini.values["imagetype"] = to_string(imageType);
    ini.values["highres"] = to_string(highRes);
    if (memcard.empty())
        ini.values["memcard"] = "SONY";
    else
        ini.values["memcard"] = memcard;

    ini.values["Favorite"] = favorite;
    ini.values["Play_using_ra"] = play_using_ra;
    ini.values["Lightgun"] = lightgun;
    ini.values["Thumbnail_record_name"] = recordName;
    ini.values["Cached_cover_path"] = coverPath;
    ini.values["Cached_snap_path"] = snapPath;

    stringstream ss;
    for (int i = 0; i < discs.size(); i++) {
        ss << Strings::escapeCommas(discs[i].diskName);
        if (i != discs.size() - 1) {
            ss << ",";
        }
    }
    ini.values["discs"] = ss.str();
    ini.save(path);
    gameIniFound = true;
}

//*******************************
// UsbGame::parseIni
//*******************************
void UsbGame::parseIni(const string &path) {
    iniValues.clear();
    IniFile ini;
    ini.load(path);
    if (ini.values.empty()) {
        gameIniFound = false;
        return;
    }
    gameIniFound = true;
    iniValues = ini.values;
}

//*******************************
// UsbGame::loadGameIni
//*******************************
void UsbGame::loadGameIni(const string &path) {
    parseIni(path);
    applyIniValues();
}

//*******************************
// UsbGames += UsbGames
//*******************************
void operator += (UsbGames &dest, const UsbGames &src) {
    copy(begin(src), end(src), back_inserter(dest));
}

} // namespace ableem
