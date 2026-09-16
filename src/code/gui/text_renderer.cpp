//
// TextRenderer: the text/token/rect half of what Gui used to be.
//
#include "text_renderer.h"
#include "../core/services/system.h"

#include <cassert>
#include <cstring>
#include <iostream>

using namespace std;
using ableem::Rect;
using ableem::Size;
using ableem::Color;

#define SCREEN_WIDTH  ableem::GuiBase::ScreenWidth

//*******************************
// TextRenderer::getR / getG / getB
//*******************************
unsigned char TextRenderer::getR(const string &val) {
    return atoi(Strings::commaSep(val, 0).c_str());
}

unsigned char TextRenderer::getG(const string &val) {
    return atoi(Strings::commaSep(val, 1).c_str());
}

unsigned char TextRenderer::getB(const string &val) {
    return atoi(Strings::commaSep(val, 2).c_str());
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
    Rect rect;
    rect.x = atoi(theme_.data.values["opscreenx"].c_str());
    rect.y = atoi(theme_.data.values["opscreeny"].c_str());
    rect.w = atoi(theme_.data.values["opscreenw"].c_str());
    rect.h = atoi(theme_.data.values["opscreenh"].c_str());

    return rect;
}

//*******************************
// TextRenderer::getTextRectOfTheme
//*******************************
Rect TextRenderer::getTextRectOfTheme() {
    Rect rect;
    rect.x = atoi(theme_.data.values["textx"].c_str());
    rect.y = atoi(theme_.data.values["texty"].c_str());
    rect.w = atoi(theme_.data.values["textw"].c_str());
    rect.h = atoi(theme_.data.values["texth"].c_str());

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
        cout << "missing check icon" << endl;
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
        font = text.themeFont_;   // if font is invalid, default to themeFont

    //
    // break up the text into tokens of text and emoji markers
    //
    string line = _text;
    if (line.empty()) line = " ";
    if (line.back() != '|') {
        line = line + "|";  // in case a terminating | is needed
    }
    auto tokenStrings = Strings::getTokens(line, '|');

    //
    // fill the info structures
    //

    for (const auto &tokenString : tokenStrings) {      // for each token string
        if (tokenString == "") continue;
        TextOrEmojiTokenInfo tokenInfo;
        tokenInfo.tokenString = tokenString;
        if (tokenString[0] == '@') {    // if emoji marker
            auto it = text.emojis_.find(tokenString.c_str() + 1);
            if (it != text.emojis_.end()) {
                tokenInfo.emoji = it->second;   // save the texture
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
                cout << "emoji not found for " << tokenString << endl;
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
            if (useTextColor) {
                font.drawColor(renderer, x + tokenInfo.rect.x, y + tokenInfo.rect.y,
                               textColor, tokenInfo.tokenString);
            } else {
                font.drawAlign(renderer, x + tokenInfo.rect.x, y + tokenInfo.rect.y,
                               ableem::Align::Left, tokenInfo.tokenString);
            }
        }
    }
}

//*******************************
// Text Rendering routines
//*******************************

//*******************************
// TextRenderer::renderText
// renders/draws the line of text and emoji icons at the chosen position on the screen.  returns the height.
//*******************************
int TextRenderer::renderText(const ableem::Font &font, const string &text, int x, int y, XAlignment xAlign) {
    AllTextOrEmojiTokenInfo allTokenInfo(*this, font, text);
    allTokenInfo.render(x, y, xAlign);

    return allTokenInfo.totalSize.h;    // return the height
}

//*******************************
// TextRenderer::renderText_WithColor
// if background == true it draws a solid grey box around/behind the text
// this routine does not support emoji icons.  text only.
//*******************************
int TextRenderer::renderText_WithColor(const ableem::Font &font, const std::string &text, int x, int y,
                                       Color textColor, XAlignment xAlign, bool background) {
    AllTextOrEmojiTokenInfo allTokenInfo(*this, font, text);
    allTokenInfo.setTextColor(textColor);
    allTokenInfo.drawBackgroundRect = background;

    allTokenInfo.render(x, y, xAlign);

    return allTokenInfo.totalSize.h;    // return the height
}

//*******************************
// TextRenderer::renderTextLine
//*******************************
int TextRenderer::renderTextLine(const string &text, int line, int yoffset, XAlignment xAlign, int xoffset,
                                 ableem::Font font) {
    if (!font.valid())
        font = themeFont_;   // default to themeFont

    Rect opscreen = getOpscreenRectOfTheme();
    int fontHeight = font.lineHeight();
    int x = opscreen.x + 10 + xoffset;
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
int TextRenderer::renderTextLineToColumns(const string &textLeft, const string &textRight,
                                          int xLeft, int xRight,
                                          int line, int yoffset, ableem::Font font) {
    renderTextLine(textLeft, line, yoffset, XALIGN_LEFT, xLeft, font);
    int h = renderTextLine(textRight, line, yoffset, XALIGN_LEFT, xRight, font);

    return h;   // rectangle height
}

//*******************************
// TextRenderer::renderTextLineOptions
//*******************************
int TextRenderer::renderTextLineOptions(const string &_text, int line, int yoffset, XAlignment xAlign, int xoffset) {
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
        return h;   // there is no check/uncheck emoji on this line
    }

    // render the check/uncheck icon on the right side of opscreen
    Rect opscreen = getOpscreenRectOfTheme();
    int fontHeight = themeFont_.lineHeight();

    int x = opscreen.x + opscreen.w - 10 - getCheckIconWidth();
    int y = (fontHeight * line) + yoffset;
    if (button == 1) {
        renderText(themeFont_, "|@Check|", x, y);
    } else if (button == 0) {
        renderText(themeFont_, "|@Uncheck|", x, y);
    }

    return h;
}

//*******************************
// TextRenderer::renderSelectionBox
//*******************************
void TextRenderer::renderSelectionBox(int line, int yoffset, int xoffset, ableem::Font font) {
    if (!font.valid())
        font = themeFont_;

    string fg = theme_.data.values["text_fg"];
    int fontHeight = font.lineHeight();
    Rect opscreen = getOpscreenRectOfTheme();
    Rect rectSelection;
    rectSelection.x = opscreen.x + 5 + xoffset;
    rectSelection.y = yoffset + fontHeight * (line);
    rectSelection.w = opscreen.w - 10 - xoffset;
    rectSelection.h = fontHeight;

    renderer_.setDrawColor(Color(getR(fg), getG(fg), getB(fg), 255));
    renderer_.setBlendMode(ableem::BlendMode::Blend);
    renderer_.drawRect(rectSelection);
}

//*******************************
// TextRenderer::renderLabelBox
//*******************************
void TextRenderer::renderLabelBox(int line, int yoffset) {
    string bg = theme_.data.values["label_bg"];
    int fontHeight = themeFont_.lineHeight();
    Rect opscreen = getOpscreenRectOfTheme();
    Rect rectSelection;
    rectSelection.x = opscreen.x + 5;
    rectSelection.y = yoffset + fontHeight * (line);
    rectSelection.w = opscreen.w - 10;
    rectSelection.h = fontHeight;

    renderer_.setDrawColor(Color(getR(bg), getG(bg), getB(bg), atoi(theme_.data.values["keyalpha"].c_str())));
    renderer_.setBlendMode(ableem::BlendMode::Blend);
    renderer_.fillRect(rectSelection);
}

//*******************************
// TextRenderer::renderTextChar
//*******************************
void TextRenderer::renderTextChar(const string &text, int line, int yoffset, int x) {
    int fontHeight = themeFont_.lineHeight();
    int y = (fontHeight * line) + yoffset;
    themeFont_.drawAlign(renderer_, x, y, ableem::Align::Left, text);
}
