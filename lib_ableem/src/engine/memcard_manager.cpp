#include "ableem/engine/memcard_manager.h"
#include "ableem/engine/environment.h"
#include "ableem/engine/filesystem.h"
#include "ableem/engine/game_types.h"
#include "ableem/engine/ini_file.h"

using namespace std;

namespace ableem {

//*******************************
// MemcardManager::setPath
//*******************************
string MemcardManager::setPath(const string &name) const {
    return gamesDir + sep + MEMCARDS_DIR_NAME + sep + name;
}

//*******************************
// MemcardManager::create
//*******************************
void MemcardManager::create(const string &name) {
    string curPath = setPath(name);
    string templates = Environment::getPathToMemcardTemplateDir();
    if (!DirEntry::exists(curPath)) {
        DirEntry::createDir(curPath);
        DirEntry::copy(templates + sep + "card1.mcd", curPath + sep + "card1.mcd");
        DirEntry::copy(templates + sep + "card2.mcd", curPath + sep + "card2.mcd");
        DirEntry::copy(templates + sep + "card1.mcd", curPath + sep + "card1.bak");
        DirEntry::copy(templates + sep + "card2.mcd", curPath + sep + "card2.bak");
    }
}

//*******************************
// MemcardManager::remove
//*******************************
void MemcardManager::remove(const string &name) {
    string curPath = setPath(name);
    if (DirEntry::exists(curPath)) {
        DirEntry::rmDir(curPath);
    }
}

//*******************************
// MemcardManager::swapIn
//*******************************
bool MemcardManager::swapIn(const string &path, const string &name) {
    backup(path);
    string customPath = setPath(name);
    if (!DirEntry::exists(customPath)) {
        restore(path);
        return false;
    } else {
        DirEntry::rmDir(path + sep + "memcards");
        DirEntry::createDir(path + sep + "memcards");
        for (const DirEntry &entry : DirEntry::diru(customPath)) {
            DirEntry::copy(customPath + sep + entry.name, path + sep + "memcards" + sep + entry.name);
        }
        return true;
    }
}

//*******************************
// MemcardManager::storeToRepo
//*******************************
void MemcardManager::storeToRepo(const string &path, const string &name) {
    string customPath = setPath(name);
    if (!DirEntry::exists(customPath)) {
        DirEntry::createDir(customPath);
    }

    // copy memcard from game to repository
    for (const DirEntry &entry : DirEntry::diru(path)) {
        string input = path + sep + entry.name;
        string output = customPath + sep + entry.name;
        DirEntry::copy(input, output);
    }
}

//*******************************
// MemcardManager::rename
//*******************************
void MemcardManager::rename(const string &oldName, const string &newName) {
    string oldPath = setPath(oldName);
    string newPath = setPath(newName);

    if (DirEntry::exists(newPath)) {
        // we already have memcard with this name
        return;
    }

    DirEntry::renameFile(oldPath, newPath);

    // now go to all game ini's and find out if needs updated
    for (const DirEntry &entry : DirEntry::diru(gamesDir)) {
        if (!DirEntry::isDirectory(gamesDir + sep + entry.name))
            continue;
        if (entry.name == SAVESTATES_DIR_NAME)
            continue;
        if (entry.name == MEMCARDS_DIR_NAME)
            continue;

        string gameIniPath = gamesDir + sep + entry.name + sep + GAME_INI;
        if (DirEntry::exists(gameIniPath)) {
            IniFile inifile;
            inifile.load(gameIniPath);

            if (inifile.values["memcard"] == oldName) {
                inifile.values["memcard"] = newName;
                inifile.save(gameIniPath);
            }
        }
    }
}

//*******************************
// MemcardManager::list
//*******************************
vector<string> MemcardManager::list() {
    vector<string> memcards;
    string customPath = gamesDir + sep + MEMCARDS_DIR_NAME;
    for (const DirEntry &entry : DirEntry::diru(customPath)) {
        if (DirEntry::isDirectory(customPath + sep + entry.name)) {
            memcards.push_back(entry.name);
        }
    }
    return memcards;
}

//*******************************
// MemcardManager::swapOut
//*******************************
void MemcardManager::swapOut(const string &path, const string &name) {
    string customPath = setPath(name);
    if (!DirEntry::exists(customPath)) {
        restore(path);
    } else {
        for (const DirEntry &entry : DirEntry::diru(customPath)) {
            DirEntry::copy(path + sep + "memcards" + sep + entry.name, customPath + sep + entry.name);
        }
        restore(path);
    }
}

//*******************************
// MemcardManager::restoreAll
//*******************************
void MemcardManager::restoreAll(const string &mainDir) {
    for (const DirEntry &entry : DirEntry::diru(mainDir)) {
        string path = mainDir + sep + entry.name;
        restore(path);
    }
}

//*******************************
// MemcardManager::backup
//*******************************
void MemcardManager::backup(const string &path) {
    string curPath = path + sep + "backup";
    if (!DirEntry::exists(curPath)) {
        DirEntry::createDir(curPath);
    } else {
        return;
    }

    string original = path + sep + "memcards";
    for (const DirEntry &entry : DirEntry::diru(original)) {
        DirEntry::copy(original + sep + entry.name, curPath + sep + entry.name);
    }
}

//*******************************
// MemcardManager::restore
//*******************************
void MemcardManager::restore(const string &path) {
    string curPath = path + sep + "backup";
    if (!DirEntry::exists(curPath)) {
        return;
    }

    string original = path + sep + "memcards";
    DirEntry::rmDir(original);
    DirEntry::createDir(original);

    for (const DirEntry &entry : DirEntry::diru(curPath)) {
        DirEntry::copy(curPath + sep + entry.name, original + sep + entry.name);
    }
    DirEntry::rmDir(curPath);
}

} // namespace ableem
