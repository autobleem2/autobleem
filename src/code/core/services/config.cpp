//
// Created by screemer on 23.12.18.
//

#include "config.h"
#include "system.h"
#include "../main.h"
#include "../model/timing.h"
#include "environment.h"

//*******************************
// Config::Config()
//*******************************
Config::Config()
{
    std::string path=Env::getWorkingPath() + sep + "config.ini";
    inifile.load(path);

    // these are no longer used
    inifile.values.erase("stheme");
    inifile.values.erase("autoregion");
    inifile.values.erase("quick");
    inifile.values.erase("quickmenu");
    inifile.values.erase("delay");
    inifile.values.erase("adv");
    inifile.values.erase("ui");   // the classic UI is gone: the app always shows the EvolutionUI launcher now
    inifile.values.erase("version");   // the build says what version it is (core/version.h) since 2026-09-18
    save();

    bool aDefaultWasSet {false};
    if (inifile.values["language"]=="")
    {
        inifile.values["language"]="English";
        aDefaultWasSet = true;
    }
    if (inifile.values["aspect"]=="")
    {
        inifile.values["aspect"]="false";
        aDefaultWasSet = true;
    }
    if (inifile.values["jewel"]=="")
    {
        inifile.values["jewel"]="default";
        aDefaultWasSet = true;
    }
    if (inifile.values["music"]=="")
    {
        inifile.values["music"]="--";
        aDefaultWasSet = true;
    }
    if (inifile.values["showingtimeout"]=="")
    {
        inifile.values["showingtimeout"]=DefaultShowingTimeoutText;
        aDefaultWasSet = true;
    }

    if (inifile.values["raconfig"]=="")
    {
        inifile.values["raconfig"]="true";
        aDefaultWasSet = true;
    }

    if (inifile.values["surprisehighscore"]=="")
    {
        inifile.values["surprisehighscore"]="0";
        aDefaultWasSet = true;
    }

    inifile.values["pcsx"]="bleemsync";

    if (aDefaultWasSet)
        save();
}

//*******************************
// Config::save
//*******************************
void Config::save()
{
    inifile.values["pcsx"]="bleemsync";
    std::string path=Env::getWorkingPath() + sep + "config.ini";
    inifile.save(path);
}
