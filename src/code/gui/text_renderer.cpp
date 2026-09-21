//
// TextRenderer: the text/token/rect half of what Gui used to be.
//
#include "text_renderer.h"
#include "panel_style.h"
#include "../core/services/system.h"

#include <cassert>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <ableem/engine/log.h>

using namespace std;
using ableem::Color;
using ableem::Rect;
using ableem::Size;

#define SCREEN_WIDTH ableem::GuiBase::ScreenWidth

//*******************************
// TextRenderer::toColor
//*******************************
Color TextRenderer::toColor(const ThemeColor &color, int alpha) {
    return Color(color.r, color.g, color.b, alpha);
}

//*******************************
// Rect and Size routines
//*******************************

//*******************************
// TextRenderer::getFontTextSize
// return the w of the rendered text and h of the font
//*******************************
Size TextRenderer::getFontTextSize(const ableem::Font &font, const char *text) {
    Size size;
    if (!font.valid()) {
        size.w = 0;
        size.h = 0;
        return size;
    }
    if (text == nullptr || strlen(text) == 0)
        size.w = 0;
    else
        size.w = font.width(text);
    size.h = font.lineHeight();

    return size;
}

//*******************************
// TextRenderer::getFontTextRect
// return a rect at (x,y) sized to the rendered text
//*******************************
Rect TextRenderer::getFontTextRect(const ableem::Font &font, const char *text, int x, int y) {
    Size size = getFontTextSize(font, text);
    Rect rect;
    rect.x = x;
    rect.y = y;
    rect.w = size.w;
    rect.h = size.h;

    return rect;
}

//*******************************
// TextRenderer::getOpscreenRectOfTheme
//*******************************
Rect TextRenderer::getOpscreenRectOfTheme() {
    const ableem::ThemePanel &panel = theme_.classic().menuPanel;
    Rect rect;
    rect.x = panel.x;
    rect.y = panel.y;
    rect.w = panel.w;
    rect.h = panel.h;

    return rect;
}

//*******************************
// TextRenderer::getTextRectOfTheme
//*******************************
Rect TextRenderer::getTextRectOfTheme() {
    const ableem::ThemeStatusBar &bar = theme_.classic().statusBar;
    Rect rect;
    rect.x = bar.x;
    rect.y = bar.y;
    rect.w = bar.w;
    rect.h = bar.h;

    return rect;
}

//*******************************
// TextRenderer::getCheckIconWidth
// returns the width of the check/uncheck icon textures
//*******************************
int TextRenderer::getCheckIconWidth() {
    auto it = emojis_.find("Check");
    if (it != emojis_.end()) {
        return it->second.size().w;
    } else {
        PLOG_WARNING << "missing check icon";
        assert(false);
    }

    return 0;
}

//*******************************
// TextRenderer::align_xPosition
//*******************************
int TextRenderer::align_xPosition(XAlignment xAlign, int x, int width) {
    if (xAlign == XALIGN_CENTER) {
        x = (SCREEN_WIDTH / 2) - width / 2;
    } else if (xAlign == XALIGN_RIGHT) {
        x = SCREEN_WIDTH - x - width;
    }

    return x;
}

//*******************************
// TextRenderer::AllTextOrEmojiTokenInfo::compute_xy_relativeOffsets
// compute x offset, center the y offset of each token to the total height
//*******************************
void TextRenderer::AllTextOrEmojiTokenInfo::compute_xy_relativeOffsets() {
    int xOffset = 0;
    for (auto &info : tokenInfos) {
        x = xOffset;
        xOffset += info.rect.w;
        info.rect.y = (totalSize.h - info.rect.h) / 2;
    }
}

