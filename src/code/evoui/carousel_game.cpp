//
// Created by screemer on 2/13/19.
//

#include "carousel_game.h"
#include "../gui/gui.h"
#include "../app.h"
#include "../core/services/retroarch.h"
#include <unistd.h>
#include <iostream>
#include "../core/services/environment.h"
#include <ableem/engine/log.h>

using namespace std;
using ableem::BlendMode;
using ableem::Color;
using ableem::Rect;
using ableem::Size;
using ableem::Texture;

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
            renderer.setDrawColor(Color(255, 255, 255, 0));
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
                    renderer.copy(coverPng, &fullRect, &outputRect);
                    renderer.setBlendMode(BlendMode::Blend);
                }
                coverPng = Texture();

                fullRect.x = 0;
                fullRect.y = 0;
                fullRect.h = 226, fullRect.w = 226;
                if (gui->assets().cdJewel.valid()) {
                    renderer.copy(gui->assets().cdJewel, &fullRect, &fullRect);
                }
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
            renderer.setDrawColor(Color(255, 255, 255, 0));
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
            float aspectRatio = (fullRect.w * 1.0f) / (fullRect.h * 1.0f);
            (void)aspectRatio; // computed but unused, kept to match the original for now
            Rect outputRect;

            // calculate output rect with aspect ratio
            int biggerSize = fullRect.w > fullRect.h ? fullRect.w : fullRect.h;

            outputRect.x = 0;
            outputRect.y = 0;
            outputRect.h = (226 * fullRect.h) / biggerSize;
            outputRect.w = (226 * fullRect.w) / biggerSize;
            outputRect.x = (226 - outputRect.w) / 2;
            outputRect.y = (226 - outputRect.h) / 2;

            renderer.setBlendMode(BlendMode::Add);
            renderer.copy(coverPng, &fullRect, &outputRect);
            renderer.setBlendMode(BlendMode::Blend);

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
// PsCarouselGame::freeTex
//*******************************
void PsCarouselGame::freeTex() {
    coverPng = Texture();
}

//*******************************
// PsCarousel::createCoverPoint
//*******************************
// The side covers stand in a row that recedes from the middle: each one a little further out, turned a
// little more towards it. Offsets are of the box's centre from the screen's centre, in pixels, and were
// chosen so that the turned covers neither overlap nor leave gaps at half size.
PsScreenpoint PsCarousel::createCoverPoint(int distance, int side) {
    static const int centreOffset[7] = {0, 190, 262, 326, 384, 438, 490};
    static const float turn[7] = {0.0f, 40.0f, 52.0f, 60.0f, 66.0f, 70.0f, 72.0f};
    const int boxWidth = static_cast<int>(226 * 0.5f);
    PsScreenpoint point;
    point.scale = 0.5f;
    point.shade = 255;
    point.y = 100;
    if (side == 0) {
        point.x = 640 - centreOffset[distance] - boxWidth / 2;
        point.angle = -turn[distance];
    } else {
        point.x = 640 + centreOffset[distance] - boxWidth / 2;
        point.angle = turn[distance];
    }
    return point;
}

//*******************************
// PsCarousel::initCoverPositions
//*******************************
void PsCarousel::initCoverPositions() {
    coverPositions.clear();

    for (int distance = 6; distance >= 1; distance--) {
        coverPositions.push_back(createCoverPoint(distance, 0));
    }

    PsScreenpoint point;
    point.x = 640 - 113;
    point.y = 180;
    point.scale = 1;
    point.shade = 255;
    coverPositions.push_back(point);

    for (int distance = 1; distance <= 6; distance++) {
        coverPositions.push_back(createCoverPoint(distance, 1));
    }
}
