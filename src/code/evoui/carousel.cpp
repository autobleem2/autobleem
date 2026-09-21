//
// Carousel: the row of covers, out of GuiLauncher.
//
#include "carousel.h"
#include "../gui/gui.h"
#include "../core/model/timing.h"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <ableem/engine/log.h>

using namespace std;

//*******************************
// Carousel::setGames
//*******************************
void Carousel::setGames(const PsGames &gamesList, BoxKind kind) {
    freeTextures();

    // copy the gamesList into the carousel - just the games there are, however few - and the empty boxes
    // for the slots the games leave bare: the kind of box the games are in (the Lightgun set mixes PS1
    // games with RetroArch ones - the first game's kind), or `kind` for a row with no game to go by
    games.clear();
    for_each(begin(gamesList), end(gamesList), [&](const PsGamePtr &game) { games.emplace_back(game); });
    boxKind = games.empty() ? kind : (games.front()->foreign ? BoxKind::BigBox : BoxKind::JewelCase);
    leftFill.assign(PsCarousel::Slots, PsCarouselGame::emptyBox());
    rightFill.assign(PsCarousel::Slots, PsCarouselGame::emptyBox());

    PLOG_DEBUG << "Setting initial positions";
    selected = games.empty() ? -1 : 0;
    setInitialPositions(selected);
}

//*******************************
// Carousel::freeTextures
//*******************************
void Carousel::freeTextures() {
    forEachItem([](PsCarouselGame &item) { item.freeTex(); });
    placeholderTex_ = ableem::Texture();
}

//*******************************
// Carousel::itemAt / loadPlaceholderTexture
//*******************************
PsCarouselGame *Carousel::itemAt(int index) {
    const int count = static_cast<int>(games.size());
    if (index >= 0 && index < count)
        return &games[index];
    if (index < 0)
        return -index - 1 < static_cast<int>(leftFill.size()) ? &leftFill[-index - 1] : nullptr;
    return index - count < static_cast<int>(rightFill.size()) ? &rightFill[index - count] : nullptr;
}

void Carousel::loadPlaceholderTexture() {
    if (placeholderTex_.valid())
        return;
    PsCarouselGame box = PsCarouselGame::emptyBox();
    box.loadPlaceholderTex(gui_.renderer(), boxKind);
    placeholderTex_ = box.coverPng;
    placeholderContent_ = box.content;
    placeholderThickness_ = box.thickness;
}

//*******************************
// Carousel::selectNext / selectPrevious
//*******************************
void Carousel::selectNext() {
    if (canSelectNext())
        selected++;
}

void Carousel::selectPrevious() {
    if (canSelectPrevious())
        selected--;
}

