#include "ableem/ui/input.h"
#include "ableem/ui/platform.h"
#include "sdl_common.h"
#include "psc_event_filter.h"
#include <iostream>
#include <fstream>
#include <memory>
#include <ableem/engine/log.h>

namespace ableem {

namespace {

//******************
// translateKeyboardToPad
//******************
// development machines usually have no gamepad. rewrite a key event into the pad event the screens expect.
// the letters used here are not used by any screen (the on-screen keyboard navigates with arrows).
//   X = cross     O = circle    S = square    T = triangle
//   I J K L = d-pad up left down right         Space = Start    B = Select (Back)
//   Q = L1        E = R1        1 = L2        2 = R2
// returns true if the event was rewritten in place.
bool translateKeyboardToPad(SDL_Event &event) {
    if (event.type != SDL_KEYDOWN && event.type != SDL_KEYUP)
        return false;
    if (event.key.repeat)
        return false;
    bool down = (event.type == SDL_KEYDOWN);
    int button = -1;
    bool dpad = false;
    switch (event.key.keysym.sym) {
    case SDLK_x:
        button = SDL_BTN_CROSS;
        break;
    case SDLK_o:
        button = SDL_BTN_CIRCLE;
        break;
    case SDLK_s:
        button = SDL_BTN_SQUARE;
        break;
    case SDLK_t:
        button = SDL_BTN_TRIANGLE;
        break;
    case SDLK_SPACE:
        button = SDL_BTN_START;
        break;
    case SDLK_b:
        button = SDL_BTN_SELECT;
        break;
    case SDLK_q:
        button = SDL_BTN_L1;
        break;
    case SDLK_e:
        button = SDL_BTN_R1;
        break;
    case SDLK_1:
        button = SDL_BTN_L2;
        break;
    case SDLK_2:
        button = SDL_BTN_R2;
        break;
    case SDLK_i:
        button = SDL_BTN_DUP;
        dpad = true;
        break;
    case SDLK_k:
        button = SDL_BTN_DDOWN;
        dpad = true;
        break;
    case SDLK_j:
        button = SDL_BTN_DLEFT;
        dpad = true;
        break;
    case SDLK_l:
        button = SDL_BTN_DRIGHT;
        dpad = true;
        break;
    default:
        return false;
    }
    event.type = dpad ? (down ? SDL_CONTROLLERHATMOTIONDOWN : SDL_CONTROLLERHATMOTIONUP)
                      : (down ? SDL_CONTROLLERBUTTONDOWN : SDL_CONTROLLERBUTTONUP);
    event.cbutton.button = button;
    event.cbutton.state = down ? SDL_PRESSED : SDL_RELEASED;
    return true;
}

Button toButton(int b) {
    if (b == SDL_BTN_CROSS)
        return Button::Cross;
    if (b == SDL_BTN_CIRCLE)
        return Button::Circle;
    if (b == SDL_BTN_SQUARE)
        return Button::Square;
    if (b == SDL_BTN_TRIANGLE)
        return Button::Triangle;
    if (b == SDL_BTN_START)
        return Button::Start;
    if (b == SDL_BTN_SELECT)
        return Button::Select;
    if (b == SDL_BTN_L1)
        return Button::L1;
    if (b == SDL_BTN_R1)
        return Button::R1;
    if (b == SDL_BTN_L2)
        return Button::L2;
    if (b == SDL_BTN_R2)
        return Button::R2;
    return Button::None;
}

Button toDpadButton(int b) {
    if (b == SDL_BTN_DUP)
        return Button::DpadUp;
    if (b == SDL_BTN_DDOWN)
        return Button::DpadDown;
    if (b == SDL_BTN_DLEFT)
        return Button::DpadLeft;
    if (b == SDL_BTN_DRIGHT)
        return Button::DpadRight;
    return Button::None;
}

// the console's front buttons come in as keyboard scancodes with no keycode of their own
Key toKey(SDL_Scancode scancode, SDL_Keycode sym) {
    if (scancode == SDL_SCANCODE_SLEEP)
        return Key::Sleep;
    if (scancode == SDL_SCANCODE_AUDIOPLAY)
        return Key::Reset;
    if (scancode == SDL_SCANCODE_EJECT)
        return Key::Open;
    switch (sym) {
    case SDLK_ESCAPE:
        return Key::Escape;
    case SDLK_RETURN:
        return Key::Return;
    case SDLK_UP:
        return Key::Up;
    case SDLK_DOWN:
        return Key::Down;
    case SDLK_LEFT:
        return Key::Left;
    case SDLK_RIGHT:
        return Key::Right;
    case SDLK_PAGEUP:
        return Key::PageUp;
    case SDLK_PAGEDOWN:
        return Key::PageDown;
    case SDLK_HOME:
        return Key::Home;
    case SDLK_END:
        return Key::End;
    case SDLK_TAB:
        return Key::Tab;
    case SDLK_BACKSPACE:
        return Key::Backspace;
    case SDLK_DELETE:
        return Key::Delete;
    default:
        return Key::Other;
    }
}

bool fileExists(const std::string &path) {
    std::ifstream f(path);
    return f.good();
}

enum { DUP = 0, DDOWN = 1, DLEFT = 2, DRIGHT = 3 };

struct Pad {
    SDL_GameController *controller = nullptr;
    SDL_Joystick *joystick = nullptr;
    std::string name, guid;
    int index = 0;
};

} // namespace

struct Input::Impl {
    Platform &platform;
    bool keyboardAsPad;
    bool powerKeyAsKey = false;
    bool dpadState[4] = {false, false, false, false};
    std::vector<std::string> mappingPaths;
    std::string currentMappingPath;
    std::vector<std::unique_ptr<Pad>> pads;

