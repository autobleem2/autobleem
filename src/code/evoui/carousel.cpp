//
// Carousel: the row of covers, out of GuiLauncher.
//
#include "carousel.h"
#include "../core/model/timing.h"

#include <algorithm>
#include <cmath>
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
        if (games.size() < PsCarousel::Slots) { // if not enough games to fill the carousel
            // duplicate the gamesList until the carousel is full
            while (games.size() < PsCarousel::Slots) {
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
        game.wanted = false;
    }

    games[selectedIndex].visible = true;
    games[selectedIndex].current = positions.coverPositions[PsCarousel::MiddleSlot];
    games[selectedIndex].screenPointIndex = PsCarousel::MiddleSlot;

    // SideCovers to the left, as many to the right; a game already placed (the list is short and wrapped)
    // keeps its slot
    int prev = selectedIndex;
    for (int slot = PsCarousel::MiddleSlot - 1; slot >= 0; slot--) {
        prev = getPreviousId(prev);
        if (!games[prev].visible) {
            games[prev].current = positions.coverPositions[slot];
            games[prev].visible = true;
            games[prev].screenPointIndex = slot;
        }
    }

    int next = selectedIndex;
    for (int slot = PsCarousel::MiddleSlot + 1; slot < PsCarousel::Slots; slot++) {
        next = getNextId(next);
        if (!games[next].visible) {
            games[next].current = positions.coverPositions[slot];
            games[next].visible = true;
            games[next].screenPointIndex = slot;
        }
    }

    // the lookahead: `prev` and `next` are the games at the two ends of the row now
    for (int i = 0; i < Lookahead; i++) {
        prev = getPreviousId(prev);
        next = getNextId(next);
        games[prev].wanted = true;
        games[next].wanted = true;
    }

    for (auto &game : games) {
        game.actual = game.current;
        game.destination = game.current;
        if (game.visible) {
            game.wanted = true;
            game.loadTex(gui_.renderer()); // a no-op for a texture already there
        } else if (!game.wanted) {
            game.freeTex();
        }
    }
}

//*******************************
// Carousel::loadOneMissingTexture
//*******************************
bool Carousel::loadOneMissingTexture() {
    for (auto &game : games) {
        if (game.wanted && !game.coverPng.valid()) {
            game.loadTex(gui_.renderer());
            return true;
        }
    }
    return false;
}

//*******************************
// Carousel::scrollLeft
//*******************************
// start scroll animation to next game
void Carousel::scrollLeft(int speed, bool eased) {
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
            game.eased = eased;

            game.screenPointIndex = nextIndex;
            game.current = game.actual;
        }
    }
}

//*******************************
// Carousel::scrollRight
//*******************************
// start scroll animation to previous game
void Carousel::scrollRight(int speed, bool eased) {
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
            game.eased = eased;

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
        games[selected].eased = true;
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
                if (game.eased && delta < 1.0f)
                    delta = easeOutCubic(delta);
                game.actual.x = game.current.x + (game.destination.x - game.current.x) * delta;
                game.actual.y = game.current.y + (game.destination.y - game.current.y) * delta;
                game.actual.scale = game.current.scale + (game.destination.scale - game.current.scale) * delta;
                game.actual.shade = game.current.shade + (game.destination.shade - game.current.shade) * delta;
                game.actual.angle = game.current.angle + (game.destination.angle - game.current.angle) * delta;

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
namespace {
// the pseudo-3D of the side covers: how far the viewer is from the screen, in pixels, which sets how much
// the near edge of a turned cover grows and the far one shrinks
const float ViewerDistance = 600.0f;
const float Pi = 3.14159265f;

// draws one cover as a box standing upright and turned `point.angle` degrees about its vertical axis: the
// front face as a perspective trapezoid, and the spine on the edge nearer to the viewer. The box is the
// game's `content` rect of its 226x226 texture (a jewel case fills it, a big box is the art's own shape),
// turned about that rect's middle, `thickness` of its width deep.
void renderTurnedCover(ableem::Renderer &renderer, const PsCarouselGame &game, const PsScreenpoint &point) {
    const ableem::Texture &tex = game.coverPng;
    const ableem::Rect &content = game.content;
    const float width = content.w * point.scale, height = content.h * point.scale;
    const float cx = point.x + (content.x + content.w / 2.0f) * point.scale;
    const float cy = point.y + (content.y + content.h / 2.0f) * point.scale;
    const float radians = point.angle * Pi / 180.0f;
    const float c = std::cos(radians), s = std::sin(radians);
    // a point in the cover's own plane (x along its width from the middle, z its depth away from the
    // viewer) turned, projected, and given back as the vertical edge it makes on screen
    auto project = [&](float x, float z) {
        float worldX = x * c - z * s;
        float worldZ = x * s + z * c;
        float k = ViewerDistance / (ViewerDistance + worldZ);
        return ableem::VerticalEdge(cx + worldX * k, cy - height / 2 * k, cy + height / 2 * k);
    };

    const float half = width / 2;
    const float depth = width * game.thickness;
    // the spine: the near edge is the left one for a cover on the right (turned to face left) and vice
    // versa; textured with a strip a few pixels in from that edge of the box, so it takes the box's colour
    const bool nearEdgeIsLeft = point.angle > 0;
    ableem::Rect spineSource(nearEdgeIsLeft ? content.x + 3 : content.x + content.w - 5, content.y, 2, content.h);
    ableem::VerticalEdge spineFront = project(nearEdgeIsLeft ? -half : half, 0);
    ableem::VerticalEdge spineBack = project(nearEdgeIsLeft ? -half : half, depth);
    int spineShade = static_cast<int>(point.shade * 0.45f);
    renderer.copyTrapezoid(tex, &spineSource, spineFront, spineBack, ableem::Color(spineShade, spineShade, spineShade));

    // the face, a little darker the more it turns away from the light in front of the screen
    int faceShade = static_cast<int>(point.shade * (0.55f + 0.45f * std::fabs(c)));
    renderer.copyTrapezoid(tex, &content, project(-half, 0), project(half, 0),
                           ableem::Color(faceShade, faceShade, faceShade));
}
} // namespace

void Carousel::render() {
    ableem::Renderer &renderer = gui_.renderer();
    const float screenMiddle = renderer.width() / 2.0f;

    // far to near, so that a cover nearer the middle is drawn over the one behind it
    vector<const PsCarouselGame *> visible;
    for (const auto &game : games) {
        if (game.visible)
            visible.push_back(&game);
    }
    auto distanceFromMiddle = [&](const PsCarouselGame *game) {
        return std::fabs(game->actual.x + 226 * game->actual.scale / 2 - screenMiddle);
    };
    stable_sort(visible.begin(), visible.end(), [&](const PsCarouselGame *a, const PsCarouselGame *b) {
        return distanceFromMiddle(a) > distanceFromMiddle(b);
    });

    for (const PsCarouselGame *game : visible) {
        ableem::Texture currentGameTex = game->coverPng;
        PsScreenpoint point = game->actual;

        if (std::fabs(point.angle) < 0.5f) {
            // facing the viewer: a plain copy, as the selected cover always is
            ableem::Rect coverRect(point.x, point.y, 226 * point.scale, 226 * point.scale);
            ableem::Rect fullRect(0, 0, 226, 226);
            currentGameTex.setColorMod(ableem::Color(point.shade, point.shade, point.shade));
            renderer.copy(currentGameTex, &fullRect, &coverRect);
        } else {
            renderTurnedCover(renderer, *game, point);
        }
    }
}
