#include "ableem/engine/ini_file.h"
#include "ableem/engine/filesystem.h"
#include "ableem/engine/strings.h"

#include <iostream>
#include <fstream>

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
        cout << "Error opening ini file: " << path << endl;
        return;
    }

    while (Strings::getlineRemoveCR(file, iniLine)) {
        Strings::removeComment(iniLine);   // remove '#' to end of line
        iniLine = trim(iniLine);
        if (iniLine.length() == 0) continue;    // blank line
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

        if (file.eof()) break;
    };
    file.close();
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
    cout << "Writing ini file: " << _path << endl;
    ofstream os;
    os.open(_path);
    if (!DirEntry::checkWritable(os, _path)) return;
    os << "[" << section << "]" << endl;
    for (map<string, string>::iterator iter = values.begin(); iter != values.end(); ++iter) {
        string k = iter->first;
        string v = iter->second;
        k = lcase(k);
        if (k == "publisher")
            Strings::cleanPublisherString(v);
        k[0] = toupper(k[0]);

        os << k << "=" << v << endl;
    }
    os.flush();
    os.close();
}

//*******************************
// IniFile::print
//*******************************
void IniFile::print() {
    cout << "section = " << section << '\n';
    cout << "path = " << path << '\n';
    cout << "entry = " << entry << '\n';

    for (auto &item : values)
        cout << item.first << " = " << item.second << '\n';
    cout << flush;
}

} // namespace ableem
