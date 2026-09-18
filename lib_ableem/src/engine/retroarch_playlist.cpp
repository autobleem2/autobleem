#include "ableem/engine/retroarch_playlist.h"
#include "ableem/engine/filesystem.h"
#include "ableem/engine/strings.h"

#include <fstream>
#include <iomanip>
#include <iostream>
#include <json.h>
#include <fifo_map.h>
#include "ableem/engine/log.h"

using namespace std;
using namespace nlohmann;

namespace ableem {

namespace {

// A workaround to use fifo_map as the json map so keys keep their insertion order; the 'less' compare is ignored
template <class K, class V, class dummy_compare, class A>
using fifo_map_workaround = fifo_map<K, V, fifo_map_compare<K>, A>;
using ordered_json = basic_json<fifo_map_workaround>;

// read a string field. a missing field or a field that is not a string returns the default.
string str(const json &item, const char *key, const string &def = "") {
    auto it = item.find(key);
    if (it == item.end() || !it->is_string())
        return def;
    return it->get<string>();
}

} // namespace

//*******************************
// RetroArchPlaylist::isJsonFormat
//*******************************
bool RetroArchPlaylist::isJsonFormat(const string &path) {
    ifstream in(path, ifstream::binary);
    string line;
    getline(in, line);
    trim(line);
    if (line.empty()) {
        return false;
    }
    return line == "{";
}

//*******************************
// RetroArchPlaylist::load
//*******************************
bool RetroArchPlaylist::load(const string &path, RetroArchPlaylistEntries &entries) {
    if (isJsonFormat(path))
        return loadJson(path, entries);
    return loadSixLine(path, entries);
}

//*******************************
// RetroArchPlaylist::loadJson
//*******************************
bool RetroArchPlaylist::loadJson(const string &path, RetroArchPlaylistEntries &entries) {
    entries.clear();
    ifstream in(path, ifstream::binary);
    if (!in.is_open()) {
        PLOG_WARNING << "Could not open playlist: " << path;
        return false;
    }

    // a truncated or hand edited playlist must not take the whole UI down (nlohmann throws on bad input)
    json j;
    try {
        in >> j;
    } catch (const json::exception &e) {
        PLOG_INFO << "Playlist " << path << " is not valid JSON: " << e.what();
        return false;
    }

    json array = j.value("items", json::array());
    if (!array.is_array()) {
        PLOG_INFO << "Playlist " << path << " has no items array";
        return false;
    }

    for (const auto &item : array) {
        if (!item.is_object())
            continue;
        RetroArchPlaylistEntry entry;
        entry.path = str(item, "path");
        entry.label = str(item, "label");
        entry.core_path = str(item, "core_path", "DETECT");
        entry.core_name = str(item, "core_name", "DETECT");
        entry.crc32 = str(item, "crc32");
        entry.db_name = str(item, "db_name");
        entries.push_back(entry);
    }
    return true;
}

//*******************************
// RetroArchPlaylist::loadSixLine
//*******************************
bool RetroArchPlaylist::loadSixLine(const string &path, RetroArchPlaylistEntries &entries) {
    entries.clear();
    ifstream in(path, ifstream::binary);
    if (!in.is_open()) {
        PLOG_WARNING << "Could not open playlist: " << path;
        return false;
    }

    // six lines per game. getline() is false at end of file or on a read error (eof() alone never becomes
    // true on a stream that failed to open, which would loop forever)
    RetroArchPlaylistEntry entry;
    while (getline(in, entry.path)) {
        if (!getline(in, entry.label))
            break;
        if (!getline(in, entry.core_path))
            break;
        if (!getline(in, entry.core_name))
            break;
        if (!getline(in, entry.crc32))
            break;
        if (!getline(in, entry.db_name))
            break;
        entries.push_back(entry);
    }
    return true;
}

//*******************************
// RetroArchPlaylist::save
//*******************************
bool RetroArchPlaylist::save(const string &path, const RetroArchPlaylistEntries &entries) {
    ordered_json j;
    j["version"] = "1.0";

    ordered_json items = ordered_json::array();
    for (const auto &entry : entries) {
        ordered_json item = ordered_json::object();
        item["path"] = entry.path;
        item["label"] = entry.label;
        item["core_path"] = entry.core_path;
        item["core_name"] = entry.core_name;
        item["crc32"] = entry.crc32;
        item["db_name"] = entry.db_name;
        items.push_back(item);
    }
    j["items"] = items;

    PLOG_INFO << j.dump();
    ofstream o(path);
    if (!DirEntry::checkWritable(o, path))
        return false;
    o << setw(2) << j << endl;
    o.flush();
    o.close();
    return true;
}

} // namespace ableem
