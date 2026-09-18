#include "ableem/ui/font.h"
#include "ableem/ui/renderer.h"
#include "sdl_common.h"
#include <cmath>
#include <iostream>
#include <vector>
#include <ableem/engine/log.h>

namespace ableem {

namespace {
void destroyFont(void *f) {
    if (f)
        FC_FreeFont(static_cast<FC_Font *>(f));
}
FC_AlignEnum toFC(Align a) {
    switch (a) {
    case Align::Center:
        return FC_ALIGN_CENTER;
    case Align::Right:
        return FC_ALIGN_RIGHT;
    case Align::Left:
    default:
        return FC_ALIGN_LEFT;
    }
}
} // namespace

Font::Font() : handle(nullptr) {}

Font::Font(void *fcFont) : handle(fcFont, destroyFont) {}

Font Font::load(Renderer &renderer, const std::string &ttfPath, int pointSize) {
    float scale = renderer.outputScale();
    int outputPointSize = static_cast<int>(std::lround(pointSize * scale));
    FC_Font *fc = FC_CreateFont();
    Uint8 ok = FC_LoadFont(fc, static_cast<SDL_Renderer *>(renderer.native()), ttfPath.c_str(), outputPointSize,
                           FC_MakeColor(255, 255, 255, 255), TTF_STYLE_NORMAL);
    if (!ok) {
        PLOG_ERROR << "FAILURE opening font " << ttfPath << " of size " << pointSize;
    } else {
        PLOG_DEBUG << "Opened font " << ttfPath << " of size " << pointSize;
    }
    Font font(fc);
    font.scale_ = scale;
    return font;
}

int Font::logical(int outputPixels) const {
    return scale_ == 1.0f ? outputPixels : static_cast<int>(std::lround(outputPixels / scale_));
}

bool Font::valid() const {
    return handle != nullptr;
}

Size Font::textSize(const std::string &text) const {
    Size s;
    if (!handle)
        return s;
    // "%s" avoids treating the text itself as a printf format string
    s.w = logical(FC_GetWidth(static_cast<FC_Font *>(handle.get()), "%s", text.c_str()));
    s.h = logical(FC_GetLineHeight(static_cast<FC_Font *>(handle.get())));
    return s;
}

int Font::lineHeight() const {
    if (!handle)
        return 0;
    return logical(FC_GetLineHeight(static_cast<FC_Font *>(handle.get())));
}

int Font::width(const std::string &text) const {
    if (!handle)
        return 0;
    return logical(FC_GetWidth(static_cast<FC_Font *>(handle.get()), "%s", text.c_str()));
}

void Font::draw(Renderer &renderer, int x, int y, const std::string &text) const {
    if (!handle)
        return;
    FC_Draw(static_cast<FC_Font *>(handle.get()), static_cast<SDL_Renderer *>(renderer.native()), x * scale_,
            y * scale_, "%s", text.c_str());
}

void Font::drawAlign(Renderer &renderer, int x, int y, Align align, const std::string &text) const {
    if (!handle)
        return;
    FC_DrawAlign(static_cast<FC_Font *>(handle.get()), static_cast<SDL_Renderer *>(renderer.native()), x * scale_,
                 y * scale_, toFC(align), "%s", text.c_str());
}

void Font::drawColor(Renderer &renderer, int x, int y, Color color, const std::string &text) const {
    if (!handle)
        return;
    SDL_Color c{color.r, color.g, color.b, color.a};
    FC_DrawColor(static_cast<FC_Font *>(handle.get()), static_cast<SDL_Renderer *>(renderer.native()), x * scale_,
                 y * scale_, c, "%s", text.c_str());
}

std::string Font::wrappedText(const std::string &text, int maxWidth) const {
    if (!handle)
        return text;
    std::vector<char> buffer(text.size() + 256);
    int len = FC_GetWrappedText(static_cast<FC_Font *>(handle.get()), buffer.data(), static_cast<int>(buffer.size()),
                                static_cast<Uint16>(maxWidth * scale_), "%s", text.c_str());
    return std::string(buffer.data(), len > 0 ? static_cast<size_t>(len) : 0);
}

int Font::columnHeight(const std::string &text, int width) const {
    if (!handle)
        return 0;
    return logical(FC_GetColumnHeight(static_cast<FC_Font *>(handle.get()), static_cast<Uint16>(width * scale_), "%s",
                                      text.c_str()));
}

int Font::drawColumn(Renderer &renderer, int x, int y, int width, Color color, const std::string &text) const {
    if (!handle)
        return 0;
    SDL_Color c{color.r, color.g, color.b, color.a};
    FC_Rect r = FC_DrawColumnColor(static_cast<FC_Font *>(handle.get()), static_cast<SDL_Renderer *>(renderer.native()),
                                   x * scale_, y * scale_, static_cast<Uint16>(width * scale_), c, "%s", text.c_str());
    return logical(static_cast<int>(r.h));
}

void Font::resetAfterRendererReset(Renderer &renderer, bool deviceLost) {
    if (!handle)
        return;
    Uint32 evType = deviceLost ? SDL_RENDER_DEVICE_RESET : SDL_RENDER_TARGETS_RESET;
    FC_ResetFontFromRendererReset(static_cast<FC_Font *>(handle.get()), static_cast<SDL_Renderer *>(renderer.native()),
                                  evType);
}

} // namespace ableem
