//
// Created by screemer on 2/13/19.
//

#include "carousel_game.h"
#include "gui/gui.h"
#include "../app.h"
#include "core/services/retroarch.h"
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

//*******************************
// PsCarouselGame::loadTex
//*******************************
void PsCarouselGame::loadTex(ableem::Renderer &renderer) {
    shared_ptr<Gui> gui(Gui::getInstance());

    if (!(*this)->foreign) {
        if (!coverPng.valid()) {
            // note: the render target is RGBA8888 rather than the original's ABGR32 - SDL blends identically
            // either way since the renderer converts at draw time, only the in-memory byte order differs.
            Texture renderSurface = Texture::createTarget(renderer, 226, 226);
            Rect fullRect;

            renderer.setTarget(&renderSurface);
            renderer.setBlendMode(BlendMode::None);
            renderSurface.setBlendMode(BlendMode::None);
            renderer.setDrawColor(Color(0, 0, 0, 0)); // transparent black: no light fringe where an edge blends
            renderer.fillRect();
            renderSurface.setBlendMode(BlendMode::Blend);
            renderer.setBlendMode(BlendMode::Blend);

            // the user's (or the covers db's) PNG next to the game, else what the scan found in
            // RetroArch's thumbnails tree (while that file is still there), else a look in the tree now -
            // an internal game, or a game scanned before the tree existed
            string imagePath = (*this)->folder + sep + (*this)->base + ".png";
            renderer.setTarget(nullptr);
            if (!DirEntry::exists(imagePath)) {
                imagePath = (*this)->coverPath;
                if (imagePath.empty() || !DirEntry::exists(imagePath)) {
                    imagePath = App::get().thumbnails().findBoxArt(ableem::ThumbnailLookup::PlayStationDbName,
                                                                   (*this)->title, (*this)->recordName);
                }
            }
            if (!imagePath.empty()) {
                coverPng = Texture::loadFile(renderer, imagePath);
            } else {
                coverPng = Texture();
#ifdef AB_DEBUG_HOST
                if ((*this)->internal) {
                    GameMetadata md;
                    if (App::get().library().metadata().findBySerial((*this)->serial, md) && !md.bytes.empty()) {
                        coverPng = Texture::loadMemory(renderer, md.bytes.data(), md.bytes.size());
                    }
                }
#endif
                if (!coverPng.valid()) { // the placeholder the scanner used to copy next to the game
                    coverPng = Texture::loadFile(renderer, Env::getWorkingPath() + sep + "default.png");
                }
            }

            if (coverPng.valid()) {
                renderer.setTarget(&renderSurface);
                fullRect.x = 0;
                fullRect.y = 0;
                fullRect.h = 226, fullRect.w = 226;

                Size s = coverPng.size();
                fullRect.w = s.w;
                fullRect.h = s.h;

                Rect outputRect;
                if (gui->assets().cdJewel.valid()) {
                    outputRect.x = 23;
                    outputRect.y = 5;
                    outputRect.h = 217;
                    outputRect.w = 199;
                } else {
                    outputRect.x = 0;
                    outputRect.y = 0;
                    outputRect.h = 226;
                    outputRect.w = 226;
                }
                if (coverPng.valid()) {
                    renderer.setBlendMode(BlendMode::Add);
                    Rect inset = insetIntoCover(outputRect);
                    renderer.copy(coverPng, &fullRect, &inset);
                    renderer.setBlendMode(BlendMode::Blend);
                }
                coverPng = Texture();

                fullRect.x = 0;
                fullRect.y = 0;
                fullRect.h = 226, fullRect.w = 226;
                if (gui->assets().cdJewel.valid()) {
                    Rect inset = insetIntoCover(fullRect);
                    renderer.copy(gui->assets().cdJewel, &fullRect, &inset);
                }
                content = insetIntoCover(fullRect);
                thickness = JewelCaseThickness;
                coverPng = renderSurface;
            }
            renderer.setTarget(nullptr);
            renderer.setBlendMode(BlendMode::Blend);
        }
    } else {
        if (!coverPng.valid()) {
            Texture renderSurface = Texture::createTarget(renderer, 226, 226);
            Rect fullRect;

            renderer.setTarget(&renderSurface);
            renderer.setBlendMode(BlendMode::None);
            renderSurface.setBlendMode(BlendMode::None);
            renderer.setDrawColor(Color(0, 0, 0, 0)); // transparent black: no light fringe where an edge blends
            renderer.fillRect();
            renderSurface.setBlendMode(BlendMode::Blend);
            renderer.setBlendMode(BlendMode::Blend);

            renderer.setTarget(nullptr);
            string imagePath;
            if (!(*this)->app) {
                // Named_Boxarts, Titles, Snaps - png or jpg, tags stripped, fuzzy on the region
                imagePath = App::get().thumbnails().findBoxArt((*this)->db_name, (*this)->title);
                if (!imagePath.empty()) {
                    coverPng = Texture::loadFile(renderer, imagePath);
                } else {
                    // use default
                    PLOG_WARNING << "boxart image NOT found for " << (*this)->title << " in " << (*this)->db_name;
                    coverPng = Texture::loadFile(renderer, Env::getWorkingPath() + sep + "evoimg/ra-cover.png");
                }
            } else {
                imagePath = (*this)->image_path;

                if (DirEntry::exists(imagePath)) {
                    coverPng = Texture::loadFile(renderer, imagePath);
                } else {
                    // use default
                    PLOG_WARNING << "boxart image NOT found for " << imagePath;
                    coverPng = Texture::loadFile(renderer, Env::getWorkingPath() + sep + "evoimg/app-cover.png");
                }
            }

            renderer.setTarget(&renderSurface);
            fullRect.x = 0;
            fullRect.y = 0;
            fullRect.h = 226, fullRect.w = 226;

            Size s = coverPng.size();
            fullRect.w = s.w;
            fullRect.h = s.h;
            Rect outputRect;

            // a big box: the art's own shape - a tall NES box, a wide SNES one - as large as fits, centred
            int biggerSize = fullRect.w > fullRect.h ? fullRect.w : fullRect.h;
            if (biggerSize <= 0)
                biggerSize = 1;
            outputRect.h = (226 * fullRect.h) / biggerSize;
            outputRect.w = (226 * fullRect.w) / biggerSize;
            outputRect.x = (226 - outputRect.w) / 2;
            outputRect.y = (226 - outputRect.h) / 2;
            Rect box = insetIntoCover(outputRect);

            renderer.setBlendMode(BlendMode::Add);
            renderer.copy(coverPng, &fullRect, &box);
            renderer.setBlendMode(BlendMode::Blend);
            if (gui->assets().bigBoxFrame.valid()) {
                drawNineSlice(renderer, gui->assets().bigBoxFrame, 7, box);
            }
            content = box;
            thickness = BigBoxThickness;

            coverPng = Texture();
            fullRect.x = 0;
            fullRect.y = 0;
            fullRect.h = 226, fullRect.w = 226;
            coverPng = renderSurface;

            renderer.setTarget(nullptr);
            renderer.setBlendMode(BlendMode::Blend);
        }
    }
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
    renderer.setTarget(&renderSurface);
    renderer.setBlendMode(BlendMode::None);
    renderSurface.setBlendMode(BlendMode::None);
    renderer.setDrawColor(Color(0, 0, 0, 0));
    renderer.fillRect();
    renderSurface.setBlendMode(BlendMode::Blend);
    renderer.setBlendMode(BlendMode::Blend);

    Rect fullRect(0, 0, 226, 226);
    const Color glass(12, 12, 16, 150);
    if (kind == BoxKind::JewelCase) {
        Rect inside = insetIntoCover(gui->assets().cdJewel.valid() ? Rect(23, 5, 199, 217) : fullRect);
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
    renderer.setTarget(nullptr);
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
PsScreenpoint PsCarousel::createCoverPoint(int distance, int side) {
    static const float turnByDistance[] = {0, 40, 52, 60, 66, 70, 72};
    const float turn = distance <= 6 ? turnByDistance[distance] : 72.0f;
    const float nearestScale = 0.5f, shrinkPerCover = 0.035f;
    const int nearestOffset = 190, nearestStep = 50;
    const int nearestShade = 255, darkenPerCover = 15;
    const int middleY = 100 + static_cast<int>(226 * nearestScale) / 2; // the row's centre line

    float scale = nearestScale;
    int offset = nearestOffset;
    for (int d = 2; d <= distance; d++) {
        scale = nearestScale * (1.0f - shrinkPerCover * (d - 1));
        offset += static_cast<int>(nearestStep * scale / nearestScale);
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

//*******************************
// PsCarousel::initCoverPositions
//*******************************
void PsCarousel::initCoverPositions() {
    coverPositions.clear();

    for (int distance = SideCovers; distance >= 1; distance--) {
        coverPositions.push_back(createCoverPoint(distance, 0));
    }

    PsScreenpoint point;
    point.x = 640 - 113;
    point.y = 180;
    point.scale = 1;
    point.shade = 255;
    coverPositions.push_back(point);

    for (int distance = 1; distance <= SideCovers; distance++) {
        coverPositions.push_back(createCoverPoint(distance, 1));
    }
}
