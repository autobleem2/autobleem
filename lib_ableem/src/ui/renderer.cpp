#include "ableem/ui/renderer.h"
#include "ableem/ui/platform.h"
#include "ableem/ui/texture.h"
#include "sdl_common.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <vector>

namespace ableem {

namespace {
SDL_BlendMode toSDL(BlendMode m) {
    switch (m) {
    case BlendMode::None:
        return SDL_BLENDMODE_NONE;
    case BlendMode::Add:
        return SDL_BLENDMODE_ADD;
    case BlendMode::Mod:
        return SDL_BLENDMODE_MOD;
    case BlendMode::Blend:
    default:
        return SDL_BLENDMODE_BLEND;
    }
}
SDL_Rect toSDL(const Rect &r) {
    return SDL_Rect{r.x, r.y, r.w, r.h};
}
} // namespace

struct Renderer::Impl {
    SDL_Renderer *renderer = nullptr;
    int width = 0, height = 0;
};

Renderer::Renderer(Platform &platform) : impl(new Impl()) {
    recreate(platform);
}

void Renderer::release() {
    if (impl->renderer) {
        SDL_DestroyRenderer(impl->renderer);
        impl->renderer = nullptr;
    }
}

void Renderer::recreate(Platform &platform) {
    release();
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

void Renderer::clear() {
    SDL_RenderClear(impl->renderer);
}
void Renderer::present() {
    SDL_RenderPresent(impl->renderer);
}

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

void Renderer::fillRects(const Rect *rects, int count) {
    if (count <= 0)
        return;
    // thread_local so repeated calls (once per frame, per color bucket) don't reallocate
    thread_local std::vector<SDL_Rect> buffer;
    buffer.resize(count);
    for (int i = 0; i < count; i++)
        buffer[i] = toSDL(rects[i]);
    SDL_RenderFillRects(impl->renderer, buffer.data(), count);
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
    if (src) {
        ssrc = toSDL(*src);
        psrc = &ssrc;
    }
    if (dst) {
        sdst = toSDL(*dst);
        pdst = &sdst;
    }
    SDL_RenderCopy(impl->renderer, static_cast<SDL_Texture *>(tex.native()), psrc, pdst);
}

void Renderer::copyTrapezoid(const Texture &tex, const Rect *src, VerticalEdge left, VerticalEdge right) {
    Rect s;
    if (src) {
        s = *src;
    } else {
        Size size = tex.size();
        s = Rect(0, 0, size.w, size.h);
    }
    bool mirrored = false;
    if (left.x > right.x) {
        std::swap(left, right);
        mirrored = true;
    }
    float width = right.x - left.x;
    float leftHeight = left.bottom - left.top, rightHeight = right.bottom - right.top;
    if (width < 1.0f || s.w <= 0 || s.h <= 0 || leftHeight <= 0.0f || rightHeight <= 0.0f)
        return;

    // perspective-correct texture mapping: 1/depth is linear across the screen, and each side's height is
    // proportional to 1/depth, so the source column for screen fraction t is t*hR / lerp(hL, hR, t)
    auto sourceAt = [&](float t) {
        float u = t * rightHeight / (leftHeight + (rightHeight - leftHeight) * t);
        return mirrored ? 1.0f - u : u;
    };
    auto *native = static_cast<SDL_Texture *>(tex.native());
    int xFirst = static_cast<int>(std::floor(left.x));
    int xLast = static_cast<int>(std::ceil(right.x));
    for (int x = xFirst; x < xLast; x++) {
        float t0 = std::min(1.0f, std::max(0.0f, (x - left.x) / width));
        float t1 = std::min(1.0f, std::max(0.0f, (x + 1 - left.x) / width));
        if (t1 <= t0)
            continue;
        float u0 = sourceAt(t0), u1 = sourceAt(t1);
        if (u0 > u1)
            std::swap(u0, u1);
        // the strip of source columns this screen column shows; at least one column wide
        int c0 = std::min(s.w - 1, static_cast<int>(u0 * s.w));
        int c1 = std::min(s.w, std::max(c0 + 1, static_cast<int>(std::ceil(u1 * s.w))));
        float tm = (t0 + t1) * 0.5f;
        int top = static_cast<int>(std::lround(left.top + (right.top - left.top) * tm));
        int bottom = static_cast<int>(std::lround(left.bottom + (right.bottom - left.bottom) * tm));
        SDL_Rect sr{s.x + c0, s.y, c1 - c0, s.h};
        SDL_Rect dr{x, top, 1, std::max(1, bottom - top)};
        SDL_RenderCopy(impl->renderer, native, &sr, &dr);
    }
}

void Renderer::setTarget(Texture *target) {
    SDL_SetRenderTarget(impl->renderer, target ? static_cast<SDL_Texture *>(target->native()) : nullptr);
}

int Renderer::width() const {
    return impl->width;
}
int Renderer::height() const {
    return impl->height;
}

void *Renderer::native() const {
    return impl->renderer;
}

} // namespace ableem
