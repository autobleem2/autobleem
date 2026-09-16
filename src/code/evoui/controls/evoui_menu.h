//
// Created by screemer on 2/16/19.
//

#pragma once

#include <ableem/ui/texture.h>
#include "evoui_obj.h"
#include <string>

#define TR_MENUON 0
#define TR_OPTION 1

//******************
// PsMenu
//******************
class PsMenu : public PsObj {
public:
    ableem::Texture settings;
    ableem::Texture guide;
    ableem::Texture memcard;
    ableem::Texture savestate;
    ableem::Texture resume;

    int x = 0, y = 0, oy = 0, ox = 0;
    int xoff[4] = {0,0,0,0};
    int yoff[4] = {0,0,0,0};

    float optionscales[4] = {1.0f,1.0f,1.0f,1.0f};

    float maxZoom = 1.5;

    int selOption=0;
    int animationStarted=0;
    int targety = 0;
    int duration = 0;
    bool active = false;

    bool foreign = false;
    int direction = 0;

    std::string path;

    void loadAssets();
    void freeAssets();
    void update(long time);
    void render();

    void setResumePic(std::string picturePath);

    int transition =0;

    PsMenu(std::string name1, std::string texPath = "");
};
