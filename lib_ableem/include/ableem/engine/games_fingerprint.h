// lib_ableem - engine: a cheap snapshot of the games directory, used to tell whether it changed since the
// last scan without doing a full scan. Deliberately size-only, no mtime - the PSC has no battery-backed
// clock, so a stored modification time cannot be trusted to stay put across a reboot (see
// DirEntry::fileSize()'s comment). A same-size in-place edit of a game image is the one change this cannot
// see; games are added/removed/replaced as whole files, so that trade-off is accepted.
#pragma once

#include <map>
#include <string>

namespace ableem {

//******************
// GamesFingerprint
//******************
class GamesFingerprint {
public:
    // walks gamesDir recursively (skipping !SaveStates, !MemCards and dot entries) and records
    // "relpath|size" for every game image file (DirEntry::isAGameFile()) plus .ecm, and "relpath/" for
    // every directory - so a renamed/moved directory is seen as a change even when its contents are not.
    // Files the app itself writes (Game.ini, pcsx.cfg, .png, .m3u) are not recorded: editing one must
    // not trigger a rescan.
    static GamesFingerprint take(const std::string &gamesDir);

    // the same over a tree of anything - RetroArch's ROM folders, where every file is a game (or hides
    // one): every file is recorded, only dot entries are skipped. An empty fingerprint for a missing dir.
    static GamesFingerprint takeAllFiles(const std::string &dir);

    bool load(const std::string &path); // false (fingerprint left empty) if the file does not exist/parse
    bool save(const std::string &path) const;
    std::string text() const; // what save() writes, as one string - for digesting a folder's state
    bool empty() const { return entries_.empty(); }

    bool operator==(const GamesFingerprint &other) const { return entries_ == other.entries_; }
    bool operator!=(const GamesFingerprint &other) const { return !(*this == other); }

private:
    // relpath -> "" for a directory, "size" for a file. A map (not unordered) so save()/load() and equality
    // do not depend on directory walk order.
    std::map<std::string, std::string> entries_;
};

} // namespace ableem
