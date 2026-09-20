#include "ableem/engine/ini_file.h"
#include "ableem/engine/filesystem.h"
#include "ableem/engine/strings.h"

#include <iostream>
#include <fstream>
#include "ableem/engine/log.h"
#include <ableem/engine/log.h>

using namespace std;

namespace ableem {

//*******************************
// IniFile::load
//*******************************
void IniFile::load(const string &_path) {
    this->path = _path;
    ifstream file;
    string iniLine;
    file.open(path);

    if (!file.good()) {
        PLOG_WARNING << "Error opening ini file: " << path;
        return;
    }

    while (Strings::getlineRemoveCR(file, iniLine)) {
        Strings::removeComment(iniLine); // remove '#' to end of line
        iniLine = trim(iniLine);
        if (iniLine.length() == 0)
            continue; // blank line
        if (iniLine[0] == '[') {
            iniLine = ltrim(iniLine);
            iniLine = iniLine.substr(1, iniLine.find(']') - 1);
            section = iniLine;
        }
        if (iniLine.find('=') != string::npos) {
            iniLine = lcase(iniLine, iniLine.find('='));
            string paramName = iniLine.substr(0, iniLine.find('='));
            string paramVal = iniLine.substr(iniLine.find('=') + 1, string::npos);
            if (paramName == "publisher")
                Strings::cleanPublisherString(paramVal);
            values[paramName] = paramVal;
        }

        if (file.eof())
            break;
    };
    file.close();
    // a file that opens but holds nothing is not "no file": it is what an unclean unmount leaves behind
    // (the first 64-bit Pi image's config.ini, 2026-09-20) - say so, the caller's defaults take over
    if (section.empty() && values.empty()) {
        PLOG_WARNING << "Ini file is empty: " << path;
    }
}

//*******************************
// IniFile::reload
//*******************************
void IniFile::reload(const string &_path) {
    values.clear();
    load(_path);
}

//*******************************
// IniFile::mergeFrom
//*******************************
void IniFile::mergeFrom(const string &_path) {
    load(_path);
}

//*******************************
// IniFile::save
//*******************************
void IniFile::save(const string &_path) {
    PLOG_INFO << "Writing ini file: " << _path;
    // written next to the target and renamed over it: a save that is cut short (a power cut, a reboot with
    // the partition still dirty) then leaves the old file, not an empty one that reads as "no settings"
    const string tmp = _path + ".tmp";
    ofstream os;
    os.open(tmp);
    if (!DirEntry::checkWritable(os, tmp))
        return;
    os << "[" << section << "]" << endl;
    for (auto &item : values) {
        string k = item.first;
        string v = item.second;
        k = lcase(k);
        if (k == "publisher")
            Strings::cleanPublisherString(v);
        k[0] = toupper(k[0]);

        os << k << "=" << v << endl;
    }
    os.flush();
    os.close();
    if (!DirEntry::replaceFile(tmp, _path)) {
        PLOG_ERROR << "Could not replace " << _path << " with the new " << tmp;
    }
}

//*******************************
// IniFile::print
//*******************************
void IniFile::print() {
    PLOG_DEBUG << "section = " << section;
    PLOG_DEBUG << "path = " << path;
    PLOG_DEBUG << "entry = " << entry;

    for (auto &item : values)
        PLOG_DEBUG << item.first << " = " << item.second;
}

} // namespace ableem
