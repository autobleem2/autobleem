// lib_ableem - engine: a flat "[section] key=value" file. Keys are lower-cased on load; "#" starts a comment.
#pragma once

#include <map>
#include <string>

namespace ableem {

//******************
// IniFile
//******************
//
// Example USB game ini file, stored in map<string, string> values (keys lower-cased):
//
// [Game]
// Automation=0
// Discs=Twisted Metal 2.pbp
// Highres=0
// Imagetype=1		// IMAGE_CUE_BIN=0, IMAGE_PBP=1
// Memcard=SONY
// Players=2
// Publisher=Sony Computer Entertainment.
// Title=Twisted Metal 2
// Year=1996
//
class IniFile {
public:
    std::string section = "";                    // example: "Game" (from [Game] above)
    std::string path = "";                       // example: "/media/Games/Racing/007 Racing (USA)/Game.ini"
    std::string entry = "";                      // example: "007 Racing (USA)"
    std::map<std::string, std::string> values;   // see example data above

    void load(const std::string &path);          // adds to / overwrites whatever is already in values
    void reload(const std::string &path);        // clears values first
    void mergeFrom(const std::string &path);     // load() spelled out: existing keys are overwritten, new ones appended

    void save(const std::string &path);          // keys are written Capitalized
    void print();
};

} // namespace ableem
