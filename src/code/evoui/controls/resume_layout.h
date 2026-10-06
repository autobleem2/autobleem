//
// ResumeLayout (UIREV-37): the numbers and the small pure rules of the resume-slot screen (PsStateSelector) - where the
// four cards, their picture wells and texts go on the 1280 x 720 canvas, which slot is the NEWEST, how the picture's
// cut corners are clipped, and the date's format. tests/screens/test_resume_layout holds them without a Gui;
// PsStateSelector::render draws with them. The design: autobleem-design themes/ab2.0.0/design/uirev37/README.md.
//
#pragma once

#include <ctime>
#include <string>

namespace ResumeLayout {

constexpr int SlotCount = 4;

constexpr int BandY = 100;
constexpr int BandH = 484; // ends at 584, clear of launcher.logo at y 591
constexpr int TitleMidY = 138;
constexpr int NameMidY = 172;

constexpr int CardW = 276;
constexpr int CardH = 300;
constexpr int CardGap = 22;
constexpr int CardX0 = 55;
constexpr int CardY = 212;

constexpr int WellInset = 14; // the well: card + (14, 14), 248 x 186
constexpr int WellW = 248;
constexpr int WellH = 186;
constexpr int PictureInset = 16; // the picture: card + (16, 16), 244 x 182
constexpr int PictureW = 244;
constexpr int PictureH = 182;
constexpr int CutCorner = 6; // the well's cut corners: top right and bottom left

constexpr int SlotNameY = 218; // card + (16, 218) top
constexpr int DateY = 254;     // card + (16, 254) top
constexpr int TextInset = 16;

constexpr int ChipRight = 260; // the NEWEST chip's right edge, card + 260
constexpr int ChipY = 222;     // card + 222
constexpr int ChipHeight = 24;
constexpr int ChipPadding = 7;

struct Box {
    int x, y, w, h;
};

// The 4:3 (CRT) design on the 640x480 canvas: the same four cards in a row, narrower - each its picture over "Slot n"
// (and the NEWEST chip) and the date under it, which wraps to a second line where it must. The numbers are the wide
// design's, fitted: the picture keeps its 4:3 shape (128 x 96), the corners are cut 4 px.
namespace Narrow {
constexpr int BandY = 58;
constexpr int BandH = 356; // ends at 414, above the hint bar (418): covers the menu caption and its line beneath, as at 720p
constexpr int TitleMidY = 82;
constexpr int NameMidY = 108;

constexpr int CardW = 148;
constexpr int CardH = 210;
constexpr int CardGap = 8;
constexpr int CardX0 = 12;
constexpr int CardY = 140;

constexpr int WellInset = 8; // the well: card + (8, 8), 132 x 100
constexpr int WellW = 132;
constexpr int WellH = 100;
constexpr int PictureInset = 10; // the picture: card + (10, 10), 128 x 96
constexpr int PictureW = 128;
constexpr int PictureH = 96;
constexpr int CutCorner = 4;

constexpr int SlotNameY = 118; // card + (10, 118) top
constexpr int DateY = 148;     // card + (10, 148) top; two lines of 20
constexpr int TextInset = 10;
constexpr int DateRoom = 132; // the width the date wraps to

constexpr int ChipRight = 140; // card + 140
constexpr int ChipY = 121;     // card + 121
constexpr int ChipHeight = 18;
constexpr int ChipPadding = 5;
} // namespace Narrow

inline Box cardBox(int slot, bool narrow = false) {
    if (narrow)
        return {Narrow::CardX0 + slot * (Narrow::CardW + Narrow::CardGap), Narrow::CardY, Narrow::CardW, Narrow::CardH};
    return {CardX0 + slot * (CardW + CardGap), CardY, CardW, CardH};
}
inline Box wellBox(const Box &card, bool narrow = false) {
    if (narrow)
        return {card.x + Narrow::WellInset, card.y + Narrow::WellInset, Narrow::WellW, Narrow::WellH};
    return {card.x + WellInset, card.y + WellInset, WellW, WellH};
}
inline Box pictureBox(const Box &card, bool narrow = false) {
    if (narrow)
        return {card.x + Narrow::PictureInset, card.y + Narrow::PictureInset, Narrow::PictureW, Narrow::PictureH};
    return {card.x + PictureInset, card.y + PictureInset, PictureW, PictureH};
}
// the chip around a word `wordW` wide, its right edge on the card's
inline Box chipBox(const Box &card, int wordW, bool narrow = false) {
    if (narrow) {
        const int w = wordW + 2 * Narrow::ChipPadding;
        return {card.x + Narrow::ChipRight - w, card.y + Narrow::ChipY, w, Narrow::ChipHeight};
    }
    const int w = wordW + 2 * ChipPadding;
    return {card.x + ChipRight - w, card.y + ChipY, w, ChipHeight};
}

// the slot with the latest time, when two or more slots are in use - else -1. `times[i]` is the slot's file time (0 =
// unknown), `used[i]` whether the slot holds a state. A tie goes to the lower slot; a slot without a known time never
// wins.
inline int newestSlot(const time_t (&times)[SlotCount], const bool (&used)[SlotCount]) {
    int usedCount = 0;
    int best = -1;
    for (int i = 0; i < SlotCount; i++) {
        if (!used[i])
            continue;
        usedCount++;
        if (times[i] > 0 && (best < 0 || times[i] > times[best]))
            best = i;
    }
    return usedCount >= 2 ? best : -1;
}

// the picture is clipped to the well's shape: how many pixels row `row` (0 = top) of a picture `h` tall loses on the
// right (the top corner) and on the left (the bottom corner), the corner cut diagonally `cut` px
struct RowInset {
    int left = 0;
    int right = 0;
};
inline RowInset rowInset(int row, int h, int cut = CutCorner) {
    RowInset in;
    if (row >= 0 && row < cut)
        in.right = cut - row;
    const int fromBottom = h - 1 - row;
    if (fromBottom >= 0 && fromBottom < cut)
        in.left = cut - fromBottom;
    return in;
}

// the date's strftime format: the user's own (config.ini "datetimeformat") when there is one, else dd.mm.yyyy hh:mm
inline std::string dateFormat(const std::string &userFormat) {
    return userFormat.empty() ? "%d.%m.%Y   %H:%M" : userFormat;
}

} // namespace ResumeLayout
