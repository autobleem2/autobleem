//
// The disc view's pure rules (docs/disc-drive-plan.md, pieces 5 and 8): what the drive chip at the top of the screen
// shows for each state of the drive, when the disc view opens by itself, the R1 footer item, and the crossfade
// between the carousel and the disc's cover. No SDL, no Gui - tested alone; evoui_disc_view.cpp draws the result.
//
#pragma once

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <string>

namespace discview {

// what the drive and its disc are, as the chip sees them (DriveWatcher::State + DiscIdentity::Kind folded)
enum class Drive {
    NoDrive,
    Empty,
    Reading,
    ReadyGame,  // a PS1 disc, its identity read
    ReadyAudio, // an audio CD
    NotPs1,     // another disc
    Unreadable
};

enum class ChipLook {
    Hidden, // no drive: nothing at all
    Dim,    // the disc icon at low brightness
    Blink,  // the icon pulses
    Lit     // the icon at full brightness
};

enum class ChipText { None, Reading, Disc, NotPs1, Unreadable };

struct Chip {
    ChipLook look = ChipLook::Hidden;
    ChipText text = ChipText::None;
    bool operator==(const Chip &o) const { return look == o.look && text == o.text; }
};

// the chip for a drive state: no drive nothing; empty a dim icon; reading a blinking icon and "Reading the disc...";
// a disc ready a lit icon and "Disc: <title>" (also while the disc view shows - the lit chip marks the disc);
// another disc dim with "This is not a PlayStation disc"
inline Chip chipFor(Drive drive) {
    switch (drive) {
    case Drive::NoDrive:
        return {ChipLook::Hidden, ChipText::None};
    case Drive::Empty:
        return {ChipLook::Dim, ChipText::None};
    case Drive::Reading:
        return {ChipLook::Blink, ChipText::Reading};
    case Drive::ReadyGame:
    case Drive::ReadyAudio:
        return {ChipLook::Lit, ChipText::Disc};
    case Drive::NotPs1:
        return {ChipLook::Dim, ChipText::NotPs1};
    case Drive::Unreadable:
        return {ChipLook::Dim, ChipText::Unreadable};
    }
    return {};
}

inline bool isReady(Drive drive) {
    return drive == Drive::ReadyGame || drive == Drive::ReadyAudio;
}

// the blink: the icon's alpha (0..255) at `ms`, a triangle wave of PeriodMs between a dim and a full brightness
const int BlinkPeriodMs = 900;
const int DimAlpha = 90;
inline int blinkAlpha(long ms) {
    const int phase = static_cast<int>(((ms % BlinkPeriodMs) + BlinkPeriodMs) % BlinkPeriodMs);
    const int half = BlinkPeriodMs / 2;
    const int up = phase < half ? phase : BlinkPeriodMs - phase; // 0 .. half
    return DimAlpha + (255 - DimAlpha) * up / half;
}

inline int chipAlpha(ChipLook look, long ms) {
    switch (look) {
    case ChipLook::Hidden:
        return 0;
    case ChipLook::Dim:
        return DimAlpha;
    case ChipLook::Blink:
        return blinkAlpha(ms);
    case ChipLook::Lit:
        return 255;
    }
    return 0;
}

// Does the disc view open now? When a disc becomes ready (the watcher's change counter moved since the view last
// looked, and the disc is ready) and the view is not already open. After Circle the disc stays and the view stays
// closed until R1: the counter has not moved.
inline bool opensByItself(Drive drive, unsigned changes, unsigned seenChanges, bool viewOpen) {
    return isReady(drive) && changes != seenChanges && !viewOpen;
}

// Does the view close by itself? The disc was taken out (anything but ready) while it shows.
inline bool closesByItself(Drive drive, bool viewOpen) {
    return viewOpen && !isReady(drive);
}

// R1 on the carousel brings the disc view back, and is in the footer only while a disc is ready
inline bool r1ShowsDisc(Drive drive, bool viewOpen) {
    return isReady(drive) && !viewOpen;
}

// The crossfade between two pictures with a background between them: the first half the old picture is covered by
// the background (alpha 0 -> 255), the second half the background gives way to the new one (255 -> 0). The swap of
// what is under it happens at the middle.
const int FadeHalfMs = 220;

inline int fadeAlpha(long ms, int halfMs = FadeHalfMs) {
    if (ms <= 0)
        return 0;
    if (ms >= 2L * halfMs)
        return 0;
    return static_cast<int>(ms < halfMs ? 255L * ms / halfMs : 255L * (2L * halfMs - ms) / halfMs);
}

inline bool fadeSwapDue(long ms, bool alreadySwapped, int halfMs = FadeHalfMs) {
    return !alreadySwapped && ms >= halfMs;
}

inline bool fadeDone(long ms, int halfMs = FadeHalfMs) {
    return ms >= 2L * halfMs;
}

// The game the resume screen after a run is about, and whose save folder the chosen slot is written to: the disc that
// just played when there is one (the carousel was left on another game while the disc view held the disc alone), else
// the carousel's selected game. A null pointer type (shared_ptr, raw) means "none".
template <typename Ptr> Ptr resumeGameFor(const Ptr &lastDisc, const Ptr &carouselSelected) {
    return lastDisc ? lastDisc : carouselSelected;
}

// "47:12" - the audio CD's total time from its length in CD frames (1/75 s); minutes may pass 59
inline std::string totalTimeText(uint32_t frames) {
    const uint32_t seconds = frames / 75;
    char buf[24];
    snprintf(buf, sizeof buf, "%u:%02u", static_cast<unsigned>(seconds / 60), static_cast<unsigned>(seconds % 60));
    return buf;
}

} // namespace discview
