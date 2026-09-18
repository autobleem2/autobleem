#include "ableem/ui/joystick.h"
#include "sdl_common.h"

namespace ableem {

struct Joystick::Impl {
    SDL_Joystick *joystick = nullptr;
    SDL_GameController *controller = nullptr;
    int index = -1;
    std::string guid;
    JoystickState state;
    ControllerState controllerState;
};

namespace {
std::string guidString(SDL_JoystickGUID guid) {
    char text[64];
    SDL_JoystickGetGUIDString(guid, text, sizeof(text));
    return text;
}
} // namespace

Joystick::Joystick() : impl(new Impl) {}

Joystick::~Joystick() {
    close();
}

int Joystick::count() {
    return SDL_NumJoysticks();
}

std::string Joystick::nameForIndex(int index) {
    const char *name = SDL_JoystickNameForIndex(index);
    return name ? name : "";
}

std::string Joystick::guidForIndex(int index) {
    return guidString(SDL_JoystickGetDeviceGUID(index));
}

bool Joystick::isGameControllerAtIndex(int index) {
    return SDL_IsGameController(index) == SDL_TRUE;
}

std::string Joystick::controllerNameForIndex(int index) {
    const char *name = SDL_GameControllerNameForIndex(index);
    return name ? name : "";
}

bool Joystick::open(int index) {
    close();
    if (index < 0 || index >= SDL_NumJoysticks())
        return false;
    impl->joystick = SDL_JoystickOpen(index);
    if (!impl->joystick)
        return false;
    impl->index = index;
    impl->guid = guidString(SDL_JoystickGetDeviceGUID(index));
    if (SDL_IsGameController(index))
        impl->controller = SDL_GameControllerOpen(index);
    impl->state.axes.assign(SDL_JoystickNumAxes(impl->joystick), 0);
    impl->state.buttons.assign(SDL_JoystickNumButtons(impl->joystick), false);
    impl->state.hats.assign(SDL_JoystickNumHats(impl->joystick), HatCentered);
    impl->controllerState = ControllerState();
    return true;
}

void Joystick::close() {
    if (impl->controller) {
        SDL_GameControllerClose(impl->controller);
        impl->controller = nullptr;
    }
    if (impl->joystick) {
        SDL_JoystickClose(impl->joystick);
        impl->joystick = nullptr;
    }
    impl->index = -1;
    impl->guid.clear();
    impl->state = JoystickState();
    impl->controllerState = ControllerState();
}

bool Joystick::isOpen() const {
    return impl->joystick != nullptr;
}

int Joystick::index() const {
    return impl->index;
}

std::string Joystick::name() const {
    if (!impl->joystick)
        return "";
    const char *name = SDL_JoystickName(impl->joystick);
    return name ? name : "";
}

std::string Joystick::guid() const {
    return impl->guid;
}

bool Joystick::isGameController() const {
    return impl->controller != nullptr;
}

void Joystick::update() {
    if (!impl->joystick)
        return;
    SDL_PumpEvents();
    SDL_JoystickUpdate();
    if (!SDL_JoystickGetAttached(impl->joystick))
        return;
    JoystickState &s = impl->state;
    s.axes.resize(SDL_JoystickNumAxes(impl->joystick));
    s.buttons.resize(SDL_JoystickNumButtons(impl->joystick));
    s.hats.resize(SDL_JoystickNumHats(impl->joystick));
    for (size_t i = 0; i < s.axes.size(); i++)
        s.axes[i] = SDL_JoystickGetAxis(impl->joystick, static_cast<int>(i));
    for (size_t i = 0; i < s.buttons.size(); i++)
        s.buttons[i] = SDL_JoystickGetButton(impl->joystick, static_cast<int>(i)) != 0;
    for (size_t i = 0; i < s.hats.size(); i++)
        s.hats[i] = SDL_JoystickGetHat(impl->joystick, static_cast<int>(i));
    if (impl->controller) {
        SDL_GameControllerUpdate();
        for (int i = 0; i < 15; i++)
            impl->controllerState.buttons[i] =
                SDL_GameControllerGetButton(impl->controller, static_cast<SDL_GameControllerButton>(i)) != 0;
        for (int i = 0; i < 6; i++)
            impl->controllerState.axes[i] =
                SDL_GameControllerGetAxis(impl->controller, static_cast<SDL_GameControllerAxis>(i));
    }
}

const JoystickState &Joystick::state() const {
    return impl->state;
}

const ControllerState &Joystick::controllerState() const {
    return impl->controllerState;
}

} // namespace ableem
