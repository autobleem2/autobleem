#include "temp_dir.h"

#include <fstream>
#include <sstream>

using namespace ableem;

//*******************************
// TempDir::makeSubDir
//*******************************
std::string TempDir::makeSubDir(const std::string &relative) const {
    std::string built = path_;
    std::string level;
    std::istringstream parts(relative);
    while (std::getline(parts, level, separator)) {
        if (level.empty())
            continue;
        built = built + sep + level;
        DirEntry::createDir(built);
    }
    return built;
}

//*******************************
// TempDir::writeFile
//*******************************
void TempDir::writeFile(const std::string &relative, const std::string &contents) const {
    std::string full = at(relative);
    std::string dir = DirEntry::getDirNameFromPath(full);
    if (dir != path_ && !DirEntry::exists(dir)) {
        makeSubDir(relative.substr(0, relative.find_last_of(separator)));
    }
    std::ofstream os(full, std::ios::binary);
    os << contents;
}

//*******************************
// TempDir::readFile
//*******************************
std::string TempDir::readFile(const std::string &relative) const {
    std::ifstream is(at(relative), std::ios::binary);
    std::ostringstream all;
    all << is.rdbuf();
    return all.str();
}
