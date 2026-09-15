
#include <string>
#include <unistd.h>
#include "util.h"
#include "DirEntry.h"
#include "engine/inifile.h"
#include "engine/memcard.h"

using namespace std;

//*******************************
// valueOrDefault
//*******************************
string valueOrDefault(string name, string def, map<string,string> iniValues) {
    string value;
    if (iniValues.find(name) != iniValues.end()) {
        value = Util::trim(iniValues.find(name)->second);
        if (value.length() == 0) {

            return def;
        }
    } else {

        value = def;
    }
    return value;
}

#define PCSX "/tmp/pcsx"

//*******************************
// main
//*******************************
int main (int argc, char *argv[])
{
    string path="/data/AppData/sony/title/";
    string sourceCard="/media/Games/!MemCards/";
    Inifile ini;
    ini.load(path+"Game.ini");
    string imageType=valueOrDefault("imagetype","0",ini.values);
    string memcard=valueOrDefault("memcard","SONY",ini.values);

    Inifile cfg;
    cfg.load("/media/Autobleem/bin/autobleem/config.ini");

    if (memcard!="SONY")
    {
        if (DirEntry::exists(sourceCard+memcard))
        {
            Memcard card("/media/Games/");
            if (!card.swapIn("./.pcsx",memcard))
            {
                memcard = "SONY";
                ini.values["memcard"]="SONY";
                ini.save(path+"Game.ini");
            };
        }
    }

    vector<string> arguments;
    for (int i=0;i<argc;i++)
    {
        arguments.push_back(argv[i]);
    }

    if (cfg.values["aspect"]=="true")
    {
        arguments.push_back("-ratio");
        arguments.push_back("1");

    } else
    {
        arguments.push_back("-ratio");
        arguments.push_back("0");
    }

    if (cfg.values["mip"]=="true")
    {
        arguments.push_back("-filter");
        arguments.push_back("1");

    } else
    {
        arguments.push_back("-filter");
        arguments.push_back("0");
    }

    if (imageType!="0")
    {
        for (int i=0;i<arguments.size();i++)
        {
            if (arguments[i]=="-cdfile")
            {
                string image=arguments[i+1];
                if (DirEntry::matchExtension(image,".cue")) {
                    image = image.substr(0, image.size() - 4);
                }
                arguments[i+1] = image;
            }
        }
    }

    // arguments[0] is this program's own name. runAndWait adds argv[0] for pcsx itself.
    arguments.erase(arguments.begin());
    Util::runAndWait(PCSX, arguments);

    if (memcard!="SONY")
    {
            Memcard card("/media/Games/");
            card.swapOut("./.pcsx",memcard);
    }

    return 0;
}
