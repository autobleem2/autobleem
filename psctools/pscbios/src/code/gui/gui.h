//
// Created by screemer on 2018-12-19.
//
#pragma once

#include "../main.h"
#include "../util.h"
#include "../DirEntry.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_mixer.h>
#include <SDL2/SDL_ttf.h>
#include <string>
#include <memory>
#include <vector>
#include "../engine/config.h"
#include "../engine/padmapper.h"
#include "gui_sdl_wrapper.h"
#include "gui_font.h"
#include "../engine/network_controller.h"
#include "SDL_FontCache.h"


#define POS_LEFT 0
#define POS_CENTER 1
#define POS_RIGHT 2


//********************
// GuiBase
//********************
class GuiBase {
public:
    SDL_Shared<SDL_Window> window;
    SDL_Shared<SDL_Renderer> renderer;

    Fonts themeFonts;
    Fonts sonyFonts;
    Config cfg;
    bool inGuiLauncher = false;

    std::string getAutobleemThemePath();
    std::string getCurrentThemePath();
    std::string getCurrentThemeImagePath();
    std::string getCurrentThemeFontPath();
    std::string getCurrentThemeSoundPath();

    GuiBase();
    ~GuiBase();
};

//********************
// Gui
//********************
class Gui : public GuiBase {
private:

    Gui() { mapper.init(); }

    string themePath;

public:

    bool wirelessFound ;
    bool ethFound;
    bool wirelessActive;
    bool ethActive ;
    string wirelessAddr ;
    string ethAddr ;
    bool btActive;
    string btName;

    void refreshInfo();

    std::vector<SDL_Joystick *> joysticks;
    vector<string> joynames;
    PadMapper mapper;
    Inifile themeData;
    Inifile defaultData;

    NetworkController networkController;


    string getClock();
    string guiGetTZ();


    void loadAssets(bool reloadMusic = true);

    void display();

    void hideMouseCursor();

    void finish();

    void drawText(const std::string & text);

    void getEmojiTextTexture(SDL_Shared<SDL_Renderer> renderer, std::string text,
                             TTF_Font_Shared font, SDL_Shared<SDL_Texture> *texture, SDL_Rect *rect);

    static void splash(const std::string & message);

    void menuSelection();

    void saveSelection();

    void renderBackground();

    int renderLogo(bool small);

    void renderStatus(std::string  text, int pos=-1);

    void renderTextBar(int height=-1);

    // returns rectangle height
    int renderTextLine(std::string  text, int line,
                       int offset = 0, int position = POS_LEFT, int xoffset = 0,
                       TTF_Font_Shared font = TTF_Font_Shared());   // font will default to themeFont in the cpp

    // returns the SDL_Rect of the screen positions if your rendered this text with these args
    // this is basically renderTextLine but doesn't render the texct and instead returns the bounding rectangle
    SDL_Rect getTextRectangleOnScreen(std::string  text, int line,
                       int offset = 0, int position = POS_LEFT, int xoffset = 0,
                       TTF_Font_Shared font = TTF_Font_Shared());    // font will default to themeFont in the cpp

    int renderTextLineToColumns(const string &textLeft, const string &textRight, int xLeft, int xRight, int line,
                                int offset = 0, TTF_Font_Shared font = TTF_Font_Shared());

    int renderTextLineOptions(const std::string & text, int line, int offset = 0,  int position = POS_LEFT, int xoffset = 0);

    void renderSelectionBox(int line, int offset, int xoffset = 0, TTF_Font_Shared font = TTF_Font_Shared());

    void renderLabelBox(int line, int offset);

    void renderTextChar(const std::string & text, int line, int offset, int posx);

    void renderFreeSpace();

    void getTextAndRect(SDL_Shared<SDL_Renderer> renderer, int x, int y, const char *text,
                        TTF_Font_Shared font, SDL_Shared<SDL_Texture> *texture, SDL_Rect *rect);

    Uint8 getR(const std::string & val);

    Uint8 getG(const std::string & val);

    Uint8 getB(const std::string & val);

    SDL_Shared<SDL_Texture> getTextureByName(string name);

    void criticalException(const std::string & text);

    SDL_Shared<SDL_Texture>
    loadThemeTexture(SDL_Shared<SDL_Renderer> renderer, std::string themePath, std::string defaultPath, std::string texname);

    void exportDBToRetroarch();


    SDL_Rect backgroundRect;
    SDL_Rect logoRect;

    SDL_Shared<SDL_Texture> backgroundImg;
    SDL_Shared<SDL_Texture> logo;
    SDL_Shared<SDL_Texture> buttonX;
    SDL_Shared<SDL_Texture> buttonO;
    SDL_Shared<SDL_Texture> buttonT;
    SDL_Shared<SDL_Texture> buttonS;
    SDL_Shared<SDL_Texture> buttonStart;
    SDL_Shared<SDL_Texture> buttonSelect;
    SDL_Shared<SDL_Texture> buttonL1;
    SDL_Shared<SDL_Texture> buttonR1;
    SDL_Shared<SDL_Texture> buttonL2;
    SDL_Shared<SDL_Texture> buttonR2;
    SDL_Shared<SDL_Texture> buttonCheck;
    SDL_Shared<SDL_Texture> buttonUncheck;
    SDL_Shared<SDL_Texture> buttonEsc;
    SDL_Shared<SDL_Texture> buttonEnter;
    SDL_Shared<SDL_Texture> buttonBackspace;
    SDL_Shared<SDL_Texture> buttonTab;
    SDL_Shared<SDL_Texture> gamepadImg;


    Mix_Music *music = nullptr;
    TTF_Font_Shared themeFont;
    FC_Font * fastFont;

    Mix_Chunk *cancel = nullptr;
    Mix_Chunk *cursor = nullptr;
    Mix_Chunk *home_down = nullptr;
    Mix_Chunk *home_up = nullptr;
    Mix_Chunk *resume = nullptr;


    string padMapping;

    Gui(Gui const &) = delete;

    Gui &operator=(Gui const &) = delete;

    static std::shared_ptr<Gui> getInstance() {
        static std::shared_ptr<Gui> s{new Gui};
        return s;
    }


};
