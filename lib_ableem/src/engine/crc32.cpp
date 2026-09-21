#include "ableem/engine/crc32.h"
#include "ableem/engine/filesystem.h"

#include <cstdio>
#include <miniz.h>
#include <vector>

using namespace std;

namespace ableem {

//*******************************
// Crc32::ofFile
//*******************************
bool Crc32::ofFile(const string &path, uint32_t &crc, uint64_t maxBytes) {
    crc = 0;
    long long size = DirEntry::fileSize(path);
    if (size < 0 || (maxBytes != 0 && static_cast<uint64_t>(size) > maxBytes))
        return false;
    FILE *f = fopen(path.c_str(), "rb");
    if (!f)
        return false;
    vector<unsigned char> buf(256 * 1024);
    mz_ulong running = MZ_CRC32_INIT;
    size_t got;
    while ((got = fread(buf.data(), 1, buf.size(), f)) > 0)
        running = mz_crc32(running, buf.data(), got);
    const bool ok = ferror(f) == 0;
    fclose(f);
    crc = ok ? static_cast<uint32_t>(running) : 0;
    return ok;
}

//*******************************
// Crc32::ofBytes
//*******************************
uint32_t Crc32::ofBytes(const string &bytes) {
    return static_cast<uint32_t>(
        mz_crc32(MZ_CRC32_INIT, reinterpret_cast<const unsigned char *>(bytes.data()), bytes.size()));
}

//*******************************
// Crc32::playlistText
//*******************************
string Crc32::playlistText(uint32_t crc) {
    char buf[16];
    snprintf(buf, sizeof(buf), "%08X|crc", static_cast<unsigned>(crc));
    return buf;
}

//*******************************
// Crc32::fromPlaylistText
//*******************************
bool Crc32::fromPlaylistText(const string &text, uint32_t &crc) {
    if (text.size() != 12 || text.compare(8, 4, "|crc") != 0)
        return false;
    uint32_t value = 0;
    for (int i = 0; i < 8; i++) {
        char c = text[i];
        int digit;
        if (c >= '0' && c <= '9')
            digit = c - '0';
        else if (c >= 'A' && c <= 'F')
            digit = c - 'A' + 10;
        else if (c >= 'a' && c <= 'f')
            digit = c - 'a' + 10;
        else
            return false;
        value = (value << 4) | static_cast<uint32_t>(digit);
    }
    if (value == 0)
        return false;
    crc = value;
    return true;
}

} // namespace ableem
