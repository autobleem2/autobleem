//
// What the Packages screens decide, as pure functions (autobleem-main docs/packages.md 5.4, 6, 7): how a start with an
// engine goes (no game data / one entry / the picker), the picker's row texts and where its cursor starts, and which
// installed Apps run a package. The screens (GuiPackagePicker, GuiPackageInfo) only draw and call these; the texts
// that reach the player are translated by the caller and passed in, so nothing here needs a language.
//
#pragma once

#include "core/services/package_service.h"

#include <algorithm>
#include <functional>
#include <string>
#include <utility>
#include <vector>

namespace packagepicker {

// the pixel width of a text, as the screen's font measures it
using Measure = std::function<int(const std::string &)>;

//******************
// decide
//******************
// 0 entries: the "No game data" message, nothing starts. 1: the engine starts at once, no screen. 2 or more: the
// picker.
enum class Start { NoData, Single, Pick };

inline Start decide(size_t entries) {
    return entries == 0 ? Start::NoData : entries == 1 ? Start::Single : Start::Pick;
}

//******************
// startRow
//******************
// the row the cursor starts on: the last choice for this App when it is still offered, else the first
inline int startRow(const std::vector<PackageEntry> &entries, const std::string &lastId) {
    if (!lastId.empty())
        for (size_t i = 0; i < entries.size(); i++)
            if (entries[i].id() == lastId)
                return static_cast<int>(i);
    return 0;
}

//******************
// firstLine
//******************
// the game's title, with its variant after it
inline std::string firstLine(const PackageEntry &entry) {
    return entry.game.variant.empty() ? entry.game.title : entry.game.title + " (" + entry.game.variant + ")";
}

//******************
// elide
//******************
// `text` cut (whole UTF-8 characters) to what fits `width`, with "..." in place of the rest; never wraps
inline std::string elide(const std::string &text, int width, const Measure &measure) {
    if (width <= 0 || measure(text) <= width)
        return text;
    const std::string dots = "...";
    std::string cut = text;
    while (!cut.empty()) {
        // drop the last character: its continuation bytes first
        do {
            cut.pop_back();
        } while (!cut.empty() && (static_cast<unsigned char>(cut.back()) & 0xC0) == 0x80);
        if (measure(cut + dots) <= width)
            return cut + dots;
    }
    return dots;
}

//******************
// secondLine
//******************
// "<package title> (<source label>) - <kind name>" on one line: when it is too wide the kind goes first, then the
// package title is elided (the source label stays)
inline std::string secondLine(const std::string &packageTitle, const std::string &sourceLabel,
                              const std::string &kindName, int width, const Measure &measure) {
    const std::string source = " (" + sourceLabel + ")";
    const std::string full = packageTitle + source + (kindName.empty() ? "" : " - " + kindName);
    if (width <= 0 || measure(full) <= width)
        return full;
    const std::string noKind = packageTitle + source;
    if (measure(noKind) <= width)
        return noKind;
    return elide(packageTitle, width - measure(source), measure) + source;
}

//******************
// runsWith
//******************
// the titles of the Apps (title, Uses=) whose Uses= names any of `kinds`, in the Apps' order, none twice
inline std::vector<std::string> runsWith(const std::vector<std::string> &kinds,
                                         const std::vector<std::pair<std::string, std::vector<std::string>>> &apps) {
    std::vector<std::string> titles;
    for (const auto &app : apps) {
        const bool runs = std::any_of(app.second.begin(), app.second.end(), [&](const std::string &use) {
            return std::find(kinds.begin(), kinds.end(), use) != kinds.end();
        });
        if (runs && std::find(titles.begin(), titles.end(), app.first) == titles.end())
            titles.push_back(app.first);
    }
    return titles;
}

} // namespace packagepicker
