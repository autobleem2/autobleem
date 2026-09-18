//
// Created by screemer on 2020-03-02.
//

#include "gui.h"
#include <SDL2/SDL_image.h>
#include <dirent.h>
#include <unistd.h>
#include <climits>
#include "../lang.h"

string getWorkingPath() {
#if defined(__x86_64__) || defined(_M_X64)

        char temp[1024];
        return (getcwd(temp, sizeof(temp)) ? string(temp) : string(""));

#else
    char temp[PATH_MAX];
        return (getcwd(temp, sizeof(temp)) ? string(temp) : string(""));
#endif
}

void Gui::init()
{
    SDL_version compiled;
    SDL_version linked;


    SDL_VERSION(&compiled);
    SDL_GetVersion(&linked);
    printf("We compiled against SDL version %d.%d.%d ...\n",
           compiled.major, compiled.minor, compiled.patch);
    printf("But we are linking against SDL version %d.%d.%d.\n",
           linked.major, linked.minor, linked.patch);

    SDL_Init(SDL_INIT_VIDEO);
    SDL_InitSubSystem(SDL_INIT_JOYSTICK);

    window = SDL_CreateWindow("ABFlashKit", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 1280, 720, 0);
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
#if defined(__x86_64__) || defined(_M_X64)

#else
    SDL_ShowCursor(SDL_DISABLE);
    SDL_SetWindowGrab(window, SDL_TRUE);
    SDL_SetRelativeMouseMode(SDL_TRUE);
#endif

    TTF_Init();
    loadAssets();
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "2");
    SDL_RaiseWindow(window);


    for (int i = 0; i < SDL_NumJoysticks(); i++) {
        SDL_JoystickOpen(i);


    }
}
void Gui::loadAssets()
{
    backgroundImg = IMG_LoadTexture(renderer,  "background.jpg");
    backgroundRect.x=0;
    backgroundRect.y=0;
    backgroundRect.w=1280;
    backgroundRect.h=720;

    buttonO = IMG_LoadTexture(renderer, "circle.png");
    buttonX = IMG_LoadTexture(renderer, "cross.png");
    buttonT = IMG_LoadTexture(renderer, "triangle.png");
    buttonS = IMG_LoadTexture(renderer, "square.png");
    themeFont = TTF_Font_Shared(TTF_OpenFont("zrnic.ttf", 24));
}

void Gui::renderBackground() {
    SDL_SetRenderDrawColor(renderer, 0x00, 0x00, 0x00, 0x00);
    SDL_RenderClear(renderer);
    SDL_RenderCopy(renderer, backgroundImg, nullptr, &backgroundRect);
}

void Gui::renderStatus(const string &text, int posy) {

    SDL_Shared<SDL_Texture> textTex;
    SDL_Rect textRec;
    SDL_SetRenderDrawColor(renderer,0,0, 0, 170);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_Rect rect;
    rect.x = 30;
    rect.y = 670;
    rect.w = 1220;
    rect.h = 33;
    SDL_RenderFillRect(renderer, &rect);

    getEmojiTextTexture(renderer, text, themeFont, &textTex, &textRec);
    int screencenter = 1280 / 2;
    textRec.x = screencenter - (textRec.w / 2);
    textRec.y = 672;
    if (posy!=-1)
    {
        textRec.y=posy;
    }
    if (textRec.w > 1220)
        textRec.w = 1220;
    SDL_RenderCopy(renderer, textTex, nullptr, &textRec);
}

//*******************************
// Gui::getTextAndRect
//*******************************
void Gui::getTextAndRect(SDL_Shared<SDL_Renderer> renderer, int x, int y, const char *text, TTF_Font_Shared font,
                         SDL_Shared<SDL_Texture> *texture, SDL_Rect *rect) {
    int text_width;
    int text_height;
    SDL_Shared<SDL_Surface> surface;

    SDL_Color textColor = {255,255, 255, 0};

    if (strlen(text) == 0) {
        *texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STATIC, 0, 0);
        rect->x = 0;
        rect->y = 0;
        rect->h = 0;
        rect->w = 0;
        return;
    }

    surface = TTF_RenderUTF8_Blended(font, text, textColor);
    *texture = SDL_CreateTextureFromSurface(renderer, surface);
    text_width = surface->w;
    text_height = surface->h;
    rect->x = x;
    rect->y = y;
    rect->w = text_width;
    rect->h = text_height;
}

