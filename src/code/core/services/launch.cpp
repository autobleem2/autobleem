//
// LaunchService: what App::launchGame and the three EmuInterceptors used to do between them.
//
#include "launch.h"
#include "environment.h"
#include "../main.h"
#include "system.h"

#include <ableem/engine/config_file_editor.h>

#include <cstdio>
#include <ctime>
#include <fstream>
#include <iostream>
#include <unistd.h>
#include <ableem/engine/log.h>

using namespace std;

namespace {

const char *const RaNeonCore = "NEON";
const char *const RaPeopsCore = "PEOPS";
const char *const PcsxNeonGpu = "builtin_gpu";

} // namespace

//*******************************
// LaunchService::pcsxLauncherScript
//*******************************
string LaunchService::pcsxLauncherScript() {
    return Env::getPathToRCDir() + sep + "launch.sh";
}

//*******************************
// LaunchService::retroArchLauncherScript
//*******************************
string LaunchService::retroArchLauncherScript() {
    return Env::getPathToRCDir() + sep + "launch_rb.sh";
}

//*******************************
// LaunchService::raSavesDir / raConfigFile / raCoreOptionsFile
//*******************************
string LaunchService::raSavesDir() {
    return Env::getPathToRetroarchDir() + sep + "saves";
}

string LaunchService::raConfigFile() {
    return Env::getPathToRetroarchDir() + sep + "retroarch.cfg";
}

string LaunchService::raCoreOptionsFile() {
    return Env::getPathToRetroarchDir() + sep + "config" + sep + "retroarch-core-options.cfg";
}

//*******************************
// LaunchService::selectionScriptFile
//*******************************
string LaunchService::selectionScriptFile() {
    return Env::getPathToRCDir() + sep + "autobleem_cfg.sh";
}

//*******************************
// LaunchService::writeSelectionScript
//*******************************
void LaunchService::writeSelectionScript() {
    ofstream os;
    string path = selectionScriptFile();
    os.open(path);
    if (!DirEntry::checkWritable(os, path))
        return; // the rc scripts then keep the previous selection
    os << "#!/bin/sh" << endl << endl;
    os << "AB_SELECTION=" << session_.menuOption << endl;
    os << "AB_THEME=" << config_.inifile.values["theme"] << endl;
    os << "AB_PCSX=" << config_.inifile.values["pcsx"] << endl;
    os << "AB_MIP=" << config_.inifile.values["mip"] << endl;

    os.flush();
    os.close();
}

//*******************************
// LaunchService::pathFor
//*******************************
LaunchService::Path LaunchService::pathFor(const PsGame &game, EmuMode mode) {
    if (game.foreign) {
        return game.app ? Path::App : Path::RetroArch;
    }
    return mode == EmuMode::Pcsx ? Path::Pcsx : Path::RetroArch;
}

//*******************************
// LaunchService::launch
//*******************************
void LaunchService::launch(PsGamePtr &game, EmuMode mode, int resumePoint) {
    writeSelectionScript();

    switch (pathFor(*game, mode)) {
    case Path::Pcsx:
        memcards_.swapInForLaunch(*game);
        resumePoints_.prepareForLaunch(*game, resumePoint);
        launchPcsx(*game, resumePoint);
        memcards_.swapOutAfterLaunch(*game);
        break;

    case Path::RetroArch:
        raMemcardIn(*game);
        launchRetroArch(*game);
        raMemcardOut(*game);
        break;

    case Path::App:
        launchApp(*game);
        break;
    }
}

//*******************************
// LaunchService::discImageFor
//*******************************
string LaunchService::discImageFor(const PsGame &game) {
    string gameFile = game.folder + sep + game.base;
    if (!(DirEntry::matchExtension(game.base, ".pbp") || DirEntry::matchExtension(game.base, ".chd"))) {
        gameFile += ".cue";
    }
    return gameFile;
}

//*******************************
// LaunchService::raBaseNameFor
//*******************************
string LaunchService::raBaseNameFor(const PsGame &game) {
    if (DirEntry::isPBPFile(game.base)) {
        return game.base.substr(0, game.base.length() - 4);
    }
    return game.base;
}

