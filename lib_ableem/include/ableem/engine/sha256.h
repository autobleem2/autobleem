// lib_ableem - engine: SHA-256 (FIPS 180-4) - for checking a downloaded file against the sum the download
// repository publishes next to it, without shelling out to sha256sum (a PC has none).
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

namespace ableem {

class Sha256 {
public:
    Sha256();
    void update(const unsigned char *data, size_t length);
    std::string hexDigest(); // finalizes; 64 lower-case hex chars

    static std::string ofBytes(const unsigned char *data, size_t length);
    static std::string ofString(const std::string &text);
    // streamed, so a file of any size is fine; "" when the file cannot be read
    static std::string ofFile(const std::string &path);

private:
    void transform(const unsigned char block[64]);
    uint32_t state[8];
    uint64_t bitCount = 0;
    unsigned char buffer[64];
    size_t bufferLen = 0;
};

} // namespace ableem
