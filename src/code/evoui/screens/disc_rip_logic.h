//
// Copying the disc in the drive to Games as a CHD (AUTOBLEEM-21): the pure rules - which disc is offered the copy, the
// names of the files, the .m3u of a set, the LibCrypt .sbi to copy, the space check, and the words of the progress
// popup (percent, speed as Nx, time left). No SDL, no Gui - tested alone; evoui_disc_rip.cpp draws and runs them.
//
#pragma once

#include "disc_view_logic.h"

#include "core/services/content_installer.h"
#include "core/services/rip_job.h"
#include "core/services/sbi_service.h"

#include <ableem/engine/filesystem.h>

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <functional>
#include <string>
#include <vector>

namespace discrip {

// 1x of a CD drive: 75 sectors of 2048 bytes a second
const double CdSpeedBytesPerSecond = 153600.0;
// a set has no more discs than this on the .m3u
const int MaxDiscs = 16;

// Square offers the copy only on a PS1 data disc the databases know: its title names the folder and the file
inline bool offered(discview::Drive drive, bool discViewOpen, bool inDatabase) {
    return drive == discview::Drive::ReadyGame && discViewOpen && inDatabase;
}

//*******************************
// names
//*******************************
// "Tekken 3 (Europe) (Disc 2)" -> 2; a name with no "(Disc N)" is disc 1
inline int discNumberOf(const std::string &recordName) {
    const size_t at = recordName.rfind("(Disc ");
    if (at == std::string::npos)
        return 1;
    size_t end = at + 6;
    int number = 0;
    while (end < recordName.size() && recordName[end] >= '0' && recordName[end] <= '9' && number < 1000)
        number = number * 10 + (recordName[end++] - '0');
    if (end == at + 6 || end >= recordName.size() || recordName[end] != ')' || number < 1)
        return 1;
    return number;
}

// the record is a disc of a set (its name has a "(Disc N)", any N)
inline bool inSet(const std::string &recordName) {
    return recordName.find("(Disc ") != std::string::npos;
}

// Games/<title>
inline std::string folderOf(const std::string &gamesDir, const std::string &title) {
    return gamesDir + "/" + GameInstaller::folderNameFor(title);
}

// "Title.chd", disc 2 and on "Title (Disc 2).chd"
inline std::string chdName(const std::string &title, int disc) {
    const std::string base = GameInstaller::folderNameFor(title);
    return disc <= 1 ? base + ".chd" : base + " (Disc " + std::to_string(disc) + ").chd";
}

// Games/<title>/<chdName>
inline std::string chdPath(const std::string &gamesDir, const std::string &title, int disc) {
    return folderOf(gamesDir, title) + "/" + chdName(title, disc);
}

// Games/<title>/<title>.m3u - the set's playlist is named for the first disc, as the scanner names it
inline std::string m3uPath(const std::string &gamesDir, const std::string &title) {
    return folderOf(gamesDir, title) + "/" + GameInstaller::folderNameFor(title) + ".m3u";
}

// "Title.chd" -> "Title.sbi": the file the emulator loads next to an image
inline std::string sbiPathFor(const std::string &chdFile) {
    const size_t dot = chdFile.rfind('.');
    return (dot == std::string::npos ? chdFile : chdFile.substr(0, dot)) + ".sbi";
}

//*******************************
// the .m3u of a set
//*******************************
// The set's CHDs that are in the folder (`exists` is asked with a file name), one per line, in disc order. "" when none.
inline std::string m3uText(const std::string &title, const std::function<bool(const std::string &)> &exists) {
    std::string text;
    for (int disc = 1; disc <= MaxDiscs; disc++) {
        const std::string name = chdName(title, disc);
        if (exists(name))
            text += name + "\n";
    }
    return text;
}

// Writes (or updates) the set's .m3u from the CHDs in the folder. Only text that changed is written. True: written
// or already right; false: nothing to list or the write failed.
inline bool updateM3u(const std::string &gamesDir, const std::string &title) {
    const std::string folder = folderOf(gamesDir, title);
    const std::string text = m3uText(title, [&folder](const std::string &name) {
        return ableem::DirEntry::exists(folder + "/" + name);
    });
    if (text.empty())
        return false;
    return ableem::DirEntry::writeFileIfChanged(m3uPath(gamesDir, title), text) !=
           ableem::DirEntry::WriteResult::Failed;
}

//*******************************
// LibCrypt
//*******************************
// The .sbi of the disc in its save folders (the library game's, or disc-<KEY>/), the first that is there: what the
// launcher fetched for the serial (<CdromId>.sbi, the first of several versions otherwise). "" when there is none.
inline std::string chooseSbi(const std::vector<std::string> &saveFolders, const std::string &serial,
                             const std::function<bool(const std::string &)> &exists) {
    if (serial.empty())
        return "";
    const std::string id = SbiService::cdromId(serial);
    const std::vector<std::string> names = {id + ".sbi", id + ".1.sbi", serial + ".sbi"};
    for (const std::string &folder : saveFolders) {
        if (folder.empty())
            continue;
        for (const std::string &name : names) {
            const std::string path = folder + "/" + name;
            if (exists(path))
                return path;
        }
    }
    return "";
}

//*******************************
// before the rip
//*******************************
enum class Check {
    Ok,
    AlreadyThere, // the target file exists
    NoRoom        // the stick has not room for the disc
};

// The target first (nothing is read from the drive for a file that is there), then the room: `needed` bytes in
// `dir`'s filesystem (a free space that cannot be asked is room, RipJob::hasFreeSpace). freeBytes: what it asked.
inline Check preflight(const std::string &target, const std::string &dir, uint64_t needed, uint64_t *freeBytes) {
    if (ableem::DirEntry::exists(target))
        return Check::AlreadyThere;
    if (!RipJob::hasFreeSpace(dir, needed, freeBytes))
        return Check::NoRoom;
    return Check::Ok;
}

//*******************************
// the words of the popup
//*******************************
// "%1 of %2" style templates: %1..%9 replaced by the arguments, each once
inline std::string fill(std::string text, const std::vector<std::string> &args) {
    for (size_t i = 0; i < args.size() && i < 9; i++) {
        const std::string mark = "%" + std::to_string(i + 1);
        const size_t at = text.find(mark);
        if (at != std::string::npos)
            text.replace(at, mark.size(), args[i]);
    }
    return text;
}

// "650 MB", "1.2 GB" (binary units)
inline std::string bytesText(uint64_t bytes) {
    char buf[32];
    const double mb = static_cast<double>(bytes) / (1024.0 * 1024.0);
    if (mb < 1024.0)
        snprintf(buf, sizeof buf, "%.0f MB", mb);
    else
        snprintf(buf, sizeof buf, "%.1f GB", mb / 1024.0);
    return buf;
}

// 0..100
inline int percentOf(uint32_t done, uint32_t total) {
    if (total == 0)
        return 0;
    return static_cast<int>(std::min<uint64_t>(100, static_cast<uint64_t>(done) * 100 / total));
}

// the drive's speed as the multiple of a 1x CD drive: "12.3x"
inline std::string speedText(double bytesPerSecond) {
    char buf[24];
    snprintf(buf, sizeof buf, "%.1fx", bytesPerSecond > 0 ? bytesPerSecond / CdSpeedBytesPerSecond : 0.0);
    return buf;
}

// the seconds left at this speed; -1 while there is no speed to tell from
inline int secondsLeft(uint32_t done, uint32_t total, double bytesPerSecond) {
    if (bytesPerSecond <= 0 || total == 0)
        return -1;
    const uint32_t remaining = total > done ? total - done : 0;
    return static_cast<int>(static_cast<double>(remaining) * DiscReader::SectorSize / bytesPerSecond + 0.5);
}

// "3:07", "1:02:30"; "--:--" for unknown
inline std::string timeText(int seconds) {
    if (seconds < 0)
        return "--:--";
    char buf[24];
    if (seconds >= 3600)
        snprintf(buf, sizeof buf, "%d:%02d:%02d", seconds / 3600, seconds / 60 % 60, seconds % 60);
    else
        snprintf(buf, sizeof buf, "%d:%02d", seconds / 60, seconds % 60);
    return buf;
}

} // namespace discrip
