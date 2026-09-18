#include "ableem/engine/games_fingerprint.h"
#include "ableem/engine/filesystem.h"
#include "ableem/engine/game_types.h"

#include <fstream>
#include <iostream>

using namespace std;

namespace ableem {

namespace {

//*******************************
// walk
//*******************************
// path is the directory being visited, relPath is its path relative to the scan root ("" at the root, no
// trailing separator otherwise). allFiles records every file rather than the game images only.
void walk(const string &path, const string &relPath, map<string, string> &entries, bool allFiles) {
    for (const DirEntry &entry : DirEntry::diru(path)) {
        if (entry.name == SAVESTATES_DIR_NAME || entry.name == MEMCARDS_DIR_NAME)
            continue;
        if (allFiles && (entry.name.empty() || entry.name[0] == '.'))
            continue;

        string childRel = relPath.empty() ? entry.name : relPath + "/" + entry.name;
        string childPath = path + sep + entry.name;

        if (entry.isDir) {
            entries[childRel + "/"] = "";
            walk(childPath, childRel, entries, allFiles);
        } else if (allFiles || DirEntry::isAGameFile(entry.name) || DirEntry::matchExtension(entry.name, EXT_ECM)) {
            long long size = DirEntry::fileSize(childPath);
            entries[childRel] = to_string(size);
        }
    }
}

} // namespace

//*******************************
// GamesFingerprint::take
//*******************************
GamesFingerprint GamesFingerprint::take(const string &gamesDir) {
    GamesFingerprint fp;
    walk(DirEntry::removeSeparatorFromEndOfPath(gamesDir), "", fp.entries_, false);
    return fp;
}

//*******************************
// GamesFingerprint::takeAllFiles
//*******************************
GamesFingerprint GamesFingerprint::takeAllFiles(const string &dir) {
    GamesFingerprint fp;
    walk(DirEntry::removeSeparatorFromEndOfPath(dir), "", fp.entries_, true);
    return fp;
}

//*******************************
// GamesFingerprint::save
//*******************************
bool GamesFingerprint::save(const string &path) const {
    ofstream os;
    os.open(path, ios::binary);
    if (!DirEntry::checkWritable(os, path))
        return false;
    for (const auto &entry : entries_) {
        os << entry.first << "\t" << entry.second << "\n";
    }
    os.flush();
    os.close();
    return true;
}

//*******************************
// GamesFingerprint::load
//*******************************
bool GamesFingerprint::load(const string &path) {
    entries_.clear();
    ifstream is(path, ios::binary);
    if (!is.is_open())
        return false;

    string line;
    while (getline(is, line)) {
        size_t tab = line.find('\t');
        if (tab == string::npos)
            continue; // malformed line, skip it rather than fail the whole load
        entries_[line.substr(0, tab)] = line.substr(tab + 1);
    }
    return true;
}

} // namespace ableem
