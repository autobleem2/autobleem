//
// Created by screemer on 2/13/19.
//

#include "carousel_game.h"
#include "gui/gui.h"
#include "../app.h"
#include "core/services/retroarch.h"
#include "core/services/cover_aspect.h"
#include <unistd.h>
#include <iostream>
#include "core/services/environment.h"
#include <ableem/engine/log.h>

using namespace std;
using ableem::BlendMode;
using ableem::Color;
using ableem::Rect;
using ableem::Size;
using ableem::Texture;

// The cover is composed into its 226x226 texture inset by this many pixels all round, the margin left
// transparent: the linear filter then blends the edge into it, which gives a turned cover
// (Renderer::copyTrapezoid) and the selected one smooth edges without multisampling - which a Pi at
// 1080p cannot afford. What the theme's layout puts at (0,0,226,226) lands inset by it.
const int CoverMargin = 2;
static Rect insetIntoCover(const Rect &r) {
    const float k = (226.0f - 2 * CoverMargin) / 226.0f;
    return Rect(CoverMargin + static_cast<int>(r.x * k), CoverMargin + static_cast<int>(r.y * k),
                static_cast<int>(r.w * k), static_cast<int>(r.h * k));
}

// How thick the two kinds of box are, as a fraction of their width - the spine the carousel shows on a
// turned cover. A CD jewel case is thin; a cardboard big box (NES, SNES, PC) is a good quarter of its
// width deep, and that depth is most of what makes it read as a box.
const float JewelCaseThickness = 0.08f;
const float BigBoxThickness = 0.22f;

// bigbox.png as a 9-slice over `dst`: the corners as they are, the edges stretched, the middle (which is
// transparent in the frame) stretched too so a frame with a tint or gloss in it would still work
static void drawNineSlice(ableem::Renderer &renderer, const Texture &frame, int border, const Rect &dst) {
    const Size size = frame.size();
    const int sx[4] = {0, border, size.w - border, size.w};
    const int sy[4] = {0, border, size.h - border, size.h};
    const int dx[4] = {dst.x, dst.x + border, dst.x + dst.w - border, dst.x + dst.w};
    const int dy[4] = {dst.y, dst.y + border, dst.y + dst.h - border, dst.y + dst.h};
    for (int row = 0; row < 3; row++) {
        for (int col = 0; col < 3; col++) {
            Rect src(sx[col], sy[row], sx[col + 1] - sx[col], sy[row + 1] - sy[row]);
            Rect out(dx[col], dy[row], dx[col + 1] - dx[col], dy[row + 1] - dy[row]);
            if (out.w > 0 && out.h > 0)
                renderer.copy(frame, &src, &out);
        }
    }
}

// The two layers of a big box that has no art (CA5): a stretchable background and a fixed-size glyph, the
// designer's evoimg/cover_bg.png (9-slice, 4 px), glyph_game.png (a RetroArch game) and glyph_app.png (an
// App), the same on every theme and not theme keys. Loaded on the first no-art cover through the theme
// images' one call (the @2x above output scale 1) and dropped with the carousel's textures.
static const int NoArtSlice = 4;
static const int NoArtGlyphSize = 96;
namespace {
struct NoArtLayers {
    Texture background;
    Texture glyphGame;
    Texture glyphApp;
};
NoArtLayers &noArtLayers() {
    static NoArtLayers layers;
    return layers;
}

const Texture &loadLayer(ableem::Renderer &renderer, Texture &slot, const char *name) {
    if (!slot.valid())
        slot = ThemeAssets::loadImage(renderer, Env::getWorkingPath() + sep + "evoimg" + sep + name);
    return slot;
}

// the system's typical box shape from resources/platform/cover_aspects.cfg (CA4): 1:1 for an unlisted system
// and for an App, which has none
CoverAspect noArtAspect(const PsGame &game) {
    static const CoverAspectTable table = CoverAspectTable::load(CoverAspectTable::pathFor(Env::getWorkingPath()));
    return game.app ? CoverAspect() : table.aspectFor(game.db_name);
}
} // namespace

void PsCarouselGame::releaseNoArtLayers() {
    noArtLayers() = NoArtLayers();
}

