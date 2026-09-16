//
// TextRenderer: the classic UI's text drawing - a line of text with |@X|-style button icons in it, laid out
// against the theme's opscreen rectangle in the theme's font.
//
#pragma once

#include "../core/services/theme.h"

#include <ableem/ableem.h>

#include <map>
#include <string>
#include <vector>

enum XAlignment { XALIGN_LEFT, XALIGN_CENTER, XALIGN_RIGHT };

//********************
// TextRenderer
//********************
// Was the text/token/rect half of Gui (about 450 lines of it). It draws with the one Renderer, in the
// theme's font unless told otherwise, and turns "|@Cross|"-style markers into the theme's button textures;
// the font and the texture map are Gui's (they change when a theme loads), which is why it holds references
// rather than copies. Screens reach it as gui->text().
class TextRenderer {
public:
    TextRenderer(ableem::Renderer &renderer, Theme &theme, ableem::Font &themeFont,
                 std::map<std::string, ableem::Texture> &emojis)
        : renderer_(renderer), theme_(theme), themeFont_(themeFont), emojis_(emojis) {}

    // the theme's colours are "r,g,b" strings
    static unsigned char getR(const std::string &val);
    static unsigned char getG(const std::string &val);
    static unsigned char getB(const std::string &val);

    //*******************************
    // Rect and Size routines
    //*******************************

    // return the w of the rendered text and h of the font
    ableem::Size getFontTextSize(const ableem::Font &font, const char *text = nullptr);
    ableem::Size getFontTextSize(const ableem::Font &font, const std::string &text = "") {
        return getFontTextSize(font, text.c_str());
    }
    // return a rect at (x,y) sized to the rendered text
    ableem::Rect getFontTextRect(const ableem::Font &font, const char *text = nullptr, int x = 0, int y = 0);
    ableem::Rect getFontTextRect(const ableem::Font &font, const std::string &text = "", int x = 0, int y = 0) {
        return getFontTextRect(font, text.c_str(), x, y);
    }

    ableem::Rect getOpscreenRectOfTheme();
    ableem::Rect getTextRectOfTheme();

    int getCheckIconWidth();    // returns the width of the check icon texture.  used to compute the x position.
    static int align_xPosition(XAlignment xAlign, int x, int width);

    //*******************************
    // Text tokenizing structure routines
    //*******************************

    struct TextOrEmojiTokenInfo {
        std::string tokenString;
        ableem::Texture emoji;   // valid() only if tokenString is an emoji marker such as "|@X|"
        ableem::Rect rect;       // position, width, and height of rendered text or emoji texture
                                 // the x, y position is relative to the upper left corner of the string
    };

    // break up the text into tokens of text or the token of an emoji icon
    // build a vector of the text, emoji texture pointers, position, width and height of each token and the
    // total width and height of the entire line.
    struct AllTextOrEmojiTokenInfo {
        TextRenderer &text;
        std::vector<TextOrEmojiTokenInfo> tokenInfos;

        ableem::Font font;
        int x = 0, y = 0;        // upper left corner of the string on the display
        ableem::Size totalSize;  // the total width and height of all the tokens
        bool useTextColor = false;
        ableem::Color textColor;
        bool drawBackgroundRect = false;

        AllTextOrEmojiTokenInfo(TextRenderer &_text, ableem::Font _font, const std::string &_string)
            : text(_text) { getTokenInfo(_font, _string); }
        void getTokenInfo(ableem::Font _font, const std::string &_text);

        void compute_xy_relativeOffsets(); // compute x offset, center the y offset of each token to the total height
        void setTextColor(ableem::Color color) { textColor = color; textColor.a = 255; useTextColor = true; }

        // renders/draws the text and emoji icons at the chosen position on the screen
        void render(int x, int y, XAlignment xAlign = XALIGN_LEFT);
    };

    //*******************************
    // Text Line Rendering routines
    //*******************************

    // renders/draws the line of text and emoji icons at the chosen position on the screen.  returns the height.
    int renderText(const ableem::Font &font, const std::string &text, int x, int y, XAlignment xAlign = XALIGN_LEFT);

    // if background == true it draws a solid grey box around/behind the text
    // this routine does not support emoji icons.  text only.
    int renderText_WithColor(const ableem::Font &font, const std::string &text, int x, int y, ableem::Color textColor,
                             XAlignment xAlign = XALIGN_LEFT, bool background = false);

    // returns rectangle height
    int renderTextLine(const std::string &text, int line, int yoffset = 0,
                       XAlignment xAlign = XALIGN_LEFT, int xoffset = 0,
                       ableem::Font font = ableem::Font());   // font will default to themeFont in the cpp

    int renderTextLineToColumns(const std::string &textLeft, const std::string &textRight, int xLeft, int xRight,
                                int line, int yoffset = 0, ableem::Font font = ableem::Font());

    int renderTextLineOptions(const std::string &text, int line, int yoffset = 0, XAlignment xAlign = XALIGN_LEFT,
                              int xoffset = 0);

    void renderSelectionBox(int line, int yoffset, int xoffset = 0, ableem::Font font = ableem::Font());

    void renderLabelBox(int line, int yoffset);

    void renderTextChar(const std::string &text, int line, int yoffset, int posx);

private:
    ableem::Renderer &renderer_;
    Theme &theme_;
    ableem::Font &themeFont_;
    std::map<std::string, ableem::Texture> &emojis_;
};
