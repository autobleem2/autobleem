//
// Theme: the UI theme's ini file and the directories it is read from.
//

#include "theme.h"
#include "../app.h"
#include "../core/services/environment.h"

#include <iostream>

using namespace std;

//*******************************
// Theme::path
//*******************************
string Theme::path() {
    string path = Env::getPathToThemesDir() + sep + App::get().config().inifile.values["theme"];
    if (!DirEntry::exists(path)) {
        path = Env::getSonyPath();
    }
    return path;
}

//*******************************
// Theme::themeSubDir
//*******************************
string Theme::themeSubDir(const string &subDir) {
    string path = Theme::path() + sep + subDir;
    if (!DirEntry::exists(path)) {
        path = Env::getSonyPath() + sep + subDir;
    }
    return path;
}

//*******************************
// Theme::imagePath / fontPath / soundPath
//*******************************
string Theme::imagePath() { return themeSubDir("images"); }

string Theme::fontPath() { return themeSubDir("font"); }

string Theme::soundPath() { return themeSubDir("sounds"); }

//*******************************
// Theme::load
//*******************************
void Theme::load() {
    defaultsPath_ = Env::getPathToThemesDir() + sep + "default" + sep;
    loadedPath_ = path() + sep;

    cout << "Loading UI theme:" << loadedPath_ << endl;
    if (!DirEntry::exists(loadedPath_ + "theme.ini")) {
        loadedPath_ = defaultsPath_;
        App::get().config().inifile.values["theme"] = "default";
        App::get().config().save();
    }

    defaults.load(defaultsPath_ + "theme.ini");
    data.load(defaultsPath_ + "theme.ini");
    data.mergeFrom(loadedPath_ + "theme.ini");    // adds to default/theme.ini values
}