//*******************************
// PsCarouselGame::artPath
//*******************************
// The image file the cover is made from. A PS1 game: the PNG next to it (the user's, or the covers db's),
// else what the scan found in RetroArch's thumbnails tree (while that file is still there), else a look in
// the tree now - an internal game, or one scanned before the tree existed - else the placeholder the
// scanner used to copy next to a game. A RetroArch game: its box art, else ra-cover.png; an App: its image,
// else app-cover.png - for those two `noArt` is set and compose() draws the two-layer placeholder at the
// system's aspect instead of the file (which is only still named so the cover loader has something to
// read). Worked out once and remembered - the carousel asks on every scroll.
const string &PsCarouselGame::artPath() {
    if (artResolved)
        return art;
    artResolved = true;
    if (!(*this)->foreign) {
        art = (*this)->folder + sep + (*this)->base + ".png";
        if (!DirEntry::exists(art)) {
            art = (*this)->coverPath;
            if (art.empty() || !DirEntry::exists(art)) {
                art = App::get().thumbnails().findBoxArt(ableem::ThumbnailLookup::PlayStationDbName, (*this)->title,
                                                         (*this)->recordName);
            }
        }
#ifdef AB_DEBUG_HOST
        if (art.empty() && (*this)->internal)
            return art; // the metadata's own picture, which loadTex reads
#endif
        if (art.empty())
            art = Env::getWorkingPath() + sep + "default.png";
    } else if (!(*this)->app) {
        // Named_Boxarts, Titles, Snaps - png or jpg, tags stripped, fuzzy on the region
        art = App::get().thumbnails().findBoxArt((*this)->db_name, (*this)->title);
        if (art.empty()) {
            PLOG_WARNING << "boxart image NOT found for " << (*this)->title << " in " << (*this)->db_name;
            art = Env::getWorkingPath() + sep + "evoimg/ra-cover.png";
            noArt = true;
        }
    } else {
        art = (*this)->image_path;
        if (!DirEntry::exists(art)) {
            PLOG_WARNING << "boxart image NOT found for " << art;
            art = Env::getWorkingPath() + sep + "evoimg/app-cover.png";
            noArt = true;
        }
    }
    return art;
}

//*******************************
// PsCarouselGame::loadTex / loadFromImage
//*******************************
void PsCarouselGame::loadTex(ableem::Renderer &renderer, Texture target) {
    if (coverPng.valid())
        return;
    const string &path = artPath();
    Texture artTex;
#ifdef AB_DEBUG_HOST
    if (path.empty() && (*this)->internal) {
        GameMetadata md;
        if (App::get().library().metadata().findBySerial((*this)->serial, md) && !md.bytes.empty())
            artTex = Texture::loadMemory(renderer, md.bytes.data(), md.bytes.size());
        if (!artTex.valid())
            artTex = Texture::loadFile(renderer, Env::getWorkingPath() + sep + "default.png");
    }
#endif
    if (!path.empty())
        artTex = Texture::loadFile(renderer, path);
    compose(renderer, artTex, target);
}

void PsCarouselGame::loadFromImage(ableem::Renderer &renderer, const ableem::Image &image, Texture target) {
    compose(renderer, Texture::fromImage(renderer, image), target);
}

