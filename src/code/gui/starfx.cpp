//
// Created by screemer on 2019-07-31.
//

#include <random>
#include "starfx.h"
#include "../gui/gui.h"

StarFx::StarFx()
{
    std::default_random_engine generator;
    std::uniform_int_distribution<int> wdistribution(0,SCREEN_WIDTH);
    std::uniform_int_distribution<int> hdistribution(0, SCREEN_HEIGHT);

    for (int i=1;i<8;i++)
    {
        for (int star=0;star<STARS_PER_LAYER;star++) {
            Star s;
            s.speed = i*SPEED_DIFFERENCE;
            s.size =  i*SIZE_DIFFERENCE;
            s.x = wdistribution(generator);
            s.y = hdistribution(generator);
            float colorval = 255.0f*(i/9.0f);
            s.color = RGB(colorval,colorval,colorval);
            s.speed = i*SPEED_DIFFERENCE;
            s.size=i*SIZE_DIFFERENCE;

            starLayers[i-1].push_back(s);
        }
    }
}

void StarFx::render()
{
    for (int i=0;i<7;i++) {
        for (int star = 0; star < STARS_PER_LAYER; star++) {
            starLayers[i][star].y+= starLayers[i][star].speed;
            if (starLayers[i][star].y>SCREEN_HEIGHT) starLayers[i][star].y-=SCREEN_HEIGHT;
        }
    }
    for (int i=0;i<7;i++) {
        for (int star = 0; star < STARS_PER_LAYER; star++) {
            ableem::Rect rect;
            rect.x=starLayers[i][star].x;
            rect.y=starLayers[i][star].y;
            rect.w=starLayers[i][star].size;
            rect.h=starLayers[i][star].size;
            ableem::Color lastColor = renderer->drawColor();
            renderer->setDrawColor(ableem::Color(starLayers[i][star].color.r,starLayers[i][star].color.g,starLayers[i][star].color.b,255));
            renderer->fillRect(rect);
            renderer->setDrawColor(lastColor);
        }
    }
}