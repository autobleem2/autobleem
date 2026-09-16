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
