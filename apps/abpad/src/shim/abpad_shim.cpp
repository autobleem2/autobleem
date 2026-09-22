// libabpad.so - what an App's SDL asks about joysticks, answered by us.
//
// Preloaded into the app by rc/app_env.sh. Every function here replaces the one of the same name in
// whatever SDL the app links, so the app sees the pad abpadd publishes, in a layout it understands,
// and never sees the real device at all.
//
// One library serves both SDLs. They export the *same names* for different things - SDL_JoystickName
// takes an index in SDL 1.2 and a handle in SDL2, and the event structures are unrelated - so each
// function asks ShimState::isSdl2() (which asks the loaded SDL, rather than guessing) and writes the
// answer in that ABI's words. That is why app_env.sh needs no detection of its own and why an App
// nobody has looked at yet is served correctly.
//
// What is deliberately *not* here:
//   - SDL_PeepEvents. Its SDL 1.2 and SDL2 signatures differ in arity, and no app we have met reads
//     its events that way. If one turns up, it wants the same treatment SDL_PollEvent gets.
//   - The SDL_GameController* family. An SDL2 app using it never calls the joystick entry points
//     (SDL2 reaches its own joystick code through the dynapi _REAL symbols inside libSDL2, which a
//     preload cannot intercept), and such an app needs nothing from us as long as SDL has a mapping
//     for the pad - which SDL_GAMECONTROLLERCONFIG gives it. See docs/virtual-gamepad-plan.md.

#include "shim/sdl_abi.h"
#include "shim/shim_state.h"

#include <cstring>

using namespace abpad;

namespace {

// What SDL_JoystickOpen hands back. The app only ever passes it to functions listed below, so it
// never needs to be anything SDL would recognise - but every function that takes a handle has to be
// interposed, or the app would hand this pointer to the real SDL.
struct FakeJoystick {
    uint32_t magic;
    int index;
};

constexpr uint32_t FakeMagic = 0x41425041u; // "ABPA"
FakeJoystick g_joysticks[MaxPads];
bool g_opened[MaxPads];

int indexOf(void *handle) {
    FakeJoystick *fake = static_cast<FakeJoystick *>(handle);
    if (!fake || fake->magic != FakeMagic) {
        return -1;
    }
    return fake->index;
}

ShimState &shim() {
    return ShimState::get();
}

// the name the app is told, which is the *virtual* pad's - an app that recognises pads by name is
// meant to recognise this one
const char *virtualName() {
    return shim().layout().name.c_str();
}

//*******************************
// writeEvent - one ShimEvent in the app's own SDL's words
//*******************************
bool writeSdl1(const ShimEvent &event, sdl1::Event *out) {
    memset(out, 0, sizeof(sdl1::Event));
    switch (event.kind) {
    case ShimEvent::Kind::ButtonDown:
    case ShimEvent::Kind::ButtonUp:
        out->jbutton.type =
            static_cast<Uint8>(event.kind == ShimEvent::Kind::ButtonDown ? sdl1::JoyButtonDown : sdl1::JoyButtonUp);
        out->jbutton.which = static_cast<Uint8>(event.pad);
        out->jbutton.button = static_cast<Uint8>(event.index);
        out->jbutton.state = static_cast<Uint8>(event.kind == ShimEvent::Kind::ButtonDown ? 1 : 0);
        return true;
    case ShimEvent::Kind::AxisMotion:
        out->jaxis.type = sdl1::JoyAxisMotion;
        out->jaxis.which = static_cast<Uint8>(event.pad);
        out->jaxis.axis = static_cast<Uint8>(event.index);
        out->jaxis.value = static_cast<Sint16>(event.value);
        return true;
    case ShimEvent::Kind::HatMotion:
        out->jhat.type = sdl1::JoyHatMotion;
        out->jhat.which = static_cast<Uint8>(event.pad);
        out->jhat.hat = static_cast<Uint8>(event.index);
        out->jhat.value = static_cast<Uint8>(event.value);
        return true;
    case ShimEvent::Kind::KeyDown:
    case ShimEvent::Kind::KeyUp:
        out->key.type = static_cast<Uint8>(event.kind == ShimEvent::Kind::KeyDown ? sdl1::KeyDown : sdl1::KeyUp);
        out->key.state = static_cast<Uint8>(event.kind == ShimEvent::Kind::KeyDown ? 1 : 0);
        out->key.keysym.sym = event.key.sdl1Sym;
        out->key.keysym.scancode = static_cast<Uint8>(event.key.sdl2Scancode);
        // a game that reads .unicode wants the character, and only for a key that has one
        out->key.keysym.unicode = (event.kind == ShimEvent::Kind::KeyDown && event.key.sdl1Sym < 128)
                                      ? static_cast<Uint16>(event.key.sdl1Sym)
                                      : 0;
        return true;
    case ShimEvent::Kind::Quit:
        out->quit.type = sdl1::Quit;
        return true;
    }
    return false;
}

bool writeSdl2(const ShimEvent &event, sdl2::Event *out) {
    memset(out, 0, sizeof(sdl2::Event));
    switch (event.kind) {
    case ShimEvent::Kind::ButtonDown:
    case ShimEvent::Kind::ButtonUp:
        out->jbutton.type = (event.kind == ShimEvent::Kind::ButtonDown) ? sdl2::JoyButtonDown : sdl2::JoyButtonUp;
        out->jbutton.which = event.pad;
        out->jbutton.button = static_cast<Uint8>(event.index);
        out->jbutton.state = static_cast<Uint8>(event.kind == ShimEvent::Kind::ButtonDown ? 1 : 0);
        return true;
    case ShimEvent::Kind::AxisMotion:
        out->jaxis.type = sdl2::JoyAxisMotion;
        out->jaxis.which = event.pad;
        out->jaxis.axis = static_cast<Uint8>(event.index);
        out->jaxis.value = static_cast<Sint16>(event.value);
        return true;
    case ShimEvent::Kind::HatMotion:
        out->jhat.type = sdl2::JoyHatMotion;
        out->jhat.which = event.pad;
        out->jhat.hat = static_cast<Uint8>(event.index);
        out->jhat.value = static_cast<Uint8>(event.value);
        return true;
    case ShimEvent::Kind::KeyDown:
    case ShimEvent::Kind::KeyUp:
        out->key.type = (event.kind == ShimEvent::Kind::KeyDown) ? sdl2::KeyDown : sdl2::KeyUp;
        out->key.state = static_cast<Uint8>(event.kind == ShimEvent::Kind::KeyDown ? 1 : 0);
        out->key.keysym.scancode = event.key.sdl2Scancode;
        out->key.keysym.sym = event.key.sdl2Keycode;
        return true;
    case ShimEvent::Kind::Quit:
        out->quit.type = sdl2::Quit;
        return true;
    }
    return false;
}

// true when the app's own SDL raised this about a real pad - which the app is not supposed to see
bool isRealJoystickEvent(const void *event) {
    if (shim().isSdl2()) {
        return sdl2::isJoystickEvent(static_cast<const sdl2::Event *>(event)->type);
    }
    return isSdl1JoystickEvent(static_cast<const sdl1::Event *>(event)->type);
}

} // namespace

