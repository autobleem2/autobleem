// lib_ableem - engine: writing a zip file, entry by entry, streamed from files of any size. What
// abflashkit's recovery backup (LBOOT.EPB: the console's partitions, zipped) needs; ZipArchive is the read
// side. Deflate at the default level, no time stamps (the console has no clock worth recording).
#pragma once

#include <memory>
#include <string>

namespace ableem {

class ZipWriter {
public:
    ZipWriter();
    ~ZipWriter(); // closes without finishing: an archive not close()d is left incomplete on purpose
    ZipWriter(const ZipWriter &) = delete;
    ZipWriter &operator=(const ZipWriter &) = delete;

    // creates (truncates) the archive; false, with the reason logged, when the file cannot be written
    bool open(const std::string &zipPath);
    // adds the file at `path` as entry `name` (forward slashes), read in chunks - a block device or a
    // multi-GB image is fine. False, logged, on a read or write error.
    bool addFile(const std::string &path, const std::string &name);
    // adds `bytes` as entry `name`
    bool addBytes(const std::string &name, const std::string &bytes);
    // writes the central directory and closes the file; false when that failed
    bool close();
    bool isOpen() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};

} // namespace ableem
