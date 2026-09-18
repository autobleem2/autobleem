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

} // namespace ableem
