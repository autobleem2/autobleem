//
// Created by screemer on 2/13/19.
//

#pragma once

#include "../core/model/ps_game.h"
#include <vector>
#include <ableem/ui/renderer.h>
#include <ableem/ui/texture.h>
#include "../core/services/retroarch.h"

//******************
// PsScreenpoint
//******************
// Where a cover is on screen: the top-left of its (unturned) box, its size as a fraction of the 226x226
// cover texture, its brightness, and how far it is turned about its vertical axis - 0 faces the viewer,
// negative is a cover left of the middle facing in towards it, positive one on the right.
class PsScreenpoint {
public:
    int x = 0;
    int y = 0;
    float scale = 0.0;
    int shade = 0;
    float angle = 0.0f; // degrees
};

//******************
// PsCarouselGame
//******************
// Note that if there are less than 13 games in the gamesList the games are duplicated to fill out the carousel.
// So more than one PsCarouselGame could be using the same PsGamePtr.
struct PsCarouselGame : public PsGamePtr {
    PsCarouselGame() = delete;
    explicit PsCarouselGame(const PsGamePtr &game) : PsGamePtr(game) {}

    PsScreenpoint current;
    PsScreenpoint destination;
    PsScreenpoint actual;
    int screenPointIndex = -1;
    int nextPointIndex = -1;
    long animationStart = 0;
    long animationDuration = 0;
    bool visible = false;
    ableem::Texture coverPng;

    void loadTex(ableem::Renderer &renderer);
    void freeTex();
};

//******************
// PsCarousel
//******************
class PsCarousel {
public:
    int activeItem = 0;
    int cycleMax = 0;
    int cycleDuration = 0;
    int previousActiveElement = 0;
    std::vector<PsScreenpoint> coverPositions;

    void initCoverPositions();

    // the `distance`th cover out from the middle (1..6), on the left (side 0) or the right (side 1)
    PsScreenpoint createCoverPoint(int distance, int side);
};
