//
// TextRenderer: the classic UI's text drawing - a line of text with |@X|-style button icons in it, laid out
// against the theme's opscreen rectangle in the theme's font.
//
#pragma once

#include "../core/services/theme.h"
#include "gui_font.h"

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

    // a theme colour with an alpha, as the renderer takes it
    static ableem::Color toColor(const ThemeColor &color, int alpha);

    // the font set fittingFont() draws from - the theme's launcher fonts; Gui hands it over on every load
    void setFonts(Fonts *fonts) { fonts_ = fonts; }
    // the transparent margin right of the check switch's art (ThemeAssets measures it): the switch is
    // drawn that much further right so its visible edge meets the row's right edge, where the values are
    void setCheckIconRightMargin(int margin) { checkIconRightMargin_ = margin; }

    //*******************************
    // fitted and wrapped text
    //*******************************
    // the width of a line with its |@X| markers laid out, in this font
    int textWidth(const ableem::Font &font, const std::string &text);
    // the largest size from maxSize down to minSize at which the line fits maxWidth (minSize when none does)
    ableem::Font fittingFont(FontType type, int maxSize, int minSize, const std::string &text, int maxWidth);
    // renderText / renderText_WithColor in the fitting font; returns the height drawn
    int renderFittedText(FontType type, int maxSize, int minSize, const std::string &text, int x, int y, int maxWidth,
                         XAlignment xAlign = XALIGN_LEFT);
    int renderFittedText_WithColor(FontType type, int maxSize, int minSize, const std::string &text, int x, int y,
                                   int maxWidth, ableem::Color textColor, XAlignment xAlign = XALIGN_LEFT);
    // a paragraph wrapped to width pixels (no |@X| markers); returns the height drawn
    int renderWrappedText(const ableem::Font &font, const std::string &text, int x, int y, int width,
                          ableem::Color textColor);
    // the text if it fits maxWidth in this font, else as much of it as does with "..." on the end
    std::string elide(const ableem::Font &font, const std::string &text, int maxWidth);

    //*******************************
    // Shadow
    //*******************************
    // A dark halo under light text, so it stays readable on a light background: the token is drawn in
    // `color` at each of the eight 1px offsets and once more 2px down-right (the drop), then in its own
    // colour on top. Dark text (the launcher's grey footer hints) and button markers are left alone.
    // Gui::loadAssets() sets it from the theme (classic.textShadow) for the classic screens; the launcher
    // swaps in its own (launcher.textShadow) around its frame.
    struct Shadow {
        bool enabled = false;
        ableem::Color color = ableem::Color(0, 0, 0, 150);

        // text at or above mid-grey gets the halo
        static bool isLight(const ableem::Color &c) { return (c.r * 299 + c.g * 587 + c.b * 114) / 1000 >= 128; }
    };
    void setShadow(const Shadow &shadow) { shadow_ = shadow; }
    const Shadow &shadow() const { return shadow_; }

    // Every run of text drawn through here is kept as a texture (the halo and the text composed once) and
    // copied thereafter, so a frame costs one copy per label instead of ten passes of one copy per glyph.
    // Gui drops the cache whenever the fonts go - a theme or language load, the display given up for an
    // emulator - since the entries are keyed on the font handles and hold textures of the renderer.
    void clearTextCache();

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

    int getCheckIconWidth(); // returns the width of the check icon texture.  used to compute the x position.
    static int align_xPosition(XAlignment xAlign, int x, int width);

    //*******************************
    // Text tokenizing structure routines
    //*******************************

    struct TextOrEmojiTokenInfo {
        std::string tokenString;
        ableem::Texture emoji; // valid() only if tokenString is an emoji marker such as "|@X|"
        ableem::Rect rect;     // position, width, and height of rendered text or emoji texture
                               // the x, y position is relative to the upper left corner of the string
    };

    // break up the text into tokens of text or the token of an emoji icon
    // build a vector of the text, emoji texture pointers, position, width and height of each token and the
    // total width and height of the entire line.
    struct AllTextOrEmojiTokenInfo {
        TextRenderer &text;
        std::vector<TextOrEmojiTokenInfo> tokenInfos;

        ableem::Font font;
        int x = 0, y = 0;       // upper left corner of the string on the display
        ableem::Size totalSize; // the total width and height of all the tokens
        bool useTextColor = false;
        ableem::Color textColor;
        bool drawBackgroundRect = false;

        AllTextOrEmojiTokenInfo(TextRenderer &_text, ableem::Font _font, const std::string &_string) : text(_text) {
            getTokenInfo(_font, _string);
        }
        void getTokenInfo(ableem::Font _font, const std::string &_text);

        void compute_xy_relativeOffsets(); // compute x offset, center the y offset of each token to the total height
        void setTextColor(ableem::Color color) {
            textColor = color;
            textColor.a = 255;
            useTextColor = true;
        }

        // renders/draws the text and emoji icons at the chosen position on the screen
        void render(int x, int y, XAlignment xAlign = XALIGN_LEFT);
    };

    //*******************************
    // Text Line Rendering routines
    //*******************************

    // renders/draws the line of text and emoji icons at the chosen position on the screen.  returns the height.
    // `color` (optional) is the text's colour instead of the font's own; the markers keep theirs
    int renderText(const ableem::Font &font, const std::string &text, int x, int y, XAlignment xAlign = XALIGN_LEFT,
                   const ableem::Color *color = nullptr);

    // if background == true it draws a solid grey box around/behind the text
    // this routine does not support emoji icons.  text only.
    int renderText_WithColor(const ableem::Font &font, const std::string &text, int x, int y, ableem::Color textColor,
                             XAlignment xAlign = XALIGN_LEFT, bool background = false);

    // returns rectangle height
    int renderTextLine(const std::string &text, int line, int yoffset = 0, XAlignment xAlign = XALIGN_LEFT,
                       int xoffset = 0,
                       ableem::Font font = ableem::Font()); // font will default to themeFont in the cpp

    int renderTextLineToColumns(const std::string &textLeft, const std::string &textRight, int xLeft, int xRight,
                                int line, int yoffset = 0, ableem::Font font = ableem::Font());

    int renderTextLineOptions(const std::string &text, int line, int yoffset = 0, XAlignment xAlign = XALIGN_LEFT,
                              int xoffset = 0);

    // the selected row, in PanelStyle's look (the band and the bar at the panel's edge); the row's height is
    // the font's line height
    void renderSelectionBox(int line, int yoffset, int xoffset = 0, ableem::Font font = ableem::Font());

    // a heading row between the rows: PanelStyle's faint band
    void renderLabelBox(int line, int yoffset);

    void renderTextChar(const std::string &text, int line, int yoffset, int posx);

private:
    // one run of text at (x, y), in `color` or the font's own if null, with the halo under it
    void drawRun(const ableem::Font &font, int x, int y, const ableem::Color *color, const std::string &run);

    struct CachedRun {
        ableem::Texture tex;
        int pad = 0; // the margin around the text the halo needs, drawn that much up and left of (x, y)
    };
    // the run's texture, composed on first use; invalid when the run has no size
    const CachedRun &cachedRun(const ableem::Font &font, const ableem::Color *color, bool halo, const std::string &run);
    std::map<std::string, CachedRun> runCache_;

    ableem::Renderer &renderer_;
    Fonts *fonts_ = nullptr;
    Theme &theme_;
    ableem::Font &themeFont_;
    std::map<std::string, ableem::Texture> &emojis_;
    Shadow shadow_;
    int checkIconRightMargin_ = 0;
};