//*******************************
// LaunchService::launchPcsx
//*******************************
// args, as rc/launch.sh reads them: ssFolder, cdfile, lang, region, gameFolder, resume, aspect, filter, pad,
// emulator (config.ini's "emulator": pcsx-ab or pcsx-abnxt - which Autobleem/bin folder the script runs)
void LaunchService::launchPcsx(PsGame &game, int resumePoint) {
    PLOG_INFO << "calling LaunchService::launchPcsx()";

    library_.updateDatePlayed(game, time(nullptr));

    string lastCDpoint = game.ssFolder + sep + "lastcdimg.txt";
    string lastCDpointX = game.ssFolder + sep + "lastcdimg." + to_string(resumePoint) + ".txt";
    vector<string> args;
    string gameFile = "";

    string region = "2"; // need to find out if console is jap to switch to 2 - later on
    string aspect = "0";
    if (config_.inifile.values["aspect"] == "true") {
        aspect = "1";
    }

    string filter = "0";
    if (config_.inifile.values["mip"] == "true") {
        filter = "1";
    } else {
        filter = "0";
    }

    trim(game.ssFolder);
    game.ssFolder = DirEntry::removeSeparatorFromEndOfPath(game.ssFolder);

    args.push_back(game.ssFolder);

    remove(lastCDpoint.c_str());

    if (DirEntry::exists(lastCDpointX)) {
        // resuming: the state's own record of which disc was in the drive is what PCSX must be given
        DirEntry::copy(lastCDpointX, lastCDpoint);
        ifstream is(lastCDpointX.c_str());
        if (is.is_open()) {
            std::string line;
            std::getline(is, line);

            // last line is our filename
            gameFile = line;
            is.close();
        }
    } else {
        gameFile = discImageFor(game);
    }

    args.push_back(gameFile);
    // hack to get language from lang file
    string langStr = _("|@lang|");
    if (langStr == "|@lang|") {
        langStr = "2";
    }
    args.push_back(langStr); // lang by language file hack
    args.push_back(region);
    args.push_back(game.folder);
    args.push_back(resumePoint != -1 ? "1" : "0");
    args.push_back(aspect);
    args.push_back(filter);
    args.push_back("NA"); // pad mapping per-game was never wired up; this was always the fallback
    args.push_back(config_.inifile.values["emulator"]);

    runner_.run(pcsxLauncherScript(), args);
    cleanupPcsxConfig(game);

    usleep(3 * 1000);
}

//*******************************
// LaunchService::cleanupPcsxConfig
//*******************************
void LaunchService::cleanupPcsxConfig(PsGame &game) {
    // copy back config to its place
    ConfigFileEditor processor;
    string newConfig = game.ssFolder + sep + "autobleem.cfg";
    if (DirEntry::exists(newConfig)) {
        // fix bios
        processor.replaceInFile(newConfig, "Bios", "Bios = SET_BY_PCSX");

        if (!game.internal) {
            DirEntry::copy(newConfig, game.ssFolder + sep + PCSX_CFG);
            DirEntry::copy(newConfig, game.folder + sep + PCSX_CFG);
        } else {
            DirEntry::copy(newConfig, game.ssFolder + sep + PCSX_CFG);
        }
        DirEntry::removeFile(newConfig);
    }
}

//*******************************
// LaunchService::launchRetroArch
//*******************************
// args, as rc/launch_rb.sh reads them: file, core
void LaunchService::launchRetroArch(PsGame &game) {
    PLOG_INFO << "calling LaunchService::launchRetroArch()";

    // one of our own games: a playlist entry's gameId is only its index in the playlist, and would name
    // some unrelated row in regional.db
    if (!game.foreign) {
        library_.updateDatePlayed(game, time(nullptr));
    }

    string gameFile = "";

    PLOG_INFO << "Starting RetroArch Emu";

    if (game.foreign) {
        PLOG_INFO << "RA FOREIGN MODE";
    }

    if (!game.foreign) {
        gameFile = discImageFor(game);
        string base = raBaseNameFor(game);
        if (DirEntry::exists(game.folder + sep + base + ".m3u")) {
            gameFile = game.folder + sep + base + ".m3u";
        }
    } else {
        gameFile = game.image_path;
    }
    // figure out which plugin is selected
    string gpu;
    if (!game.foreign) {
        ConfigFileEditor processor;
        string path = game.folder;
        if (game.internal) {
            path = game.ssFolder;
        }

        gpu = processor.getValue(path, "gpu3");
        gpu = Strings::trim(gpu);
        if (gpu.empty()) {
            gpu = PcsxNeonGpu;
        }
    } else {
        gpu = "NONE";
    }
    PLOG_INFO << "Using GPU plugin: " << gpu;

    string RACore = RaNeonCore;
    if (gpu != PcsxNeonGpu) {
        RACore = RaPeopsCore;
    }

    if (game.foreign) {
        RACore = game.core_path;
    }
    vector<string> args{gameFile, RACore};

    // core config here - to be optional
    if (config_.inifile.values["raconfig"] == "true") {
        backupRaConfig();
        transferRaConfig(game);
    }

    runner_.run(retroArchLauncherScript(), args);
    usleep(3 * 1000);

    // core config here - to be optional
    if (config_.inifile.values["raconfig"] == "true") {
        restoreRaConfig();
    }
}

