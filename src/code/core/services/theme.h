//
// Theme: the UI theme's theme.json and the directory it is read from.
//
#pragma once

#include "config.h"
#include "../main.h"

#include <string>

//******************
// Theme
//******************
// Everything about the current theme that is *not* graphics: which directory it lives in, and its
// ThemeSpec - the selected theme's theme.json merged over themes/default/theme.json, so a partial theme
// still has a value for every key, with every file name resolved to an absolute path (the theme's own
// file when it has one, the default theme's otherwise). Owned by App; ThemeAssets, AppAudio and the
// launcher only turn the values here into textures, sounds and fonts.
//
// A theme dropped in as <name>.zip is unpacked to <name>/ by ThemeInstaller, and a theme folder still in the
// old layout (theme.ini + the console's data tree) is converted in place by ThemeConverter, both the first
// time load() meets them. Nothing here knows the console's real paths: they all
// come from Env.
class Theme {
public:
    // which theme is config.ini's "theme"; a folder that is not a theme at all is written back there as "default"
    explicit Theme(Config &config) : config_(config) {}

    // (re)reads themes/default/theme.json and merges the selected theme's theme.json over it, converting
    // either folder from the old layout first. If the selected folder is not a theme it falls back to
    // "default" and writes that back to config.ini.
    void load();

    std::string path();          // <themes>/<name>, or <themes>/default when there is no such folder

    // the merged, resolved theme. Every file field is an absolute path or "" (no theme has that file).
    const ThemeSpec &spec() const { return spec_; }
    const ClassicTheme &classic() const { return spec_.classic; }
    const LauncherTheme &launcher() const { return spec_.launcher; }
    const ableem::ThemeSounds &sounds() const { return spec_.sounds; }
    const ableem::ThemeMusic &music() const { return spec_.music; }

    // the directory load() actually read
    const std::string &loadedPath() const { return loadedPath_; }

private:
    Config &config_;
    ThemeSpec spec_;
    std::string loadedPath_;

    static std::string defaultsPath();
};
