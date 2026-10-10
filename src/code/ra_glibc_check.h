// Does the RetroArch program need a newer system library than the console has? Pure, so a test can say it.
//
// A RetroArch built on a newer system than the console's (a stock RetroBoot 1.1 tree next to a later build) names
// the glibc versions it was linked against ("GLIBC_2.28") in its version needs (.gnu.version_r); the console's
// dynamic loader refuses such a program at once ("version `GLIBC_2.28' not found") and nothing ever shows on screen.
// The launcher reads those names from the ELF sections before it starts RetroArch and compares the highest with
// the running libc.
#pragma once

#include <cctype>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <istream>
#include <map>
#include <mutex>
#include <string>
#include <vector>

#ifdef __GLIBC__
#include <gnu/libc-version.h>
#endif

struct GlibcVersion {
    int major = 0;
    int minor = 0;

    bool known() const {
        return major > 0;
    }
    bool operator<(const GlibcVersion &o) const {
        return major != o.major ? major < o.major : minor < o.minor;
    }
};

// "2.28" or "2.28.1" (a libc's own version string): the first two numbers; an unknown version when there are none
inline GlibcVersion parseGlibcVersion(const std::string &text) {
    GlibcVersion v;
    size_t i = 0;
    auto number = [&](int &out) {
        if (i >= text.size() || !isdigit(static_cast<unsigned char>(text[i])))
            return false;
        long n = 0;
        while (i < text.size() && isdigit(static_cast<unsigned char>(text[i])) && n < 100000)
            n = n * 10 + (text[i++] - '0');
        out = static_cast<int>(n);
        return true;
    };
    int major = 0;
    int minor = 0;
    if (!number(major) || i >= text.size() || text[i] != '.')
        return v;
    ++i;
    if (!number(minor))
        return v;
    v.major = major;
    v.minor = minor;
    return v;
}

// the highest "GLIBC_<major>.<minor>" in the bytes (GLIBC_PRIVATE and other names without numbers do not count);
// an unknown version when there is none - a program that is not a glibc one
inline GlibcVersion highestGlibcNeeded(const std::string &bytes) {
    static const std::string Tag = "GLIBC_";
    GlibcVersion best;
    size_t at = bytes.find(Tag);
    while (at != std::string::npos) {
        const GlibcVersion v = parseGlibcVersion(bytes.substr(at + Tag.size(), 24));
        if (v.known() && best < v)
            best = v;
        at = bytes.find(Tag, at + Tag.size());
    }
    return best;
}

// true when the program needs a newer glibc than the one running; false when either is unknown (nothing to
// judge by: let the program start)
inline bool needsNewerGlibc(const GlibcVersion &needed, const GlibcVersion &have) {
    return needed.known() && have.known() && have < needed;
}

namespace glibc_elf {

constexpr size_t MaxSectionTable = 4u << 20; // bytes a section header table, a version section or a string table may take
constexpr uint32_t ShtGnuVerneed = 0x6ffffffe;

inline uint64_t le(const std::vector<char> &b, size_t at, size_t width) {
    uint64_t v = 0;
    for (size_t i = 0; i < width; ++i)
        v |= static_cast<uint64_t>(static_cast<unsigned char>(b[at + i])) << (8 * i);
    return v;
}

// size bytes at offset; empty when out of the file or too big to be sane
inline bool readAt(std::istream &in, uint64_t offset, uint64_t size, uint64_t fileSize, std::vector<char> &out) {
    if (size == 0 || size > MaxSectionTable || offset > fileSize || size > fileSize - offset)
        return false;
    out.resize(static_cast<size_t>(size));
    in.clear();
    in.seekg(static_cast<std::streamoff>(offset), std::ios::beg);
    in.read(out.data(), static_cast<std::streamsize>(size));
    return static_cast<uint64_t>(in.gcount()) == size;
}

} // namespace glibc_elf

