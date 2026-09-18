//
// Config: resources/config.ini, the app's own settings.
//
#pragma once

#include "../main.h"

//******************
// Config
//******************
// config.ini on top of ableem::IniFile. The constructor loads it, drops the keys older AutoBleem versions
// wrote, and fills in a default for every key the UI expects, so the rest of the app can read a value without
// checking whether it is there. Keys are lower-cased by IniFile, e.g. inifile.values["theme"].
//
// Owned by App (App::config()) - there is no global instance. Bool-ish values are the strings "true"/"false".
class Config {
public:
    IniFile inifile;

    Config();
    void save(); // writes resources/config.ini back out
};