extern "C" {

// the real function of the same name, further along the search order
#ifndef _WIN32
#include <dlfcn.h>
#define REAL(name, type) reinterpret_cast<type>(dlsym(RTLD_NEXT, name))
#else
#define REAL(name, type) static_cast<type>(nullptr)
#endif

//*******************************
// how many pads there are, and what they are called
//*******************************
int SDL_NumJoysticks() {
    ShimState &state = shim();
    if (!state.active()) {
        using Fn = int (*)();
        Fn real = REAL("SDL_NumJoysticks", Fn);
        return real ? real() : 0;
    }
    state.update();
    return state.padCount();
}

// SDL 1.2: SDL_JoystickName(int index). SDL2: SDL_JoystickName(SDL_Joystick *). One symbol, so the
// argument is taken as a machine word and read as whichever this process means.
const char *SDL_JoystickName(void *argument) {
    if (!shim().active()) {
        using Fn = const char *(*)(void *);
        Fn real = REAL("SDL_JoystickName", Fn);
        return real ? real(argument) : "";
    }
    return virtualName();
}

const char *SDL_JoystickNameForIndex(int index) { // SDL2 only
    if (!shim().active()) {
        using Fn = const char *(*)(int);
        Fn real = REAL("SDL_JoystickNameForIndex", Fn);
        return real ? real(index) : "";
    }
    return (index >= 0 && index < shim().padCount()) ? virtualName() : nullptr;
}

int SDL_JoystickIndex(void *handle) { // SDL 1.2 only
    int index = indexOf(handle);
    return index < 0 ? 0 : index;
}

int SDL_JoystickInstanceID(void *handle) { // SDL2 only: an id, and index serves as one
    int index = indexOf(handle);
    return index < 0 ? -1 : index;
}

int SDL_JoystickOpened(int index) { // SDL 1.2 only
    return (index >= 0 && index < MaxPads && g_opened[index]) ? 1 : 0;
}

int SDL_JoystickGetAttached(void *handle) { // SDL2: SDL_bool
    return indexOf(handle) >= 0 ? 1 : 0;
}

//*******************************
// opening and closing
//*******************************
void *SDL_JoystickOpen(int index) {
    ShimState &state = shim();
    if (!state.active()) {
        using Fn = void *(*)(int);
        Fn real = REAL("SDL_JoystickOpen", Fn);
        return real ? real(index) : nullptr;
    }
    if (index < 0 || index >= state.padCount()) {
        return nullptr;
    }
    state.update();
    g_joysticks[index].magic = FakeMagic;
    g_joysticks[index].index = index;
    g_opened[index] = true;
    state.log("abpad: the app opened joystick %d", index);
    return &g_joysticks[index];
}

void SDL_JoystickClose(void *handle) {
    int index = indexOf(handle);
    if (index < 0) {
        using Fn = void (*)(void *);
        Fn real = REAL("SDL_JoystickClose", Fn);
        if (real) {
            real(handle); // not ours: it must be a real one from before we were asked anything
        }
        return;
    }
    g_opened[index] = false;
}

//*******************************
// the pad's shape
//*******************************
int SDL_JoystickNumAxes(void *handle) {
    return indexOf(handle) < 0 ? 0 : shim().layout().axisCount;
}

int SDL_JoystickNumButtons(void *handle) {
    return indexOf(handle) < 0 ? 0 : shim().layout().buttonCount;
}

int SDL_JoystickNumHats(void *handle) {
    return indexOf(handle) < 0 ? 0 : shim().layout().hatCount;
}

int SDL_JoystickNumBalls(void *handle) {
    return 0; // no pad has had a ball since the 1990s, and no layout offers one
}

//*******************************
// reading it
//*******************************
void SDL_JoystickUpdate() {
    shim().update();
}

Sint16 SDL_JoystickGetAxis(void *handle, int axis) {
    int index = indexOf(handle);
    if (index < 0) {
        return 0;
    }
    shim().update(); // a game that polls rather than reads events still gets fresh values
    return shim().raw(index).axis(axis);
}

Uint8 SDL_JoystickGetButton(void *handle, int button) {
    int index = indexOf(handle);
    if (index < 0) {
        return 0;
    }
    shim().update();
    return shim().raw(index).button(button) ? 1 : 0;
}

Uint8 SDL_JoystickGetHat(void *handle, int hat) {
    int index = indexOf(handle);
    if (index < 0) {
        return 0;
    }
    shim().update();
    return shim().raw(index).hat(hat);
}

int SDL_JoystickGetBall(void *handle, int ball, int *dx, int *dy) {
    if (dx) {
        *dx = 0;
    }
    if (dy) {
        *dy = 0;
    }
    return indexOf(handle) < 0 ? -1 : 0;
}

int SDL_JoystickEventState(int state) {
    constexpr int Query = -1;
    ShimState &shimState = shim();
    if (state != Query) {
        shimState.setWantsEvents(state != 0);
    }
    // the app's own SDL is told the same, so it stops raising events about the real pad
    using Fn = int (*)(int);
    Fn real = REAL("SDL_JoystickEventState", Fn);
    if (real) {
        real(state);
    }
    return shimState.wantsEvents() ? 1 : 0;
}

//*******************************
// events
//*******************************
// The app's own events come through first - it still has a keyboard, a window and a clock - with the
// real pad's dropped on the way past, and ours are added when there is nothing else to say.
int SDL_PollEvent(void *event) {
    using Fn = int (*)(void *);
    Fn real = REAL("SDL_PollEvent", Fn);
    ShimState &state = shim();

    if (!state.active()) {
        return real ? real(event) : 0;
    }
    state.update();

    while (real && real(event)) {
        if (!isRealJoystickEvent(event)) {
            return 1;
        }
    }
    if (!event || !state.wantsEvents()) {
        return 0;
    }

    ShimEvent ours;
    if (!state.nextEvent(ours)) {
        return 0;
    }
    return state.isSdl2() ? (writeSdl2(ours, static_cast<sdl2::Event *>(event)) ? 1 : 0)
                          : (writeSdl1(ours, static_cast<sdl1::Event *>(event)) ? 1 : 0);
}

int SDL_WaitEvent(void *event) {
    using Delay = void (*)(Uint32);
    Delay delay = REAL("SDL_Delay", Delay);
    for (;;) {
        if (SDL_PollEvent(event)) {
            return 1;
        }
        if (delay) {
            delay(5); // the pad is polled by us, so waiting has to be a sleep, not a block
        } else {
            return 0;
        }
    }
}

} // extern "C"