//*******************************
// TextRenderer::AllTextOrEmojiTokenInfo::getTokenInfo
// break up the text into tokens of pure text or an emoji icon marker
// return a vector of the text, emoji texture pointers, width and height of each token and the total width and height.
//*******************************
void TextRenderer::AllTextOrEmojiTokenInfo::getTokenInfo(ableem::Font _font, const string &_text) {
    font = _font;
    if (!font.valid())
        font = text.themeFont_; // if font is invalid, default to themeFont

    //
    // break up the text into tokens of text and emoji markers
    //
    string line = _text;
    if (line.empty())
        line = " ";
    if (line.back() != '|') {
        line = line + "|"; // in case a terminating | is needed
    }
    auto tokenStrings = Strings::getTokens(line, '|');

    //
    // fill the info structures
    //

    for (const auto &tokenString : tokenStrings) { // for each token string
        if (tokenString == "")
            continue;
        TextOrEmojiTokenInfo tokenInfo;
        tokenInfo.tokenString = tokenString;
        if (tokenString[0] == '@') { // if emoji marker
            auto it = text.emojis_.find(tokenString.c_str() + 1);
            if (it != text.emojis_.end()) {
                tokenInfo.emoji = it->second; // save the texture
                Size s = it->second.size();
                tokenInfo.rect.x = 0;
                tokenInfo.rect.y = 0;
                tokenInfo.rect.w = s.w;
                tokenInfo.rect.h = s.h;
                // update overall size
                totalSize.w += s.w;
                if (s.h > totalSize.h)
                    totalSize.h = s.h;
                // add the token info
                tokenInfos.emplace_back(tokenInfo);
            } else {
                PLOG_WARNING << "emoji not found for " << tokenString;
            }
        } else {
            tokenInfo.rect = text.getFontTextRect(font, tokenString);
            // update overall size
            totalSize.w += tokenInfo.rect.w;
            if (tokenInfo.rect.h > totalSize.h)
                totalSize.h = tokenInfo.rect.h;
            // add the token info
            tokenInfos.emplace_back(tokenInfo);
        }
    }

    int xOffset = 0;
    for (auto &tokenInfo : tokenInfos) {
        // set the x posit within the string
        tokenInfo.rect.x = xOffset;
        xOffset += tokenInfo.rect.w;
        // adjust the text y and emoji y so they are centered in the total height
        tokenInfo.rect.y = (totalSize.h - tokenInfo.rect.h) / 2;
    }
}

//*******************************
// TextRenderer::AllTextOrEmojiTokenInfo::render
// renders/draws the text and emoji icons at the chosen position on the screen
//*******************************
void TextRenderer::AllTextOrEmojiTokenInfo::render(int x, int y, XAlignment xAlign) {
    ableem::Renderer &renderer = text.renderer_;

    // compute x offset, center the y offset of each token to the total height
    compute_xy_relativeOffsets();

    // adjust the upper left corner postion if needed
    if (xAlign != XALIGN_LEFT)
        x = align_xPosition(xAlign, x, totalSize.w);

    if (drawBackgroundRect) {
        // render a grey box behind the text
        renderer.setDrawColor(Color(0, 0, 0, 70));
        Rect backRect;
        backRect.x = x - 10;
        backRect.y = y - 2;
        backRect.w = totalSize.w + 20;
        backRect.h = totalSize.h + 4;

        renderer.fillRect(backRect);
    }

    for (auto &tokenInfo : tokenInfos) {
        if (tokenInfo.emoji.valid()) {
            // the token is an emoji texture
            Rect tempRect = tokenInfo.rect;
            tempRect.x += x;
            tempRect.y += y;
            renderer.copy(tokenInfo.emoji, nullptr, &tempRect);
        } else {
            // the token is text
            text.drawRun(font, x + tokenInfo.rect.x, y + tokenInfo.rect.y, useTextColor ? &textColor : nullptr,
                         tokenInfo.tokenString);
        }
    }
}

//*******************************
// TextRenderer::drawRun
// one run of text, in `color` or the font's own, with the halo under it when the shadow is on and the
// text is light
//*******************************
void TextRenderer::drawRun(const ableem::Font &font, int x, int y, const Color *color, const string &run) {
    if (!font.valid() || run.empty())
        return;
    const bool halo = shadow_.enabled && (color == nullptr || Shadow::isLight(*color));
    const CachedRun &cached = cachedRun(font, color, halo, run);
    if (!cached.tex.valid())
        return;
    Size size = cached.tex.size();
    Rect dst(x - cached.pad, y - cached.pad, size.w, size.h);
    renderer_.copy(cached.tex, nullptr, &dst);
}

