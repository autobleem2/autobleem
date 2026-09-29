//
// Carousel: the row of covers, out of GuiLauncher.
//
#include "carousel.h"
#include "gui/gui.h"
#include "app_base.h"
#include "core/model/timing.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <ableem/engine/log.h>

using namespace std;

namespace {
// the selected cover's shine (Carousel::drawShine): its crossing, and the pause after the row comes to rest
// before it starts
const long ShineMs = 600;
const long ShineDelayMs = 120;
} // namespace

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
    texturesDrawnAt_ = gui_.renderer().targetsLost(); // composed just now
}

//*******************************
// Carousel::freeTextures
//*******************************
void Carousel::freeTextures() {
    layerValid_ = false; // a texture made later may get a freed one's address: never trust the signature then
    forEachItem([](PsCarouselGame &item) { item.freeTex(); });
    placeholderTex_ = ableem::Texture();
    glowTex_ = ableem::Texture();
    shineTex_ = ableem::Texture();
    targetPool_.clear();
    loader_.clear();
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
// Carousel::reloadLostTextures
//*******************************
void Carousel::reloadLostTextures() {
    const unsigned long lost = gui_.renderer().targetsLost();
    if (lost == texturesDrawnAt_)
        return;
    texturesDrawnAt_ = lost;
    freeTextures();
    bool placeholderShown = false;
    forEachItem([&](PsCarouselGame &item) {
        if (item.placeholder)
            placeholderShown = placeholderShown || item.visible;
        else if (item.visible || item.wanted)
            item.loadTex(gui_.renderer());
    });
    if (placeholderShown) {
        loadPlaceholderTexture();
        forEachItem([&](PsCarouselGame &item) {
            if (item.placeholder)
                item.coverPng = placeholderTex_;
        });
    }
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
void Carousel::setInitialPositions(int selectedIndex, bool waitForCovers) {
    layerValid_ = false; // textures are loaded and freed here
    placement_++;
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

    // the lookahead: the games just past the two ends of the row are decoded ahead
    for (int i = 1; i <= Lookahead; i++) {
        PsCarouselGame *before = itemAt(middle - PsCarousel::MiddleSlot - i);
        PsCarouselGame *after = itemAt(middle + PsCarousel::MiddleSlot + i);
        if (before && !before->placeholder)
            before->wanted = true;
        if (after && !after->placeholder)
            after->wanted = true;
    }

    ableem::Renderer &renderer = gui_.renderer();
    bool placeholderShown = false;
    forEachItem([&](PsCarouselGame &item) {
        item.actual = item.current;
        item.destination = item.current;
        if (item.placeholder) {
            placeholderShown = placeholderShown || item.visible;
            return;
        }
        if (!item.visible && !item.wanted)
            return; // kept or evicted below
        item.wanted = true;
        item.lastWanted = placement_;
        if (item.visible && !item.coverPng.valid()) {
            if (waitForCovers && !item.artFailed) {
                ableem::Image image;
                if (loader_.take(item.artPath(), image))
                    item.loadFromImage(renderer, image, spareTarget());
                else
                    item.loadTex(renderer, spareTarget());
                item.artFailed = !item.coverPng.valid();
                Gui::tickBusy();
            }
            // a cover still missing is shown as an empty box until it arrives
            placeholderShown = placeholderShown || !item.coverPng.valid();
        }
    });
    evictCovers();
    requestMissingCovers(middle);
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
// Carousel::coverCacheLimit / evictCovers / spareTarget
//*******************************
int Carousel::coverCacheLimit() {
    static const int limit = [] {
        const char *v = getenv("AB_COVER_CACHE");
        const int n = v && *v ? atoi(v) : 0;
        return n > 0 ? n : 80;
    }();
    return limit;
}

void Carousel::evictCovers() {
    vector<PsCarouselGame *> spare; // held but neither shown nor wanted
    int held = 0;
    for (auto &game : games) {
        if (!game.coverPng.valid())
            continue;
        held++;
        if (!game.visible && !game.wanted)
            spare.push_back(&game);
    }
    if (held <= coverCacheLimit())
        return;
    sort(spare.begin(), spare.end(),
         [](const PsCarouselGame *a, const PsCarouselGame *b) { return a->lastWanted < b->lastWanted; });
    for (PsCarouselGame *game : spare) {
        if (held <= coverCacheLimit())
            break;
        if (targetPool_.size() < 8)
            targetPool_.push_back(game->coverPng); // composed into again by the next cover
        game->freeTex();
        held--;
    }
}

ableem::Texture Carousel::spareTarget() {
    if (targetPool_.empty())
        return ableem::Texture();
    ableem::Texture target = targetPool_.back();
    targetPool_.pop_back();
    return target;
}

//*******************************
// Carousel::requestMissingCovers
//*******************************
void Carousel::requestMissingCovers(int middle) {
    vector<string> paths;
    const int count = static_cast<int>(games.size());
    for (int d = 0; d <= PsCarousel::MiddleSlot + Lookahead; d++) {
        for (int side : {1, -1}) {
            if (d == 0 && side < 0)
                continue;
            const int i = middle + side * d;
            if (i < 0 || i >= count)
                continue;
            PsCarouselGame &game = games[i];
            if (game.coverPng.valid() || game.artFailed || !(game.visible || game.wanted))
                continue;
            const string &path = game.artPath();
            if (!path.empty())
                paths.push_back(path);
        }
    }
    loader_.want(paths);
}

//*******************************
// Carousel::pumpCovers
//*******************************
bool Carousel::pumpCovers() {
    if (games.empty())
        return false;
    int budget = scrolling ? 1 : 2;
    bool did = false;
    const int count = static_cast<int>(games.size());
    const int middle = selectedIsValid() ? selected : 0;
    for (int d = 0; d <= PsCarousel::MiddleSlot + Lookahead && budget > 0; d++) {
        for (int side : {1, -1}) {
            if ((d == 0 && side < 0) || budget == 0)
                continue;
            const int i = middle + side * d;
            if (i < 0 || i >= count)
                continue;
            PsCarouselGame &game = games[i];
            if (game.coverPng.valid() || game.artFailed || !(game.visible || game.wanted))
                continue;
            const string &path = game.artPath();
            if (path.empty()) { // a dev host's internal game: its picture comes from the metadata
                game.loadTex(gui_.renderer(), spareTarget());
            } else {
                ableem::Image image;
                if (!loader_.take(path, image))
                    continue;
                game.loadFromImage(gui_.renderer(), image, spareTarget());
            }
            game.artFailed = !game.coverPng.valid();
            layerValid_ = false;
            did = true;
            budget--;
        }
    }
    return did;
}

//*******************************
// Carousel::stepStart
//*******************************
// when a scroll step begins: now - or, for a held stick's step that follows the last one (not eased, and
// that one ended no more than a step ago), exactly where the last one ended, so the row keeps its speed
// instead of losing the part of a frame between the end of a step and the start of the next
long Carousel::stepStart(int speed, bool eased) {
    const long now = gui_.platform().ticks();
    long start = now;
    if (!eased && chainEnd != 0 && now >= chainEnd && now - chainEnd < speed)
        start = chainEnd;
    chainEnd = eased ? 0 : start + speed;
    return start;
}

//*******************************
// Carousel::scrollLeft
//*******************************
// start scroll animation to next game
void Carousel::scrollLeft(int speed, bool eased) {
    scrolling = true;
    long time = stepStart(speed, eased);
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
    long time = stepStart(speed, eased);
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

bool Carousel::animating() const {
    if (scrolling)
        return true;
    if (shineAt_ != 0 && gui_.platform().ticks() < shineAt_ + ShineMs)
        return true;
    for (const auto &game : games)
        if (game.visible && game.animationStart != 0)
            return true;
    return false;
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
        setInitialPositions(selected, false); // the end of a scroll step: the new covers come in the background
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

// the box mirrored in a glossy floor under it: its bottom slice, upside down, from each edge's own foot
// down, fading out - the side covers' reflection half their height, the selected cover's a short one
// (it would run into Play). A turned box reflects its face and spine as the box itself shows them.
void renderReflection(ableem::Renderer &renderer, const PsCarouselGame &game, const PsScreenpoint &point) {
    const ableem::Texture &tex = game.coverPng;
    const ableem::Rect &content = game.content;
    const float nearness = std::min(1.0f, std::max(0.0f, (point.scale - 0.5f) / 0.5f));
    const float depth = 0.5f - 0.2f * nearness; // of the box's height
    const unsigned char startAlpha = 135;
    const ableem::Rect slice(content.x, content.y + static_cast<int>(content.h * (1.0f - depth)), content.w,
                             static_cast<int>(content.h * depth));
    auto mirrored = [&](const ableem::VerticalEdge &e) {
        const float height = e.bottom - e.top;
        return ableem::VerticalEdge(e.x, e.bottom, e.bottom + height * depth);
    };
    auto draw = [&](const ableem::Rect &src, const ableem::VerticalEdge &a, const ableem::VerticalEdge &b, int shade) {
        const auto s = static_cast<unsigned char>(std::min(255, std::max(0, shade)));
        renderer.copyTrapezoidFaded(tex, &src, mirrored(a), mirrored(b), ableem::Color(s, s, s, startAlpha),
                                    ableem::Color(s, s, s, 0), true);
    };

    const float width = content.w * point.scale, height = content.h * point.scale;
    const float cx = point.x + (content.x + content.w / 2.0f) * point.scale;
    const float cy = point.y + (content.y + content.h / 2.0f) * point.scale;
    if (std::fabs(point.angle) < 0.5f) {
        draw(slice, ableem::VerticalEdge(cx - width / 2, cy - height / 2, cy + height / 2),
             ableem::VerticalEdge(cx + width / 2, cy - height / 2, cy + height / 2), point.shade);
        return;
    }
    const float radians = point.angle * Pi / 180.0f;
    const float c = std::cos(radians), s = std::sin(radians);
    auto project = [&](float x, float z) {
        float worldX = x * c - z * s;
        float worldZ = x * s + z * c;
        float k = ViewerDistance / (ViewerDistance + worldZ);
        return ableem::VerticalEdge(cx + worldX * k, cy - height / 2 * k, cy + height / 2 * k);
    };
    const float half = width / 2;
    const float boxDepth = width * game.thickness;
    const bool nearEdgeIsLeft = point.angle > 0;
    const ableem::Rect spineSlice(nearEdgeIsLeft ? content.x + 3 : content.x + content.w - 5, slice.y, 2, slice.h);
    draw(spineSlice, project(nearEdgeIsLeft ? -half : half, 0), project(nearEdgeIsLeft ? -half : half, boxDepth),
         static_cast<int>(point.shade * 0.45f));
    draw(slice, project(-half, 0), project(half, 0), static_cast<int>(point.shade * (0.55f + 0.45f * std::fabs(c))));
}
} // namespace

void Carousel::render() {
    ableem::Renderer &renderer = gui_.renderer();
    reloadLostTextures();
    const float screenMiddle = renderer.width() / 2.0f;

    // far to near, so that a cover nearer the middle is drawn over the one behind it
    vector<const PsCarouselGame *> visible;
    forEachItem([&](const PsCarouselGame &game) {
        // a game whose cover is still on its way stands as an empty box (drawCovers)
        if (game.visible && (game.coverPng.valid() || (!game.placeholder && placeholderTex_.valid())))
            visible.push_back(&game);
    });
    auto distanceFromMiddle = [&](const PsCarouselGame *game) {
        return std::fabs(game->actual.x + 226 * game->actual.scale / 2 - screenMiddle);
    };
    stable_sort(visible.begin(), visible.end(), [&](const PsCarouselGame *a, const PsCarouselGame *b) {
        return distanceFromMiddle(a) > distanceFromMiddle(b);
    });

    const long now = gui_.platform().ticks();
    drawGlow(now);

    // a render target is not multisampled: with MSAA on, the row is drawn to the screen every frame, so the
    // covers keep their smooth edges at rest too (a few dozen geometry calls - the layer's saving is small then)
    static const bool layersOn = [this] {
        const char *v = getenv("AB_LAYERS");
        return !(v && strcmp(v, "0") == 0) && gui_.platform().multisampleSamples() == 0;
    }();
    if (!layersOn) {
        drawCovers(visible);
        drawShine(now);
        return;
    }

    // what this frame would draw
    vector<uintptr_t> signature;
    signature.reserve(visible.size() * 7 + 1);
    signature.push_back(renderer.targetsLost());
    auto bits = [](float f) {
        uint32_t u;
        memcpy(&u, &f, sizeof(u));
        return static_cast<uintptr_t>(u);
    };
    for (const PsCarouselGame *game : visible) {
        const PsScreenpoint &p = game->actual;
        signature.push_back(reinterpret_cast<uintptr_t>(game->coverPng.native()));
        signature.push_back(game->placeholder ? 1 : 0);
        signature.push_back(static_cast<uintptr_t>(p.x));
        signature.push_back(static_cast<uintptr_t>(p.y));
        signature.push_back(bits(p.scale));
        signature.push_back(bits(p.angle));
        signature.push_back(static_cast<uintptr_t>(p.shade));
    }

    if (!(layerValid_ && signature == layerSignature_)) {
        layerValid_ = false;
        const bool still = signature == lastSignature_; // the same as the frame before: the row stands
        lastSignature_ = signature;
        if (!still) {
            drawCovers(visible); // moving: straight to the screen, at the full rate
            drawShine(now);
            return;
        }
        if (!layer_.valid()) {
            layer_ = ableem::Texture::createTarget(renderer, renderer.width(), renderer.height());
            layer_.setBlendMode(ableem::BlendMode::Premultiplied);
        }
        const ableem::Color keep = renderer.drawColor();
        renderer.pushTarget(&layer_);
        renderer.setBlendMode(ableem::BlendMode::None);
        renderer.setDrawColor(ableem::Color(0, 0, 0, 0));
        renderer.fillRect();
        renderer.setBlendMode(ableem::BlendMode::Blend);
        drawCovers(visible);
        renderer.popTarget();
        renderer.setDrawColor(keep);
        layerSignature_ = signature;
        layerValid_ = true;
    }
    renderer.copy(layer_);
    drawShine(now);
}

//*******************************
// Carousel::drawGlow / drawShine
//*******************************
namespace {
// the selected cover's centre and size on screen, and how much it is "the selected one" - 1 in the middle
// slot, 0 once it is half-way to the next (so the light hands over as the row scrolls)
struct Spot {
    float cx = 0, cy = 0, size = 0, strength = 0;
};
Spot spotOf(const PsCarouselGame &game, float screenMiddle) {
    Spot spot;
    const PsScreenpoint &p = game.actual;
    const ableem::Rect &content = game.content;
    spot.size = content.w * p.scale;
    spot.cx = p.x + (content.x + content.w / 2.0f) * p.scale;
    spot.cy = p.y + (content.y + content.h / 2.0f) * p.scale;
    const float off = std::min(1.0f, std::fabs(spot.cx - screenMiddle) / 150.0f);
    const float big = std::min(1.0f, std::max(0.0f, (p.scale - 0.5f) / 0.5f));
    spot.strength = (1.0f - off) * big;
    return spot;
}
} // namespace

void Carousel::drawGlow(long now) {
    if (!selectedIsValid() || !games[selected].visible || !games[selected].coverPng.valid())
        return;
    ableem::Renderer &renderer = gui_.renderer();
    const Spot spot = spotOf(games[selected], renderer.width() / 2.0f);
    if (spot.strength < 0.02f)
        return;
    if (!glowTex_.valid()) {
        // a soft square of light, brightest in the middle: nested rects, each adding a little, in a target
        // (so premultiplied), drawn scaled - the linear filter rounds the steps off
        const int size = 128, rings = 32;
        glowTex_ = ableem::Texture::createTarget(renderer, size, size);
        glowTex_.setBlendMode(ableem::BlendMode::Premultiplied);
        const ableem::Color keep = renderer.drawColor();
        renderer.pushTarget(&glowTex_);
        renderer.setBlendMode(ableem::BlendMode::None);
        renderer.setDrawColor(ableem::Color(0, 0, 0, 0));
        renderer.fillRect();
        renderer.setBlendMode(ableem::BlendMode::Blend);
        renderer.setDrawColor(ableem::Color(255, 255, 255, 12));
        for (int i = 0; i < rings; i++) {
            const int inset = i * (size / 2) / rings;
            const ableem::Rect r(inset, inset, size - 2 * inset, size - 2 * inset);
            renderer.fillRects(&r, 1);
        }
        renderer.popTarget();
        renderer.setDrawColor(keep);
    }
    const ableem::ThemeColor &selection = AppBase::get().theme().launcher().colors.selection;
    const ableem::Color light =
        selection.set ? ableem::Color(selection.r, selection.g, selection.b) : ableem::Color(255, 255, 255);
    // breathing, 0.8..1 every 5.6 s
    const float pulse = 0.9f + 0.1f * std::sin(static_cast<float>(now) / 900.0f);
    const float k = 0.55f * spot.strength * pulse; // a hint of light, not a lamp (the owner's look check)
    // premultiplied: the colour and the alpha both scale, or the light would not fade with them
    glowTex_.setColorMod(ableem::Color(static_cast<unsigned char>(light.r * k), static_cast<unsigned char>(light.g * k),
                                       static_cast<unsigned char>(light.b * k)));
    glowTex_.setAlphaMod(static_cast<unsigned char>(255 * k));
    const float margin = 44.0f * spot.size / 222.0f;
    const float side = spot.size + 2 * margin;
    renderer.copy(glowTex_, nullptr, ableem::FRect(spot.cx - side / 2, spot.cy - side / 2, side, side));
}

void Carousel::drawShine(long now) {
    if (!selectedIsValid())
        return;
    const PsCarouselGame &game = games[selected];
    const bool resting = !scrolling && game.animationStart == 0;
    if (!resting) {
        shineFor_ = -1; // the row moved: the next rest shines again
        shineAt_ = 0;
        return;
    }
    if (shineFor_ != selected) {
        shineFor_ = selected;
        const bool wanted = AppBase::get().config().inifile.values["covershine"] != "false";
        shineAt_ = wanted ? now + ShineDelayMs : 0;
    }
    if (shineAt_ == 0 || now < shineAt_ || now >= shineAt_ + ShineMs || !game.coverPng.valid() ||
        std::fabs(game.actual.angle) >= 0.5f)
        return;
    ableem::Renderer &renderer = gui_.renderer();
    if (!shineTex_.valid()) {
        // a band of light across, clear at both sides: each column its own alpha, added onto the cover
        const int width = 64;
        shineTex_ = ableem::Texture::createTarget(renderer, width, 4);
        shineTex_.setBlendMode(ableem::BlendMode::Add);
        const ableem::Color keep = renderer.drawColor();
        renderer.pushTarget(&shineTex_);
        renderer.setBlendMode(ableem::BlendMode::None);
        for (int x = 0; x < width; x++) {
            const float t = std::sin(Pi * (x + 0.5f) / width);
            renderer.setDrawColor(ableem::Color(255, 255, 255, static_cast<unsigned char>(110 * t * t)));
            const ableem::Rect column(x, 0, 1, 4);
            renderer.fillRects(&column, 1);
        }
        renderer.popTarget();
        renderer.setBlendMode(ableem::BlendMode::Blend);
        renderer.setDrawColor(keep);
    }
    // the band runs from just off the cover's left edge to just off its right, clipped to the cover
    const Spot spot = spotOf(game, renderer.width() / 2.0f);
    const float t = static_cast<float>(now - shineAt_) / ShineMs;
    const float eased = t * t * (3.0f - 2.0f * t);
    const float left = spot.cx - spot.size / 2, right = spot.cx + spot.size / 2;
    const float band = spot.size * 0.35f;
    const float from = left - band + (right - left + band) * eased;
    const float x0 = std::max(left, from), x1 = std::min(right, from + band);
    if (x1 <= x0)
        return;
    const int texW = 64;
    const int u0 = static_cast<int>((x0 - from) / band * texW),
              u1 = static_cast<int>(std::ceil((x1 - from) / band * texW));
    const ableem::Rect src(u0, 0, std::max(1, std::min(texW, u1) - u0), 4);
    renderer.copy(shineTex_, &src, ableem::FRect(x0, spot.cy - spot.size / 2, x1 - x0, spot.size));
}

//*******************************
// Carousel::drawCovers
//*******************************
void Carousel::drawCovers(const vector<const PsCarouselGame *> &visible) {
    ableem::Renderer &renderer = gui_.renderer();
    for (const PsCarouselGame *game : visible) {
        if (!game->placeholder && game->coverPng.valid())
            renderReflection(renderer, *game, game->actual);
    }
    PsCarouselGame standIn = PsCarouselGame::emptyBox();
    for (const PsCarouselGame *game : visible) {
        if (!game->coverPng.valid()) { // its cover has not arrived yet: an empty box where it will be
            standIn.actual = game->actual;
            standIn.coverPng = placeholderTex_;
            standIn.content = placeholderContent_;
            standIn.thickness = placeholderThickness_;
            game = &standIn;
        }
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
