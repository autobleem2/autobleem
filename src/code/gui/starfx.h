//
// Created by screemer on 2019-07-31.
//

#pragma once

#include <vector>
#include <ableem/ui/renderer.h>

#define STARS_PER_LAYER 60
#define SPEED_DIFFERENCE 1.0f
#define SIZE_DIFFERENCE 0.34f

using namespace std;

class RGB {
public:
    RGB() : RGB(0, 0, 0, 0) {};

    RGB(unsigned char r, unsigned char g, unsigned char b) : RGB(r, g, b, 0xFF) {};

    RGB(unsigned char r, unsigned char g, unsigned char b, unsigned char a) {
        this->r = r;
        this->g = g;
        this->b = b;
        this->a = a;
    }

    unsigned char r, g, b, a;


};

class Star {
public:
    float x, y;
    RGB color;
    float speed;
    float size;
};

class StarFx {
public:
    StarFx();
    void render();
    ableem::Renderer *renderer = nullptr;   // set by the owning screen before the first render()

private:
    vector<Star> starLayers[7];
};
