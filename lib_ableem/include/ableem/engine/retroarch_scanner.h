// lib_ableem - engine: the offline scan of RetroArch's ROM folders. <roms>/<system>/ holds the games of one
// system, named as RetroArch's databases name it ("Nintendo - Nintendo Entertainment System"); the scanner
// turns each folder into <playlists>/<system>.lpl the way RetroArch's own Import Content would, from the
// file names alone - no database, no network. Identification by .rdb and box art are later passes.
//
// One entry per game, not per file: a .cue hides the .bins it names, an .m3u hides its discs, and a .zip is
// one entry - "file.zip#entry" when the core cannot read archives itself and the archive holds one ROM,
// "file.zip" whole for a core that does (arcade sets). The label is the file's stem, tags kept
// ("Adventures of Lolo (USA)" is what the thumbnails are named after).
//
// An existing playlist is merged, never replaced: entries that point outside the system's ROM folder
// (the user's own additions) stay as they are; an entry under it whose file is still there is kept
// exactly (RetroArch's scanner may have identified it - its label and CRC are better than ours); a
// vanished file's entry goes; new files are added. The header RetroArch writes (version 1.5, sort mode,
// scan settings) is carried over. Written next to the playlist and renamed into place, only when the
// content actually changed.
#pragma once

#include "retroarch_cores.h"
#include "retroarch_playlist.h"

#include <string>
#include <vector>

namespace ableem {

class ScanProgressListener;

//******************
// RetroArchSystem
//******************
// one row of the system table: the folder under <roms> and what plays it
struct RetroArchSystem {
    std::string name;                // the folder name = the database name = the playlist's stem
    std::string coreName = "DETECT"; // what every new entry names as its core
    std::string corePath = "DETECT";
    std::vector<std::string> extensions; // the files the core accepts, without the dot, any case
    bool blockExtract = false;           // the core reads archives itself: a .zip is one whole entry
};

using RetroArchSystems = std::vector<RetroArchSystem>;

//******************
// RetroArchScanResult
//******************
struct RetroArchScanResult {
    int systemsScanned = 0;
    int gamesFound = 0;                        // entries under the ROM folders, over every system
    std::vector<std::string> playlistsWritten; // "<system>.lpl" for every playlist whose content changed
    std::vector<std::string> unknownFolders;   // <roms>/<x> with no system in the table for it
};

//******************
// RetroArchScanner
//******************
class RetroArchScanner {
public:
    struct Options {
        std::string romsDir;      // the ROM folders, as this machine sees them
        std::string playlistsDir; // where the .lpl files go (created when missing)
        // the ROM folders as the playlists should name them - the same place seen from the machine that
        // will read them ("/media/roms" for a console stick written on a PC). "" means romsDir.
        std::string targetRomsDir;
    };

    explicit RetroArchScanner(ScanProgressListener *listener = nullptr) : listener_(listener) {}

    // one system per database an installed core plays, with that core
    static RetroArchSystems systemsFrom(const CoreInfoTable &cores);

    // every <romsDir>/<system> folder that has a system in the table, reported as ScanStage::ScanningRoms
    // (detail: the system name, done/total: the folder's index and the folder count). AutoBleem's own
    // export, the Apps list and RetroArch's Favorites/History are never touched, whatever the folders are
    // named.
    RetroArchScanResult scan(const Options &options, const RetroArchSystems &systems);

    // the entries one folder yields - paths under targetFolder, sorted by path. Public for the tests.
    static RetroArchPlaylistEntries scanFolder(const std::string &folder, const std::string &targetFolder,
                                               const RetroArchSystem &system);

    // the merge of an existing playlist with a fresh scan of its folder - see the header comment.
    // sourceFolder is the folder on this machine, targetFolder what the playlist names it; an existing
    // entry under either is "ours".
    static RetroArchPlaylistEntries merge(const RetroArchPlaylistEntries &existing,
                                          const RetroArchPlaylistEntries &fresh, const std::string &sourceFolder,
                                          const std::string &targetFolder);

    // "archive.zip#entry" -> "archive.zip"; a plain path unchanged
    static std::string filePart(const std::string &path);
    // playlists the scanner must never write, by stem
    static bool isReservedPlaylist(const std::string &stem);

private:
    ScanProgressListener *listener_;
};

} // namespace ableem
