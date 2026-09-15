#include "ableem/renderer.h"
#include "ableem/platform.h"
#include "ableem/texture.h"
#include "sdl_common.h"
#include <stdexcept>

namespace ableem {

namespace {
SDL_BlendMode toSDL(BlendMode m) {
    switch (m) {
        case BlendMode::None:  return SDL_BLENDMODE_NONE;
        case BlendMode::Add:   return SDL_BLENDMODE_ADD;
        case BlendMode::Mod:   return SDL_BLENDMODE_MOD;
        case BlendMode::Blend: default: return SDL_BLENDMODE_BLEND;
    }
}
SDL_Rect toSDL(const Rect &r) { return SDL_Rect{ r.x, r.y, r.w, r.h }; }
} // namespace

struct Renderer::Impl {
    SDL_Renderer *renderer = nullptr;
    int width = 0, height = 0;
};

Renderer::Renderer(Platform &platform) : impl(new Impl()) {
    SDL_Window *window = static_cast<SDL_Window *>(platform.nativeWindow());
    impl->renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!impl->renderer) {
        throw std::runtime_error(std::string("SDL_CreateRenderer failed: ") + SDL_GetError());
    }
    SDL_GetWindowSize(window, &impl->width, &impl->height);
}

Renderer::~Renderer() {
    if (impl->renderer) {
        SDL_DestroyRenderer(impl->renderer);
    }
    delete impl;
}

void Renderer::clear() { SDL_RenderClear(impl->renderer); }
void Renderer::present() { SDL_RenderPresent(impl->renderer); }

void Renderer::setDrawColor(Color c) {
    SDL_SetRenderDrawColor(impl->renderer, c.r, c.g, c.b, c.a);
}

Color Renderer::drawColor() const {
    Uint8 r, g, b, a;
    SDL_GetRenderDrawColor(impl->renderer, &r, &g, &b, &a);
    return Color(r, g, b, a);
}

void Renderer::setBlendMode(BlendMode mode) {
    SDL_SetRenderDrawBlendMode(impl->renderer, toSDL(mode));
}

void Renderer::fillRect(const Rect &r) {
    SDL_Rect sr = toSDL(r);
    SDL_RenderFillRect(impl->renderer, &sr);
}

void Renderer::fillRect() {
    SDL_RenderFillRect(impl->renderer, nullptr);
}

void Renderer::drawRect(const Rect &r) {
    SDL_Rect sr = toSDL(r);
    SDL_RenderDrawRect(impl->renderer, &sr);
}

void Renderer::drawLine(Point a, Point b) {
    SDL_RenderDrawLine(impl->renderer, a.x, a.y, b.x, b.y);
}

void Renderer::copy(const Texture &tex, const Rect *src, const Rect *dst) {
    SDL_Rect ssrc, sdst;
    SDL_Rect *psrc = nullptr, *pdst = nullptr;
    if (src) { ssrc = toSDL(*src); psrc = &ssrc; }
    if (dst) { sdst = toSDL(*dst); pdst = &sdst; }
    SDL_RenderCopy(impl->renderer, static_cast<SDL_Texture *>(tex.native()), psrc, pdst);
}

void Renderer::setTarget(Texture *target) {
    SDL_SetRenderTarget(impl->renderer, target ? static_cast<SDL_Texture *>(target->native()) : nullptr);
}

int Renderer::width() const { return impl->width; }
int Renderer::height() const { return impl->height; }

void *Renderer::native() const { return impl->renderer; }

} // namespace ableem
