#include "ableem/engine/retroarch_scanner.h"
#include "ableem/engine/filesystem.h"
#include "ableem/engine/game_scanner.h"
#include "ableem/engine/log.h"
#include "ableem/engine/strings.h"
#include "ableem/engine/zip_archive.h"

#include <algorithm>
#include <cstdio>
#include <fstream>
#include <map>
#include <set>

using namespace std;

namespace ableem {

namespace {

const char *const UnknownCrc = "00000000|crc";

// the name after the last '/', its extension (lower-cased, no dot) and its stem
string baseName(const string &path) {
    size_t slash = path.find_last_of('/');
    return slash == string::npos ? path : path.substr(slash + 1);
}
string dirPart(const string &path) { // "" for a bare name
    size_t slash = path.find_last_of('/');
    return slash == string::npos ? "" : path.substr(0, slash);
}
string extensionOf(const string &path) {
    string name = baseName(path);
    size_t dot = name.find_last_of('.');
    return dot == string::npos ? "" : toLowerCopy(name.substr(dot + 1));
}
string stemOf(const string &path) {
    string name = baseName(path);
    size_t dot = name.find_last_of('.');
    return dot == string::npos ? name : name.substr(0, dot);
}
string join(const string &dir, const string &name) {
    return dir.empty() ? name : dir + "/" + name;
}
bool startsWith(const string &s, const string &prefix) {
    return s.compare(0, prefix.size(), prefix) == 0;
}

string crcText(uint32_t crc) {
    char buf[16];
    snprintf(buf, sizeof(buf), "%08X|crc", static_cast<unsigned>(crc));
    return buf;
}

// every file under `dir`, as forward-slash paths relative to it, in name order
void walk(const string &dir, const string &rel, vector<string> &files) {
    DirEntries entries = DirEntry::diru(dir);
    sort(entries.begin(), entries.end(), DirEntry::sortDirEntryByName);
    for (const DirEntry &entry : entries) {
        if (entry.name.empty() || entry.name[0] == '.')
            continue;
        string childRel = join(rel, entry.name);
        if (entry.isDir)
            walk(dir + sep + entry.name, childRel, files);
        else
            files.push_back(childRel);
    }
}

// the discs an .m3u lists, relative to its folder
vector<string> m3uToDiscList(const string &m3uFile) {
    vector<string> discs;
    ifstream in(m3uFile);
    string line;
    while (getline(in, line)) {
        trim(line);
        if (line.empty() || line[0] == '#')
            continue;
        replace(line.begin(), line.end(), '\\', '/');
        discs.push_back(line);
    }
    return discs;
}

bool labelLess(const RetroArchPlaylistEntry &a, const RetroArchPlaylistEntry &b) {
    return lessCaseInsensitive(a.label, b.label);
}

bool sameEntry(const RetroArchPlaylistEntry &a, const RetroArchPlaylistEntry &b) {
    return a.path == b.path && a.label == b.label && a.core_path == b.core_path && a.core_name == b.core_name &&
           a.crc32 == b.crc32 && a.db_name == b.db_name;
}

} // namespace

//*******************************
// RetroArchSystem::readsArchives
//*******************************
bool RetroArchSystem::readsArchives() const {
    if (blockExtract)
        return true;
    for (const string &ext : extensions) {
        if (toLowerCopy(ext) == "zip")
            return true;
    }
    return false;
}

//*******************************
// RetroArchScanner::loadFolderAliases
//*******************************
map<string, string> RetroArchScanner::loadFolderAliases(const string &cfgPath) {
    map<string, string> aliases;
    ifstream in(cfgPath);
    string line;
    while (getline(in, line)) {
        trim(line);
        if (line.empty() || line[0] == '#' || line.find('=') == string::npos)
            continue;
        string folder = line.substr(0, line.find('='));
        string db = line.substr(line.find('=') + 1);
        trim(folder);
        trim(db);
        if (!folder.empty() && !db.empty())
            aliases[folder] = db;
    }
    return aliases;
}

//*******************************
// RetroArchScanner::systemsFrom
//*******************************
RetroArchSystems RetroArchScanner::systemsFrom(const CoreInfoTable &cores) {
    RetroArchSystems systems;
    for (const string &db : cores.databases()) {
        CoreInfoPtr core = cores.coreForDatabase(db);
        if (!core)
            continue;
        RetroArchSystem system;
        system.name = db;
        system.coreName = core->name;
        system.corePath = core->core_path;
        system.extensions = core->extensions;
        system.blockExtract = core->block_extract;
        systems.push_back(system);
    }
    return systems;
}

//*******************************
// RetroArchScanner::filePart
//*******************************
string RetroArchScanner::filePart(const string &path) {
    size_t hash = path.find('#');
    return hash == string::npos ? path : path.substr(0, hash);
}

//*******************************
// RetroArchScanner::isReservedPlaylist
//*******************************
bool RetroArchScanner::isReservedPlaylist(const string &stem) {
    // AutoBleem's PS1 export, the Apps list, and the two RetroArch keeps itself
    return stem == "AutoBleem" || stem == "Applications" || startsWith(stem, "content_");
}

//*******************************
// RetroArchScanner::scanFolder
//*******************************
RetroArchPlaylistEntries RetroArchScanner::scanFolder(const string &folder, const string &targetFolder,
                                                      const RetroArchSystem &system) {
    RetroArchPlaylistEntries entries;

    set<string> accepted;
    for (const string &ext : system.extensions)
        accepted.insert(toLowerCopy(ext));
    // an archive is always a candidate: RetroArch extracts it for a core that cannot read it itself, and
    // the frontend opens a .7z the same way (which this scanner cannot look into, so it goes in whole)
    auto isArchive = [](const string &rel) {
        const string ext = extensionOf(rel);
        return ext == "zip" || ext == "7z";
    };
    auto accepts = [&](const string &rel) { return accepted.count(extensionOf(rel)) > 0 || isArchive(rel); };

    vector<string> files;
    walk(DirEntry::removeSeparatorFromEndOfPath(folder), "", files);

    // what the containers hide: the .bins a .cue names, the discs an .m3u lists, a .ccd's image. Compared
    // lower-cased - a cue written on a case-insensitive filesystem need not match the disk's spelling.
    set<string> hidden;
    for (const string &rel : files) {
        if (!accepts(rel))
            continue;
        const string ext = extensionOf(rel);
        const string dir = dirPart(rel);
        if (ext == "cue") {
            for (const string &bin : DirEntry::cueToBinList(folder + sep + rel))
                hidden.insert(toLowerCopy(join(dir, bin)));
        } else if (ext == "m3u") {
            for (const string &disc : m3uToDiscList(folder + sep + rel))
                hidden.insert(toLowerCopy(join(dir, disc)));
        } else if (ext == "ccd") {
            string stem = join(dir, stemOf(rel));
            hidden.insert(toLowerCopy(stem + ".img"));
            hidden.insert(toLowerCopy(stem + ".sub"));
        }
    }

    const string dbName = system.name + ".lpl";
    auto add = [&](const string &path, const string &label, const string &crc) {
        RetroArchPlaylistEntry entry;
        entry.path = path;
        entry.label = label;
        entry.core_path = system.corePath.empty() ? "DETECT" : system.corePath;
        entry.core_name = system.coreName.empty() ? "DETECT" : system.coreName;
        entry.crc32 = crc;
        entry.db_name = dbName;
        entries.push_back(entry);
    };

    for (const string &rel : files) {
        if (!accepts(rel) || hidden.count(toLowerCopy(rel)))
            continue;
        const string target = targetFolder + "/" + rel;
        if (extensionOf(rel) == "zip" && !system.readsArchives()) {
            // the core cannot read archives: RetroArch extracts the ROM inside and names the entry
            // "archive#rom" - the archive's CRC table gives the ROM's CRC for free
            vector<ZipEntry> inside;
            if (!ZipArchive::listEntries(folder + sep + rel, inside)) {
                PLOG_INFO << "Not a readable archive, skipped: " << rel;
                continue;
            }
            vector<ZipEntry> roms;
            for (const ZipEntry &e : inside) {
                if (!e.isDir && extensionOf(e.name) != "zip" && accepted.count(extensionOf(e.name)))
                    roms.push_back(e);
            }
            if (roms.empty()) {
                PLOG_INFO << "No " << system.name << " ROM inside, skipped: " << rel;
                continue;
            }
            // one ROM is the archive's game and takes the archive's name (what the thumbnails are named
            // after); a pack of several is several games, each named after its own file
            for (const ZipEntry &rom : roms)
                add(target + "#" + rom.name, roms.size() == 1 ? stemOf(rel) : stemOf(rom.name), crcText(rom.crc));
        } else {
            add(target, stemOf(rel), UnknownCrc);
        }
    }
    return entries;
}

//*******************************
// RetroArchScanner::merge
//*******************************
RetroArchPlaylistEntries RetroArchScanner::merge(const RetroArchPlaylistEntries &existing,
                                                 const RetroArchPlaylistEntries &fresh, const string &sourceFolder,
                                                 const string &targetFolder) {
    const string sourcePrefix = sourceFolder + "/";
    const string targetPrefix = targetFolder + "/";

    // an entry's file as this machine finds it, "" when the entry is not under our folder at all
    auto sourceFileOf = [&](const string &path) -> string {
        string file = filePart(path);
        if (startsWith(file, targetPrefix))
            return sourcePrefix + file.substr(targetPrefix.size());
        if (startsWith(file, sourcePrefix))
            return file;
        return "";
    };

    RetroArchPlaylistEntries merged;
    set<string> taken; // files (source paths) an entry already covers
    for (const RetroArchPlaylistEntry &entry : existing) {
        string file = sourceFileOf(entry.path);
        if (file.empty()) {
            merged.push_back(entry); // not ours: the user's own addition, kept as is
            continue;
        }
        if (!DirEntry::exists(file))
            continue; // vanished
        merged.push_back(entry);
        taken.insert(file);
    }
    for (const RetroArchPlaylistEntry &entry : fresh) {
        string file = sourceFileOf(entry.path);
        if (!file.empty() && taken.count(file))
            continue; // already there, under whatever name RetroArch or we gave it
        merged.push_back(entry);
    }
    stable_sort(merged.begin(), merged.end(), labelLess);
    return merged;
}

//*******************************
// RetroArchScanner::scan
//*******************************
RetroArchScanResult RetroArchScanner::scan(const Options &options, const RetroArchSystems &systems) {
    RetroArchScanResult result;
    const string romsDir = DirEntry::removeSeparatorFromEndOfPath(options.romsDir);
    const string targetRomsDir =
        DirEntry::removeSeparatorFromEndOfPath(options.targetRomsDir.empty() ? options.romsDir : options.targetRomsDir);
    const string playlistsDir = DirEntry::removeSeparatorFromEndOfPath(options.playlistsDir);

    if (!DirEntry::isDirectory(romsDir)) {
        PLOG_INFO << "No ROM folders at " << romsDir;
        return result;
    }
    map<string, const RetroArchSystem *> byName;
    for (const RetroArchSystem &system : systems)
        byName[system.name] = &system;

    DirEntries folders = DirEntry::diru_DirsOnly(romsDir);
    sort(folders.begin(), folders.end(), DirEntry::sortDirEntryByName);
    vector<pair<string, string>> known; // folder name, database name
    for (const DirEntry &folder : folders) {
        if (folder.name.empty() || folder.name[0] == '.')
            continue;
        auto alias = options.folderAliases.find(folder.name);
        const string dbName = alias == options.folderAliases.end() ? folder.name : alias->second;
        if (isReservedPlaylist(dbName) || byName.find(dbName) == byName.end()) {
            PLOG_INFO << "No core plays a system called '" << dbName << "' - folder " << folder.name << " skipped";
            result.unknownFolders.push_back(folder.name);
            continue;
        }
        known.emplace_back(folder.name, dbName);
    }
    if (known.empty())
        return result;

    if (!DirEntry::isDirectory(playlistsDir) && !DirEntry::createDir(playlistsDir)) {
        PLOG_WARNING << "Cannot create " << playlistsDir;
        return result;
    }

    int index = 0;
    for (const auto &folderAndDb : known) {
        const string &name = folderAndDb.first;
        const RetroArchSystem &system = *byName[folderAndDb.second];
        index++;
        if (listener_)
            listener_->onScanProgress(ScanStage::ScanningRoms, name, index, static_cast<int>(known.size()));

        const string sourceFolder = romsDir + sep + name;
        const string targetFolder = targetRomsDir + "/" + name;
        RetroArchPlaylistEntries fresh = scanFolder(sourceFolder, targetFolder, system);

        // two folders may feed one playlist (Arcade and SNK - Neo Geo both into FBNeo's): each pass sees
        // the other's entries as foreign and leaves them, so the file is read fresh every time
        const string playlistPath = playlistsDir + sep + system.name + ".lpl";
        RetroArchPlaylistEntries existing;
        RetroArchPlaylistHeader header;
        const bool hadPlaylist = DirEntry::exists(playlistPath);
        if (hadPlaylist && !RetroArchPlaylist::load(playlistPath, existing, &header)) {
            // unreadable: not ours to guess at - leave it alone rather than replace what the user had
            PLOG_WARNING << "Playlist " << playlistPath << " could not be read, not rewritten";
            continue;
        }
        RetroArchPlaylistEntries merged = merge(existing, fresh, sourceFolder, targetFolder);

        int ours = 0;
        for (const RetroArchPlaylistEntry &entry : merged) {
            const string file = filePart(entry.path);
            if (startsWith(file, targetFolder + "/") || startsWith(file, sourceFolder + "/"))
                ours++;
        }
        result.systemsScanned++;
        result.gamesFound += ours;

        bool unchanged = hadPlaylist && merged.size() == existing.size() &&
                         equal(merged.begin(), merged.end(), existing.begin(), sameEntry);
        if (unchanged || (!hadPlaylist && merged.empty()))
            continue;

        // write beside it and swap: RetroArch may be reading the playlist this very moment
        const string tempPath = playlistPath + ".tmp";
        if (!RetroArchPlaylist::save(tempPath, merged, header) || !DirEntry::replaceFile(tempPath, playlistPath)) {
            PLOG_WARNING << "Could not write " << playlistPath;
            DirEntry::removeFile(tempPath);
            continue;
        }
        PLOG_INFO << "Playlist " << system.name << ".lpl: " << merged.size() << " entries (" << ours << " from " << name
                  << ")";
        const string written = system.name + ".lpl";
        if (find(result.playlistsWritten.begin(), result.playlistsWritten.end(), written) ==
            result.playlistsWritten.end())
            result.playlistsWritten.push_back(written);
    }
    return result;
}

} // namespace ableem
