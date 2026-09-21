#pragma once

#include <memory>
#include <string>
#include "types.h"

namespace ableem {

class Renderer;

//******************
// PixelLock
//******************
// RAII lock on a streaming texture's pixel buffer (always RGBA8888, one uint32 per pixel). Unlocks (and
// uploads the pixels) when it goes out of scope. Replaces the SDL_LockTexture/SDL_AllocFormat/SDL_MapRGBA/
// SDL_GetRGBA dance that engine/cardedit.cpp used to do by hand.
class ABLEEM_API PixelLock {
public:
    PixelLock(PixelLock &&other) noexcept;
    PixelLock &operator=(PixelLock &&) = delete;
    PixelLock(const PixelLock &) = delete;
    ~PixelLock();

    int width() const { return w; }
    int height() const { return h; }

    Color get(int x, int y) const;
    void set(int x, int y, Color c);

private:
    friend class Texture;
    PixelLock(void *texture, int w, int h);
    void *tex;
    unsigned int *pixels = nullptr;
    int pitchPixels = 0;
    int w = 0, h = 0;
};

//******************
// Texture
//******************
// Cheap shared handle (copy freely); the underlying GPU texture is released when the last copy is destroyed.
class ABLEEM_API Texture {
public:
    Texture();
    Texture(const Texture &) = default;
    Texture &operator=(const Texture &) = default;

    // an empty path gives an invalid Texture at once, no error logged - "no picture" is a normal answer
    static Texture loadFile(Renderer &renderer, const std::string &path);
    // the bounding box of the image file's non-transparent pixels (alpha > 0), in its own pixels - what a
    // layout aligns to when the art sits inside a transparent margin; the whole image when it cannot be
    // read or has no alpha
    static Rect opaqueBounds(const std::string &path);
    // decodes an in-memory image (e.g. a cover PNG blob read from a database)
    static Texture loadMemory(Renderer &renderer, const void *data, unsigned int size);
    // a render target texture (used for the carousel cover compositing)
    static Texture createTarget(Renderer &renderer, int w, int h);
    // a texture whose pixels can be locked and written directly (used by the memory card editor)
    static Texture createStreaming(Renderer &renderer, int w, int h);

    bool valid() const;
    // in logical pixels for a render target (what createTarget was asked for), in its own for anything else
    Size size() const;
    // texture pixels per logical pixel: the renderer's output scale for a render target, 1 for everything
    // else - Renderer::copy scales a source rect by it, so a target is addressed like the screen
    float pixelScale() const { return pixelScale_; }

    void setBlendMode(BlendMode mode);
    void setColorMod(Color c);
    void setAlphaMod(unsigned char a);

    PixelLock lock();

private:
    friend class Renderer;
    explicit Texture(void *nativeTexture);
    std::shared_ptr<void> handle;
    float pixelScale_ = 1.0f;

public:
    void *native() const { return handle.get(); }
};

} // namespace ableem
