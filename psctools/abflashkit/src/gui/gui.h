//
// Created by screemer on 2020-03-02.
//

#ifndef ABFLASHKIT_GUI_H
#define ABFLASHKIT_GUI_H
#include <vector>
#include <string>

#include "gui_sdl_wrapper.h"
#include "gui_font_wrapper.h"
#include "../engine/padmapper.h"
#include "../engine/config.h"

using namespace std;

#define PCS_BTN_CROSS      2
#define PCS_BTN_CIRCLE     1
#define PCS_BTN_TRIANGLE   0
#define PCS_BTN_SQUARE     3

#define POS_LEFT 0
#define POS_CENTER 1
#define POS_RIGHT 2

class Gui {
public:

    Gui() {}
    SDL_Shared<SDL_Window> window;
    SDL_Shared<SDL_Renderer> renderer;

    SDL_Shared<SDL_Texture> buttonX;
    SDL_Shared<SDL_Texture> buttonO;
    SDL_Shared<SDL_Texture> buttonT;
    SDL_Shared<SDL_Texture> buttonS;


    void drawText(const string & text);
    void loadAssets();

    void init();
    PadMapper mapper;
    Config cfg;
    Gui(Gui const &) = delete;

    Gui &operator=(Gui const &) = delete;

    static std::shared_ptr<Gui> getInstance() {
        static std::shared_ptr<Gui> s{new Gui};
        return s;
    }

public:

    SDL_Rect backgroundRect;
    TTF_Font_Shared themeFont;
    SDL_Shared<SDL_Texture> backgroundImg;
    void renderBackground();
    void renderTextBar();
    void renderStatus(const std::string & text, int pos=-1);
    void getTextAndRect(SDL_Shared<SDL_Renderer> renderer, int x, int y, const char *text, TTF_Font_Shared font,
                             SDL_Shared<SDL_Texture> *texture, SDL_Rect *rect);
    void getEmojiTextTexture(SDL_Shared<SDL_Renderer> renderer, string text, TTF_Font_Shared font,
                             SDL_Shared<SDL_Texture> *texture,
                             SDL_Rect *rect);
    int renderTextLine(const std::string & text, int line,
                       int offset = 0, int position = POS_LEFT, int xoffset = 0,
                       TTF_Font_Shared font = TTF_Font_Shared());   // font will default to themeFont in the cpp
};


#endif //ABFLASHKIT_GUI_H