//*******************************
// PsCarouselGame::compose
//*******************************
// The cover in its 226x226 render target, inset by CoverMargin into transparent black: a PS1 game's art in
// the theme's jewel case, a RetroArch game's or an App's at its own shape in the big-box frame.
void PsCarouselGame::compose(ableem::Renderer &renderer, const Texture &artTex, Texture target) {
    const bool bigBox = (*this)->foreign;
    if (!artTex.valid() && !bigBox) {
        coverPng = Texture(); // no picture: no cover (a big box still gets its empty frame)
        return;
    }
    shared_ptr<Gui> gui(Gui::getInstance());
    if (!target.valid())
        target = Texture::createTarget(renderer, 226, 226);

    // note: the render target is RGBA8888 rather than the original's ABGR32 - SDL blends identically either
    // way since the renderer converts at draw time, only the in-memory byte order differs.
    renderer.pushTarget(&target);
    renderer.setBlendMode(BlendMode::None);
    target.setBlendMode(BlendMode::None);
    renderer.setDrawColor(Color(0, 0, 0, 0)); // transparent black: no light fringe where an edge blends
    renderer.fillRect();
    target.setBlendMode(BlendMode::Blend);
    renderer.setBlendMode(BlendMode::Blend);

    const Size s = artTex.size();
    const Rect artRect(0, 0, s.w, s.h);
    const Rect fullRect(0, 0, 226, 226);
    if (!bigBox) {
        // the jewel case's window is (25,7)-(222,219) in every frame evoimg ships (198x213); the art fills it
        // a pixel under the frame all round, at its own proportions - the part that does not fit is cut from
        // both sides evenly, never stretched (the square art used to be pulled 9% taller, 2 px off to the left)
        Rect outputRect = gui->assets().cdJewel.valid() ? Rect(24, 6, 200, 215) : fullRect;
        Rect source = artRect;
        if (s.w > 0 && s.h > 0) {
            const float want = static_cast<float>(outputRect.w) / outputRect.h;
            if (static_cast<float>(s.w) / s.h > want) {
                source.w = static_cast<int>(s.h * want + 0.5f);
                source.x = (s.w - source.w) / 2;
            } else {
                source.h = static_cast<int>(s.w / want + 0.5f);
                source.y = (s.h - source.h) / 2;
            }
        }
        Rect inset = insetIntoCover(outputRect);
        renderer.setBlendMode(BlendMode::Add);
        renderer.copy(artTex, &source, &inset);
        renderer.setBlendMode(BlendMode::Blend);
        if (gui->assets().cdJewel.valid()) {
            Rect box = insetIntoCover(fullRect);
            renderer.copy(gui->assets().cdJewel, &fullRect, &box);
        }
        content = insetIntoCover(fullRect);
        thickness = JewelCaseThickness;
    } else {
        // a big box: the art's own shape - a tall NES box, a wide SNES one - as large as fits, centred. With no
        // art (CA4/CA5) the shape is the system's typical one and the face is the two-layer placeholder
        artPath();
        int shapeW = s.w;
        int shapeH = s.h;
        if (noArt) {
            const CoverAspect aspect = noArtAspect(**this);
            shapeW = aspect.w;
            shapeH = aspect.h;
        }
        int biggerSize = std::max(shapeW, shapeH);
        if (biggerSize <= 0)
            biggerSize = 1;
        Rect outputRect;
        outputRect.h = (226 * shapeH) / biggerSize;
        outputRect.w = (226 * shapeW) / biggerSize;
        outputRect.x = (226 - outputRect.w) / 2;
        outputRect.y = (226 - outputRect.h) / 2;
        Rect box = insetIntoCover(outputRect);
        if (noArt) {
            NoArtLayers &layers = noArtLayers();
            const Texture &background = loadLayer(renderer, layers.background, "cover_bg.png");
            if (background.valid())
                drawNineSlice(renderer, background, NoArtSlice, box);
            const Texture &glyph = (*this)->app ? loadLayer(renderer, layers.glyphApp, "glyph_app.png")
                                                : loadLayer(renderer, layers.glyphGame, "glyph_game.png");
            if (glyph.valid()) {
                const int side = std::min(NoArtGlyphSize, std::min(box.w, box.h));
                Rect out(box.x + (box.w - side) / 2, box.y + (box.h - side) / 2, side, side);
                renderer.copy(glyph, nullptr, &out);
            }
        } else {
            renderer.setBlendMode(BlendMode::Add);
            renderer.copy(artTex, &artRect, &box);
            renderer.setBlendMode(BlendMode::Blend);
        }
        if (gui->assets().bigBoxFrame.valid())
            drawNineSlice(renderer, gui->assets().bigBoxFrame, 7, box);
        content = box;
        thickness = BigBoxThickness;
    }
    renderer.popTarget();
    renderer.setBlendMode(BlendMode::Blend);
    coverPng = target;
}

//*******************************
// PsCarouselGame::loadPlaceholderTex
//*******************************
// The box with nothing in it: where a game's art would be, a dark glass (a translucent near-black, so the
// background shows through it faintly) under the jewel case, or inside the big-box frame - a square box,
// the shape ra-cover.png and app-cover.png give a game without art. Carousel draws it greyed and
// see-through on top of that.
void PsCarouselGame::loadPlaceholderTex(ableem::Renderer &renderer, BoxKind kind) {
    shared_ptr<Gui> gui(Gui::getInstance());
    Texture renderSurface = Texture::createTarget(renderer, 226, 226);
    renderer.pushTarget(&renderSurface);
    renderer.setBlendMode(BlendMode::None);
    renderSurface.setBlendMode(BlendMode::None);
    renderer.setDrawColor(Color(0, 0, 0, 0));
    renderer.fillRect();
    renderSurface.setBlendMode(BlendMode::Blend);
    renderer.setBlendMode(BlendMode::Blend);

    Rect fullRect(0, 0, 226, 226);
    const Color glass(12, 12, 16, 150);
    if (kind == BoxKind::JewelCase) {
        Rect inside = insetIntoCover(gui->assets().cdJewel.valid() ? Rect(24, 6, 200, 215) : fullRect);
        renderer.setDrawColor(glass);
        renderer.fillRect(inside);
        Rect box = insetIntoCover(fullRect);
        if (gui->assets().cdJewel.valid())
            renderer.copy(gui->assets().cdJewel, &fullRect, &box);
        content = box;
        thickness = JewelCaseThickness;
    } else {
        Rect box = insetIntoCover(fullRect);
        renderer.setDrawColor(glass);
        renderer.fillRect(box);
        if (gui->assets().bigBoxFrame.valid())
            drawNineSlice(renderer, gui->assets().bigBoxFrame, 7, box);
        content = box;
        thickness = BigBoxThickness;
    }
    coverPng = renderSurface;
    renderer.popTarget();
    renderer.setBlendMode(BlendMode::Blend);
}

