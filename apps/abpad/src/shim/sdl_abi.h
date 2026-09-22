#ifndef ABPAD_SDL_ABI_H
#define ABPAD_SDL_ABI_H

// The two SDLs, as much of them as the shim has to speak.
//
// Nothing here is included from an SDL header on purpose: the shim is loaded into a process whose SDL
// we did not build and cannot link against, and the two SDLs use the *same symbol names* for
// different things, so one set of headers could not describe both anyway. What is written out below
// is public ABI - the event type numbers and the event struct layouts - which neither SDL has changed
// since 1.2.0 and 2.0.0 respectively and neither can change without breaking every program built
// against it. The structs are spelled with the same field types in the same order as SDL spells them,
// so the compiler lays them out identically for this target.

#include <cstdint>

namespace abpad {

using Uint8 = uint8_t;
using Sint16 = int16_t;
using Uint16 = uint16_t;
using Sint32 = int32_t;
using Uint32 = uint32_t;

// SDL_INIT_JOYSTICK, the same bit in both
constexpr Uint32 SdlInitJoystick = 0x00000200u;

//*******************************
// SDL 1.2
//*******************************
namespace sdl1 {

enum EventType {
    KeyDown = 2,
    KeyUp = 3,
    JoyAxisMotion = 7,
    JoyBallMotion = 8,
    JoyHatMotion = 9,
    JoyButtonDown = 10,
    JoyButtonUp = 11,
    Quit = 12,
};

struct Keysym {
    Uint8 scancode;
    int sym; // SDLKey, an enum
    int mod; // SDLMod, an enum
    Uint16 unicode;
};

struct KeyboardEvent {
    Uint8 type;
    Uint8 which;
    Uint8 state;
    Keysym keysym;
};

struct JoyAxisEvent {
    Uint8 type;
    Uint8 which;
    Uint8 axis;
    Sint16 value;
};

struct JoyButtonEvent {
    Uint8 type;
    Uint8 which;
    Uint8 button;
    Uint8 state;
};

struct JoyHatEvent {
    Uint8 type;
    Uint8 which;
    Uint8 hat;
    Uint8 value;
};

struct QuitEvent {
    Uint8 type;
};

union Event {
    Uint8 type;
    KeyboardEvent key;
    JoyAxisEvent jaxis;
    JoyButtonEvent jbutton;
    JoyHatEvent jhat;
    QuitEvent quit;
    Uint8 padding[32];
};

} // namespace sdl1

//*******************************
// SDL2
//*******************************
namespace sdl2 {

enum EventType {
    Quit = 0x100,
    KeyDown = 0x300,
    KeyUp = 0x301,
    JoyAxisMotion = 0x600,
    JoyBallMotion = 0x601,
    JoyHatMotion = 0x602,
    JoyButtonDown = 0x603,
    JoyButtonUp = 0x604,
    JoyDeviceAdded = 0x605,
    JoyDeviceRemoved = 0x606,
    ControllerAxisMotion = 0x650,
    ControllerButtonDown = 0x651,
    ControllerButtonUp = 0x652,
    ControllerDeviceAdded = 0x653,
    ControllerDeviceRemoved = 0x654,
    ControllerDeviceRemapped = 0x655,
};

struct Keysym {
    Sint32 scancode; // SDL_Scancode
    Sint32 sym;      // SDL_Keycode
    Uint16 mod;
    Uint32 unused;
};

struct KeyboardEvent {
    Uint32 type;
    Uint32 timestamp;
    Uint32 windowID;
    Uint8 state;
    Uint8 repeat;
    Uint8 padding2;
    Uint8 padding3;
    Keysym keysym;
};

struct JoyAxisEvent {
    Uint32 type;
    Uint32 timestamp;
    Sint32 which;
    Uint8 axis;
    Uint8 padding1;
    Uint8 padding2;
    Uint8 padding3;
    Sint16 value;
    Uint16 padding4;
};

struct JoyButtonEvent {
    Uint32 type;
    Uint32 timestamp;
    Sint32 which;
    Uint8 button;
    Uint8 state;
    Uint8 padding1;
    Uint8 padding2;
};

struct JoyHatEvent {
    Uint32 type;
    Uint32 timestamp;
    Sint32 which;
    Uint8 hat;
    Uint8 value;
    Uint8 padding1;
    Uint8 padding2;
};

struct ControllerAxisEvent {
    Uint32 type;
    Uint32 timestamp;
    Sint32 which;
    Uint8 axis;
    Uint8 padding1;
    Uint8 padding2;
    Uint8 padding3;
    Sint16 value;
    Uint16 padding4;
};

struct ControllerButtonEvent {
    Uint32 type;
    Uint32 timestamp;
    Sint32 which;
    Uint8 button;
    Uint8 state;
    Uint8 padding1;
    Uint8 padding2;
};

struct ControllerDeviceEvent {
    Uint32 type;
    Uint32 timestamp;
    Sint32 which;
};

struct JoyDeviceEvent {
    Uint32 type;
    Uint32 timestamp;
    Sint32 which;
};

struct QuitEvent {
    Uint32 type;
    Uint32 timestamp;
};

union Event {
    Uint32 type;
    KeyboardEvent key;
    JoyAxisEvent jaxis;
    JoyButtonEvent jbutton;
    JoyHatEvent jhat;
    JoyDeviceEvent jdevice;
    ControllerAxisEvent caxis;
    ControllerButtonEvent cbutton;
    ControllerDeviceEvent cdevice;
    QuitEvent quit;
    Uint8 padding[56];
};

// true for an event the app's own SDL raised about a real joystick or controller - the shim swallows
// those, because the pad they speak of is one the app is not supposed to be able to see
inline bool isJoystickEvent(Uint32 type) {
    return (type >= JoyAxisMotion && type <= JoyDeviceRemoved) ||
           (type >= ControllerAxisMotion && type <= ControllerDeviceRemapped);
}

} // namespace sdl2

inline bool isSdl1JoystickEvent(Uint8 type) {
    return type >= sdl1::JoyAxisMotion && type <= sdl1::JoyButtonUp;
}

} // namespace abpad

#endif
