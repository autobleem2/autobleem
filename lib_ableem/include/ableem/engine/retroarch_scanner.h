// lib_ableem - engine: the offline scan of RetroArch's ROM folders. <roms>/<system>/ holds the games of one
// system, named as RetroArch's databases name it ("Nintendo - Nintendo Entertainment System"); the scanner
// turns each folder into <playlists>/<system>.lpl the way RetroArch's own Import Content would, from the
// file names alone - no database, no network. Identification by .rdb and box art are later passes.
//
// One entry per game, not per file: a .cue hides the .bins it names, an .m3u hides its discs, and a .zip is
// one entry - "file.zip#entry" when the core cannot read archives itself and the archive holds one ROM,
// "file.zip" whole for a core that does (arcade sets: the core lists "zip" among its extensions - no .info
// in the current bundle says block_extract, that flag is only honoured as well). The label is the file's
// stem, tags kept ("Adventures of Lolo (USA)" is what the thumbnails are named after).
//
// A folder is normally named as the database (= the playlist) is; Options::folderAliases maps the ones
// that are not ("Arcade" -> "FBNeo - Arcade Games"). Two folders may feed one playlist that way.
//
// With the system's .rdb at hand (Options::rdbDir, libretro-database's files) every entry is identified
// before the merge: a zipped ROM by the CRC the archive records, a loose file by its CRC (read once, up to
// Options::maxCrcBytes - CD images are never hashed), an arcade set by its archive name against the
// database's rom_name. A hit gives the entry the database's name - exactly what libretro-thumbnails names
// the box art - and an identified name replaces an unidentified one an earlier scan left in the playlist.
// No database, or no hit, keeps the file's stem. Nothing is dropped for not being in the database.
//
// A rescan is cheap where nothing changed. With Options::stateFile the scanner remembers, per folder, a
// digest of the folder's file list and sizes, the playlist's size and the database's size (no mtimes -
// the PSC's clock cannot be trusted): a folder whose digest is what it was last time is not listed,
// opened, hashed, looked up or merged - its playlist is only read for the counts. And a loose file the
// playlist already has an entry for (with a CRC - ours from an earlier scan, or RetroArch's) is never
// hashed again: the entry's CRC is taken as the file's, and the database asked about that.
//
// An existing playlist is merged, never replaced: entries that point outside the system's ROM folder
// (the user's own additions) stay as they are; an entry under it whose file is still there is kept
// exactly (RetroArch's scanner may have identified it - its label and CRC are better than ours); a
// vanished file's entry goes; new files are added. The header RetroArch writes (version 1.5, sort mode,
// scan settings) is carried over. Written next to the playlist and renamed into place, only when the
// content actually changed.
#pragma once

#include "rdb_reader.h"
#include "retroarch_cores.h"
#include "retroarch_playlist.h"

#include <cstdint>

#include <map>
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
    bool blockExtract = false;           // the core reads archives itself even without "zip" above
    // a .zip is handed over whole (never opened) when the core lists the extension or says block_extract
    bool readsArchives() const;
};

using RetroArchSystems = std::vector<RetroArchSystem>;

//******************
// ScannedRom
//******************
// one entry a folder scan yields, with what the identification pass needs to know about it
struct ScannedRom {
    RetroArchPlaylistEntry entry;
    std::string sourcePath;    // the file on this machine (the archive, for an archive member)
    uint32_t crc = 0;          // the ROM's CRC when known (an archive member's, from the central directory)
    bool wholeArchive = false; // an archive handed to the core as it is (an arcade set)
    bool identified = false;   // the label is the database's name
};

using ScannedRoms = std::vector<ScannedRom>;

//******************
// RetroArchScanResult
//******************
struct RetroArchScanResult {
    struct Game {
        std::string database; // the playlist's stem = the thumbnails folder
        std::string label;
    };
    int systemsScanned = 0;
    int gamesFound = 0;                        // entries under the ROM folders, over every system
    int gamesIdentified = 0;                   // of those, the ones a database named
    std::vector<Game> games;                   // the entries under the ROM folders, as the playlists have them
    std::vector<std::string> playlistsWritten; // "<system>.lpl" for every playlist whose content changed
    int systemsSkipped = 0;                    // folders left alone because nothing about them changed (stateFile)
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
        // folder name -> database name, for the folders not named as their database is
        std::map<std::string, std::string> folderAliases;
        // where <database>.rdb files are; "" = no identification
        std::string rdbDir;
        // a loose file bigger than this is not hashed (a CD image: its entry keeps the file's name)
        uint64_t maxCrcBytes = 64 * 1024 * 1024;
        // where the per-folder digests of the last scan are kept ("" = every folder is scanned every time)
        std::string stateFile;
    };

    explicit RetroArchScanner(ScanProgressListener *listener = nullptr) : listener_(listener) {}

    // one system per database an installed core plays, with that core
    static RetroArchSystems systemsFrom(const CoreInfoTable &cores);

    // every <romsDir>/<folder> whose database (the folder's name, or its alias) has a system in the table,
    // reported as ScanStage::ScanningRoms (detail: the folder name, done/total: the folder's index and the
    // folder count). AutoBleem's own export, the Apps list and RetroArch's Favorites/History are never
    // touched, whatever the folders are named.
    RetroArchScanResult scan(const Options &options, const RetroArchSystems &systems);

    // "folder=database" lines ('#' comments) into an alias map; a missing file gives an empty map
    static std::map<std::string, std::string> loadFolderAliases(const std::string &cfgPath);

    // the entries one folder yields - paths under targetFolder, sorted by path. Public for the tests.
    static ScannedRoms scanFolder(const std::string &folder, const std::string &targetFolder,
                                  const RetroArchSystem &system);

    // names every entry the database knows - see the header comment; returns how many it named
    static int identify(ScannedRoms &roms, const RdbReader &rdb, uint64_t maxCrcBytes);

    // a loose ROM the existing playlist has an entry for, with a CRC, takes that CRC - so identify()
    // does not hash it again. sourceFolder/targetFolder as for merge(). Returns how many were seeded.
    static int seedCrcsFromPlaylist(ScannedRoms &roms, const RetroArchPlaylistEntries &existing,
                                    const std::string &sourceFolder, const std::string &targetFolder);

    // the merge of an existing playlist with a fresh scan of its folder - see the header comment.
    // sourceFolder is the folder on this machine, targetFolder what the playlist names it; an existing
    // entry under either is "ours".
    static RetroArchPlaylistEntries merge(const RetroArchPlaylistEntries &existing, const ScannedRoms &fresh,
                                          const std::string &sourceFolder, const std::string &targetFolder);

    // "archive.zip#entry" -> "archive.zip"; a plain path unchanged
    static std::string filePart(const std::string &path);
    // playlists the scanner must never write, by stem
    static bool isReservedPlaylist(const std::string &stem);

private:
    ScanProgressListener *listener_;
};

} // namespace ableem
