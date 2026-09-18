// lib_ableem - engine: MD5 (RFC 1321) - for SerialScanner::serialFromMd5 and for checking a file against a
// published sum without shelling out to md5sum (which does not exist on every target).
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

namespace ableem {

class Md5 {
public:
    Md5();
    void update(const unsigned char *data, size_t length);
    std::string hexDigest(); // finalizes; 32 lower-case hex chars

    static std::string ofBytes(const unsigned char *data, size_t length);
    static std::string ofString(const std::string &text);
    // streamed, so a partition image of any size is fine; "" when the file cannot be read
    static std::string ofFile(const std::string &path);

private:
    void transform(const unsigned char block[64]);
    uint32_t state[4];
    uint64_t bitCount = 0;
    unsigned char buffer[64];
    size_t bufferLen = 0;
};

} // namespace ableem
