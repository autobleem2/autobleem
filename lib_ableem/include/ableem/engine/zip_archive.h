// lib_ableem - engine: reading a .zip file (deflate/stored), for themes dropped as archives. The one place
// that touches the vendored miniz; the app never includes it.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace ableem {

//******************
// ZipEntry
//******************
// one entry as the central directory describes it - no decompression involved, so the CRC of every ROM in
// an archive comes for free
struct ZipEntry {
    std::string name; // directories as "name/"
    bool isDir = false;
    uint32_t crc = 0;  // crc32 of the uncompressed bytes, as the archiver recorded it
    uint64_t size = 0; // uncompressed
};

//******************
// ZipArchive
//******************
// extract() unpacks every file into `destDir`, creating directories as it goes. Entries that would land
// outside destDir (absolute names, "..", drive letters, backslashes) are refused and fail the whole
// extraction - an archive is untrusted input. Nothing is written until the names have all been checked.
class ZipArchive {
public:
    // the file names in the archive, directories as "name/". Empty (and false) when it is not a zip.
    static bool list(const std::string &zipPath, std::vector<std::string> &names);
    // the same with each entry's recorded CRC and size
    static bool listEntries(const std::string &zipPath, std::vector<ZipEntry> &entries);

    // unpacks into destDir (created if missing). False, with the reason logged, on a bad archive, a bad
    // name, or a file that could not be written; whatever was already extracted is left for the caller.
    static bool extract(const std::string &zipPath, const std::string &destDir);

    // an entry name the extractor will accept: relative, forward slashes, no ".." segment
    static bool isSafeName(const std::string &name);
};

} // namespace ableem
