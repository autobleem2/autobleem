#include "shim/shim_state.h"

#include <cstdarg>
#include <cstdlib>
#include <cstring>

#ifndef _WIN32
#include <dlfcn.h>
#endif

using namespace std;

namespace abpad {

// update() runs once per frame of whatever the app is doing, so this is a count of frames rather
// than a duration - about a second and a half at anything from 30 fps up, and longer on a game that
// crawls, which is the right way round: a struggling game should be harder to quit by accident.
constexpr unsigned HotkeyHoldCycles = 60;

//*******************************
// ShimState::get
//*******************************
ShimState &ShimState::get() {
    // a function-local static, not a global: a global's constructor would run while the process is
    // still being wired up, before we know whether this app will ask us anything at all
    static ShimState instance;
    return instance;
}

//*******************************
// ShimState::ShimState
//*******************************
ShimState::ShimState() {
    const char *logPath = getenv("AB_PAD_LOG");
    if (logPath && *logPath) {
        log_ = fopen(logPath, "a");
    } else if (getenv("AB_PAD_VERBOSE")) {
        log_ = stderr;
    }

    loadProfile();
    layout_ = &virtualLayout(profile_.virtualPad);
    for (int i = 0; i < ElementCount; ++i) {
        string name = profile_.keyFor(static_cast<Element>(i));
        if (!name.empty()) {
            keys_[i] = keyCodeFromName(name);
            if (!keys_[i].valid()) {
                log("abpad: %s is bound to \"%s\", which is not a key name I know",
                    elementName(static_cast<Element>(i)), name.c_str());
            }
        }
    }

    active_ = profile_.mode != PadMode::Off;
    shmPath_ = defaultShmPath();
    if (!active_) {
        log("abpad: mode is off - standing aside");
        return;
    }

    for (RawPadState &raw : raw_) {
        raw = buildRawState(*layout_, ControllerState());
    }
    log("abpad: %s, %d pad(s) as \"%s\", movement %s, from %s", padModeName(profile_.mode), profile_.players,
        layout_->name.c_str(), movementAidName(profile_.movement), shmPath_.c_str());
}

//*******************************
// ShimState::loadProfile
//*******************************
void ShimState::loadProfile() {
    const char *defaults = getenv("AB_PAD_DEFAULTS");
    if (defaults && *defaults) {
        profile_.loadFile(defaults);
    }
    const char *path = getenv("AB_PAD_PROFILE");
    if (path && *path) {
        profile_.loadFile(path); // the app's own, over the defaults
    }
    // one env var so a profile can be overridden from a shell without editing anything - which is how
    // a person on a console with no editor tries "does joystick mode fix it?"
    const char *mode = getenv("AB_PAD_MODE");
    if (mode && *mode) {
        profile_.mode = padModeFromName(mode);
    }
    const char *virtualPad = getenv("AB_PAD_VIRTUAL");
    if (virtualPad && *virtualPad) {
        profile_.virtualPad = virtualPadKindFromName(virtualPad);
    }
    const char *movement = getenv("AB_PAD_MOVEMENT");
    if (movement && *movement) {
        profile_.movement = movementAidFromName(movement);
    }
    const char *players = getenv("AB_PAD_PLAYERS");
    if (players && *players) {
        int count = atoi(players);
        profile_.players = (count < 1) ? 1 : (count > MaxPads ? MaxPads : count);
    }
}

//*******************************
// ShimState::isSdl2
//*******************************
bool ShimState::isSdl2() {
    if (sdl2_ < 0) {
#ifdef _WIN32
        sdl2_ = 1;
#else
        // The loaded SDL is asked, not guessed - but asked the right way round. Looking for a
        // GameController symbol would be wrong on a system where libSDL-1.2.so.0 is sdl12-compat,
        // which implements the SDL 1.2 API *on top of* SDL2: both libraries are then in the process,
        // the SDL2 symbol is found, and an SDL 1.2 app gets answered in SDL2's event structures.
        // So the question is "is there an SDL 1.2 here?" instead: SDL_SetVideoMode exists only in
        // SDL 1.2 (and in sdl12-compat, which is the point), and an app that has one is an app
        // speaking that ABI whatever else is loaded beside it.
        sdl2_ = dlsym(RTLD_NEXT, "SDL_SetVideoMode") != nullptr ? 0 : 1;
#endif
        log("abpad: the app's SDL is %s", sdl2_ ? "SDL2" : "SDL 1.2");
    }
    return sdl2_ == 1;
}

//*******************************
// ShimState::log
//*******************************
void ShimState::log(const char *format, ...) {
    if (!log_) {
        return;
    }
    va_list args;
    va_start(args, format);
    vfprintf(log_, format, args);
    va_end(args);
    fputc('\n', log_);
    fflush(log_);
}

//*******************************
// ShimState::diff - what changed between two raw states, as events
//*******************************
void ShimState::diff(int pad, const RawPadState &before, const RawPadState &after) {
    for (size_t i = 0; i < after.buttons.size(); ++i) {
        if (before.button(static_cast<int>(i)) != after.buttons[i]) {
            ShimEvent event;
            event.kind = after.buttons[i] ? ShimEvent::Kind::ButtonDown : ShimEvent::Kind::ButtonUp;
            event.pad = pad;
            event.index = static_cast<int>(i);
            events_.push_back(event);
        }
    }
    for (size_t i = 0; i < after.axes.size(); ++i) {
        if (before.axis(static_cast<int>(i)) != after.axes[i]) {
            ShimEvent event;
            event.kind = ShimEvent::Kind::AxisMotion;
            event.pad = pad;
            event.index = static_cast<int>(i);
            event.value = after.axes[i];
            events_.push_back(event);
        }
    }
    for (size_t i = 0; i < after.hats.size(); ++i) {
        if (before.hat(static_cast<int>(i)) != after.hats[i]) {
            ShimEvent event;
            event.kind = ShimEvent::Kind::HatMotion;
            event.pad = pad;
            event.index = static_cast<int>(i);
            event.value = after.hats[i];
            events_.push_back(event);
        }
    }
}

//*******************************
// ShimState::diffController - the same change, in the game controller view
//*******************************
// SDL raises both a joystick event and a controller event for one press on a pad it has a mapping
// for, so the shim does too: an app that reads either API sees what it expects, and an app that
// happens to read both is not surprised by one of them being silent.
void ShimState::diffController(int pad, const ControllerState &before, const ControllerState &after) {
    for (int i = 0; i < ButtonElementCount; ++i) {
        Element element = static_cast<Element>(i);
        if (before.button(element) != after.button(element)) {
            ShimEvent event;
            event.kind =
                after.button(element) ? ShimEvent::Kind::ControllerButtonDown : ShimEvent::Kind::ControllerButtonUp;
            event.pad = pad;
            event.index = i; // Element's order is SDL_CONTROLLER_BUTTON_*'s order
            events_.push_back(event);
        }
    }
    for (int i = 0; i < AxisElementCount; ++i) {
        Element element = static_cast<Element>(FirstAxisElement + i);
        if (before.axis(element) != after.axis(element)) {
            ShimEvent event;
            event.kind = ShimEvent::Kind::ControllerAxisMotion;
            event.pad = pad;
            event.index = i;
            event.value = after.axis(element);
            events_.push_back(event);
        }
    }
}

//*******************************
// ShimState::update
//*******************************
void ShimState::update() {
    if (!active_) {
        return;
    }
    if (!shared_) {
        if (attemptsLeft_ == 0) {
            return;
        }
        --attemptsLeft_;
        if (!block_.openReadOnly(shmPath_, sizeof(SharedState))) {
            return; // abpadd may still be starting; the app carries on with a pad that reads neutral
        }
        shared_ = static_cast<const SharedState *>(block_.data());
        log("abpad: reading %s", shmPath_.c_str());
    }

    Snapshot snapshot;
    if (!readSnapshot(*shared_, snapshot)) {
        return; // a torn or foreign block: keep the last good state rather than jerk the pad
    }

    // an app that waits for a device event before opening anything has to be told the pads are here
    if (!announcedPads_ && eventsEnabled_) {
        for (int pad = 0; pad < profile_.players; ++pad) {
            ShimEvent event;
            event.kind = ShimEvent::Kind::ControllerAdded;
            event.pad = pad;
            events_.push_back(event);
        }
        announcedPads_ = true;
    }

    for (int pad = 0; pad < profile_.players; ++pad) {
        // a pad that is not plugged in reads centred and unpressed, rather than disappearing
        ControllerState controller;
        if (pad < snapshot.padCount && snapshot.connected[pad]) {
            controller = snapshot.pads[pad];
        }
        applyMovementAid(controller, profile_.movement);
        ControllerState previous = controller_[pad];
        controller_[pad] = controller;

        RawPadState after = buildRawState(*layout_, controller);
        if (eventsEnabled_) {
            if (profile_.wantsJoystick()) {
                diff(pad, raw_[pad], after);
                diffController(pad, previous, controller);
            }
            if (profile_.wantsKeyboard()) {
                for (int i = 0; i < ElementCount; ++i) {
                    if (!keys_[i].valid()) {
                        continue;
                    }
                    Element element = static_cast<Element>(i);
                    bool was = lastHeld_[pad][i];
                    bool now = controller.held(element);
                    if (was != now) {
                        ShimEvent event;
                        event.kind = now ? ShimEvent::Kind::KeyDown : ShimEvent::Kind::KeyUp;
                        event.pad = pad;
                        event.key = keys_[i];
                        events_.push_back(event);
                    }
                    lastHeld_[pad][i] = now;
                }
            }
        }
        raw_[pad] = after;
    }

    // The hotkey on any pad, held rather than pressed. Several of these apps have no way out, and
    // the ones that do put it behind a menu that a pad cannot always reach - but Start and Select are
    // live buttons in most games, so a moment's overlap must not end the game. Held long enough, the
    // app is *asked* to quit; keep holding and abpadd, which knows the app's pid, ends it whether it
    // agreed or not. One gesture, escalating, so a person who just wants out holds until it goes.
    bool held = false;
    for (int pad = 0; pad < profile_.players; ++pad) {
        held = held || hotkeyHeld(profile_.hotkey, controller_[pad]);
    }
    if (!held) {
        hotkeyHeldCycles_ = 0;
        hotkeyQuitSent_ = false;
    } else if (++hotkeyHeldCycles_ >= HotkeyHoldCycles && !hotkeyQuitSent_) {
        log("abpad: the hotkey was held - asking the app to quit");
        ShimEvent event;
        event.kind = ShimEvent::Kind::Quit;
        events_.push_back(event);
        hotkeyQuitSent_ = true;
    }

    // The console's Reset button, which abpadd reads (the app never sees it as anything it knows): the
    // same request, at once - a single press is the way out, as it is in pcsx. abpadd ends the app
    // itself if it does not go.
    if (!quitBaselineTaken_) {
        quitRequestsSeen_ = snapshot.quitRequests;
        quitBaselineTaken_ = true;
    } else if (snapshot.quitRequests != quitRequestsSeen_) {
        quitRequestsSeen_ = snapshot.quitRequests;
        log("abpad: Reset was pressed - asking the app to quit");
        ShimEvent event;
        event.kind = ShimEvent::Kind::Quit;
        events_.push_back(event);
    }

    // an app that never reads its events must not grow a queue for ever
    while (events_.size() > 256) {
        events_.pop_front();
    }
}

//*******************************
// ShimState::raw / nextEvent
//*******************************
const RawPadState &ShimState::raw(int pad) {
    static const RawPadState nothing;
    if (pad < 0 || pad >= MaxPads) {
        return nothing;
    }
    return raw_[pad];
}

const ControllerState &ShimState::controller(int pad) {
    static const ControllerState nothing;
    if (pad < 0 || pad >= MaxPads) {
        return nothing;
    }
    return controller_[pad];
}

bool ShimState::nextEvent(ShimEvent &out) {
    if (events_.empty()) {
        return false;
    }
    out = events_.front();
    events_.pop_front();
    return true;
}

} // namespace abpad
