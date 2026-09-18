#pragma once

#include <string>
#include <functional>
#include "types.h"

namespace ableem {

//******************
// Platform
//******************
// Owns process-wide SDL setup (video/audio subsystems, the window). One instance lives inside GuiBase.
// Also carries the small pile of "which device am I running on" policy that used to be scattered #ifdefs:
// on a dev host (PC/Mac/Raspberry Pi debug build/Windows) the cursor is not grabbed, and the app is expected
// to translate keyboard input into pad events (see Input::setKeyboardAsPad).
class ABLEEM_API Platform {
public:
    Platform(const Platform &) = delete;
    Platform &operator=(const Platform &) = delete;
    ~Platform();

    // ticks in milliseconds since the platform was created (SDL_GetTicks)
    unsigned int ticks() const;
    void delay(unsigned int ms) const;

    // human readable "compiled against X, linked against Y" style string, for logging
    std::string versionString() const;

    // true when running on a development machine rather than the real target (console/RPi image).
    bool isDevHost() const;

    // hides the mouse cursor and grabs/relative-mode it. no-op on a dev host.
    void hideAndGrabCursor();

    // 0/1/2 forwarded to the SDL_HINT_RENDER_SCALE_QUALITY hint
    void setScaleQuality(int quality);

    // Give the display up and take it back. On a bare KMS/DRM system (a Raspberry Pi with no compositor)
    // the window *is* the DRM master and only one process can hold it: an emulator started while the
    // window exists cannot open the display at all. releaseDisplay() destroys the window and quits SDL's
    // video subsystem (which is what actually drops the DRM master); acquireDisplay() brings both back with
    // the same title and size. The Renderer must be released before and recreated after - GuiBase does
    // the whole sequence, see GuiBase::releaseDisplay()/acquireDisplay(). hasDisplay() tells which state
    // it is in. No-ops when already in the requested state.
    void releaseDisplay();
    void acquireDisplay();
    bool hasDisplay() const;

    // called by Input::poll() when the console power button or Esc is seen. the app is expected to show a
    // message and actually power off/exit; the library has no policy of its own here.
    void setPowerOffHandler(std::function<void()> handler);

    // calls SDL_Quit(). must run after every ableem object (this Platform included) has already been
    // destroyed - register it with atexit() from main(), before constructing the first GuiBase.
    static void shutdownSDL();

private:
    friend class GuiBase;
    // a window of outputWidth x outputHeight pixels showing a logicalWidth x logicalHeight canvas, with
    // multisampleSamples-x MSAA on its GL context when that is not 0 (see GuiBase)
    Platform(const std::string &windowTitle, int logicalWidth, int logicalHeight, int outputWidth, int outputHeight,
             int multisampleSamples);
    struct Impl;
    Impl *impl;

public:
    // the canvas the app draws on (see Renderer); the window may be bigger
    int logicalWidth() const;
    int logicalHeight() const;
    // the size of the display the window will go on, before any window exists (initialises SDL's video
    // subsystem to ask); {0, 0} if it cannot be told. What GuiBase's outputScale can be decided from.
    static Size desktopDisplaySize();
    // the MSAA the window actually got (0 when it was not asked for or the driver refused it)
    int multisampleSamples() const;

    // internal: used by Input to invoke the app's power-off handler and by Renderer/Texture/Font/Audio to
    // reach the underlying SDL objects without exposing them in a public header.
    void *nativeWindow() const;
    void invokePowerOffHandler() const;
};

} // namespace ableem
