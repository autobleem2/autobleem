//
// Created by screemer on 23.12.18.
//

#include "config.h"
#include "system.h"
#include "../main.h"
#include "../model/timing.h"
#include "core/version.h"
#include "environment.h"

//*******************************
// Config::Config()
//*******************************
Config::Config() {
    std::string path = Env::getPathToStateDir() + sep + "config.ini";
    inifile.load(path);

    // these are no longer used
    inifile.values.erase("stheme");
    inifile.values.erase("autoregion");
    inifile.values.erase("quick");
    inifile.values.erase("quickmenu");
    inifile.values.erase("delay");
    inifile.values.erase("adv");
    inifile.values.erase("ui");      // the classic UI is gone: the app always shows the EvolutionUI launcher now
    inifile.values.erase("version"); // the build says what version it is (core/version.h) since 2026-09-18
    inifile.values.erase("cfg");     // the selection script is <rc>/autobleem_cfg.sh (LaunchService), not a key
    save();

    bool aDefaultWasSet{false};
    if (inifile.values["language"] == "") {
        inifile.values["language"] = "English";
        aDefaultWasSet = true;
    }
    // the shipped config.ini says ab2 too; this is for a config.ini that is missing or came back empty (an
    // unclean unmount on the first Pi boot did that) - the launcher should still come up in its own theme
    if (inifile.values["theme"] == "") {
        inifile.values["theme"] = "ab2";
        aDefaultWasSet = true;
    }
    if (inifile.values["aspect"] == "") {
        inifile.values["aspect"] = "false";
        aDefaultWasSet = true;
    }
    // which PS1 emulator a game starts in (Options -> "PS1 Emulator"): pcsx-abnxt, the next one
    // (Autobleem/bin/emunxt) - the default on every build since 2026-09-21 - or pcsx-ab, the one AutoBleem
    // has always shipped (Autobleem/bin/emu); the launch scripts get the name as their last argument. Both
    // read the same pcsx.cfg and memory cards; a resume point written by one does not load in the other
    // (different save-state versions), the game then starts fresh.
    if (inifile.values["emulator"] != "pcsx-ab" && inifile.values["emulator"] != "pcsx-abnxt") {
        inifile.values["emulator"] = "pcsx-abnxt";
        aDefaultWasSet = true;
    }
    if (inifile.values["jewel"] == "") {
        inifile.values["jewel"] = "default";
        aDefaultWasSet = true;
    }
    if (inifile.values["music"] == "") {
        inifile.values["music"] = "--";
        aDefaultWasSet = true;
    }
    if (inifile.values["showingtimeout"] == "") {
        inifile.values["showingtimeout"] = DefaultShowingTimeoutText;
        aDefaultWasSet = true;
    }

    if (inifile.values["raconfig"] == "") {
        inifile.values["raconfig"] = "true";
        aDefaultWasSet = true;
    }
    // the scan may fetch missing box art (and the databases) from libretro's servers, where the platform
    // has a download_command and the server answers; Options -> "Fetch box art online"
    if (inifile.values["online"] == "") {
        inifile.values["online"] = "true";
        aDefaultWasSet = true;
    }
    // the launcher's online update check (UpdateService, Options -> "Updates"): off | stable | latest.
    // A pre-release build follows the pre-releases by default, a release build the releases.
    if (inifile.values["updates"] == "") {
        inifile.values["updates"] = Version::isPreRelease() ? "latest" : "stable";
        aDefaultWasSet = true;
    }
    // the classic screens' font: the theme's, unless "themefont" is off and "font" names a .ttf/.otf from
    // retroarch/fonts, resources/fonts or the theme's own folder (Options -> Font; "--" is the theme's)
    if (inifile.values["themefont"] == "") {
        inifile.values["themefont"] = "true";
        aDefaultWasSet = true;
    }
    if (inifile.values["font"] == "") {
        inifile.values["font"] = "--";
        aDefaultWasSet = true;
    }

    if (inifile.values["surprisehighscore"] == "") {
        inifile.values["surprisehighscore"] = "0";
        aDefaultWasSet = true;
    }

    inifile.values["pcsx"] = "bleemsync";

    if (aDefaultWasSet)
        save();
}

//*******************************
// Config::save
//*******************************
void Config::save() {
    inifile.values["pcsx"] = "bleemsync";
    std::string path = Env::getPathToStateDir() + sep + "config.ini";
    inifile.save(path);
}
