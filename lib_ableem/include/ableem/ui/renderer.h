#pragma once

#include "types.h"

namespace ableem {

class Platform;
class Texture;

//******************
// Renderer
//******************
// Wraps the one SDL_Renderer the app uses. Owned by GuiBase; screens receive a reference.
class ABLEEM_API Renderer {
public:
    Renderer(const Renderer &) = delete;
    Renderer &operator=(const Renderer &) = delete;
    ~Renderer();

    void clear();
    void present();

    void setDrawColor(Color c);
    Color drawColor() const;
    void setBlendMode(BlendMode mode);

    void fillRect(const Rect &r);
    void fillRect(); // fills the entire current render target (screen or whatever setTarget() pointed at)
    // fills every rect in one draw call, all in the current draw color - far cheaper than looping fillRect()
    // for things like a starfield with hundreds of small rects on weak hardware.
    void fillRects(const Rect *rects, int count);
    void drawRect(const Rect &r);
    void drawLine(Point a, Point b);

    // src/dst nullptr means "whole texture" / "whole render target"
    void copy(const Texture &tex, const Rect *src = nullptr, const Rect *dst = nullptr);

    // nullptr switches back to rendering to the screen
    void setTarget(Texture *target);

    int width() const;
    int height() const;

private:
    friend class Platform;
    friend class GuiBase;
    friend class Texture;
    friend class Font;
    explicit Renderer(Platform &platform);
    // the display hand-off (GuiBase::releaseDisplay()/acquireDisplay()): destroy the SDL renderer while
    // keeping this object - everything holds a reference to it - and make a new one on the new window.
    // Every Texture and Font made with the old renderer must be gone before release(): SDL frees them with
    // the renderer, and a handle destroyed later would free them twice.
    void release();
    void recreate(Platform &platform);
    struct Impl;
    Impl *impl;

public:
    void *native() const; // internal use by Texture/Font implementations
};

} // namespace ableem
