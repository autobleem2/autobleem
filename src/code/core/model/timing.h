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

// GuiSplash::loop(): how long the fully-faded-in splash holds before fading back out, milliseconds
#define SplashHoldDuration (2 * TicksPerSecond)
// GuiLauncher: how long the launcher takes to fade in from black when it is first shown, milliseconds
#define LauncherFadeInDuration 300
