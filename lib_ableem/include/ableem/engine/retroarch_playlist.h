// lib_ableem - engine: RetroArch .lpl playlist files. Two formats exist: the current JSON one
// ({"version":"1.0","items":[{path,label,core_path,core_name,crc32,db_name},...]}) and the legacy
// six-lines-per-entry text one. Reading handles both; writing produces JSON.
#pragma once

#include <string>
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

//******************
// RetroArchPlaylist
//******************
class RetroArchPlaylist {
public:
    static bool isJsonFormat(const std::string &path); // first non-blank line is "{"

    // whichever format the file is in. false (and no entries) when the file cannot be opened or is not
    // valid; a JSON item that is not an object is skipped, a missing/non-string field reads as "".
    static bool load(const std::string &path, RetroArchPlaylistEntries &entries);
    static bool loadJson(const std::string &path, RetroArchPlaylistEntries &entries);
    static bool loadSixLine(const std::string &path, RetroArchPlaylistEntries &entries);

    // JSON, pretty-printed, keys in the order RetroArch writes them. false when the file cannot be written.
    static bool save(const std::string &path, const RetroArchPlaylistEntries &entries);
};

} // namespace ableem