//*******************************
// LaunchService::raMemcardIn
//*******************************
void LaunchService::raMemcardIn(PsGame &game) {
    if (!game.foreign) {
        memcards_.swapInForLaunch(game);

        // Copy the card moved to RA
        string inpath = game.ssFolder + sep + "memcards" + sep + "card1.mcd";
        string outpath = raSavesDir() + sep + raBaseNameFor(game) + ".srm";
        string backup = outpath + ".bak";
        if (!DirEntry::exists(backup)) {
            if (DirEntry::exists(outpath)) {
                DirEntry::copy(outpath, backup);
            }
        }
        if (DirEntry::exists(inpath)) {
            DirEntry::copy(inpath, outpath);
        }
    }
}

//*******************************
// LaunchService::raMemcardOut
//*******************************
void LaunchService::raMemcardOut(PsGame &game) {
    if (!game.foreign) {
        memcards_.swapOutAfterLaunch(game);

        string outpath = game.ssFolder + sep + "memcards" + sep + "card1.mcd";
        string inpath = raSavesDir() + sep + raBaseNameFor(game) + ".srm";
        string backup = inpath + ".bak";
        if (DirEntry::exists(inpath)) {
            DirEntry::copy(inpath, outpath);
        }

        if (DirEntry::exists(backup)) {
            DirEntry::removeFile(inpath);
            DirEntry::renameFile(backup, inpath);
        }
    }
}

//*******************************
// LaunchService::backupRaConfig
//*******************************
void LaunchService::backupRaConfig() {
    DirEntry::copy(raCoreOptionsFile(), raCoreOptionsFile() + ".bak");
    DirEntry::copy(raConfigFile(), raConfigFile() + ".bak");
}

//*******************************
// LaunchService::restoreRaConfig
//*******************************
void LaunchService::restoreRaConfig() {
    if (DirEntry::exists(raCoreOptionsFile() + ".bak")) {
        DirEntry::copy(raCoreOptionsFile() + ".bak", raCoreOptionsFile());
        DirEntry::removeFile(raCoreOptionsFile() + ".bak");
    }
    if (DirEntry::exists(raConfigFile() + ".bak")) {
        DirEntry::copy(raConfigFile() + ".bak", raConfigFile());
        DirEntry::removeFile(raConfigFile() + ".bak");
    }
}

