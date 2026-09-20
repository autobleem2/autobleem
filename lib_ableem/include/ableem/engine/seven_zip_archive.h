// lib_ableem - engine: reading a .7z file, for RetroArch's Windows builds (libretro ships them as 7z
// archives: RetroArch.7z, RetroArch_cores.7z). The one place that touches the vendored LZMA SDK 7z reader
// (third_party/lzma-7z); the app never includes it. Without CHD support (ABLEEM_NO_CHD - the codecs are
// libchdr's lzma) every call answers false.
#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace ableem {

//******************
// SevenZipEntry
//******************
struct SevenZipEntry {
    std::string name; // forward slashes, directories as "name/"
    bool isDir = false;
    uint64_t size = 0; // unpacked
};

//******************
// SevenZipArchive
//******************
// extract() unpacks every file into `destDir`, creating directories as it goes. Entries that would land
// outside destDir (absolute names, "..", drive letters, backslashes) are refused and fail the whole
// extraction - an archive is untrusted input. A 7z decodes a solid block at a time into memory (the SDK's
// extractor has no streaming), so the largest block's unpacked size is the memory a run needs.
class SevenZipArchive {
public:
    // false when an entry is not wanted; the whole archive when not given
    using Filter = std::function<bool(const SevenZipEntry &)>;
    // bytes unpacked so far, of the archive's total unpacked size
    using Progress = std::function<void(uint64_t done, uint64_t total)>;

    // the entries as the archive lists them. Empty (and false) when it is not a 7z.
    static bool list(const std::string &archivePath, std::vector<SevenZipEntry> &entries);

    // unpacks into destDir (created if missing); `prefix` names the folder inside the archive to take
    // (with its trailing slash; "" = everything), which is stripped from the destination names. False,
    // with the reason in `error`, on a bad archive, a bad name, or a file that could not be written;
    // whatever was already extracted is left for the caller.
    static bool extract(const std::string &archivePath, const std::string &destDir, std::string &error,
                        const Filter &filter = Filter(), const Progress &progress = Progress(),
                        const std::string &prefix = "");
};

} // namespace ableem