//*******************************
// TextRenderer::cachedRun
//*******************************
const TextRenderer::CachedRun &TextRenderer::cachedRun(const ableem::Font &font, const Color *color, bool halo,
                                                       const string &run) {
    // the key: which font, which colour (or the font's own), whether the halo is under it, and the text
    char head[64];
    snprintf(head, sizeof(head), "%p|%08x|%08x|", font.native(),
             color ? (color->r << 24 | color->g << 16 | color->b << 8 | color->a) : 0xffffffffu,
             halo ? (shadow_.color.r << 24 | shadow_.color.g << 16 | shadow_.color.b << 8 | shadow_.color.a) : 0u);
    string key = head + run;
    auto found = runCache_.find(key);
    if (found != runCache_.end())
        return found->second;

    // a screen's worth is a few dozen; a runaway (a clock, say) is cut off by starting over
    if (runCache_.size() > 512)
        runCache_.clear();

    CachedRun entry;
    entry.pad = halo ? 3 : 0;
    Size text = font.textSize(run);
    if (text.w > 0 && text.h > 0) {
        entry.tex = ableem::Texture::createTarget(renderer_, text.w + 2 * entry.pad, text.h + 2 * entry.pad);
        entry.tex.setBlendMode(ableem::BlendMode::Blend);
        renderer_.setTarget(&entry.tex);
        renderer_.setBlendMode(ableem::BlendMode::None);
        renderer_.setDrawColor(Color(0, 0, 0, 0)); // transparent, and dark where the edges blend into it
        renderer_.fillRect();
        renderer_.setBlendMode(ableem::BlendMode::Blend);
        const int x = entry.pad, y = entry.pad;
        if (halo) {
            // the halo first, so the text itself lands on top of it
            static const int offsets[][2] = {{-1, -1}, {0, -1}, {1, -1}, {-1, 0}, {1, 0},
                                             {-1, 1},  {0, 1},  {1, 1},  {2, 2}};
            for (const auto &offset : offsets)
                font.drawColor(renderer_, x + offset[0], y + offset[1], shadow_.color, run);
        }
        if (color != nullptr)
            font.drawColor(renderer_, x, y, *color, run);
        else
            font.drawAlign(renderer_, x, y, ableem::Align::Left, run);
        renderer_.setTarget(nullptr);
    }
    return runCache_.emplace(key, entry).first->second;
}

//*******************************
// TextRenderer::clearTextCache
//*******************************
void TextRenderer::clearTextCache() {
    runCache_.clear();
}

//*******************************
// Text Rendering routines
//*******************************

//*******************************
// TextRenderer::renderText
// renders/draws the line of text and emoji icons at the chosen position on the screen.  returns the height.
//*******************************
int TextRenderer::renderText(const ableem::Font &font, const string &text, int x, int y, XAlignment xAlign,
                             const Color *color) {
    AllTextOrEmojiTokenInfo allTokenInfo(*this, font, text);
    if (color != nullptr)
        allTokenInfo.setTextColor(*color);
    allTokenInfo.render(x, y, xAlign);

    return allTokenInfo.totalSize.h; // return the height
}

//*******************************
// TextRenderer::renderText_WithColor
// if background == true it draws a solid grey box around/behind the text
// this routine does not support emoji icons.  text only.
//*******************************
int TextRenderer::renderText_WithColor(const ableem::Font &font, const std::string &text, int x, int y, Color textColor,
                                       XAlignment xAlign, bool background) {
    AllTextOrEmojiTokenInfo allTokenInfo(*this, font, text);
    allTokenInfo.setTextColor(textColor);
    allTokenInfo.drawBackgroundRect = background;

    allTokenInfo.render(x, y, xAlign);

    return allTokenInfo.totalSize.h; // return the height
}

