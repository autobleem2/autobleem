//
// ChannelWatermark (UIREV-40): what the little tag in the EvolutionUI's top-left corner says. The build names its
// channel itself - AB_BUILD_CHANNEL, given by whatever builds it: dev | nightly | prerelease | release, unset = dev,
// so any hand build says DEV - and the tag follows it:
//   dev          DEV + the build's short commit hash                         "DEV  c7357bb"
//   nightly      NIGHTLY + the short version                                 "NIGHTLY  2.0.0-a2.314"
//   prerelease   the tag's own word - ALPHA / BETA / RC - + the short version "ALPHA  2.0.0-a1"
//                (a pre-release tag with any other word says TESTING)
//   release      no tag
// The version text (Env::productVersion(): the package's VERSION file, else the build's git describe) is only
// parsed for the short version and the pre-release word, never for the channel: the leading "v" and the git hash go,
// the commit count is written ".<count>", alpha/beta are shortened to a/b - "2.0.0-a2.470", "2.0.0-rc1".
// Pure text work, so tests/screens/test_channel_watermark holds it without a Gui; GuiLauncher::renderChannelWatermark
// draws the result.
//
#pragma once

#include <cctype>
#include <cstring>
#include <string>
#include <vector>

namespace ChannelWatermark {

enum class Channel { Release, Prerelease, Nightly, Dev };

struct Tag {
    Channel channel = Channel::Release;
    std::string word;    // "DEV" / "NIGHTLY" / "ALPHA" / "BETA" / "RC" / "TESTING"; "" for a release
    std::string version; // the short version (DEV: the short commit hash)
    bool shown() const { return channel != Channel::Release; }
};

// the build's channel from its AB_BUILD_CHANNEL value; unset or anything unknown is a hand build: dev
inline Channel channelFromName(const char *name) {
    if (name && std::strcmp(name, "nightly") == 0)
        return Channel::Nightly;
    if (name && std::strcmp(name, "prerelease") == 0)
        return Channel::Prerelease;
    if (name && std::strcmp(name, "release") == 0)
        return Channel::Release;
    return Channel::Dev;
}

namespace detail {

inline bool allDigits(const std::string &s) {
    if (s.empty())
        return false;
    for (char c : s)
        if (!std::isdigit(static_cast<unsigned char>(c)))
            return false;
    return true;
}

// the package's nightly suffix "-n<fingerprint>": "n" + 4 or more hex digits ("n72fd67")
inline bool isFingerprint(const std::string &s) {
    if (s.size() < 5 || s[0] != 'n')
        return false;
    for (size_t i = 1; i < s.size(); i++)
        if (!std::isxdigit(static_cast<unsigned char>(s[i])))
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

// the short version of a version string ("v2.0.0-alpha2-470-g1760cc8" -> "2.0.0-a2.470") and the pre-release word
// of its tag ("ALPHA", "BETA", "RC"; empty when it has none or another one). Both empty for anything not a version.
inline std::string shortVersion(const std::string &versionString, std::string &preWord) {
    preWord.clear();
    std::string v = versionString;
    if (!v.empty() && (v[0] == 'v' || v[0] == 'V'))
        v.erase(0, 1);
    if (v.empty() || !std::isdigit(static_cast<unsigned char>(v[0])))
        return ""; // "dev" and the like: no version to show

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

    if (!tokens.empty() && tokens.back() == "dirty")
        tokens.pop_back();
    while (tokens.size() > 1 && (isHash(tokens.back()) || isFingerprint(tokens.back())))
        tokens.pop_back();
    std::string count;
    if (tokens.size() > 1 && allDigits(tokens.back())) {
        count = tokens.back();
        tokens.pop_back();
    }
    if (tokens.empty())
        return "";

    std::string out;
    for (size_t i = 0; i < tokens.size(); i++)
        out += (i ? "-" : "") + (i ? abbreviate(tokens[i]) : tokens[i]);
    if (tokens.size() > 1) {
        const std::string &t = tokens[1];
        if (t.compare(0, 5, "alpha") == 0)
            preWord = "ALPHA";
        else if (t.compare(0, 4, "beta") == 0)
            preWord = "BETA";
        else if (t.compare(0, 2, "rc") == 0)
            preWord = "RC";
    }
    if (!count.empty())
        out += "." + count;
    return out;
}

} // namespace detail

// the tag for a build: its channel (given by the build), the version text and the commit hash it was built from
inline Tag tagFor(Channel channel, const std::string &versionString, const std::string &commitHash) {
    Tag tag;
    tag.channel = channel;
    std::string preWord;
    switch (channel) {
    case Channel::Release:
        break;
    case Channel::Dev:
        tag.word = "DEV";
        tag.version = commitHash.substr(0, 7);
        break;
    case Channel::Nightly:
        tag.word = "NIGHTLY";
        tag.version = detail::shortVersion(versionString, preWord);
        break;
    case Channel::Prerelease:
        tag.version = detail::shortVersion(versionString, preWord);
        tag.word = preWord.empty() ? "TESTING" : preWord;
        break;
    }
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
