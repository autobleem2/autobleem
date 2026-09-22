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

#include <cstdlib>
#include <cstring>
#include <string>

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
    case ShimEvent::Kind::ControllerButtonDown:
    case ShimEvent::Kind::ControllerButtonUp:
    case ShimEvent::Kind::ControllerAxisMotion:
    case ShimEvent::Kind::ControllerAdded:
        return false; // SDL 1.2 has no game controller API to raise these in
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
    case ShimEvent::Kind::ControllerButtonDown:
    case ShimEvent::Kind::ControllerButtonUp:
        out->cbutton.type = (event.kind == ShimEvent::Kind::ControllerButtonDown) ? sdl2::ControllerButtonDown
                                                                                  : sdl2::ControllerButtonUp;
        out->cbutton.which = event.pad;
        out->cbutton.button = static_cast<Uint8>(event.index);
        out->cbutton.state = static_cast<Uint8>(event.kind == ShimEvent::Kind::ControllerButtonDown ? 1 : 0);
        return true;
    case ShimEvent::Kind::ControllerAxisMotion:
        out->caxis.type = sdl2::ControllerAxisMotion;
        out->caxis.which = event.pad;
        out->caxis.axis = static_cast<Uint8>(event.index);
        out->caxis.value = static_cast<Sint16>(event.value);
        return true;
    case ShimEvent::Kind::ControllerAdded:
        out->cdevice.type = sdl2::ControllerDeviceAdded;
        out->cdevice.which = event.pad; // on ADDED this is a device index, and the indices are ours
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
// the game controller view of the same virtual pads
//*******************************
// An SDL2 app that uses this API never calls the joystick entry points, so without these it would
// reach past the shim to the real pad - a different pad from the one every other app is given, with
// a different layout and none of the profile remapping, keyboard mode or hotkey. So the same virtual
// pads are offered here too. This view needs no layout: SDL's controller model *is* the model the
// daemon publishes, so the answers come straight from it.
//
// Every function taking a handle has to be here, or the app would hand our pointer to the real SDL.
// Two are deliberately absent - SDL_GameControllerGetBindForButton and ...ForAxis return a struct by
// value whose ABI is not worth guessing, and nothing we have met calls them. Modern SDL rejects a
// handle it does not know rather than following it, so a miss is an error and not a crash.

int SDL_IsGameController(int index) {
    ShimState &state = shim();
    if (!state.active()) {
        using Fn = int (*)(int);
        Fn real = REAL("SDL_IsGameController", Fn);
        return real ? real(index) : 0;
    }
    return (index >= 0 && index < state.padCount()) ? 1 : 0;
}

const char *SDL_GameControllerNameForIndex(int index) {
    if (!shim().active()) {
        using Fn = const char *(*)(int);
        Fn real = REAL("SDL_GameControllerNameForIndex", Fn);
        return real ? real(index) : nullptr;
    }
    return (index >= 0 && index < shim().padCount()) ? virtualName() : nullptr;
}

void *SDL_GameControllerOpen(int index) {
    ShimState &state = shim();
    if (!state.active()) {
        using Fn = void *(*)(int);
        Fn real = REAL("SDL_GameControllerOpen", Fn);
        return real ? real(index) : nullptr;
    }
    if (index < 0 || index >= state.padCount()) {
        return nullptr;
    }
    state.update();
    g_joysticks[index].magic = FakeMagic;
    g_joysticks[index].index = index;
    g_opened[index] = true;
    state.log("abpad: the app opened controller %d", index);
    return &g_joysticks[index];
}

void SDL_GameControllerClose(void *handle) {
    int index = indexOf(handle);
    if (index < 0) {
        using Fn = void (*)(void *);
        Fn real = REAL("SDL_GameControllerClose", Fn);
        if (real) {
            real(handle);
        }
        return;
    }
    g_opened[index] = false;
}

const char *SDL_GameControllerName(void *handle) {
    return indexOf(handle) < 0 ? nullptr : virtualName();
}