//*******************************
// TextRenderer::renderTextLine
//*******************************
int TextRenderer::renderTextLine(const string &text, int line, int yoffset, XAlignment xAlign, int xoffset,
                                 ableem::Font font) {
    if (!font.valid())
        font = themeFont_; // default to themeFont

    Rect opscreen = getOpscreenRectOfTheme();
    int fontHeight = font.lineHeight();
    int x = opscreen.x + PanelStyle::RowInset + 8 + xoffset; // level with the header's title
    int y = (fontHeight * line) + yoffset;

    if (line < 0) {
        line = -line;
        y = line;
    }

    return renderText(font, text, x, y, xAlign);
}

//*******************************
// TextRenderer::renderTextLineToColumns
//*******************************
int TextRenderer::renderTextLineToColumns(const string &textLeft, const string &textRight, int xLeft, int xRight,
                                          int line, int yoffset, ableem::Font font) {
    renderTextLine(textLeft, line, yoffset, XALIGN_LEFT, xLeft, font);
    int h = renderTextLine(textRight, line, yoffset, XALIGN_LEFT, xRight, font);

    return h; // rectangle height
}

//*******************************
// TextRenderer::renderTextLineOptions
//*******************************
int TextRenderer::renderTextLineOptions(const string &_text, int line, int yoffset, XAlignment xAlign, int xoffset,
                                        int rightEdge) {
    string text = _text;

    // if there is a check or uncheck icon, flag which one and remove the emoji toekn from the string
    int button = -1;
    if (text.find("|@Check|") != std::string::npos) {
        button = 1;
    }
    if (text.find("|@Uncheck|") != std::string::npos) {
        button = 0;
    }
    if (button != -1) {
        text = text.substr(0, text.find("|"));
    }

    // render the text string without the check/uncheck icon
    int h = renderTextLine(text, line, yoffset, xAlign, xoffset);

    if (button == -1) {
        return h; // there is no check/uncheck emoji on this line
    }

    // render the check/uncheck icon on the right side of opscreen
    Rect opscreen = getOpscreenRectOfTheme();
    int fontHeight = themeFont_.lineHeight();

    const int right = rightEdge > 0 ? rightEdge : opscreen.x + opscreen.w - PanelStyle::RowInset - 8;
    int x = right - getCheckIconWidth() + checkIconRightMargin_;
    int y = (fontHeight * line) + yoffset;
    if (line < 0)
        y = -line; // an absolute y, as renderTextLine takes it
    if (button == 1) {
        renderText(themeFont_, "|@Check|", x, y);
    } else if (button == 0) {
        renderText(themeFont_, "|@Uncheck|", x, y);
    }

    return h;
}

//*******************************
// TextRenderer::renderRowValue
//*******************************
void TextRenderer::renderRowValue(const string &value, int line, int yoffset, int rightEdge, ableem::Font font) {
    if (!font.valid())
        font = themeFont_;
    Rect opscreen = getOpscreenRectOfTheme();
    const int right = rightEdge > 0 ? rightEdge : opscreen.x + opscreen.w - PanelStyle::RowInset - 8;
    int y = (font.lineHeight() * line) + yoffset;
    if (line < 0)
        y = -line;
    renderText(font, value, ableem::GuiBase::ScreenWidth - right, y, XALIGN_RIGHT);
}

//*******************************
// TextRenderer::renderSelectionBox
//*******************************
void TextRenderer::renderSelectionBox(int line, int yoffset, int xoffset, ableem::Font font, int rightEdge) {
    if (!font.valid())
        font = themeFont_;

    int fontHeight = font.lineHeight();
    Rect opscreen = getOpscreenRectOfTheme();
    Rect rectSelection;
    rectSelection.x = opscreen.x + 1 + xoffset;
    rectSelection.y = yoffset + fontHeight * (line);
    rectSelection.w = (rightEdge > 0 ? rightEdge + 12 : opscreen.x + opscreen.w - 1) - rectSelection.x;
    rectSelection.h = fontHeight;

    PanelStyle::fromTheme(theme_.launcher()).selection(renderer_, rectSelection);
}

