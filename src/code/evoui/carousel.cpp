//
// Carousel: the row of covers, out of GuiLauncher.
//
#include "carousel.h"
#include "gui/gui.h"
#include "app_base.h"
#include "core/model/cover_light.h"

#include <ab_gui/ambient.h>
#include <ab_gui/screen_stack.h>

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
    // the old row's moves stop with it (its covers go; the new ones start at rest)
    movesOwner_.cancel();
    mainMove_ = CarouselMotion::MoveRef();

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
    PsCarouselGame::releaseNoArtLayers();
    placeholderTex_ = ableem::Texture();
    glowTex_ = ableem::Texture();
    shineTex_ = ableem::Texture();
    // the shine's state goes with its texture: a new row selects game 0 again, and a stale shineFor_ == 0 meant
    // "already shone" - no crossing, and the freed texture never reloaded (BUG-35)
    shineFor_ = -1;
    shineAt_ = 0;
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
            // a new row never waits for a decode: a cover the loader already has goes up now, the rest show the
            // default box and arrive through pumpCovers() - a row of big box art opened in seconds on the console
            // when every slot's PNG was decoded here first
            if (waitForCovers && !item.artFailed) {
                ableem::Image image;
                const string &path = item.artPath();
                if (path.empty()) {
                    item.loadTex(renderer, spareTarget()); // a dev host's internal game: no file to decode
                    item.artFailed = !item.coverPng.valid();
                } else if (loader_.take(path, image)) {
                    item.loadFromImage(renderer, image, spareTarget());
                    item.artFailed = !item.coverPng.valid();
                }
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
// Carousel::stepStart / uiTweens / startMove
//*******************************
// when a scroll step begins: now - or, for a held stick's step that follows the last one (not eased, and
// that one ended no more than a step ago), exactly where the last one ended, so the row keeps its speed
// instead of losing the part of a frame between the end of a step and the start of the next
long Carousel::stepStart(int speed, bool eased) {
    return CarouselMotion::stepStart(gui_.platform().ticks(), chainEnd, speed, eased);
}

// the program's one Tweens (the screen stack's), clocked by the platform's ticks
abgui::Tweens &Carousel::uiTweens() {
    return Gui::getInstance()->uiContext().stack().tweens();
}

// a move of the covers that set off together: one tween run from `startedAt` (G5o5, carousel_motion.h)
CarouselMotion::MoveRef Carousel::startMove(long startedAt, int durationMs, bool eased) {
    return moves_.start(uiTweens(), movesOwner_, static_cast<unsigned int>(startedAt),
                        static_cast<unsigned int>(durationMs), eased);
}

//*******************************
// Carousel::scrollLeft
//*******************************
// start scroll animation to next game
void Carousel::scrollLeft(int speed, bool eased) {
    scrolling = true;
    const CarouselMotion::MoveRef move = startMove(stepStart(speed, eased), speed, eased);
    forEachItem([&](PsCarouselGame &game) {
        if (game.visible) {
            int nextIndex = game.screenPointIndex;

            if (game.screenPointIndex != 0) {
                nextIndex = game.screenPointIndex - 1;
            } else {
                game.visible = false;
            }
            game.destination = positions.coverPositions[nextIndex];
            game.move = move;

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
    const CarouselMotion::MoveRef move = startMove(stepStart(speed, eased), speed, eased);
    forEachItem([&](PsCarouselGame &game) {
        if (game.visible) {
            int nextIndex = game.screenPointIndex;
            if (game.screenPointIndex != static_cast<int>(positions.coverPositions.size()) - 1) {
                nextIndex = game.screenPointIndex + 1;
            } else {
                game.visible = false;
            }
            game.destination = positions.coverPositions[nextIndex];
            game.move = move;

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
    mainMove_ = startMove(gui_.platform().ticks(), 200, true);
    games[selected].move = mainMove_;
}

bool Carousel::animating() const {
    if (scrolling)
        return true;
    if (shineAt_ != 0 && gui_.platform().ticks() < shineAt_ + ShineMs)
        return true;
    for (const auto &game : games)
        if (game.visible && game.move.set())
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
    // its raise or lowering, if that is what it is on, stops: nothing else moves with it
    if (games[selected].move.set() && games[selected].move.id == mainMove_.id)
        moves_.cancel(uiTweens(), mainMove_);
    games[selected].move = CarouselMotion::MoveRef();
}

//*******************************
// Carousel::updateVisibility
//*******************************
// update potentially visible covers to save the memory
void Carousel::updateVisibility() {
    bool allAnimationFinished = true;
    forEachItem([&](const PsCarouselGame &game) {
        if (game.move.set() && game.visible) {
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
// this method runs during the loop to update positions of the covers during animation: the tweens to this pass's
// time, then each visible cover part way along its move - or, the move over, on its destination (G5o5: the
// positions the hand-written timer gave, at every time - carousel_motion.h, test_carousel_motion)
void Carousel::updatePositions() {
    abgui::Tweens &runs = uiTweens();
    runs.update();
    forEachItem([&](PsCarouselGame &game) {
        if (game.visible)
            CarouselMotion::advance(game, moves_, runs);
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
void renderReflection(ableem::Renderer &renderer, const PsCarouselGame &game, const PsScreenpoint &point,
                      unsigned char alpha) {
    const ableem::Texture &tex = game.coverPng;
    const ableem::Rect &content = game.content;
    const float nearness = std::min(1.0f, std::max(0.0f, (point.scale - 0.5f) / 0.5f));
    const float depth = 0.5f - 0.2f * nearness; // of the box's height
    const auto startAlpha = static_cast<unsigned char>(135 * alpha / 255);
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
    drawGlow();

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
// the selected cover's face on screen (its box: a jewel case's square, a big box's art at its own aspect), its
// centre and scale, and how much it is "the selected one" - 1 in the middle slot, 0 once it is half-way to the
// next (so the light hands over as the row scrolls)
struct Spot {
    CoverLight::Box face;
    float cx = 0, cy = 0, scale = 1, strength = 0;
};
Spot spotOf(const PsCarouselGame &game, float screenMiddle) {
    Spot spot;
    const PsScreenpoint &p = game.actual;
    const ableem::Rect &content = game.content;
    spot.face.x = p.x + content.x * p.scale;
    spot.face.y = p.y + content.y * p.scale;
    spot.face.w = content.w * p.scale;
    spot.face.h = content.h * p.scale;
    spot.scale = p.scale;
    spot.cx = p.x + (content.x + content.w / 2.0f) * p.scale;
    spot.cy = p.y + (content.y + content.h / 2.0f) * p.scale;
    const float off = std::min(1.0f, std::fabs(spot.cx - screenMiddle) / 150.0f);
    const float big = std::min(1.0f, std::max(0.0f, (p.scale - 0.5f) / 0.5f));
    spot.strength = (1.0f - off) * big;
    return spot;
}
} // namespace

void Carousel::drawGlow() {
    if (!selectedIsValid() || !games[selected].visible || !games[selected].coverPng.valid())
        return;
    ableem::Renderer &renderer = gui_.renderer();
    const Spot spot = spotOf(games[selected], renderer.width() / 2.0f);
    if (spot.strength < 0.02f)
        return;
    // breathing, 0.8..1 every 5.6 s - sin(ticks / 900), the ticks from an ambient tween (G5o2) that starts with the
    // first frame the glow is drawn and keeps the phase the clock itself would give
    if (!glowRuns_) {
        glowRuns_ = true;
        abgui::Tweens &tweens = Gui::getInstance()->uiContext().stack().tweens();
        const unsigned int startTicks = tweens.now(); // the tweens' own clock, which is the platform's ticks
        glowPhase_ = abgui::ambient::clockPhaseStart(startTicks);
        tweens.start(abgui::ambient::clockPhase(glowPhase_, startTicks), glowOwner_);
    }
    const float pulse = 0.9f + 0.1f * std::sin(glowPhase_ / 900.0f);
    // the theme's coverGlow frame (G5k), when it has one: round the face, its slices and bleed scaled with the cover,
    // at the glow's alpha (strength x breathing), tinted by the theme's own `tint` (selection) - instead of the square
    {
        abgui::Context &ctx = Gui::getInstance()->uiContext();
        if (ctx.frame("coverGlow").valid()) {
            const ableem::Rect face(
                static_cast<int>(std::lround(spot.face.x)), static_cast<int>(std::lround(spot.face.y)),
                static_cast<int>(std::lround(spot.face.w)), static_cast<int>(std::lround(spot.face.h)));
            ctx.style().drawFrame(ctx, "coverGlow", face, static_cast<unsigned char>(255 * spot.strength * pulse),
                                  CoverLight::glowFrameScale(spot.face));
            return;
        }
    }
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
    const float k = 0.55f * spot.strength * pulse; // a hint of light, not a lamp (the owner's look check)
    // premultiplied: the colour and the alpha both scale, or the light would not fade with them
    glowTex_.setColorMod(ableem::Color(static_cast<unsigned char>(light.r * k), static_cast<unsigned char>(light.g * k),
                                       static_cast<unsigned char>(light.b * k)));
    glowTex_.setAlphaMod(static_cast<unsigned char>(255 * k));
    // round the face's real width and height (a tall or a wide box too), 44 px past each edge at scale 1
    const CoverLight::Box glow = CoverLight::glowBox(spot.face, spot.scale);
    renderer.copy(glowTex_, nullptr, ableem::FRect(glow.x, glow.y, glow.w, glow.h));
}

void Carousel::drawShine(long now) {
    if (!selectedIsValid())
        return;
    const PsCarouselGame &game = games[selected];
    const bool resting = !scrolling && !game.move.set();
    if (!resting) {
        shineFor_ = -1; // the row moved: the next rest shines again
        shineAt_ = 0;
        return;
    }
    ableem::Renderer &renderer = gui_.renderer();
    if (shineFor_ != selected) {
        shineFor_ = selected;
        const bool wanted = AppBase::get().config().inifile.values["covershine"] != "false";
        shineAt_ = wanted ? now + ShineDelayMs : 0;
        // the same built-in picture on every theme (not a theme key): a square of white with one soft diagonal
        // band in its alpha, loaded once a crossing is due - a missing file then costs one log line, not one a frame
        if (wanted && !shineTex_.valid()) {
            shineTex_ = ThemeAssets::loadImage(renderer, Env::getWorkingPath() + sep + "evoimg/sheen.png");
            if (shineTex_.valid())
                shineTex_.setBlendMode(ableem::BlendMode::Blend);
        }
    }
    if (shineAt_ == 0 || now < shineAt_ || now >= shineAt_ + ShineMs || !game.coverPng.valid() || !shineTex_.valid() ||
        std::fabs(game.actual.angle) >= 0.5f)
        return;
    // the band crosses the face from just off its left edge to just off its right, the picture drawn at the
    // face's height (so the band keeps its angle on any aspect) and clipped to its width (CoverLight::shineSlice).
    // A no-art box (noArt: the two-layer placeholder at its system's aspect) is composed into coverPng like real
    // art, its `content` the face - it crosses it the same way; only an empty box or a loading stand-in has no cover
    const Spot spot = spotOf(game, renderer.width() / 2.0f);
    const float t = static_cast<float>(now - shineAt_) / ShineMs;
    const float eased = t * t * (3.0f - 2.0f * t);
    const ableem::Size texSize = shineTex_.size();
    const CoverLight::ShineSlice slice = CoverLight::shineSlice(spot.face, eased, texSize.w);
    if (!slice.visible)
        return;
    const ableem::Rect src(slice.srcX, 0, slice.srcW, texSize.h);
    renderer.copy(shineTex_, &src, ableem::FRect(slice.dst.x, slice.dst.y, slice.dst.w, slice.dst.h));
}

//*******************************
// Carousel::drawCovers
//*******************************
void Carousel::drawCovers(const vector<const PsCarouselGame *> &visible) {
    ableem::Renderer &renderer = gui_.renderer();
    for (const PsCarouselGame *game : visible) {
        if (!game->coverPng.valid())
            continue; // a cover still on its way: its stand-in has no reflection for the moment it shows
        if (!game->placeholder) {
            renderReflection(renderer, *game, game->actual, 255);
        } else { // an empty box reflects as it is drawn: darker and see-through
            PsScreenpoint point = game->actual;
            point.shade = static_cast<int>(point.shade * PlaceholderShade);
            renderReflection(renderer, *game, point, PlaceholderAlpha);
        }
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