int SDL_GameControllerGetAttached(void *handle) {
    return indexOf(handle) >= 0 ? 1 : 0;
}

void *SDL_GameControllerGetJoystick(void *handle) {
    // the joystick behind our controller is our joystick, so the two views stay one pad
    return indexOf(handle) < 0 ? nullptr : handle;
}

void *SDL_GameControllerFromInstanceID(int instance) {
    if (!shim().active()) {
        using Fn = void *(*)(int);
        Fn real = REAL("SDL_GameControllerFromInstanceID", Fn);
        return real ? real(instance) : nullptr;
    }
    if (instance < 0 || instance >= shim().padCount()) {
        return nullptr;
    }
    return &g_joysticks[instance];
}

Uint8 SDL_GameControllerGetButton(void *handle, int button) {
    int index = indexOf(handle);
    if (index < 0 || button < 0 || button >= ButtonElementCount) {
        return 0;
    }
    shim().update();
    return shim().controller(index).button(static_cast<Element>(button)) ? 1 : 0;
}

Sint16 SDL_GameControllerGetAxis(void *handle, int axis) {
    int index = indexOf(handle);
    if (index < 0 || axis < 0 || axis >= AxisElementCount) {
        return 0;
    }
    shim().update();
    return shim().controller(index).axis(static_cast<Element>(FirstAxisElement + axis));
}

void SDL_GameControllerUpdate() {
    shim().update();
}

int SDL_GameControllerEventState(int state) {
    return SDL_JoystickEventState(state);
}

// the caller frees this with SDL_free, so it has to come from SDL's own allocator
char *SDL_GameControllerMapping(void *handle) {
    if (indexOf(handle) < 0) {
        return nullptr;
    }
    std::string line = shim().layout().mapping.toLine();
    using Alloc = void *(*)(size_t);
    Alloc allocate = REAL("SDL_malloc", Alloc);
    char *copy = static_cast<char *>(allocate ? allocate(line.size() + 1) : malloc(line.size() + 1));
    if (copy) {
        memcpy(copy, line.c_str(), line.size() + 1);
    }
    return copy;
}

int SDL_GameControllerRumble(void *handle, Uint16 low, Uint16 high, Uint32 milliseconds) {
    return indexOf(handle) < 0 ? -1 : 0; // the virtual pad does not shake, and says so politely
}

// The rest of the handle-taking calls, which newer SDL added and newer apps use. EDuke32 asks
// SDL_GameControllerHasAxis and ...HasButton before it will read anything; unanswered, those go to
// the real SDL with our handle, which rightly does not know it, and the app concludes the pad has no
// controls at all. A pad that reads as empty is worse than no pad, so they are answered here.

int SDL_GameControllerHasAxis(void *handle, int axis) {
    return (indexOf(handle) >= 0 && axis >= 0 && axis < AxisElementCount) ? 1 : 0;
}

int SDL_GameControllerHasButton(void *handle, int button) {
    return (indexOf(handle) >= 0 && button >= 0 && button < ButtonElementCount) ? 1 : 0;
}

int SDL_GameControllerHasRumble(void *handle) {
    return 0; // nothing to shake
}

int SDL_GameControllerHasRumbleTriggers(void *handle) {
    return 0;
}

const char *SDL_GameControllerGetSerial(void *handle) {
    return nullptr; // a serial number the virtual pad does not have, said plainly
}

Uint16 SDL_GameControllerGetVendor(void *handle) {
    return indexOf(handle) < 0 ? 0 : 0x045e; // Microsoft, to match the pad we claim to be
}

Uint16 SDL_GameControllerGetProduct(void *handle) {
    return indexOf(handle) < 0 ? 0 : 0x028e; // the wired Xbox 360 pad
}

Uint16 SDL_GameControllerGetProductVersion(void *handle) {
    return indexOf(handle) < 0 ? 0 : 0x0110;
}

int SDL_GameControllerGetType(void *handle) {
    return indexOf(handle) < 0 ? 0 : 1; // SDL_CONTROLLER_TYPE_XBOX360
}

