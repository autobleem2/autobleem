#include "ableem/ui/platform.h"
#include "sdl_common.h"
#include <ableem/engine/log.h>
#include <sstream>
#include <stdexcept>

namespace ableem {

struct Platform::Impl {
    SDL_Window *window = nullptr;
    std::function<void()> powerOffHandler;
    std::string windowTitle;   // kept for acquireDisplay()
    int width = 0, height = 0; // the window
    int logicalWidth = 0, logicalHeight = 0;
    int multisampleSamples = 0; // asked for, then what was got
    bool fullscreen = false;    // the whole desktop rather than a width x height window
};

namespace {
// The renderer SDL will pick is its GL one everywhere this runs (opengl on a PC, opengles2 on a Pi and,
// presumably, the console), and that renderer draws through the window's GL context - so asking for a
// multisampled context before the window is created gives every quad the renderer draws, the carousel's
// cover strips included, real MSAA edges. SDL_WINDOW_OPENGL makes the window come with that context at
// once instead of being recreated by the renderer later. A driver without MSAA fails the window, and the
// window is then made again without it.
SDL_Window *createWindow(const std::string &title, int w, int h, int &samples, bool fullscreen) {
    // the desktop's own mode, no modeset: what a launcher that hands the screen to an emulator and takes it
    // back wants (a mode change would flash the display twice per game)
    const Uint32 flags = fullscreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0;
    if (samples > 0) {
#ifdef _WIN32
        SDL_SetHint(SDL_HINT_RENDER_DRIVER, "opengl"); // Windows would otherwise take direct3d, which ignores this
#endif
        SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 1);
        SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, samples);
        SDL_Window *window = SDL_CreateWindow(title.c_str(), SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, w, h,
                                              SDL_WINDOW_OPENGL | flags);
        if (window)
            return window;
        PLOG_WARNING << "No " << samples << "x multisampled GL window (" << SDL_GetError() << ") - going without";
        SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 0);
        SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, 0);
        samples = 0;
    }
    return SDL_CreateWindow(title.c_str(), SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, w, h, flags);
}
} // namespace

int Platform::multisampleSamples() const {
    return impl->multisampleSamples;
}

Size Platform::desktopDisplaySize() {
    Size s;
    if (SDL_InitSubSystem(SDL_INIT_VIDEO) != 0)
        return s;
    SDL_DisplayMode mode;
    if (SDL_GetDesktopDisplayMode(0, &mode) == 0) {
        s.w = mode.w;
        s.h = mode.h;
    }
    return s;
}

int Platform::logicalWidth() const {
    return impl->logicalWidth;
}
int Platform::logicalHeight() const {
    return impl->logicalHeight;
}

Platform::Platform(const std::string &windowTitle, int logicalWidth, int logicalHeight, int outputWidth,
                   int outputHeight, int multisampleSamples, bool fullscreen)
    : impl(new Impl()) {
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        throw std::runtime_error(std::string("SDL_Init failed: ") + SDL_GetError());
    }
    SDL_InitSubSystem(SDL_INIT_AUDIO);
    SDL_InitSubSystem(SDL_INIT_GAMECONTROLLER);

    impl->windowTitle = windowTitle;
    impl->width = outputWidth;
    impl->height = outputHeight;
    impl->logicalWidth = logicalWidth;
    impl->logicalHeight = logicalHeight;
    impl->multisampleSamples = multisampleSamples;
    impl->fullscreen = fullscreen;
    impl->window = createWindow(windowTitle, outputWidth, outputHeight, impl->multisampleSamples, fullscreen);
    if (!impl->window) {
        throw std::runtime_error(std::string("SDL_CreateWindow failed: ") + SDL_GetError());
    }
    if (fullscreen) {
        int w = 0, h = 0;
        SDL_GetWindowSize(impl->window, &w, &h);
        PLOG_INFO << "Full-screen window: " << w << "x" << h;
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
    os << "compiled against SDL " << static_cast<int>(compiled.major) << "." << static_cast<int>(compiled.minor) << "."
       << static_cast<int>(compiled.patch) << ", linked against SDL " << static_cast<int>(linked.major) << "."
       << static_cast<int>(linked.minor) << "." << static_cast<int>(linked.patch);
    return os.str();
}

std::string Platform::linkedVersion() const {
    SDL_version linked;
    SDL_GetVersion(&linked);
    return std::to_string(linked.major) + "." + std::to_string(linked.minor) + "." + std::to_string(linked.patch);
}

std::string Platform::osName() {
    return SDL_GetPlatform();
}

std::string Platform::videoDriverName() const {
    const char *name = SDL_GetCurrentVideoDriver();
    return name ? name : "";
}

std::string Platform::displayModeString() const {
    if (!impl->window)
        return "";
    SDL_DisplayMode mode;
    if (SDL_GetWindowDisplayMode(impl->window, &mode) != 0)
        return "";
    std::string text = std::to_string(mode.w) + "x" + std::to_string(mode.h);
    if (mode.refresh_rate > 0)
        text += " @ " + std::to_string(mode.refresh_rate) + " Hz";
    return text;
}

bool Platform::isDevHost() const {
#ifdef ABLEEM_DEV_HOST
    return true;
#else
    return false;
#endif
}

void Platform::hideAndGrabCursor() {
    if (!impl->window)
        return;
    SDL_ShowCursor(SDL_DISABLE);
    SDL_SetWindowGrab(impl->window, SDL_TRUE);
    SDL_SetRelativeMouseMode(SDL_TRUE);
}

void Platform::releaseDisplay() {
    if (!impl->window)
        return;
    SDL_DestroyWindow(impl->window);
    impl->window = nullptr;
    // destroying the window is not enough on KMSDRM: the DRM device stays open (and this process its
    // master) until the video subsystem goes away
    SDL_QuitSubSystem(SDL_INIT_VIDEO);
}

void Platform::acquireDisplay() {
    if (impl->window)
        return;
    if (SDL_InitSubSystem(SDL_INIT_VIDEO) != 0) {
        throw std::runtime_error(std::string("SDL_InitSubSystem(VIDEO) failed: ") + SDL_GetError());
    }
    impl->window =
        createWindow(impl->windowTitle, impl->width, impl->height, impl->multisampleSamples, impl->fullscreen);
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

void Platform::raiseWindow() {
    if (!impl->window)
        return;
    SDL_RaiseWindow(impl->window);
}

void Platform::minimizeWindow() {
    if (!impl->window)
        return;
    SDL_MinimizeWindow(impl->window);
}

void Platform::restoreWindow() {
    if (!impl->window)
        return;
    SDL_RestoreWindow(impl->window);
    SDL_RaiseWindow(impl->window);
}

void Platform::hideWindow() {
    if (impl->window)
        SDL_HideWindow(impl->window);
}

void Platform::showWindow() {
    if (!impl->window)
        return;
    SDL_ShowWindow(impl->window);
    SDL_RaiseWindow(impl->window);
}

void Platform::setScaleQuality(int quality) {
    char buf[2] = {static_cast<char>('0' + quality), 0};
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
