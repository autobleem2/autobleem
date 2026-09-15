// lib_ableem - engine: the named memory-card sets under <games>/!MemCards and swapping them in and out of a
// game's save-state folder (<save states>/<game>/memcards, with a "backup" sibling while a set is swapped in).
#pragma once

#include <string>
#include <vector>

namespace ableem {

//******************
// MemcardManager
//******************
class MemcardManager {
public:
    explicit MemcardManager(const std::string &gamesDir) : gamesDir(gamesDir) { }

    void create(const std::string &name);                           // a fresh set from the blank template cards
    void remove(const std::string &name);
    void rename(const std::string &oldName, const std::string &newName);   // also updates every Game.ini that used it
    std::vector<std::string> list();

    void backup(const std::string &gameSaveStatePath);              // memcards/ -> backup/ (once)
    void restore(const std::string &gameSaveStatePath);             // backup/ -> memcards/, then removes backup/
    void restoreAll(const std::string &saveStatesDir);              // restore() every game folder
    bool swapIn(const std::string &gameSaveStatePath, const std::string &name);    // false if the set does not exist
    void swapOut(const std::string &gameSaveStatePath, const std::string &name);   // copies the cards back into the set, then restore()
    void storeToRepo(const std::string &memcardsPath, const std::string &name);    // copies a game's cards into a (new) set

private:
    std::string gamesDir;
    std::string setPath(const std::string &name) const;
};

} // namespace ableem
