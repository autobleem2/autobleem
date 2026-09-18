//
// LightgunService: which games are light-gun games, and where that is remembered.
//
#include "lightgun.h"
#include "environment.h"
#include "../main.h"

#include <fstream>
#include <iostream>

using namespace std;

//*******************************
// LightgunService::LightgunService
//*******************************
LightgunService::LightgunService(ableem::GameLibrary &library) : library_(library) {
    reload();
}

//*******************************
// LightgunService::lightgunsFile
//*******************************
string LightgunService::lightgunsFile() {
    return Env::getPathToSystemDir() + sep + "lightguns.txt";
}

//*******************************
// LightgunService::isLightgun
//*******************************
bool LightgunService::isLightgun(const PsGame &game) const {
    if (game.app) return false;
    if (game.foreign) return raPaths_.count(game.image_path) != 0;
    return game.lightgun;
}

//*******************************
// LightgunService::setRetroArchLightgun
//*******************************
void LightgunService::setRetroArchLightgun(const PsGame &game, bool on) {
    if (!game.foreign || game.app || game.image_path.empty()) return;
    bool changed = on ? raPaths_.insert(game.image_path).second : raPaths_.erase(game.image_path) != 0;
    if (changed) save();
}

//*******************************
// LightgunService::reload
//*******************************
void LightgunService::reload() {
    raPaths_.clear();
    string path = lightgunsFile();
    if (!DirEntry::exists(path)) return;

    ifstream in(path);
    string line;
    bool dropped = false;
    while (getline(in, line)) {
        line = Strings::trim(line);
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) continue;
        if (DirEntry::exists(line)) {
            raPaths_.insert(line);
        } else {
            cout << "lightguns.txt: " << line << " is gone - dropped" << endl;
            dropped = true;
        }
    }
    in.close();
    if (dropped) save();
}

//*******************************
// LightgunService::save
//*******************************
void LightgunService::save() const {
    string path = lightgunsFile();
    if (raPaths_.empty()) {
        if (DirEntry::exists(path)) DirEntry::removeFile(path);
        return;
    }
    ofstream out(path);
    if (!DirEntry::checkWritable(out, path)) return;
    for (const string &p : raPaths_) out << p << '\n';
}
