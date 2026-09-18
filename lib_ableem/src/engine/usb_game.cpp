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
            cout << i << discs[i].diskName << discs[i].cueFound << endl;
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
    if (!coverImageFound) {
        if (failureReasons)
            failureReasons->emplace_back("Cover image file not found");
        result = false;
    }
    if (!pcsxCfgFound) {
        if (failureReasons)
            failureReasons->emplace_back("pcsx.cfg file not found");
        result = false;
    }

    if (!result) {
        cerr << "Game: " << title << " Validation Failed" << endl;
    }

    return result;
}

//*******************************
// UsbGame::print
//*******************************
bool UsbGame::print() {
    cout << "-------------------" << endl;
    cout << "Printing game data:" << endl;
    cout << "-----------------" << endl;
    cout << "AUTOMATION: " << automationUsed << endl;
    cout << "Game folder id: " << folder_id << endl;
    cout << "Game: " << title << endl;
    cout << "Players: " << players << endl;
    cout << "Publisher: " << publisher << endl;
    cout << "Year: " << year << endl;
    cout << "Serial: " << serial << endl;
    cout << "Region: " << region << endl;
    cout << "GameData found: " << gameDataFound << endl;
    cout << "Game.ini found: " << gameIniFound << endl;
    cout << "Game.ini valid: " << gameIniValid << endl;
    cout << "PNG found:" << coverImageFound << endl;
    cout << "pcsx.cfg found: " << pcsxCfgFound << endl;
    cout << "TotalDiscs: " << discs.size() << endl;
    cout << "Favorite: " << favorite << endl;
    cout << "Play Using RA: " << play_using_ra << endl;
    cout << "Last Played: " << last_played << endl;

    for (int i = 0; i < discs.size(); i++) {
        cout << "  Disc:" << i + 1 << "  " << discs[i].diskName << endl;
        cout << "  CUE found: " << discs[i].cueFound << endl;
        cout << "  BIN correct: " << discs[i].binVerified << endl;
    }

    vector<string> failureReasons;
    bool result = verify(&failureReasons);
    if (result) {
        cout << "-------Game Verify OK-------" << endl;
    } else {
        cout << "------Game Verify FAIL------" << endl;
        for (const auto & reason : failureReasons)
            cout << "Reason: " << reason << endl;
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
            cout << "Switching automation in PBP" << endl;
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
            cout << "Switching automation in CHD" << endl;
        }
    }
    if (DirEntry::imageTypeUsesACueFile(this->imageType)) {
        if (discs.size() == 0) {
            automationUsed = true;
            cout << "Switching automation no discs" << endl;
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

    if (discs.size() > 0) {
        if (!coverImageFound) {
            automationUsed = true;
            cout << "Switching automation no image" << endl;
            string source = workingPath + sep + "default.png";
            string destination = fullPath + sep + discs[0].diskName + ".png";
            cerr << "SRC:" << source << " DST:" << destination << endl;
            DirEntry::copy(source, destination);
            // maybe we can do better ?
            cout << "getting serial from Image File" << endl;
         
            string serial = SerialScanner::readSerial(imageType, fullPath, firstBinPath);
            if (serial != "") {

                if (metadata.findBySerial(serial, md)) {
                    metadataLoaded = true;
                    cout << "Updating cover in recoverMissingFiles()" << destination << endl;
                    ofstream pngFile;
                    pngFile.open(destination, ios::binary);
                    if (DirEntry::checkWritable(pngFile, destination)) {
                        pngFile.write(md.bytes.data(), md.bytes.size());
                        pngFile.flush();
                        pngFile.close();
                        automationUsed = false;
                        coverImageFound = true;
                    }
                };
                md.clearCover();

            }
            coverImageFound = true;
        }
    }

    if (!pcsxCfgFound) {
        automationUsed = true;
        cout << "Switching automation no pcsx" << endl;
        string source = workingPath + sep + PCSX_CFG;
        string destination = fullPath + sep + PCSX_CFG;
        cerr << "SRC:" << source << " DST:" << destination << endl;

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
    // favorite and play_using_ra are newer fields that older Game.ini files lack, so a missing one must
    // not set automationUsed. (The key was misspelt "play_us_ra" here until 2026-09 - the fork's d80f67b7 -
    // which reset every game's "Play using RA" to false on each scan.)
    favorite = valueOrDefault("favorite", "0", false);
    play_using_ra = valueOrDefault("play_using_ra", "false", false);

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
    //cout << "Overwritting ini file" << path << endl;
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
