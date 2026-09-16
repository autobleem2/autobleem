//
// Theme: the UI theme's ini file and the directories it is read from.
//
#pragma once

#include <string>
#include "../core/main.h"

//******************
// Theme
//******************
// Everything about the current theme that is *not* graphics: which directory it lives in, and the merged
// theme.ini (the selected theme's values on top of themes/default/theme.ini, so a partial theme still has a
// value for every key). Owned by App; the Gui only turns the values here into textures and fonts.
//
// The directory getters fall back to the stock Sony data dir (Env::getSonyPath()) when the theme, or one of
// its sub-directories, is missing - that is what makes a theme that ships only, say, its own images still
// work. Nothing here knows the console's real paths: they all come from Env.
class Theme {
public:
    // (re)reads themes/default/theme.ini and merges the selected theme's theme.ini over it. If the selected
    // theme has no theme.ini at all it falls back to "default" and writes that back to config.ini.
    void load();

    std::string path();          // <themes>/<name>, or the Sony data dir
    std::string imagePath();     // path()/images
    std::string fontPath();      // path()/font
    std::string soundPath();     // path()/sounds

    // the merged theme.ini. `data` is what screens read (gui->themeData used to be this same object);
    // `defaults` is themes/default/theme.ini alone, used when the theme has no file for a texture.
    IniFile data;
    IniFile defaults;

    const std::string &value(const std::string &key) { return data.values[key]; }
    int intValue(const std::string &key) { return atoi(data.values[key].c_str()); }

    // the directory load() actually read, with a trailing separator. Used to resolve the relative file names
    // in theme.ini (music, images, font).
    const std::string &loadedPath() const { return loadedPath_; }
    const std::string &defaultsPath() const { return defaultsPath_; }

private:
    std::string loadedPath_;
    std::string defaultsPath_;

    // themes/<name>/<subDir>, falling back to the sony data dir if either is missing
    std::string themeSubDir(const std::string &subDir);
};
