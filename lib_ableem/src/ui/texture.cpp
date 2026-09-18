#include "ableem/ui/texture.h"
#include "ableem/ui/renderer.h"
#include "sdl_common.h"
#include <iostream>
#include <ableem/engine/log.h>

namespace ableem {

namespace {
void destroyTexture(void *t) {
    if (t)
        SDL_DestroyTexture(static_cast<SDL_Texture *>(t));
}
// RGBA8888 pixel format, allocated once and reused (matches the original engine/cardedit.cpp behavior).
SDL_PixelFormat *rgba8888Format() {
    static SDL_PixelFormat *fmt = SDL_AllocFormat(SDL_PIXELFORMAT_RGBA8888);
    return fmt;
}
} // namespace

Texture::Texture() : handle(nullptr) {}

Texture::Texture(void *nativeTexture) : handle(nativeTexture, destroyTexture) {}

Texture Texture::loadFile(Renderer &renderer, const std::string &path) {
    SDL_Texture *t = IMG_LoadTexture(static_cast<SDL_Renderer *>(renderer.native()), path.c_str());
    if (!t) {
        PLOG_ERROR << "Could not load texture: " << path << " (" << IMG_GetError() << ")";
    }
    return Texture(t);
}

Texture Texture::loadMemory(Renderer &renderer, const void *data, unsigned int size) {
    SDL_RWops *rw = SDL_RWFromConstMem(data, static_cast<int>(size));
    // freesrc=1: IMG_LoadTexture_RW closes/frees the RWops for us
    SDL_Texture *t = IMG_LoadTexture_RW(static_cast<SDL_Renderer *>(renderer.native()), rw, 1);
    if (!t) {
        PLOG_ERROR << "Could not load texture from memory (" << IMG_GetError() << ")";
    }
    return Texture(t);
}

Texture Texture::createTarget(Renderer &renderer, int w, int h) {
    SDL_Texture *t = SDL_CreateTexture(static_cast<SDL_Renderer *>(renderer.native()), SDL_PIXELFORMAT_RGBA8888,
                                       SDL_TEXTUREACCESS_TARGET, w, h);
    return Texture(t);
}

Texture Texture::createStreaming(Renderer &renderer, int w, int h) {
    SDL_Texture *t = SDL_CreateTexture(static_cast<SDL_Renderer *>(renderer.native()), SDL_PIXELFORMAT_RGBA8888,
                                       SDL_TEXTUREACCESS_STREAMING, w, h);
    return Texture(t);
}

bool Texture::valid() const {
    return handle != nullptr;
}

Size Texture::size() const {
    Size s;
    if (handle) {
        SDL_QueryTexture(static_cast<SDL_Texture *>(handle.get()), nullptr, nullptr, &s.w, &s.h);
    }
    return s;
}

void Texture::setBlendMode(BlendMode mode) {
    if (!handle)
        return;
    SDL_BlendMode m = SDL_BLENDMODE_BLEND;
    switch (mode) {
    case BlendMode::None:
        m = SDL_BLENDMODE_NONE;
        break;
    case BlendMode::Add:
        m = SDL_BLENDMODE_ADD;
        break;
    case BlendMode::Mod:
        m = SDL_BLENDMODE_MOD;
        break;
    case BlendMode::Blend:
    default:
        m = SDL_BLENDMODE_BLEND;
        break;
    }
    SDL_SetTextureBlendMode(static_cast<SDL_Texture *>(handle.get()), m);
}

void Texture::setColorMod(Color c) {
    if (!handle)
        return;
    SDL_SetTextureColorMod(static_cast<SDL_Texture *>(handle.get()), c.r, c.g, c.b);
}

void Texture::setAlphaMod(unsigned char a) {
    if (!handle)
        return;
    SDL_SetTextureAlphaMod(static_cast<SDL_Texture *>(handle.get()), a);
}

PixelLock Texture::lock() {
    Size s = size();
    return PixelLock(handle.get(), s.w, s.h);
}

//******************
// PixelLock
//******************
PixelLock::PixelLock(void *texture, int _w, int _h) : tex(texture), w(_w), h(_h) {
    void *raw = nullptr;
    int pitchBytes = 0;
    if (SDL_LockTexture(static_cast<SDL_Texture *>(tex), nullptr, &raw, &pitchBytes) == 0) {
        pixels = static_cast<unsigned int *>(raw);
        pitchPixels = pitchBytes / 4;
    }
}

PixelLock::PixelLock(PixelLock &&other) noexcept
    : tex(other.tex), pixels(other.pixels), pitchPixels(other.pitchPixels), w(other.w), h(other.h) {
    other.tex = nullptr;
    other.pixels = nullptr;
}

PixelLock::~PixelLock() {
    if (tex) {
        SDL_UnlockTexture(static_cast<SDL_Texture *>(tex));
    }
}

Color PixelLock::get(int x, int y) const {
    if (!pixels)
        return Color(0, 0, 0, 0);
    Uint32 raw = pixels[y * pitchPixels + x];
    Uint8 r, g, b, a;
    SDL_GetRGBA(raw, rgba8888Format(), &r, &g, &b, &a);
    return Color(r, g, b, a);
}

void PixelLock::set(int x, int y, Color c) {
    if (!pixels)
        return;
    pixels[y * pitchPixels + x] = SDL_MapRGBA(rgba8888Format(), c.r, c.g, c.b, c.a);
}

} // namespace ableem
