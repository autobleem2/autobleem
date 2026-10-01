//
// The game details section's layout (UIREV-35, design: autobleem-design themes/ab2.0.0/design/uirev35/README.md): a
// compact 3-row facts grid, 132 px high, then the icon row. Pure - which rows a game shows, with which text, and where
// the badges sit - so tests/screens/test_meta_layout holds it without a Gui; PsMeta::render draws the result.
//
#pragma once

#include <string>
#include <vector>

namespace MetaLayout {

// all positions are relative to the section's origin (x, y)
constexpr int Height = 132;   // the whole section
constexpr int TitleSize = 26; // bold, fitted down to TitleMinSize
constexpr int TitleMinSize = 12;
constexpr int RuleY = 34; // 1 px rule under the title
constexpr int RuleWidth = 470;
constexpr int GridY = 40; // first fact row
constexpr int RowPitch = 20;
constexpr int LabelSize = 12; // bold capitals
constexpr int LabelDrop = 3;  // the label sits this much lower than the value
constexpr int ValueX = 118;
constexpr int ValueSize = 16; // medium
constexpr int LabelWidth = ValueX - 4;
constexpr int IconRowY = 102;
constexpr int IconSize = 30;
constexpr int PlayersTextX = 36;
constexpr int DiscX = 128;
constexpr int DiscCountX = 164;
constexpr int BadgePitch = 38;
constexpr int BadgesRight = 470; // the last badge ends here
constexpr int MaxFacts = 3;

enum class Kind { Ps1, RetroArch, App };

// an App's description (its author text) has no label: it is not a publisher, and it is wrapped across the whole
// width from the grid's top down to the section's bottom (an App has no icon row)
constexpr int DescriptionWidth = RuleWidth;
constexpr int DescriptionLines = (Height - GridY) / RowPitch;

struct Fact {
    std::string label; // the English key (PUBLISHER, SERIAL, LAST PLAYED, CORE): translated when drawn; empty = none
    std::string value;
    bool wrapped = false; // the value is a description: wrapped to DescriptionWidth, at most DescriptionLines lines
};

inline bool knownYear(const std::string &year) {
    return !year.empty() && year != "0";
}

// the rows a game shows, in order; a field with nothing to show leaves its row out (no gap)
inline std::vector<Fact> facts(Kind kind, const std::string &publisher, const std::string &year,
                               const std::string &serial, const std::string &region, const std::string &lastPlayed,
                               const std::string &coreName, bool canShowLastPlayed) {
    std::vector<Fact> rows;
    if (kind == Kind::App) {
        // what an App's manifest gives is a line about it (who made it and what it is), not a publisher
        const std::string text = knownYear(year) ? (publisher.empty() ? year : publisher + ", " + year) : publisher;
        if (!text.empty())
            rows.push_back({"", text, true});
        return rows;
    }
    if (!publisher.empty())
        rows.push_back({"PUBLISHER", knownYear(year) ? publisher + ", " + year : publisher});
    else if (knownYear(year))
        rows.push_back({"PUBLISHER", year});
    if (kind == Kind::Ps1) {
        std::string serialLine = serial;
        if (!region.empty())
            serialLine += (serialLine.empty() ? "" : "  \xC2\xB7  ") + region;
        if (!serialLine.empty())
            rows.push_back({"SERIAL", serialLine});
        if (canShowLastPlayed && !lastPlayed.empty())
            rows.push_back({"LAST PLAYED", lastPlayed});
    } else if (kind == Kind::RetroArch && !coreName.empty()) {
        rows.push_back({"CORE", coreName});
    }
    if (static_cast<int>(rows.size()) > MaxFacts)
        rows.resize(MaxFacts);
    return rows;
}

// wrapped lines cut to `maxLines`: the lines past the last one are joined onto it (one space apart), so the caller
// elides that last line and the reader sees the text goes on
inline std::vector<std::string> capLines(std::vector<std::string> lines, int maxLines) {
    if (maxLines < 1 || static_cast<int>(lines.size()) <= maxLines)
        return lines;
    std::string last = lines[maxLines - 1];
    for (size_t i = static_cast<size_t>(maxLines); i < lines.size(); i++)
        last += " " + lines[i];
    lines.resize(static_cast<size_t>(maxLines));
    lines.back() = last;
    return lines;
}

// the x (from the section's x) of badge `index` of `count`: right-aligned, the last one ends at BadgesRight
inline int badgeX(int count, int index) {
    return BadgesRight - IconSize - (count - 1 - index) * BadgePitch;
}

} // namespace MetaLayout