    explicit Impl(Platform &p) : platform(p), keyboardAsPad(p.isDevHost()) {}

    void registerPad(int joystickIndex) {
        SDL_Joystick *js = SDL_JoystickOpen(joystickIndex);
        if (!js)
            return;
        SDL_JoystickGUID guid = SDL_JoystickGetGUID(js);
        char guidStr[64];
        SDL_JoystickGetGUIDString(guid, guidStr, sizeof(guidStr));
        if (!SDL_IsGameController(joystickIndex)) {
            SDL_JoystickClose(js);
            return;
        }
        SDL_GameController *controller = SDL_GameControllerOpen(joystickIndex);
        if (!controller)
            return;

        std::unique_ptr<Pad> pad = std::make_unique<Pad>();
        pad->controller = controller;
        pad->joystick = SDL_GameControllerGetJoystick(controller);
        pad->guid = guidStr;
        pad->name = SDL_GameControllerName(controller);
        pad->index = joystickIndex;
        PLOG_INFO << "New GameController: " << pad->name << " GUID: " << pad->guid;
        pads.push_back(std::move(pad));
    }

    void removePad(int joystickInstanceId) {
        for (size_t i = 0; i < pads.size(); i++) {
            if (SDL_JoystickInstanceID(pads[i]->joystick) == joystickInstanceId) {
                PLOG_INFO << "Pad disconnected: " << pads[i]->index << ":" << pads[i]->name;
                SDL_GameControllerClose(pads[i]->controller);
                pads.erase(pads.begin() + i);
                return;
            }
        }
    }
};

Input::Input(Platform &platform) : impl(new Impl(platform)) {
    reinstallEventFilter();
}

void Input::reinstallEventFilter() {
    SDL_SetEventFilter(&playstation_event_filter, nullptr);
}

Input::~Input() {
    flushPads();
    delete impl;
}

bool Input::poll(Event &out) {
    out = Event();
    SDL_Event e;
    if (!SDL_PollEvent(&e))
        return false;

    if (e.type == SDL_JOYDEVICEADDED) {
        impl->registerPad(e.jdevice.which);
        out.type = Event::Type::PadAdded;
        return true;
    }
    if (e.type == SDL_JOYDEVICEREMOVED) {
        impl->removePad(e.jdevice.which);
        out.type = Event::Type::PadRemoved;
        return true;
    }

    if (impl->keyboardAsPad) {
        translateKeyboardToPad(e); // mutates e in place; falls through to the normal handling below
    }

    if (e.type == SDL_QUIT) {
        out.type = Event::Type::Quit;
        return true;
    }

    if (e.type == SDL_KEYDOWN && (e.key.keysym.scancode == SDL_SCANCODE_SLEEP || e.key.keysym.sym == SDLK_ESCAPE)) {
        if (impl->powerKeyAsKey) {
            out.type = Event::Type::KeyDown;
            out.key = Key::Sleep;
            return true;
        }
        impl->platform.invokePowerOffHandler();
        return true; // swallowed: the app decides what powering off means, we just report it happened
    }

    if (e.type == SDL_RENDER_TARGETS_RESET || e.type == SDL_RENDER_DEVICE_RESET) {
        out.type = Event::Type::RenderReset;
        return true;
    }

    switch (e.type) {
    case SDL_KEYDOWN:
        out.type = Event::Type::KeyDown;
        out.key = toKey(e.key.keysym.scancode, e.key.keysym.sym);
        return true;
    case SDL_KEYUP:
        out.type = Event::Type::KeyUp;
        out.key = toKey(e.key.keysym.scancode, e.key.keysym.sym);
        return true;
    case SDL_TEXTINPUT:
        out.type = Event::Type::TextInput;
        out.text = e.text.text;
        return true;
    case SDL_CONTROLLERBUTTONDOWN:
        out.type = Event::Type::ButtonDown;
        out.button = toButton(e.cbutton.button);
        return true;
    case SDL_CONTROLLERBUTTONUP:
        out.type = Event::Type::ButtonUp;
        out.button = toButton(e.cbutton.button);
        return true;
    case SDL_CONTROLLERHATMOTIONDOWN:
        if (e.cbutton.button == SDL_BTN_DUP)
            impl->dpadState[DUP] = true;
        else if (e.cbutton.button == SDL_BTN_DDOWN)
            impl->dpadState[DDOWN] = true;
        else if (e.cbutton.button == SDL_BTN_DLEFT)
            impl->dpadState[DLEFT] = true;
        else if (e.cbutton.button == SDL_BTN_DRIGHT)
            impl->dpadState[DRIGHT] = true;
        out.type = Event::Type::DpadDown;
        out.button = toDpadButton(e.cbutton.button);
        return true;
    case SDL_CONTROLLERHATMOTIONUP:
        if (e.cbutton.button == SDL_BTN_DUP)
            impl->dpadState[DUP] = false;
        else if (e.cbutton.button == SDL_BTN_DDOWN)
            impl->dpadState[DDOWN] = false;
        else if (e.cbutton.button == SDL_BTN_DLEFT)
            impl->dpadState[DLEFT] = false;
        else if (e.cbutton.button == SDL_BTN_DRIGHT)
            impl->dpadState[DRIGHT] = false;
        out.type = Event::Type::DpadUp;
        out.button = toDpadButton(e.cbutton.button);
        return true;
    default:
        return true; // event consumed from the queue; nothing translatable in it
    }
}

void Input::flushEvents() {
    SDL_PumpEvents();
    SDL_FlushEvents(SDL_FIRSTEVENT, SDL_LASTEVENT);
}

bool Input::padEventPending() const {
    SDL_PumpEvents();
    SDL_Event e;
    int n = SDL_PeepEvents(&e, 1, SDL_PEEKEVENT, SDL_CONTROLLERAXISMOTION, SDL_CONTROLLERDEVICEREMAPPED);
    n += SDL_PeepEvents(&e, 1, SDL_PEEKEVENT, SDL_CONTROLLERHATMOTIONUP, SDL_CONTROLLERHATMOTIONDOWN);
    if (impl->keyboardAsPad) {
        // on a dev host the pad is the keyboard, so a key going up is the "another event" a screen's
        // fast-forward loop is waiting for; without this the loop never sees the release and repeats forever
        n += SDL_PeepEvents(&e, 1, SDL_PEEKEVENT, SDL_KEYDOWN, SDL_KEYUP);
    }
    return n > 0;
}

bool Input::dpadUp() const {
    return impl->dpadState[DUP];
}
bool Input::dpadDown() const {
    return impl->dpadState[DDOWN];
}
bool Input::dpadLeft() const {
    return impl->dpadState[DLEFT];
}
bool Input::dpadRight() const {
    return impl->dpadState[DRIGHT];
}
bool Input::dpadCentered() const {
    return !impl->dpadState[DUP] && !impl->dpadState[DDOWN] && !impl->dpadState[DLEFT] && !impl->dpadState[DRIGHT];
}

void Input::setKeyboardAsPad(bool enabled) {
    impl->keyboardAsPad = enabled;
}

void Input::setPowerKeyAsKey(bool enabled) {
    impl->powerKeyAsKey = enabled;
}

void Input::loadMappings(const std::vector<std::string> &gameControllerDbPaths) {
    impl->mappingPaths = gameControllerDbPaths;
}

std::string Input::currentMappingPath() const {
    return impl->currentMappingPath;
}

bool Input::addMapping(const std::string &line) {
    int result = SDL_GameControllerAddMapping(line.c_str());
    if (result < 0) {
        PLOG_WARNING << "SDL refused the pad mapping: " << SDL_GetError();
        return false;
    }
    return true;
}

std::string Input::mappingForDeviceIndex(int index) const {
    char *mapping = SDL_GameControllerMappingForDeviceIndex(index);
    if (!mapping)
        return "";
    std::string result = mapping;
    SDL_free(mapping);
    return result;
}

void Input::probePads() {
    SDL_InitSubSystem(SDL_INIT_JOYSTICK);
    SDL_InitSubSystem(SDL_INIT_GAMECONTROLLER);
    if (!impl->pads.empty()) {
        flushPads();
    }

    bool mappingsLoaded = false;
    for (const std::string &path : impl->mappingPaths) {
        if (fileExists(path)) {
            int loaded = SDL_GameControllerAddMappingsFromFile(path.c_str());
            PLOG_INFO << "Loaded pad mappings " << loaded << " from " << path;
            mappingsLoaded = true;
            impl->currentMappingPath = path;
            break;
        }
    }
    if (!mappingsLoaded) {
        PLOG_WARNING << "default mapping db in use - no gamecontrollerdb.txt file found";
    }

    std::string zeroGuid = "00000000000000000000000000000000";
    for (int i = 0; i < SDL_NumJoysticks(); ++i) {
        SDL_JoystickGUID guid = SDL_JoystickGetDeviceGUID(i);
        char guidStr[64];
        SDL_JoystickGetGUIDString(guid, guidStr, sizeof(guidStr));
        if (zeroGuid == guidStr) {
            continue; // invalid gamepad
        }
        if (SDL_IsGameController(i)) {
            impl->registerPad(i);
        }
    }
}

void Input::flushPads() {
    for (auto &pad : impl->pads) {
        SDL_GameControllerClose(pad->controller);
    }
    impl->pads.clear();
    SDL_QuitSubSystem(SDL_INIT_JOYSTICK);
    SDL_QuitSubSystem(SDL_INIT_GAMECONTROLLER);
}

int Input::activePadCount() const {
    return static_cast<int>(impl->pads.size());
}
int Input::joystickCount() const {
    return SDL_NumJoysticks();
}

std::vector<PadInfo> Input::pads() const {
    std::vector<PadInfo> result;
    for (const auto &pad : impl->pads) {
        result.push_back(PadInfo{pad->name, pad->guid, pad->index});
    }
    return result;
}

} // namespace ableem
