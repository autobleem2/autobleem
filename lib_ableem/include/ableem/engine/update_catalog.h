// lib_ableem - engine: what the download repository says is current, and what the launcher remembers
// about updates - the JSON side of the launcher's online update (core/services/update_service.*), kept in
// the engine because the app includes no JSON library of its own.
//
//   releases/latest.json, releases/unstable.json   -> ReleaseCatalog   (tools/repo_index.py writes them)
//   rpi/retroarch/latest.json                      -> RetroArchCatalog
//   <usb>/System/update.json                       -> UpdateState     (last check, skipped/postponed)
//   <usb>/System/Updates/pending.json              -> PendingUpdate   (what the launcher downloaded and
//                                                     verified, for the Pi's autobleem-update script)
#pragma once

#include <cstdint>
#include <map>
#include <string>

namespace ableem {

struct UpdateFile {
    std::string name;
    std::string url;
    std::string sha256;
    uint64_t size = 0;
    bool valid() const { return !url.empty() && !sha256.empty(); }
};

//******************
// ReleaseCatalog
//******************
// One release as the site describes it: the version tag ("v2.0.0-pre0-df68521" for a pre-release, the
// tag plus the short hash), and its packages by platform key - "rpi", "rpi64", "psc", "win", "updateroms".
struct ReleaseCatalog {
    std::string version;
    bool prerelease = false;
    std::string date;
    std::map<std::string, UpdateFile> files;

    bool parse(const std::string &jsonText);
    bool load(const std::string &path);
    const UpdateFile *fileFor(const std::string &platformKey) const;
};

//******************
// RetroArchCatalog
//******************
// The newest RetroArch build for the Pi, by architecture ("armhf", "arm64").
struct RetroArchCatalog {
    std::string version;
    std::map<std::string, UpdateFile> files;

    bool parse(const std::string &jsonText);
    bool load(const std::string &path);
    const UpdateFile *fileFor(const std::string &arch) const;
};

//******************
// UpdateState
//******************
// What the launcher remembers between runs, so a check happens once a day and a "skip" or "remind me
// tomorrow" is honoured. Times are unix seconds (the Pi has NTP; 0 = never).
struct UpdateState {
    int64_t lastCheck = 0;
    int64_t postponedUntil = 0;
    std::string skippedVersion;   // an AutoBleem version the user said no to
    std::string skippedRetroArch; // a RetroArch version the user said no to

    bool load(const std::string &path);
    bool save(const std::string &path) const;
};

//******************
// PendingUpdate
//******************
// The artefacts downloaded and verified, waiting for the update script: file names relative to the
// Updates directory. Either may be empty.
struct PendingUpdate {
    std::string autobleemVersion;
    std::string autobleemFile;
    std::string retroarchVersion;
    std::string retroarchFile;

    bool load(const std::string &path);
    bool save(const std::string &path) const;
};

} // namespace ableem