int SDL_GameControllerGetPlayerIndex(void *handle) {
    return indexOf(handle);
}

void SDL_GameControllerSetPlayerIndex(void *handle, int player) {
    // the player a pad is is the daemon's to decide, and it decided when it gave out the slots
}

int SDL_GameControllerSetLED(void *handle, Uint8 red, Uint8 green, Uint8 blue) {
    return indexOf(handle) < 0 ? -1 : 0;
}

int SDL_GameControllerRumbleTriggers(void *handle, Uint16 left, Uint16 right, Uint32 milliseconds) {
    return indexOf(handle) < 0 ? -1 : 0;
}

// and the joystick-side equivalents, for an app that asks the same questions of the other view
int SDL_JoystickHasRumble(void *handle) {
    return 0;
}

const char *SDL_JoystickGetSerial(void *handle) {
    return nullptr;
}

Uint16 SDL_JoystickGetVendor(void *handle) {
    return indexOf(handle) < 0 ? 0 : 0x045e;
}

Uint16 SDL_JoystickGetProduct(void *handle) {
    return indexOf(handle) < 0 ? 0 : 0x028e;
}

Uint16 SDL_JoystickGetProductVersion(void *handle) {
    return indexOf(handle) < 0 ? 0 : 0x0110;
}

int SDL_JoystickGetType(void *handle) {
    return indexOf(handle) < 0 ? 0 : 1; // SDL_JOYSTICK_TYPE_GAMECONTROLLER
}

int SDL_JoystickGetPlayerIndex(void *handle) {
    return indexOf(handle);
}

void SDL_JoystickSetPlayerIndex(void *handle, int player) {}

int SDL_JoystickRumble(void *handle, Uint16 low, Uint16 high, Uint32 milliseconds) {
    return indexOf(handle) < 0 ? -1 : 0;
}

int SDL_JoystickCurrentPowerLevel(void *handle) {
    return indexOf(handle) < 0 ? -1 : 5; // SDL_JOYSTICK_POWER_WIRED
}

//*******************************
// the mouse cursor
//*******************************
// A console and a Pi appliance have no mouse, so a cursor drawn over the game is simply wrong - but
// it is the *app* that asks for one, not the driver, and plenty of them do (SDLPoP turns one on with
// its menu). So the request is refused: the cursor is hidden when the window appears and every
// attempt to bring it back is answered by hiding it again.
//
// SDL 1.2 and SDL2 declare SDL_ShowCursor identically, so one function serves both; the window call
// they hang it on differs, and those are separate symbols, so both can be here without a conflict.

void hideCursorNow() {
    using Fn = int (*)(int);
    Fn real = REAL("SDL_ShowCursor", Fn);
    if (real) {
        real(0); // SDL_DISABLE, the same value in both
    }
}

int SDL_ShowCursor(int toggle) {
    using Fn = int (*)(int);
    Fn real = REAL("SDL_ShowCursor", Fn);
    if (shim().profile().hideCursor) {
        if (real) {
            real(0);
        }
        return 0; // and a query is told what is true: there is no cursor
    }
    return real ? real(toggle) : 0;
}

void *SDL_CreateWindow(const char *title, int x, int y, int w, int h, Uint32 flags) { // SDL2
    using Fn = void *(*)(const char *, int, int, int, int, Uint32);
    Fn real = REAL("SDL_CreateWindow", Fn);
    void *window = real ? real(title, x, y, w, h, flags) : nullptr;
    if (shim().profile().hideCursor) {
        hideCursorNow(); // some drivers show one the moment there is a window to show it over
    }
    return window;
}

void *SDL_SetVideoMode(int width, int height, int bpp, Uint32 flags) { // SDL 1.2
    using Fn = void *(*)(int, int, int, Uint32);
    Fn real = REAL("SDL_SetVideoMode", Fn);
    void *surface = real ? real(width, height, bpp, flags) : nullptr;
    if (shim().profile().hideCursor) {
        hideCursorNow();
    }
    return surface;
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
