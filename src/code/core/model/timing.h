//
// Timing constants shared by the services and the screens.
//
#pragma once

// every tick and timeout in the app is in milliseconds - this is the conversion, not an SDL detail.
#define TicksPerSecond 1000
// how long a notification line stays up when no timeout is given, and the config.ini default for the same
// value. the two must agree, which is why they sit together.
#define DefaultShowingTimeout (2 * TicksPerSecond)
#define DefaultShowingTimeoutText "2"

// how often ScanService's watcher takes a fresh fingerprint of the games directory when nothing asked for a
// scan directly (milliseconds) - see ScanService::checkForChanges()
#define ScanWatchInterval (10 * TicksPerSecond)

// GuiSplash::loop(): how long the screen stays black after the window comes up before the splash fades in,
// milliseconds. A TV takes a moment to lock onto the freshly set HDMI mode at boot - on the Pi 400 the
// whole fade-in/hold went by before the picture appeared - so the splash waits for it.
#define SplashSettleDuration (1500)
// GuiSplash::loop(): how long the fully-faded-in splash holds before fading back out, milliseconds
#define SplashHoldDuration (2 * TicksPerSecond)
// GuiLauncher: how long the launcher takes to fade in from black when it is first shown, milliseconds
#define LauncherFadeInDuration 300

// GuiLauncher: one carousel step from a tap, milliseconds; from a held stick, after CarouselHoldDelay,
// the steps follow each other without a pause, each this long
#define CarouselScrollDuration 110
#define CarouselHeldScrollDuration 80
#define CarouselHoldDelay 300

// The curve the UI's animations follow: quick to leave, settling into the end - a cover, a menu or a
// panel then looks like it arrives rather than stopping dead. t is the fraction of the animation's time
// gone, 0..1; the result is the fraction of the way it is, 0..1. Not for a held stick, where the row is
// meant to move at one speed (Carousel::scrollLeft's `eased`).
inline float easeOutCubic(float t) {
    if (t <= 0.0f)
        return 0.0f;
    if (t >= 1.0f)
        return 1.0f;
    float u = 1.0f - t;
    return 1.0f - u * u * u;
}