//*******************************
// TextRenderer::renderLabelBox
//*******************************
void TextRenderer::renderLabelBox(int line, int yoffset) {
    int fontHeight = themeFont_.lineHeight();
    Rect opscreen = getOpscreenRectOfTheme();
    Rect rectSelection;
    rectSelection.x = opscreen.x + 1;
    rectSelection.y = yoffset + fontHeight * (line);
    rectSelection.w = opscreen.w - 2;
    rectSelection.h = fontHeight;

    PanelStyle::fromTheme(theme_.launcher()).label(renderer_, rectSelection);
}

//*******************************
// TextRenderer::renderTextChar
//*******************************
void TextRenderer::renderTextChar(const string &text, int line, int yoffset, int x) {
    int fontHeight = themeFont_.lineHeight();
    int y = (fontHeight * line) + yoffset;
    drawRun(themeFont_, x, y, nullptr, text);
}

//*******************************
// TextRenderer::textWidth
//*******************************
int TextRenderer::textWidth(const ableem::Font &font, const string &text) {
    AllTextOrEmojiTokenInfo info(*this, font, text);
    return info.totalSize.w;
}

//*******************************
// TextRenderer::fittingFont
//*******************************
ableem::Font TextRenderer::fittingFont(FontType type, int maxSize, int minSize, const string &text, int maxWidth) {
    if (fonts_ == nullptr)
        return themeFont_;
    if (minSize > maxSize)
        minSize = maxSize;
    for (int size = maxSize; size > minSize; size--) {
        ableem::Font &font = fonts_->atSize(type, size);
        if (textWidth(font, text) <= maxWidth)
            return font;
    }
    return fonts_->atSize(type, minSize);
}

//*******************************
// TextRenderer::renderFittedText
//*******************************
int TextRenderer::renderFittedText(FontType type, int maxSize, int minSize, const string &text, int x, int y,
                                   int maxWidth, XAlignment xAlign) {
    return renderText(fittingFont(type, maxSize, minSize, text, maxWidth), text, x, y, xAlign);
}

//*******************************
// TextRenderer::renderFittedText_WithColor
//*******************************
int TextRenderer::renderFittedText_WithColor(FontType type, int maxSize, int minSize, const string &text, int x, int y,
                                             int maxWidth, ableem::Color textColor, XAlignment xAlign) {
    return renderText_WithColor(fittingFont(type, maxSize, minSize, text, maxWidth), text, x, y, textColor, xAlign);
}

//*******************************
// TextRenderer::renderWrappedText
//*******************************
int TextRenderer::renderWrappedText(const ableem::Font &font, const string &text, int x, int y, int width,
                                    ableem::Color textColor) {
    if (shadow_.enabled && Shadow::isLight(textColor)) {
        static const int offsets[][2] = {{-1, -1}, {0, -1}, {1, -1}, {-1, 0}, {1, 0}, {-1, 1}, {0, 1}, {1, 1}, {2, 2}};
        for (const auto &offset : offsets)
            font.drawColumn(renderer_, x + offset[0], y + offset[1], width, shadow_.color, text);
    }
    return font.drawColumn(renderer_, x, y, width, textColor, text);
}

//*******************************
// TextRenderer::elide
//*******************************
string TextRenderer::elide(const ableem::Font &font, const string &text, int maxWidth) {
    if (font.width(text) <= maxWidth)
        return text;
    const string dots = "...";
    string cut = text;
    while (!cut.empty() && font.width(cut + dots) > maxWidth) {
        cut.pop_back();
        while (!cut.empty() && (static_cast<unsigned char>(cut.back()) & 0xC0) == 0x80)
            cut.pop_back(); // a whole UTF-8 char
    }
    return cut + dots;
}
