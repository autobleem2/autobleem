//
// The CD player screen's pure rules (evoui_cd_player.cpp draws them): the times, the track grid's cursor and rows,
// how many segments of a bar are lit, the repeat cycle and the lines of text. No SDL, no Gui - tested alone.
//
#pragma once

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <string>

namespace cdplayer {

const int GridColumns = 10;
const uint32_t FramesPerSecond = 75; // a CD frame is 1/75 s
const uint32_t SeekStepFrames = 5 * FramesPerSecond;

// "4:01" for a number of CD frames (whole seconds, rounded down); minutes may pass 59 ("75:10")
inline std::string clockText(uint32_t frames) {
    const uint32_t seconds = frames / FramesPerSecond;
    char buf[24];
    snprintf(buf, sizeof buf, "%u:%02u", static_cast<unsigned>(seconds / 60), static_cast<unsigned>(seconds % 60));
    return buf;
}

// "04", the number on a track's key
inline std::string twoDigits(int number) {
    char buf[16];
    snprintf(buf, sizeof buf, "%02d", number);
    return buf;
}

// the focus (0-based index into the tracks) after a d-pad press: dx steps by one, dy by a row. Left/Right wrap
// from the first to the last track and back; Up/Down stop at the first and last row (a short last row is reached
// at its end).
inline int moveFocus(int focus, int dx, int dy, int count) {
    if (count <= 0)
        return 0;
    if (dx != 0)
        return ((focus + dx) % count + count) % count;
    if (dy > 0) {
        const int down = focus + GridColumns;
        if (down < count)
            return down;
        const int lastRow = (count - 1) / GridColumns;
        return focus / GridColumns < lastRow ? count - 1 : focus;
    }
    if (dy < 0)
        return focus >= GridColumns ? focus - GridColumns : focus;
    return focus;
}

// the first grid row to show so the focused one is visible, `visibleRows` rows fit
inline int firstRowFor(int focus, int firstRow, int visibleRows, int count) {
    const int rows = (std::max(count, 1) + GridColumns - 1) / GridColumns;
    const int row = focus / GridColumns;
    if (row < firstRow)
        firstRow = row;
    if (row >= firstRow + visibleRows)
        firstRow = row - visibleRows + 1;
    return std::max(0, std::min(firstRow, std::max(0, rows - visibleRows)));
}

// how many of `segments` a level of 0..1 lights: 0 only for a level of 0, else at least one
inline int litSegments(float level, int segments) {
    if (level <= 0.0f)
        return 0;
    const int n = static_cast<int>(level * static_cast<float>(segments) + 0.5f);
    return std::max(1, std::min(segments, n));
}

// Off -> Disc -> Track -> Off, the order Triangle steps through
enum class RepeatMode { Off, Track, Disc };
inline RepeatMode nextRepeat(RepeatMode m) {
    switch (m) {
    case RepeatMode::Off:
        return RepeatMode::Disc;
    case RepeatMode::Disc:
        return RepeatMode::Track;
    case RepeatMode::Track:
        return RepeatMode::Off;
    }
    return RepeatMode::Off;
}

// the seek target of L2/R2: `direction` +1/-1 steps of SeekStepFrames from `position`, never below 0
inline uint32_t seekTarget(uint32_t position, int direction) {
    if (direction < 0)
        return position > SeekStepFrames ? position - SeekStepFrames : 0;
    return position + SeekStepFrames;
}

} // namespace cdplayer
