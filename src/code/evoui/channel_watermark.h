//
// ChannelWatermark (UIREV-40): what the little tag in the EvolutionUI's top-left corner says. A nightly or testing
// build names its channel (NIGHTLY / TESTING) and its short version; a release says nothing. Pure text work on the
// build's version string (Env::productVersion(): the package's VERSION file, else the build's git describe), so
// tests/screens/test_channel_watermark holds it without a Gui; GuiLauncher::renderChannelWatermark draws the result.
//
// The channel is read off the version, the way the site names its builds:
//   v2.0.0                  a release: no tag
//   v2.0.0-rc1, -alpha2     a tagged pre-release: TESTING
//   v2.0.0-alpha2-470-gabc  commits past a tag (a nightly), or a dirty tree: NIGHTLY
// The short version drops the leading "v" and the git hash, and writes the commit count as ".<count>":
// "2.0.0-a2.470" (alpha/beta shortened to a/b), "2.0.0-rc1".
//
#pragma once

#include <cctype>
#include <string>
#include <vector>

namespace ChannelWatermark {

enum class Channel { None, Testing, Nightly };

struct Tag {
    Channel channel = Channel::None;
    std::string word;    // "NIGHTLY" / "TESTING"; "" for a release
    std::string version; // the short version
    bool shown() const { return channel != Channel::None; }
};

namespace detail {

inline bool allDigits(const std::string &s) {
    if (s.empty())
        return false;
    for (char c : s)
        if (!std::isdigit(static_cast<unsigned char>(c)))
            return false;
    return true;
}

// a git hash as describe writes it: "g" + 7 or more hex digits, or the bare hex digits
inline bool isHash(const std::string &s) {
    size_t start = (!s.empty() && s[0] == 'g') ? 1 : 0;
    if (s.size() - start < 7)
        return false;
    for (size_t i = start; i < s.size(); i++)
        if (!std::isxdigit(static_cast<unsigned char>(s[i])))
            return false;
    return true;
}

// "alpha2" -> "a2", "beta1" -> "b1"; anything else as it is
inline std::string abbreviate(const std::string &token) {
    if (token.compare(0, 5, "alpha") == 0)
        return "a" + token.substr(5);
    if (token.compare(0, 4, "beta") == 0)
        return "b" + token.substr(4);
    return token;
}

} // namespace detail

// the tag for a version string; Channel::None (and empty texts) for a release or anything not a version
inline Tag tagFor(const std::string &versionString) {
    Tag tag;
    std::string v = versionString;
    if (!v.empty() && (v[0] == 'v' || v[0] == 'V'))
        v.erase(0, 1);
    if (v.empty() || !std::isdigit(static_cast<unsigned char>(v[0])))
        return tag; // "dev" and the like: not a build of a channel

    std::vector<std::string> tokens;
    size_t from = 0;
    while (from <= v.size()) {
        size_t dash = v.find('-', from);
        if (dash == std::string::npos)
            dash = v.size();
        if (dash > from)
            tokens.push_back(v.substr(from, dash - from));
        from = dash + 1;
    }

    bool ahead = false; // past its tag: a nightly
    if (!tokens.empty() && tokens.back() == "dirty") {
        ahead = true;
        tokens.pop_back();
    }
    while (tokens.size() > 1 && detail::isHash(tokens.back())) {
        ahead = true;
        tokens.pop_back();
    }
    std::string count;
    if (tokens.size() > 1 && detail::allDigits(tokens.back())) {
        count = tokens.back();
        tokens.pop_back();
        ahead = true;
    }
    if (tokens.empty())
        return tag;

    const bool preRelease = tokens.size() > 1;
    if (!ahead && !preRelease)
        return tag; // a plain tag: a release

    tag.channel = ahead ? Channel::Nightly : Channel::Testing;
    tag.word = ahead ? "NIGHTLY" : "TESTING";
    for (size_t i = 0; i < tokens.size(); i++)
        tag.version += (i ? "-" : "") + (i ? detail::abbreviate(tokens[i]) : tokens[i]);
    if (!count.empty())
        tag.version += "." + count;
    return tag;
}

// the tag's place: under a two-pad battery plate, which ends at y 66 (x 2, y 2, 64 high), the same with or without one
constexpr int X = 12;
constexpr int Y = 72;
constexpr int PlateGap = 6; // clear of the plate: Y - 66, so a plate taller than two pads' pushes the tag down

// the tag's y: Y, or lower when the plate (its bottom edge, 0 when there is none) comes down past Y - PlateGap
inline int yBelow(int plateBottom) {
    return plateBottom + PlateGap > Y ? plateBottom + PlateGap : Y;
}
constexpr int ChipHeight = 24;
constexpr int ChipPadding = 7; // the word's inset in the chip; the chip is the word + 2 * this
constexpr int VersionGap = 8;  // the chip to the version
constexpr int Alpha = 204;     // 80 %: a mark, not a control

} // namespace ChannelWatermark