//*******************************
// Carousel::setInitialPositions
//*******************************
// initialize a table with positions for covers: the selected game in the middle slot, its neighbours
// in the slots either side, and an empty box in every slot the row has no game for (an empty row is
// placed around index 0, which is then its first empty box)
void Carousel::setInitialPositions(int selectedIndex) {
    forEachItem([](PsCarouselGame &item) {
        item.visible = false;
        item.wanted = false;
    });

    const int middle = games.empty() ? 0 : selectedIndex;
    for (int slot = 0; slot < PsCarousel::Slots; slot++) {
        PsCarouselGame *item = itemAt(middle + slot - PsCarousel::MiddleSlot);
        if (!item)
            continue;
        item->current = positions.coverPositions[slot];
        item->visible = true;
        item->screenPointIndex = slot;
    }

    // the lookahead: the games just past the two ends of the row keep their textures ready
    for (int i = 1; i <= Lookahead; i++) {
        PsCarouselGame *before = itemAt(middle - PsCarousel::MiddleSlot - i);
        PsCarouselGame *after = itemAt(middle + PsCarousel::MiddleSlot + i);
        if (before && !before->placeholder)
            before->wanted = true;
        if (after && !after->placeholder)
            after->wanted = true;
    }

    bool placeholderShown = false;
    forEachItem([&](PsCarouselGame &item) {
        item.actual = item.current;
        item.destination = item.current;
        if (item.placeholder) {
            placeholderShown = placeholderShown || item.visible;
        } else if (item.visible) {
            item.wanted = true;
            item.loadTex(gui_.renderer()); // a no-op for a texture already there
            Gui::tickBusy();
        } else if (!item.wanted) {
            item.freeTex();
        }
    });
    if (placeholderShown) {
        loadPlaceholderTexture();
        forEachItem([&](PsCarouselGame &item) {
            if (item.placeholder) {
                item.coverPng = placeholderTex_;
                item.content = placeholderContent_;
                item.thickness = placeholderThickness_;
            }
        });
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
    forEachItem([&](PsCarouselGame &game) {
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
    });
}

//*******************************
// Carousel::scrollRight
//*******************************
// start scroll animation to previous game
void Carousel::scrollRight(int speed, bool eased) {
    scrolling = true;
    long time = gui_.platform().ticks();
    forEachItem([&](PsCarouselGame &game) {
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
    });
}

//*******************************
// Carousel::moveMainCover / snapMainCover
//*******************************
namespace {
// where the selected cover sits: in the row (the Games state), or raised above the game menu
PsScreenpoint mainCoverPoint(bool toGamesRow) {
    PsScreenpoint point;
    point.x = 640 - 113;
    point.y = toGamesRow ? 180 : 90;
    point.scale = 1;
    point.shade = toGamesRow ? 255 : 220;
    return point;
}
} // namespace

void Carousel::moveMainCover(bool toGamesRow) {
    if (!selectedIsValid())
        return;
    games[selected].destination = mainCoverPoint(toGamesRow);
    games[selected].animationStart = gui_.platform().ticks();
    games[selected].animationDuration = 200;
    games[selected].eased = true;
}

void Carousel::snapMainCover(bool toGamesRow) {
    if (!selectedIsValid())
        return;
    PsScreenpoint point = mainCoverPoint(toGamesRow);
    games[selected].destination = point;
    games[selected].actual = point;
    games[selected].current = point;
    games[selected].animationStart = 0;
}

//*******************************
// Carousel::updateVisibility
//*******************************
// update potentially visible covers to save the memory
void Carousel::updateVisibility() {
    bool allAnimationFinished = true;
    forEachItem([&](const PsCarouselGame &game) {
        if ((game.animationStart != 0) && game.visible) {
            allAnimationFinished = false;
        }
    });

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
    forEachItem([&](PsCarouselGame &game) {
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
    });
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
// an empty box is drawn this much darker than a cover in its slot, and this see-through
const float PlaceholderShade = 0.55f;
const unsigned char PlaceholderAlpha = 150;

// draws one cover as a box standing upright and turned `point.angle` degrees about its vertical axis: the
// front face as a perspective trapezoid, and the spine on the edge nearer to the viewer. The box is the
// game's `content` rect of its 226x226 texture (a jewel case fills it, a big box is the art's own shape),
// turned about that rect's middle, `thickness` of its width deep.
void renderTurnedCover(ableem::Renderer &renderer, const PsCarouselGame &game, const PsScreenpoint &point,
                       unsigned char alpha) {
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
    renderer.copyTrapezoid(tex, &spineSource, spineFront, spineBack,
                           ableem::Color(spineShade, spineShade, spineShade, alpha));

    // the face, a little darker the more it turns away from the light in front of the screen
    int faceShade = static_cast<int>(point.shade * (0.55f + 0.45f * std::fabs(c)));
    renderer.copyTrapezoid(tex, &content, project(-half, 0), project(half, 0),
                           ableem::Color(faceShade, faceShade, faceShade, alpha));
}
} // namespace

void Carousel::render() {
    ableem::Renderer &renderer = gui_.renderer();
    const float screenMiddle = renderer.width() / 2.0f;

    // far to near, so that a cover nearer the middle is drawn over the one behind it
    vector<const PsCarouselGame *> visible;
    forEachItem([&](const PsCarouselGame &game) {
        if (game.visible && game.coverPng.valid())
            visible.push_back(&game);
    });
    auto distanceFromMiddle = [&](const PsCarouselGame *game) {
        return std::fabs(game->actual.x + 226 * game->actual.scale / 2 - screenMiddle);
    };
    stable_sort(visible.begin(), visible.end(), [&](const PsCarouselGame *a, const PsCarouselGame *b) {
        return distanceFromMiddle(a) > distanceFromMiddle(b);
    });

    for (const PsCarouselGame *game : visible) {
        ableem::Texture currentGameTex = game->coverPng;
        PsScreenpoint point = game->actual;
        // an empty box: darker and see-through, so it reads as the shelf and not as a game
        const unsigned char alpha = game->placeholder ? PlaceholderAlpha : 255;
        if (game->placeholder)
            point.shade = static_cast<int>(point.shade * PlaceholderShade);

        if (std::fabs(point.angle) < 0.5f) {
            // facing the viewer: a plain copy, as the selected cover always is
            ableem::Rect coverRect(point.x, point.y, 226 * point.scale, 226 * point.scale);
            ableem::Rect fullRect(0, 0, 226, 226);
            currentGameTex.setColorMod(ableem::Color(point.shade, point.shade, point.shade));
            currentGameTex.setAlphaMod(alpha);
            renderer.copy(currentGameTex, &fullRect, &coverRect);
        } else {
            renderTurnedCover(renderer, *game, point, alpha);
        }
    }
}
