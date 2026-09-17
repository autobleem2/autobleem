//
// Theme: the UI theme's theme.json and the directory it is read from.
//

#include "theme.h"
#include "environment.h"
#include "theme_converter.h"

#include <iostream>

using namespace std;

namespace {
const char *THEME_JSON = "theme.json";

// a folder is a theme when it has a theme.json, or the old layout the converter reads
bool isTheme(const string &dir) {
    return DirEntry::exists(dir + sep + THEME_JSON) || ThemeConverter::needsConversion(dir);
}
} // namespace

//*******************************
// Theme::defaultsPath
//*******************************
string Theme::defaultsPath() {
    return Env::getPathToThemesDir() + sep + "default";
}

//*******************************
// Theme::path
//*******************************
string Theme::path() {
    string path = Env::getPathToThemesDir() + sep + config_.inifile.values["theme"];
    if (!DirEntry::exists(path)) {
        path = defaultsPath();
    }
    return path;
}

//*******************************
// Theme::load
//*******************************
void Theme::load() {
    const string defaultsDir = defaultsPath();
    loadedPath_ = path();

    cout << "Loading UI theme:" << loadedPath_ << endl;
    if (!isTheme(loadedPath_)) {
        loadedPath_ = defaultsDir;
        config_.inifile.values["theme"] = "default";
        config_.save();
    }

    if (ThemeConverter::needsConversion(defaultsDir)) ThemeConverter::convert(defaultsDir);
    if (loadedPath_ != defaultsDir && ThemeConverter::needsConversion(loadedPath_))
        ThemeConverter::convert(loadedPath_);

    ThemeSpec defaults;
    defaults.load(defaultsDir + sep + THEME_JSON);

    spec_ = ThemeSpec();
    spec_.load(loadedPath_ + sep + THEME_JSON);
    spec_.mergeOver(defaults);
    spec_.resolveFiles(loadedPath_, defaults, defaultsDir);
}
