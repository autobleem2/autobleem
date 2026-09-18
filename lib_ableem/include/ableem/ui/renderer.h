#pragma once

#include "types.h"

namespace ableem {

class Platform;
class Texture;

//******************
// Renderer
//******************
// Wraps the one SDL_Renderer the app uses. Owned by GuiBase; screens receive a reference.
//
// Coordinates are logical: the app draws on a GuiBase::ScreenWidth x ScreenHeight canvas whatever the window
// is. The window may be bigger by `outputScale()` (a 1920x1080 window for a 1280x720 canvas is 1.5), and
// every drawing call here maps logical to output pixels, so nothing in the app changes with the window. What
// gets sharper at a scale above 1: textures loaded from files (drawn from more of their pixels), Fonts (loaded
// bigger, see Font::load) and render targets (Texture::createTarget allocates output pixels). At scale 1 the
// mapping is the identity on integers.
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

    // Pseudo-3D: draws `src` (nullptr = the whole texture) into the trapezoid whose vertical sides are `left`
    // and `right` - what a rectangle standing in 3D and turned about its vertical axis looks like on screen.
    // Each side's height is taken as its depth cue (the taller side is the nearer one), and the texture's
    // columns are spread across the width perspective-correctly, so a cover turned 60 degrees does not
    // "swim". `tint` multiplies the texture's colours (the caller's shading), white leaves them alone.
    // With SDL 2.0.18 or newer the whole thing is one SDL_RenderGeometry call - two vertices per output
    // column, the tint in the vertex colours; older SDLs (the console's) get one SDL copy per column, to a
    // fraction of a pixel from 2.0.10 on. Either way a multisampled window (GuiBase's multisampleSamples)
    // smooths the sloping edges. With left.x > right.x the back of the rectangle is showing and the
    // texture is drawn mirrored.
    void copyTrapezoid(const Texture &tex, const Rect *src, VerticalEdge left, VerticalEdge right,
                       Color tint = Color());

    // nullptr switches back to rendering to the screen
    void setTarget(Texture *target);

    // the logical canvas, in the app's coordinates
    int width() const;
    int height() const;
    // output pixels per logical pixel (1 unless the window is bigger than the canvas)
    float outputScale() const;

    // Frame statistics, on when AB_FRAME_STATS is in the environment (read once, at construction): every
    // 5 s a PLOG_INFO line with the frame time (average and worst), how many frames took over 20 ms, and
    // the copies and texture switches per frame. copies() is what present() will report for the frame in
    // progress; Font adds its glyphs through countCopies() since it draws past this class.
    static bool statsEnabled();
    void countCopies(int n);
    // a logical rect in output pixels, edges rounded so that neighbouring rects still tile
    Rect toOutput(const Rect &r) const;

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
