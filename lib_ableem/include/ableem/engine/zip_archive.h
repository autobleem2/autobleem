// lib_ableem - engine: reading a .zip file (deflate/stored), for themes dropped as archives. The one place
// that touches the vendored miniz; the app never includes it.
#pragma once

#include <string>
#include <vector>

namespace ableem {

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

    // unpacks into destDir (created if missing). False, with the reason logged, on a bad archive, a bad
    // name, or a file that could not be written; whatever was already extracted is left for the caller.
    static bool extract(const std::string &zipPath, const std::string &destDir);

    // an entry name the extractor will accept: relative, forward slashes, no ".." segment
    static bool isSafeName(const std::string &name);
};

} // namespace ableem
