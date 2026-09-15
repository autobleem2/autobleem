//
// Created by screemer on 2/11/19.
//

#pragma once

#include <ableem/renderer.h>
#include <ableem/texture.h>
#include <string>
#include "../gui/gui.h"

//******************
// PsObj
//******************
class PsObj {
public:
    virtual ~PsObj() {}

    std::shared_ptr<Gui> gui;
    ableem::Renderer &renderer;
    int x = 0, y = 0, w = 0, h = 0;
    int ox = 0, oy = 0, ow = 0, oh = 0;

    std::string name;

    ableem::Texture tex;

    bool visible = false;
    long lastTime = 0;

    PsObj(std::string name1, std::string texPath = "");

    virtual void load(const std::string & imagePath);

    virtual void destroy();

    virtual void update(long /*time*/) {};

    virtual void render();
};