// The highest "GLIBC_x.y" the dynamic loader will check in an ELF program: the version names of the SHT_GNU_verneed
// section (.gnu.version_r), found through the section headers; a few small reads, never the whole file (a RetroArch
// binary is tens of megabytes, on a USB stick). Unknown for anything that is not a little-endian 32 or 64 bit ELF
// or is cut short: the program is then let start.
inline GlibcVersion highestGlibcNeededInElf(std::istream &in) {
    using namespace glibc_elf;
    GlibcVersion best;
    in.clear();
    in.seekg(0, std::ios::end);
    const std::streamoff end = in.tellg();
    if (end <= 0)
        return best;
    const uint64_t fileSize = static_cast<uint64_t>(end);

    std::vector<char> head;
    if (!readAt(in, 0, 64, fileSize, head)) {
        // a 32 bit header is 52 bytes
        if (!readAt(in, 0, 52, fileSize, head))
            return best;
    }
    if (head[0] != 0x7f || head[1] != 'E' || head[2] != 'L' || head[3] != 'F')
        return best;
    const bool is64 = head[4] == 2;
    if ((head[4] != 1 && !is64) || head[5] != 1) // 32 or 64 bit; little-endian only
        return best;
    if (head.size() < (is64 ? 64u : 52u))
        return best;

    const uint64_t shoff = is64 ? le(head, 0x28, 8) : le(head, 0x20, 4);
    const size_t shentsize = static_cast<size_t>(is64 ? le(head, 0x3A, 2) : le(head, 0x2E, 2));
    uint64_t shnum = is64 ? le(head, 0x3C, 2) : le(head, 0x30, 2);
    const size_t minEntry = is64 ? 64 : 40;
    if (shoff == 0 || shentsize < minEntry)
        return best;

    std::vector<char> table;
    if (shnum == 0) { // more sections than fit: the count is the size of the first one
        if (!readAt(in, shoff, shentsize, fileSize, table))
            return best;
        shnum = is64 ? le(table, 0x20, 8) : le(table, 0x14, 4);
    }
    if (shnum == 0 || shnum > MaxSectionTable / shentsize ||
        !readAt(in, shoff, shnum * shentsize, fileSize, table))
        return best;

    struct Section {
        uint32_t type;
        uint64_t offset;
        uint64_t size;
        uint32_t link;
    };
    auto section = [&](uint64_t index) {
        const size_t at = static_cast<size_t>(index) * shentsize;
        Section s;
        s.type = static_cast<uint32_t>(le(table, at + 4, 4));
        s.offset = is64 ? le(table, at + 0x18, 8) : le(table, at + 0x10, 4);
        s.size = is64 ? le(table, at + 0x20, 8) : le(table, at + 0x14, 4);
        s.link = static_cast<uint32_t>(is64 ? le(table, at + 0x28, 4) : le(table, at + 0x18, 4));
        return s;
    };

    for (uint64_t i = 0; i < shnum; ++i) {
        const Section verneed = section(i);
        if (verneed.type != ShtGnuVerneed || verneed.link >= shnum)
            continue;
        const Section strings = section(verneed.link);
        std::vector<char> needs;
        std::vector<char> names;
        if (!readAt(in, verneed.offset, verneed.size, fileSize, needs) ||
            !readAt(in, strings.offset, strings.size, fileSize, names))
            continue;

        // Elf_Verneed: version u16, cnt u16, file u32, aux u32, next u32; Elf_Vernaux: hash u32, flags u16,
        // other u16, name u32, next u32 - the same 16 bytes in both classes
        size_t need = 0;
        for (size_t guard = 0; guard < needs.size() / 16 + 1; ++guard) {
            if (need + 16 > needs.size())
                break;
            const size_t count = static_cast<size_t>(le(needs, need + 2, 2));
            size_t aux = need + static_cast<size_t>(le(needs, need + 8, 4));
            for (size_t n = 0; n < count && aux + 16 <= needs.size() && aux >= need; ++n) {
                const uint64_t nameAt = le(needs, aux + 8, 4);
                if (nameAt < names.size()) {
                    const std::string name(names.data() + nameAt, strnlen(names.data() + nameAt, names.size() - nameAt));
                    if (name.compare(0, 6, "GLIBC_") == 0) {
                        const GlibcVersion v = parseGlibcVersion(name.substr(6));
                        if (best < v)
                            best = v;
                    }
                }
                const uint64_t next = le(needs, aux + 12, 4);
                if (next == 0)
                    break;
                aux += static_cast<size_t>(next);
            }
            const uint64_t nextNeed = le(needs, need + 12, 4);
            if (nextNeed == 0)
                break;
            need += static_cast<size_t>(nextNeed);
        }
    }
    return best;
}

// how many times a file was parsed (not served from the cache): a test reads it
inline int &glibcFileParses() {
    static int parses = 0;
    return parses;
}

// the same for a file. The verdict is kept in memory by path, size and modification time, so the next start of
// RetroArch in the process does not read the file again; a file that cannot be looked at is unknown, not kept.
inline GlibcVersion highestGlibcNeededInFile(const std::string &path) {
    struct Entry {
        uintmax_t size;
        std::filesystem::file_time_type time;
        GlibcVersion version;
    };
    static std::map<std::string, Entry> cache;
    static std::mutex lock;

    std::error_code sizeError;
    std::error_code timeError;
    const uintmax_t size = std::filesystem::file_size(path, sizeError);
    const auto time = std::filesystem::last_write_time(path, timeError);
    if (sizeError || timeError)
        return GlibcVersion();

    std::lock_guard<std::mutex> guard(lock);
    auto found = cache.find(path);
    if (found != cache.end() && found->second.size == size && found->second.time == time)
        return found->second.version;

    ++glibcFileParses();
    GlibcVersion version;
    std::ifstream in(path, std::ios::binary);
    if (in)
        version = highestGlibcNeededInElf(in);
    cache[path] = Entry{size, time, version};
    return version;
}

// the libc this process runs on; unknown where there is no glibc (Windows, a musl build)
inline GlibcVersion runningGlibc() {
#ifdef __GLIBC__
    return parseGlibcVersion(gnu_get_libc_version());
#else
    return GlibcVersion();
#endif
}
