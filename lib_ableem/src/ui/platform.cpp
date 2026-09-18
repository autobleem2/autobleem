#include "ableem/ui/platform.h"
#include "sdl_common.h"
#include <sstream>
#include <stdexcept>

namespace ableem {

struct Platform::Impl {
    SDL_Window *window = nullptr;
    std::function<void()> powerOffHandler;
    std::string windowTitle;   // kept for acquireDisplay()
    int width = 0, height = 0;
};

Platform::Platform(const std::string &windowTitle, int width, int height) : impl(new Impl()) {
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        throw std::runtime_error(std::string("SDL_Init failed: ") + SDL_GetError());
    }
    SDL_InitSubSystem(SDL_INIT_AUDIO);
    SDL_InitSubSystem(SDL_INIT_GAMECONTROLLER);

    impl->windowTitle = windowTitle;
    impl->width = width;
    impl->height = height;
    impl->window = SDL_CreateWindow(windowTitle.c_str(), SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
                                     width, height, 0);
    if (!impl->window) {
        throw std::runtime_error(std::string("SDL_CreateWindow failed: ") + SDL_GetError());
    }

#ifndef ABLEEM_DEV_HOST
    hideAndGrabCursor();
#endif

    TTF_Init();
    Mix_Init(0);
}

Platform::~Platform() {
    Mix_Quit();
    TTF_Quit();
    if (impl->window) {
        SDL_DestroyWindow(impl->window);
    }
    delete impl;
    // SDL_Quit() is intentionally not called here: on some platforms (and in the app's own main()) it must
    // run after every other SDL-backed object (textures, fonts...) has already been destroyed. Call
    // SDL_Quit() yourself (e.g. via atexit) once the whole GuiBase - and everything built from it - is gone.
}

unsigned int Platform::ticks() const {
    return SDL_GetTicks();
}

void Platform::delay(unsigned int ms) const {
    SDL_Delay(ms);
}

std::string Platform::versionString() const {
    SDL_version compiled;
    SDL_version linked;
    SDL_VERSION(&compiled);
    SDL_GetVersion(&linked);
    std::ostringstream os;
    os << "compiled against SDL " << (int)compiled.major << "." << (int)compiled.minor << "." << (int)compiled.patch
       << ", linked against SDL " << (int)linked.major << "." << (int)linked.minor << "." << (int)linked.patch;
    return os.str();
}

bool Platform::isDevHost() const {
#ifdef ABLEEM_DEV_HOST
    return true;
#else
    return false;
#endif
}

void Platform::hideAndGrabCursor() {
    if (!impl->window) return;
    SDL_ShowCursor(SDL_DISABLE);
    SDL_SetWindowGrab(impl->window, SDL_TRUE);
    SDL_SetRelativeMouseMode(SDL_TRUE);
}

void Platform::releaseDisplay() {
    if (!impl->window) return;
    SDL_DestroyWindow(impl->window);
    impl->window = nullptr;
    // destroying the window is not enough on KMSDRM: the DRM device stays open (and this process its
    // master) until the video subsystem goes away
    SDL_QuitSubSystem(SDL_INIT_VIDEO);
}

void Platform::acquireDisplay() {
    if (impl->window) return;
    if (SDL_InitSubSystem(SDL_INIT_VIDEO) != 0) {
        throw std::runtime_error(std::string("SDL_InitSubSystem(VIDEO) failed: ") + SDL_GetError());
    }
    impl->window = SDL_CreateWindow(impl->windowTitle.c_str(), SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
                                     impl->width, impl->height, 0);
    if (!impl->window) {
        throw std::runtime_error(std::string("SDL_CreateWindow failed: ") + SDL_GetError());
    }
#ifndef ABLEEM_DEV_HOST
    hideAndGrabCursor();
#endif
}

bool Platform::hasDisplay() const {
    return impl->window != nullptr;
}

void Platform::setScaleQuality(int quality) {
    char buf[2] = { static_cast<char>('0' + quality), 0 };
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, buf);
}

void Platform::setPowerOffHandler(std::function<void()> handler) {
    impl->powerOffHandler = std::move(handler);
}

void *Platform::nativeWindow() const {
    return impl->window;
}

void Platform::invokePowerOffHandler() const {
    if (impl->powerOffHandler) {
        impl->powerOffHandler();
    }
}

void Platform::shutdownSDL() {
    SDL_Quit();
}

} // namespace ableem
