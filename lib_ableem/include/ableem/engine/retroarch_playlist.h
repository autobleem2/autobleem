// lib_ableem - engine: RetroArch .lpl playlist files. Two formats exist: the current JSON one
// ({"version":"1.0","items":[{path,label,core_path,core_name,crc32,db_name},...]}) and the legacy
// six-lines-per-entry text one. Reading handles both; writing produces JSON.
//
// A playlist RetroArch itself wrote (version 1.5 since 1.9) carries a header before the items -
// default_core_path, sort_mode, scan_content_dir and the like. The loader hands that back as a
// RetroArchPlaylistHeader so a rewrite keeps every field it does not understand.
#pragma once

#include <string>
#include <utility>
#include <vector>

namespace ableem {

//******************
// RetroArchPlaylistEntry
//******************
struct RetroArchPlaylistEntry {
    std::string path;      // the rom / disc image / .m3u
    std::string label;     // display name
    std::string core_path; // "DETECT" when RetroArch should pick the core
    std::string core_name; // "DETECT" likewise
    std::string crc32;     // "00000000|crc" when unknown
    std::string db_name;   // the playlist file name (RetroArch uses it to find the matching database)
};

using RetroArchPlaylistEntries = std::vector<RetroArchPlaylistEntry>;

// the top-level fields other than "items", in file order, each value as its JSON text ("\"1.5\"", "0",
// "true", ...) - kept opaque so nothing is lost between a load and a save
using RetroArchPlaylistHeader = std::vector<std::pair<std::string, std::string>>;

//******************
// RetroArchPlaylist
//******************
class RetroArchPlaylist {
public:
    static bool isJsonFormat(const std::string &path); // the first non-blank line starts with "{"

    // whichever format the file is in. false (and no entries) when the file cannot be opened or is not
    // valid; a JSON item that is not an object is skipped, a missing/non-string field reads as "".
    static bool load(const std::string &path, RetroArchPlaylistEntries &entries,
                     RetroArchPlaylistHeader *header = nullptr);
    static bool loadJson(const std::string &path, RetroArchPlaylistEntries &entries,
                         RetroArchPlaylistHeader *header = nullptr);
    static bool loadSixLine(const std::string &path, RetroArchPlaylistEntries &entries);

    // JSON, pretty-printed, keys in the order RetroArch writes them. The header goes first as loaded (a
    // "version" of "1.0" when it has none); a six-line file loads with an empty header and so is written
    // back as JSON 1.0, which RetroArch reads and upgrades on its own. False when the file cannot be written.
    static bool save(const std::string &path, const RetroArchPlaylistEntries &entries,
                     const RetroArchPlaylistHeader &header = RetroArchPlaylistHeader());
};

} // namespace ableem
