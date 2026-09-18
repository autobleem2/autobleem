// lib_ableem - engine: reads the ISO9660 volume descriptor and directory tree of a PS1 disc image
// (.bin/.img, or .chd when built with CHD support). https://en.wikipedia.org/wiki/ISO_9660
#pragma once

#include <string>
#include <vector>

namespace ableem {

//******************
// IsoDirectory
//******************
class IsoDirectory {
public:
    std::string systemName; // "PLAYSTATION" on a PS1 disc; "UNKNOWN" when the image could not be read
    std::string volumeName;
    std::vector<std::string> rootDir; // every file/dir name found, version suffix (";1") removed
};

//******************
// IsoDirectoryReader
//******************
class IsoDirectoryReader {
public:
    // maxLevel = how deep to descend into sub-directories (1 = root only). an unreadable image gives
    // an "UNKNOWN"/"UNKNOWN" result with an empty rootDir.
    static IsoDirectory read(const std::string &imagePath, int maxLevel, bool isChd);
};

} // namespace ableem
