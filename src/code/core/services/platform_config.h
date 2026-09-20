//
// PlatformConfig: what differs between the targets (console / Raspberry Pi / dev host) about where things
// are, read from resources/platform/<platform>.ini instead of being compiled in.
//
#pragma once

#include <string>
#include <vector>

//******************
// PlatformConfig
//******************
// The keys of resources/platform/{psc,rpi,pc}.ini (see psc.ini for the meaning of each). load() reads the
// file the build's platform name selects, and apply() hands the result to Env - so that the RetroArch
// directory, the PS1 core a playlist names and where the RetroArch binary may be are data, and main.cpp and
// the services carry no #ifdef for them. A missing file or key falls back to the console's layout, which is
// also what a fake USB tree on a dev host looks like.
struct PlatformConfig {
    std::string retroarchDir = "RetroArch/bin";                   // relative to the USB root unless absolute
    std::string retroarchCore = "cores/pcsx_rearmed_libretro.so"; // relative to retroarchDir unless absolute
    std::vector<std::string> retroarchBinaries{"retroarch"};      // each relative to retroarchDir unless absolute
    std::string retroarchRomsDir = "RetroArch/roms"; // the other systems' ROM folders; relative to the USB root
    std::string retroarchBiosDir = "RetroArch/bios"; // RetroArch's system dir, the cores' BIOS files; likewise
    std::string downloadCommand; // fetches %u to %o, with its own timeout; "" = this platform cannot (the console)
    // what this platform calls its USB root - the prefix its playlists carry ("/media" on the console,
    // "/media/autobleem" on a Pi). Not applied to Env: it is for a tool writing the target's files from
    // another machine (UpdateRoms), where Env's root is that machine's.
    std::string usbRoot;

    // the file for this build's platform, next to the other resources: <resourcesDir>/platform/<name>.ini
    static std::string pathFor(const std::string &resourcesDir, const std::string &platformName);

    // reads the ini; every key it does not have keeps its default above. Never fails: a missing file
    // simply means the defaults.
    static PlatformConfig load(const std::string &iniPath);

    // resolves the relative paths against the USB root and tells Env
    void apply() const;

    // "a;b;c" -> {"a", "b", "c"}, trimmed, empties dropped
    static std::vector<std::string> splitList(const std::string &value);
};
