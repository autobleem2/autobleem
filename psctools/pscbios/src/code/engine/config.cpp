//
// Created by screemer on 23.12.18.
//

#include "config.h"
#include "../util.h"
#include "../DirEntry.h"
#include "../environment.h"

//*******************************
// Config::Config()
//*******************************
Config::Config()
{
    std::string path=Env::getWorkingPath() + sep + "config.ini";
    inifile.load(path);

    if (inifile.values["language"]=="")
    {
        inifile.values["language"]="English";
    }


    if (inifile.values["music"]=="")
    {
        inifile.values["music"]="--";
    }

    // try to load language from autobleem

    if (DirEntry::exists("/media/Autobleem/bin/autobleem/config.ini"))
    {
        Inifile abconfig;
        abconfig.load("/media/Autobleem/bin/autobleem/config.ini");
        inifile.values["language"]=abconfig.values["language"];
    }


}

//*******************************
// Config::save
//*******************************
void Config::save()
{

}
