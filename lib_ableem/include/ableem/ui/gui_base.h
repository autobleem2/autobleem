#pragma once

#include <memory>
#include "platform.h"
#include "renderer.h"
#include "input.h"
#include "audio.h"

namespace ableem {

//******************
// GuiBase
//******************
// Base class for the app's single top-level Gui object. Constructs and owns the platform/renderer/input/audio
// stack in that order (and destroys them in reverse order). The app derives from this and adds everything
// that is theme/config/database related - lib_ableem knows nothing about themes, ini files or game data.
class ABLEEM_API GuiBase {
public:
    static constexpr int ScreenWidth = 1280;
    static constexpr int ScreenHeight = 720;

    explicit GuiBase(const std::string &windowTitle = "AutoBleem", int width = ScreenWidth, int height = ScreenHeight);
    virtual ~GuiBase();

    Platform &platform() { return *platform_; }
    Renderer &renderer() { return *renderer_; }
    Input &input() { return *input_; }
    Audio &audio() { return audio_; }

    // Hand the display to another program and take it back afterwards - see Platform::releaseDisplay().
    // The caller must have dropped every Texture and Font before releaseDisplay() (they die with the
    // renderer) and reloads them after acquireDisplay(). The Platform, Renderer and Input objects stay,
    // so references to them held all over the app remain valid.
    void releaseDisplay();
    void acquireDisplay();

private:
    // order matters: platform must outlive renderer, both must outlive input/audio use of them
    std::unique_ptr<Platform> platform_;
    std::unique_ptr<Renderer> renderer_;
    std::unique_ptr<Input> input_;
    Audio audio_;
};

} // namespace ableem
