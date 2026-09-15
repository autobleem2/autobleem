//
// Created by screemer on 2018-12-19.
//
#pragma once

#include "../main.h"
#include <ableem/ableem.h>
#include <string>
#include <memory>
#include "../engine/config.h"
#include "../engine/scanner.h"
#include "../launcher/ps_game.h"
#include "../util.h"
#include "gui_font.h"
#include "../environment.h"
#include "../session.h"

using namespace std;

#define SCREEN_WIDTH  ableem::GuiBase::ScreenWidth
#define SCREEN_HEIGHT ableem::GuiBase::ScreenHeight

enum XAlignment { XALIGN_LEFT, XALIGN_CENTER, XALIGN_RIGHT };

//********************
// Gui
//********************
// All SDL access lives in lib_ableem; Gui derives from ableem::GuiBase (window/renderer/input/audio) and adds
// everything theme/config/database related, which lib_ableem intentionally knows nothing about.
class Gui : public ableem::GuiBase {
private:

    Gui();

    std::string themePath;

public:
    IniFile themeData;
    IniFile defaultData;
    Config cfg;

    Fonts themeFonts;
    Fonts sonyFonts;

    std::string getCurrentThemePath();
    std::string getCurrentThemeImagePath();
    std::string getCurrentThemeFontPath();
    std::string getCurrentThemeSoundPath();

    void loadAssets(bool reloadMusic = true);

    void display(bool resume);

    void hideMouseCursor();

    void finish();


    static void splash(const std::string & message);

    void menuSelection();

    unsigned char getR(const std::string &val);

    unsigned char getG(const std::string &val);

    unsigned char getB(const std::string &val);

    void criticalException(const std::string &text);

    ableem::Texture loadThemeTexture(const string& themePath, const string& defaultPath, const string& texname);

    void stopAudio();
    void playMusic(bool customMusic, string musicPath);
    void restartAudio(int freq);
    void freeMusic();
    bool customMusic=false;
    int freq = 44100;
    string musicPath;

    ableem::Rect backgroundRect;
    ableem::Rect logoRect;

    ableem::Texture backgroundImg;
    ableem::Texture logo;
    ableem::Texture cdJewel;
    std::map<std::string, ableem::Texture> buttonTextureMap;

    ableem::Music music;
    ableem::Font themeFont;

    ableem::Sound cancel;
    ableem::Sound cursor;
    ableem::Sound home_down;
    ableem::Sound home_up;
    ableem::Sound resume;

    Gui(Gui const &) = delete;

    Gui &operator=(Gui const &) = delete;

    static std::shared_ptr<Gui> getInstance() {
        static std::shared_ptr<Gui> s{new Gui};
        return s;
    }

    static bool sortByTitle(const PsGamePtr &i, const PsGamePtr &j) { return lessCaseInsensitive(i->title, j->title); }

    //*******************************
    // Rect and Size routines
    //*******************************

    // return the w of the rendered text and h of the font
    ableem::Size getFontTextSize(const ableem::Font &font, const char *text=nullptr);
    ableem::Size getFontTextSize(const ableem::Font &font, const string& text="") {
        return getFontTextSize(font, text.c_str());
    }
    // return a rect at (x,y) sized to the rendered text
    ableem::Rect getFontTextRect(const ableem::Font &font, const char *text=nullptr, int x=0, int y=0);
    ableem::Rect getFontTextRect(const ableem::Font &font, const string& text="", int x=0, int y=0) {
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
        std::vector<TextOrEmojiTokenInfo> tokenInfos;

        ableem::Font font;
        int x=0,y=0;             // upper left corner of the string on the display
        ableem::Size totalSize;  // the total width and height of all the tokens
        bool useTextColor = false;
        ableem::Color textColor;
        bool drawBackgroundRect = false;

        AllTextOrEmojiTokenInfo() { }
        AllTextOrEmojiTokenInfo(ableem::Font _font, const std::string & _text) { getTokenInfo(_font, _text); }
        void getTokenInfo(ableem::Font _font, const std::string & _text);

        void compute_xy_relativeOffsets(); // compute x offset, center the y offset of each token to the total height
        void setTextColor(ableem::Color color) { textColor = color; textColor.a = 255; useTextColor = true; }

        // renders/draws the text and emoji icons at the chosen position on the screen
        void render(int x, int y, XAlignment xAlign = XALIGN_LEFT);
    };

    //*******************************
    // Text Line Rendering routines
    //*******************************

    // renders/draws the line of text and emoji icons at the chosen position on the screen.  returns the height.
    int renderText(const ableem::Font &font, const std::string & text, int x, int y, XAlignment xAlign = XALIGN_LEFT);

    // if background == true it draws a solid grey box around/behind the text
    // this routine does not support emoji icons.  text only.
    int renderText_WithColor(const ableem::Font &font, const std::string & text, int x, int y, ableem::Color textColor,
                             XAlignment xAlign = XALIGN_LEFT, bool background = false);

    // returns rectangle height
    int renderTextLine(const std::string & text, int line, int yoffset = 0,
                       XAlignment xAlign = XALIGN_LEFT, int xoffset = 0,
                       ableem::Font font = ableem::Font());   // font will default to themeFont in the cpp

    int renderTextLineToColumns(const string &textLeft, const string &textRight, int xLeft, int xRight, int line,
                                int yoffset = 0, ableem::Font font = ableem::Font());

    int renderTextLineOptions(const std::string & text, int line, int yoffset = 0,  XAlignment xAlign = XALIGN_LEFT, int xoffset = 0);

    void renderSelectionBox(int line, int yoffset, int xoffset = 0, ableem::Font font = ableem::Font());

    void renderLabelBox(int line, int yoffset);

    void renderTextChar(const std::string & text, int line, int yoffset, int posx);

    void renderFreeSpace();

    void renderBackground();

    int renderLogo(bool small);

    void renderStatus(const std::string & text, int pos=-1);

    void renderTextBar();

    void drawText(const std::string &text, const string &topLine="");
};