//*******************************
// LaunchService::transferRaConfig
//*******************************
void LaunchService::transferRaConfig(PsGame &game) {
    const string coreOptions = raCoreOptionsFile();
    const string raConfig = raConfigFile();

    if (!game.foreign) {
        string path = game.folder;
        if (game.internal) {
            path = game.ssFolder;
        }
        ConfigFileEditor processor;

        int highres = atoi(processor.getValue(path, "gpu_neon.enhancement_enable").c_str());
        int speedhack = atoi(processor.getValue(path, "gpu_neon.enhancement_no_main").c_str());
        int clock = strtol(processor.getValue(path, "psx_clock").c_str(), nullptr, 16);
        int dither = atoi(processor.getValue(path, "gpu_peops.iUseDither").c_str());
        int interpolation = strtol(processor.getValue(path, "spu_config.iUseInterpolation").c_str(), nullptr, 16);

        int scanlines = atoi(processor.getValue(path, "scanlines").c_str());
        int scanline_level = strtol(processor.getValue(path, "scanline_level").c_str(), nullptr, 16);
        int frameskip = atoi(processor.getValue(path, "frameskip3").c_str());
        string slowBoot = processor.getValue(path, "SlowBoot");
        bool bootLogo = slowBoot.empty() || atoi(slowBoot.c_str()) != 0;

        // the core options
        if (highres != 0)
            processor.replaceInFile(coreOptions, "pcsx_rearmed_neon_enhancement_enable",
                                    "pcsx_rearmed_neon_enhancement_enable = \"enabled\" ");
        else
            processor.replaceInFile(coreOptions, "pcsx_rearmed_neon_enhancement_enable",
                                    "pcsx_rearmed_neon_enhancement_enable = \"disabled\" ");

        if (dither != 0)
            processor.replaceInFile(coreOptions, "pcsx_rearmed_dithering", "pcsx_rearmed_dithering = \"enabled\" ");
        else
            processor.replaceInFile(coreOptions, "pcsx_rearmed_dithering", "pcsx_rearmed_dithering = \"disabled\" ");

        if (speedhack != 0)
            processor.replaceInFile(coreOptions, "pcsx_rearmed_neon_enhancement_no_main",
                                    "pcsx_rearmed_neon_enhancement_no_main = \"enabled\" ");
        else
            processor.replaceInFile(coreOptions, "pcsx_rearmed_neon_enhancement_no_main",
                                    "pcsx_rearmed_neon_enhancement_no_main = \"disabled\" ");

        processor.replaceInFile(coreOptions, "pcsx_rearmed_psxclock",
                                "pcsx_rearmed_psxclock = \"" + to_string(clock) + "\" ");
        processor.replaceInFile(coreOptions, "pcsx_rearmed_show_bios_bootlogo",
                                string("pcsx_rearmed_show_bios_bootlogo = \"") + (bootLogo ? "enabled" : "disabled") +
                                    "\" ");
        processor.replaceInFile(coreOptions, "pcsx_rearmed_nocdaudio", "pcsx_rearmed_nocdaudio  = \"enabled\" ");

        if (interpolation == 0) {
            processor.replaceInFile(coreOptions, "pcsx_rearmed_spu_interpolation",
                                    "pcsx_rearmed_spu_interpolation = \"off\" ");
        }
        if (interpolation == 1) {
            processor.replaceInFile(coreOptions, "pcsx_rearmed_spu_interpolation",
                                    "pcsx_rearmed_spu_interpolation = \"simple\" ");
        }
        if (interpolation == 2) {
            processor.replaceInFile(coreOptions, "pcsx_rearmed_spu_interpolation",
                                    "pcsx_rearmed_spu_interpolation = \"gaussian\" ");
        }
        if (interpolation == 3) {
            processor.replaceInFile(coreOptions, "pcsx_rearmed_spu_interpolation",
                                    "pcsx_rearmed_spu_interpolation = \"cubic\" ");
        }

        processor.replaceInFile(coreOptions, "pcsx_rearmed_frameskip",
                                "pcsx_rearmed_frameskip  = \"" + to_string(frameskip) + "\" ");
        if (scanlines == 1) {
            float opacity = scanline_level / 100.0f;
            processor.replaceInFile(raConfig, "input_overlay", "input_overlay  = \":/overlay/scanlines.cfg\" ");
            processor.replaceInFile(raConfig, "input_overlay_enable", "input_overlay_enable  = \"true\" ");
            processor.replaceInFile(raConfig, "input_overlay_opacity",
                                    "input_overlay_opacity  = \"" + to_string(opacity) + "\" ");
        }
    }

    // retroarch.cfg
    ConfigFileEditor processor;
    string aspect = config_.inifile.values["aspect"]; // true - 1280x720 - false 960x720
    string filter = config_.inifile.values["mip"];    // true - billiner
    if (aspect == "true") {
        // widescreen
        processor.replaceInFile(raConfig, "custom_viewport_width", "custom_viewport_width  = \"1280\" ");
        processor.replaceInFile(raConfig, "custom_viewport_height", "custom_viewport_height  = \"720\" ");
        processor.replaceInFile(raConfig, "custom_viewport_x", "custom_viewport_x  = \"0\" ");
        processor.replaceInFile(raConfig, "custom_viewport_y", "custom_viewport_y  = \"0\" ");
        processor.replaceInFile(raConfig, "aspect_ratio_index", "aspect_ratio_index  = \"23\" ");
    } else {
        // 4:3
        processor.replaceInFile(raConfig, "custom_viewport_width", "custom_viewport_width  = \"960\" ");
        processor.replaceInFile(raConfig, "custom_viewport_height", "custom_viewport_height  = \"720\" ");
        processor.replaceInFile(raConfig, "custom_viewport_x", "custom_viewport_x  = \"160\" ");
        processor.replaceInFile(raConfig, "custom_viewport_y", "custom_viewport_y  = \"0\" ");
        processor.replaceInFile(raConfig, "aspect_ratio_index", "aspect_ratio_index  = \"0\" ");
    }

    if (filter != "true") {
        processor.replaceInFile(raConfig, "video_smooth", "video_smooth  = \"true\" ");
    } else {
        processor.replaceInFile(raConfig, "video_smooth", "video_smooth  = \"false\" ");
    }
}

//*******************************
// LaunchService::launchApp
//*******************************
void LaunchService::launchApp(PsGame &game) {
    PLOG_INFO << "calling LaunchService::launchApp()";
    PLOG_INFO << "Starting External App";

    library_.updateDatePlayed(game, time(nullptr));

    if (game.foreign) {
        PLOG_INFO << "FOREIGN MODE";
    }

    string link = game.base + sep + game.startup;

    runner_.run(link, {});
    usleep(3 * 1000);
}
