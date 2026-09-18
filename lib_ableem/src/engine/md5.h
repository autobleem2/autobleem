// lib_ableem - engine (private): MD5 (RFC 1321), enough for SerialScanner::serialFromMd5 without shelling out
// to md5sum (which does not exist on every target).
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

private:
    void transform(const unsigned char block[64]);
    uint32_t state[4];
    uint64_t bitCount = 0;
    unsigned char buffer[64];
    size_t bufferLen = 0;
};

} // namespace ableem
