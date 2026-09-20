#include "ableem/engine/config_file_editor.h"
#include "ableem/engine/environment.h"
#include "ableem/engine/filesystem.h"
#include "ableem/engine/strings.h"

#include <fstream>
#include <iostream>
#include <vector>
#include "ableem/engine/log.h"

using namespace std;

namespace ableem {

namespace {

// Does this line set `property`? Both are already lower-cased. The key must be the whole word: followed by
// whitespace, '=' or the end of the line. Matching it as a bare prefix used to make "input_overlay" claim
// "input_overlay_enable" and "input_overlay_opacity" too (and "pcsx_rearmed_frameskip" the newer
// "pcsx_rearmed_frameskip_type"), so a replace clobbered its neighbours and the replaces meant for them
// then found nothing.
bool lineSetsProperty(const string &lcaseline, const string &lcasepattern) {
    if (lcaseline.rfind(lcasepattern, 0) != 0)
        return false;
    if (lcaseline.size() == lcasepattern.size())
        return true;
    char next = lcaseline[lcasepattern.size()];
    return next == ' ' || next == '\t' || next == '=' || next == '\r';
}

} // namespace

//*******************************
// ConfigFileEditor::replaceProperty
//*******************************
// A key the file does not have is appended (since 2026-09-20): a pcsx.cfg copied from an older default
// has no line for an option added later (SlowBoot), and the editor's change must still land.
void ConfigFileEditor::replaceProperty(string fullCfgFilePath, string property, string newline) {
    if (!DirEntry::exists(fullCfgFilePath)) {
        PLOG_INFO << "  cfg file doesn't exist";
        return;
    }
    // do not store if file not updated (one less iocall on filesystem)
    bool fileUpdated = false;

    fstream file(fullCfgFilePath, ios::in);
    vector<string> lines;
    lines.clear();

    if (file.is_open()) {

        string line;
        vector<string> lines;

        while (getline(file, line)) {

            string::size_type pos = 0;
            string lcaseline = line;
            string lcasepattern = property;
            lcase(lcaseline);
            lcase(lcasepattern);

            if (lineSetsProperty(lcaseline, lcasepattern)) {
                fileUpdated = true;
                PLOG_INFO << "  new line: '" << newline << "'";
                lines.push_back(newline);
            } else {
                lines.push_back(line);
            }
        }
        file.close();
        if (!fileUpdated) {
            PLOG_INFO << "  appending: '" << newline << "'";
            lines.push_back(newline);
            fileUpdated = true;
        }
        if (fileUpdated) {
            file.open(fullCfgFilePath, ios::out | ios::trunc);

            for (const auto &i : lines) {
                file << i << endl;
            }
            file.flush();
            file.close();
        }
    }
}

//*******************************
// ConfigFileEditor::getValueFromCfgFile
//*******************************
string ConfigFileEditor::getValueFromCfgFile(string fullCfgFilePath, string property) {
    fstream file(fullCfgFilePath, ios::in);
    vector<string> lines;
    lines.clear();

    if (file.is_open()) {
        string line;
        while (getline(file, line)) {
            string lcaseline = line;
            string lcasepattern = property;
            lcase(lcaseline);
            lcase(lcasepattern);

            if (lineSetsProperty(lcaseline, lcasepattern)) {
                string value = line.substr(lcaseline.find("=") + 1);
                if (!value.empty() && value.back() == '\r') {
                    value.pop_back(); // remove the trailing /r
                }
                trim(value); // remove leading and trailing spaces
                PLOG_INFO << "  return: '" << value << "'";
                return value;
            }
        }
        file.close();
    }
    PLOG_INFO << "  return: ''";
    return "";
}

//*******************************
// ConfigFileEditor::getValue
// example gamePath = "/media/Games/!SaveStates/7"
// example gamePath = "/media/Games/!SaveStates/Driver 2" or
// example gamePath = "/media/Games/Racing/Driver 2"
//*******************************
string ConfigFileEditor::getValue(string gamePath, string property) {
    string fullCfgFilePath = gamePath + sep + PCSX_CFG;
    if (!DirEntry::exists(fullCfgFilePath)) {
        PLOG_INFO << "  cfg file doesn't exist";
        PLOG_INFO << "  return: ''";
        return "";
    }

    return getValueFromCfgFile(fullCfgFilePath, property);
}

//*******************************
// ConfigFileEditor::replacePropertyInAllCfgsInDir
// example pathToCfgDir = "/media/Games/!SaveStates/12/cfg"
// example pathToCfgDir = "/media/Games/!SaveStates/Driver 2/cfg"
//*******************************
void ConfigFileEditor::replacePropertyInAllCfgsInDir(string pathToCfgDir, string property, string newline) {
    PLOG_INFO << "cfg replaceInAllCfg, '" << pathToCfgDir << "', '" << property << "'";
    for (const DirEntry &cfgEntry : DirEntry::diru_FilesOnly(pathToCfgDir)) {
        if (DirEntry::matchExtension(cfgEntry.name, ".cfg")) {
            string fullCfgFilePath = pathToCfgDir + sep + cfgEntry.name;
            replaceProperty(fullCfgFilePath, property, newline);
        }
    }
}

//*******************************
// ConfigFileEditor::replaceInternal
// example gamePathInSaveStates = "/media/Games/!SaveStates/12"
//*******************************
void ConfigFileEditor::replaceInternal(string gamePathInSaveStates, string property, string newline) {
    string realCfgPath = gamePathInSaveStates + sep + PCSX_CFG;
    replaceProperty(realCfgPath, property, newline);

    replacePropertyInAllCfgsInDir(gamePathInSaveStates + sep + "cfg", property, newline);
}

//*******************************
// ConfigFileEditor::replaceUsb
// example entry = "Driver 2"
// example gamePath = "/media/Games/Racing"
//*******************************
void ConfigFileEditor::replaceUsb(string entry, string gamePath, string property, string newline) {
    string realCfgPath = gamePath + sep + entry + sep + PCSX_CFG;
    replaceProperty(realCfgPath, property, newline); // replace in the game dir pcsx.cfg

    realCfgPath = Environment::getPathToSaveStatesDir() + sep + entry + sep + PCSX_CFG;
    replaceProperty(realCfgPath, property, newline); // replace in the !SaveStates/game/pcsx.cfg

    // replace in the !SaveStates/game/cfg/*.cfg
    replacePropertyInAllCfgsInDir(Environment::getPathToSaveStatesDir() + sep + entry + sep + "cfg", property, newline);
}

//*******************************
// ConfigFileEditor::replace
// example entry = "Driver 2"
// example gamePath = "/media/Games/Racing/Driver 2", internal = false
// example gamePath = "/media/Games/!SaveStates/12", internal = true
//*******************************
void ConfigFileEditor::replace(string entry, string gamePath, string property, string newline, bool internal) {
    if (internal)
        replaceInternal(gamePath, property, newline);
    else
        replaceUsb(entry, DirEntry::getDirNameFromPath(gamePath), property, newline);
}

//*******************************
// ConfigFileEditor::replaceInFile
//*******************************
void ConfigFileEditor::replaceInFile(std::string fullCfgFilePath, std::string property, std::string newline) {
    PLOG_INFO << "cfg replaceInFile, '" << fullCfgFilePath << "', '" << property << "'";
    replaceProperty(fullCfgFilePath, property, newline);
}

} // namespace ableem
