// lib_ableem - engine: finds a PS1 game's serial number (SLUS-01234, SCES-00001, ...) in its disc image.
#pragma once

#include <string>

#include "game_types.h"

namespace ableem {

//******************
// SerialScanner
//******************
class SerialScanner {
public:
    // "SLUS_012.34" / "SLUS01234" -> "SLUS-01234"
    static std::string normalizeSerial(std::string serial);

    // readSerialFromImage() and, when that finds nothing, readSerialByWorkaround(). "" when neither does.
    // path = the game dir; firstBinPath = the first .bin/.img/.chd of the game ("" for a PBP)
    static std::string readSerial(ImageType imageType, std::string path, std::string firstBinPath = "");

    // PBP: the DISC_ID field of the embedded SFO. cue/bin, img, chd: the first root-dir file (or the volume
    // name) that starts with a known publisher prefix, up to 3 directory levels deep.
    static std::string readSerialFromImage(ImageType imageType, std::string path, std::string firstBinPath = "");

    // games whose image carries no serial at all - currently only Resident Evil 1.5 (BH2), identified by md5
    static std::string readSerialByWorkaround(ImageType imageType, std::string path, std::string firstBinPath);

    // md5 of the first 1 MB followed by md5 of the last 1 MB of the file (64 hex chars)
    static std::string serialFromMd5(std::string scanFile);

    static std::string serialToRegion(const std::string &serial); // "US", "Europe-Aus", "Japan" or ""
};

} // namespace ableem