void Gui::getEmojiTextTexture(SDL_Shared<SDL_Renderer> renderer, string text, TTF_Font_Shared font,
                              SDL_Shared<SDL_Texture> *texture,
                              SDL_Rect *rect) {
    if (text.empty()) text = " ";
    if (text.back() != '|') {
        text = text + "|";
    }

    vector<SDL_Shared<SDL_Texture>> textTexures;
    vector<string> textParts;
    std::string delimiter = "|";

    size_t pos = 0;
    std::string token;
    while ((pos = text.find(delimiter)) != std::string::npos) {
        token = text.substr(0, pos);
        if (!token.empty())
            textParts.push_back(token);
        text.erase(0, pos + delimiter.length());
    }

    for (const string &str:textParts) {
        if (str.empty()) continue;
        if (str[0] == '@') {
            string icon = str.substr(1);

            if (icon == "O") {
                textTexures.push_back(buttonO);
            }

            if (icon == "X") {
                textTexures.push_back(buttonX);
            }

            if (icon == "T") {
                textTexures.push_back(buttonT);
            }

            if (icon == "S") {
                textTexures.push_back(buttonS);
            }

        } else {
            SDL_Shared<SDL_Texture> textTex = nullptr;
            SDL_Rect textRec;
            getTextAndRect(renderer, 0, 662, str.c_str(), font, &textTex,
                           &textRec);
            textTexures.push_back(textTex);
        }
    }

    int w = 0;
    int h = 0;

    for (SDL_Shared<SDL_Texture> tex:textTexures) {
        Uint32 format;
        int access;
        int tw, th;
        SDL_QueryTexture(tex, &format, &access, &tw, &th);

        w += tw;
        if (th > h) h = th;
    }

    *texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, w, h);
    SDL_SetTextureBlendMode(*texture, SDL_BLENDMODE_NONE);
    SDL_SetRenderTarget(renderer, *texture);
    SDL_SetTextureBlendMode(*texture, SDL_BLENDMODE_BLEND);

    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);
    SDL_RenderClear(renderer);

    int xpos = 0;
    for (SDL_Shared<SDL_Texture> tex:textTexures) {
        Uint32 format;
        int access;
        int tw, th;
        SDL_QueryTexture(tex, &format, &access, &tw, &th);

        SDL_Rect posRect;
        posRect.x = xpos;

        posRect.y = 0;

        if (th != h) {
            posRect.y = (h - th) / 2;
        }
        posRect.w = tw;
        posRect.h = th;
        xpos += tw;
        SDL_RenderCopy(renderer, tex, nullptr, &posRect);
    }
    rect->w = w;
    rect->h = h;
    rect->x = 0;
    rect->y = 0;
    SDL_SetRenderTarget(renderer, nullptr);

    textTexures.clear();
}

void Gui::drawText(const string &text) {
    renderBackground();
    renderStatus(text);
    SDL_RenderPresent(renderer);
}

void Gui::renderTextBar() {

    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 127);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

    SDL_Rect rect2;
    rect2.x = 10;
    rect2.y = 10;
    rect2.w = 1280-10;
    rect2.h = 600;

    SDL_RenderFillRect(renderer, &rect2);
}


//*******************************
// Gui::renderTextLine
//*******************************
int Gui::renderTextLine(const string &text, int line, int offset,  int position, int xoffset, TTF_Font_Shared font) {
    if (!font)
        font = themeFont;   // default to themeFont

    SDL_Rect rect2;
    rect2.x = 10;
    rect2.y = 10;
    rect2.w = 1280-10;
    rect2.h = 600;

    SDL_Shared<SDL_Texture> textTex;
    SDL_Rect textRec;

    getTextAndRect(renderer, 0, 0, "*", font, &textTex, &textRec);
    int lineh = textRec.h;
    getEmojiTextTexture(renderer, text, font, &textTex, &textRec);
    textRec.x = rect2.x + 10 + xoffset;
    textRec.y = (lineh * line) + offset;

    if (line<0)
    {
        line=-line;
        textRec.y=line;
    }

    if (textRec.w >= (1280 - rect2.x * 4)) {
        textRec.w = (1280 - rect2.x * 4);
    }
    if (position==POS_CENTER) {
        textRec.x = (1280 / 2) - textRec.w / 2;
    }
    if (position==POS_RIGHT) {
        textRec.x = 1280 - textRec.x - textRec.w;
    }

    SDL_RenderCopy(renderer, textTex, nullptr, &textRec);

    return textRec.h;
}