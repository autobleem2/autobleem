// Does the RetroArch program need a newer system library than the console has? Pure, so a test can say it.
//
// A RetroArch built on a newer system than the console's (a stock RetroBoot 1.1 tree next to a later build) names
// the glibc versions it was linked against ("GLIBC_2.28") in its dynamic symbol strings; the console's dynamic
// loader refuses such a program at once ("version `GLIBC_2.28' not found") and nothing ever shows on screen.
// The launcher reads those names before it starts RetroArch and compares the highest with the running libc.
#pragma once

#include <cctype>
#include <cstddef>
#include <fstream>
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

// the same for a file, read in pieces (a RetroArch binary is tens of megabytes and the console has little memory);
// the end of each piece is kept in front of the next so a name cut by the border is still found
inline GlibcVersion highestGlibcNeededInFile(const std::string &path) {
    GlibcVersion best;
    std::ifstream in(path, std::ios::binary);
    if (!in)
        return best;
    constexpr size_t Piece = 1 << 20;
    constexpr size_t Overlap = 32;
    std::vector<char> buffer(Piece);
    std::string carry;
    while (in) {
        in.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
        const std::streamsize got = in.gcount();
        if (got <= 0)
            break;
        const std::string piece = carry + std::string(buffer.data(), static_cast<size_t>(got));
        const GlibcVersion v = highestGlibcNeeded(piece);
        if (best < v)
            best = v;
        carry = piece.size() > Overlap ? piece.substr(piece.size() - Overlap) : piece;
    }
    return best;
}

// the libc this process runs on; unknown where there is no glibc (Windows, a musl build)
inline GlibcVersion runningGlibc() {
#ifdef __GLIBC__
    return parseGlibcVersion(gnu_get_libc_version());
#else
    return GlibcVersion();
#endif
}
