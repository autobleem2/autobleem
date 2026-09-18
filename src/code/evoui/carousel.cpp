//
// Carousel: the row of covers, out of GuiLauncher.
//
#include "carousel.h"

#include <algorithm>
#include <iostream>
#include <ableem/engine/log.h>

using namespace std;

//*******************************
// Carousel::setGames
//*******************************
void Carousel::setGames(const PsGames &gamesList) {
    freeTextures();

    // copy the gamesList into the carousel
    games.clear();
    for_each(begin(gamesList), end(gamesList), [&](const PsGamePtr &game) { games.emplace_back(game); });

    // save the actual number of (non-duplicated) games for the "showing" display
    numberOfNonDuplicatedGames = games.size();

    // if there are games in the carousel but not enough to fill it, duplicate the games until it is full
    if (games.size() > 0) {
        if (games.size() < 13) {    // if not enough games to fill the carousel
            // duplicate the gamesList until the carousel is full
            while (games.size() < 13) {
                for (const auto &game : gamesList)
                    games.emplace_back(game);
            }
        }
    }

    PLOG_DEBUG << "Setting initial positions";
    if (games.empty()) {
        selected = -1;
    } else {
        selected = 0;
        setInitialPositions(0);
    }
}

//*******************************
// Carousel::freeTextures
//*******************************
void Carousel::freeTextures() {
    for (auto &game : games) {
        game.freeTex();
    }
}

//*******************************
// Carousel::selectNext / selectPrevious
//*******************************
void Carousel::selectNext() {
    selected++;
    if (selected >= static_cast<int>(games.size())) {
        selected = 0;
    }
}

void Carousel::selectPrevious() {
    selected--;
    if (selected < 0) {
        selected = games.size() - 1;
    }
}

//*******************************
// Carousel::getNextId / getPreviousId
//*******************************
// just small method to get next / prev game
int Carousel::getNextId(int id) const {
    int next = id + 1;
    if (next >= static_cast<int>(games.size())) {
        return 0;
    }
    return next;
}

int Carousel::getPreviousId(int id) const {
    int prev = id - 1;
    if (prev < 0) {
        return games.size() - 1;
    }
    return prev;
}

//*******************************
// Carousel::setInitialPositions
//*******************************
// initialize a table with positions for covers
void Carousel::setInitialPositions(int selectedIndex) {
    for (auto &game : games) {
        game.visible = false;
    }

    games[selectedIndex].visible = true;
    games[selectedIndex].current = positions.coverPositions[6];
    games[selectedIndex].screenPointIndex = 6;

    // six to the left, six to the right; a game already placed (the list is short and wrapped) keeps its slot
    int prev = selectedIndex;
    for (int slot = 5; slot >= 0; slot--) {
        prev = getPreviousId(prev);
        if (!games[prev].visible) {
            games[prev].current = positions.coverPositions[slot];
            games[prev].visible = true;
            games[prev].screenPointIndex = slot;
        }
    }

    int next = selectedIndex;
    for (int slot = 7; slot <= 12; slot++) {
        next = getNextId(next);
        if (!games[next].visible) {
            games[next].current = positions.coverPositions[slot];
            games[next].visible = true;
            games[next].screenPointIndex = slot;
        }
    }

    for (auto &game : games) {
        game.actual = game.current;
        game.destination = game.current;
        if (game.visible) {
            game.loadTex(gui_.renderer());
        } else {
            game.freeTex();
        }
    }
}

//*******************************
// Carousel::scrollLeft
//*******************************
// start scroll animation to next game
void Carousel::scrollLeft(int speed) {
    scrolling = true;
    long time = gui_.platform().ticks();
    for (auto &game : games) {
        if (game.visible) {
            int nextIndex = game.screenPointIndex;

            if (game.screenPointIndex != 0) {
                nextIndex = game.screenPointIndex - 1;
            } else {
                game.visible = false;
            }
            game.destination = positions.coverPositions[nextIndex];
            game.animationDuration = speed;
            game.animationStart = time;

            game.screenPointIndex = nextIndex;
            game.current = game.actual;
        }
    }
}

//*******************************
// Carousel::scrollRight
//*******************************
// start scroll animation to previous game
void Carousel::scrollRight(int speed) {
    scrolling = true;
    long time = gui_.platform().ticks();
    for (auto &game : games) {
        if (game.visible) {
            int nextIndex = game.screenPointIndex;
            if (game.screenPointIndex != static_cast<int>(positions.coverPositions.size()) - 1) {
                nextIndex = game.screenPointIndex + 1;
            } else {
                game.visible = false;
            }
            game.destination = positions.coverPositions[nextIndex];
            game.animationDuration = speed;
            game.animationStart = time;

            game.screenPointIndex = nextIndex;
            game.current = game.actual;
        }
    }
}

//*******************************
// Carousel::moveMainCover
//*******************************
void Carousel::moveMainCover(bool toGamesRow) {
    if (selected == -1) {
        return;
    }
    PsScreenpoint point1;
    point1.x = 640 - 113;
    point1.y = 180;
    point1.scale = 1;
    point1.shade = 255;

    PsScreenpoint point2;
    point2.x = 640 - 113;
    point2.y = 90;
    point2.scale = 1;
    point2.shade = 220;

    long time = gui_.platform().ticks();

    if (selectedIsValid()) {
        games[selected].destination = toGamesRow ? point1 : point2;
        games[selected].animationStart = time;
        games[selected].animationDuration = 200;
    }
}

//*******************************
// Carousel::updateVisibility
//*******************************
// update potentially visible covers to save the memory
void Carousel::updateVisibility() {
    bool allAnimationFinished = true;
    for (const auto &game : games) {
        if ((game.animationStart != 0) && game.visible) {
            allAnimationFinished = false;
        }
    }

    if (allAnimationFinished && scrolling) {
        setInitialPositions(selected);
        scrolling = false;
    }
}

//*******************************
// Carousel::updatePositions
//*******************************
// this method runs during the loop to update positions of the covers during animation
void Carousel::updatePositions() {
    long currentTime = gui_.platform().ticks();
    for (auto &game : games) {
        if (game.visible) {
            if (game.animationStart != 0) {
                long position = currentTime - game.animationStart;
                float delta = position * 1.0f / game.animationDuration;
                game.actual.x = game.current.x + (game.destination.x - game.current.x) * delta;
                game.actual.y = game.current.y + (game.destination.y - game.current.y) * delta;
                game.actual.scale = game.current.scale + (game.destination.scale - game.current.scale) * delta;
                game.actual.shade = game.current.shade + (game.destination.shade - game.current.shade) * delta;

                if (delta > 1.0f) {
                    game.actual = game.destination;
                    game.current = game.destination;
                    game.animationStart = 0;
                }
            }
        }
    }
    updateVisibility();
}

//*******************************
// Carousel::render
//*******************************
void Carousel::render() {
    ableem::Renderer &renderer = gui_.renderer();
    for (const auto &game : games) {
        if (game.visible) {
            ableem::Texture currentGameTex = game.coverPng;
            PsScreenpoint point = game.actual;

            ableem::Rect coverRect;
            coverRect.x = point.x;
            coverRect.y = point.y;
            coverRect.w = 226 * point.scale;
            coverRect.h = 226 * point.scale;

            ableem::Rect fullRect;
            fullRect.x = 0;
            fullRect.y = 0;
            fullRect.w = 226;
            fullRect.h = 226;
            currentGameTex.setColorMod(ableem::Color(point.shade, point.shade, point.shade));
            renderer.copy(currentGameTex, &fullRect, &coverRect);
        }
    }
}