//*******************************
// PsCarouselGame::freeTex
//*******************************
void PsCarouselGame::freeTex() {
    coverPng = Texture();
}

//*******************************
// PsCarousel::createCoverPoint
//*******************************
// The side covers stand on a shelf that recedes from the middle into the distance: each one a little
// further out, a little smaller and darker, and turned a little more towards the middle. Being further back,
// an outer cover is rightly drawn behind its inner neighbour where the two overlap. The step between
// neighbours shrinks with their size so the row reads as evenly spaced in depth; the twelfth is the last one
// partly on screen and the fourteenth's box is wholly off it (its left edge at -65 for the left side).
namespace {
// the 1280x720 shelf, its numbers as constants - kept apart so it stays exactly what it was (its float steps rounded as
// shelfScale/shelfStep say: the 32-bit PC stick's x87 maths would move an outer cover a pixel otherwise)
PsScreenpoint wideCoverPoint(int distance, int side, float turn) {
    const float nearestScale = 0.5f;
    const int nearestOffset = 190, nearestStep = 50;
    const int nearestShade = 255, darkenPerCover = 15;
    const int middleY = 100 + static_cast<int>(226 * nearestScale) / 2; // the row's centre line

    float scale = nearestScale;
    int offset = nearestOffset;
    for (int d = 2; d <= distance; d++) {
        scale = shelfScale(nearestScale, d);
        offset += shelfStep(nearestStep, scale, nearestScale);
    }
    const int boxWidth = static_cast<int>(226 * scale);

    PsScreenpoint point;
    point.scale = scale;
    point.shade = nearestShade - darkenPerCover * (distance - 1);
    point.y = middleY - boxWidth / 2;
    if (side == 0) {
        point.x = 640 - offset - boxWidth / 2;
        point.angle = -turn;
    } else {
        point.x = 640 + offset - boxWidth / 2;
        point.angle = turn;
    }
    return point;
}
} // namespace

PsScreenpoint PsCarousel::createCoverPoint(int distance, int side) {
    static const float turnByDistance[] = {0, 40, 52, 60, 66, 70, 72};
    const float turn = distance <= 6 ? turnByDistance[distance] : 72.0f;
    if (geometry.isWide())
        return wideCoverPoint(distance, side, turn);
    // the 4:3 layout's shelf: its place, size and spacing (EvoLayout::Carousel::shelfSlot)
    const EvoLayout::ShelfSlot slot = geometry.shelfSlot(distance, side);
    PsScreenpoint point;
    point.scale = slot.scale;
    // darker further out, from 255 to 60 at the outermost as on 16:9 (15 a cover over 14), over this row's length
    point.shade = 255 - 195 * (distance - 1) / std::max(1, sideCovers() - 1);
    point.x = slot.x;
    point.y = slot.y;
    point.angle = side == 0 ? -turn : turn;
    return point;
}

//*******************************
// PsCarousel::initCoverPositions
//*******************************
void PsCarousel::initCoverPositions() {
    coverPositions.clear();

    for (int distance = sideCovers(); distance >= 1; distance--) {
        coverPositions.push_back(createCoverPoint(distance, 0));
    }

    PsScreenpoint point;
    point.x = geometry.mainX();
    point.y = geometry.mainY(false);
    point.scale = geometry.mainScale;
    point.shade = 255;
    coverPositions.push_back(point);

    for (int distance = 1; distance <= sideCovers(); distance++) {
        coverPositions.push_back(createCoverPoint(distance, 1));
    }
}
