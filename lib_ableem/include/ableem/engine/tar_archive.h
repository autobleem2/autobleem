// lib_ableem - engine: reading a .tar.gz (or a plain .tar) - what the download repository's packages are
// (the stick's file system, the cores, the libraries, the apps, the sample games). ustar and GNU tar
// (long names through the 'L' entry, the ustar prefix), gzip through miniz's raw inflate. Symbolic links
// are reported but never made: the sticks are FAT, and the console's scripts make what they need.
#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace ableem {

struct TarEntry {
    std::string name; // as stored, "./" and a leading "/" stripped: "Autobleem/rc/launch.sh"
    uint64_t size = 0;
    bool isDir = false;
    bool isSymlink = false;
    bool isFile = false; // a regular file (everything else - devices, fifos - is skipped)
    unsigned mode = 0644;
};

//******************
// TarArchive
//******************
class TarArchive {
public:
    // per entry, before it is extracted: return false to skip it (its bytes are still read past)
    using Filter = std::function<bool(const TarEntry &)>;
    // after every entry, with the bytes of the *compressed* file read so far and its total - progress
    using Progress = std::function<void(uint64_t done, uint64_t total)>;

    // the entries, in order
    static bool list(const std::string &tarPath, std::vector<TarEntry> &entries, std::string &error);

    // unpacks under destDir (created as needed); a name with ".." or an absolute one is refused. `prefix`
    // (e.g. "Apps/") is stripped from every name when given, and entries outside it are skipped.
    static bool extract(const std::string &tarPath, const std::string &destDir, std::string &error,
                        const Filter &filter = Filter(), const Progress &progress = Progress(),
                        const std::string &prefix = "");

    // one entry's bytes (a VERSION file, a manifest) without unpacking anything else; false when absent
    static bool readEntry(const std::string &tarPath, const std::string &name, std::string &data, std::string &error);

    static bool isSafeName(const std::string &name);
};

} // namespace ableem
