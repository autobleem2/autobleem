#pragma once

#include <memory>
#include <string>
#include "types.h"

namespace ableem {

class Renderer;

//******************
// Font
//******************
// Cheap shared handle over a cached bitmap font (SDL_FontCache under the hood). Copy freely.
class ABLEEM_API Font {
public:
    Font();
    Font(const Font &) = default;
    Font &operator=(const Font &) = default;

    static Font load(Renderer &renderer, const std::string &ttfPath, int pointSize);

    bool valid() const;

    Size textSize(const std::string &text) const;
    int lineHeight() const;
    int width(const std::string &text) const;

    void draw(Renderer &renderer, int x, int y, const std::string &text) const;
    void drawAlign(Renderer &renderer, int x, int y, Align align, const std::string &text) const;
    void drawColor(Renderer &renderer, int x, int y, Color color, const std::string &text) const;

    // wraps text to fit maxWidth pixels, returning it with '\n' inserted
    std::string wrappedText(const std::string &text, int maxWidth) const;

    // must be called after the SDL renderer target or device is reset, which SDL_FontCache needs to know
    // about to keep its glyph cache valid. deviceLost distinguishes a full device reset from a target reset.
    void resetAfterRendererReset(Renderer &renderer, bool deviceLost);

private:
    explicit Font(void *fcFont);
    std::shared_ptr<void> handle;

public:
    void *native() const { return handle.get(); }
};

} // namespace ableem
