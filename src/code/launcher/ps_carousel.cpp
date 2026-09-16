//
// Created by screemer on 2/13/19.
//

#include "ps_carousel.h"
#include "../gui/gui.h"
#include "../engine/scanner.h"
#include "../app.h"
#include "../core/services/retroarch.h"
#include <unistd.h>
#include <iostream>
#include "../core/environment.h"

using namespace std;
using ableem::Texture;
using ableem::Rect;
using ableem::Size;
using ableem::Color;
using ableem::BlendMode;

#define SLOT_SIZE 120

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

            string imagePath = (*this)->folder + sep + (*this)->base + ".png";
            renderer.setTarget(nullptr);
            if (DirEntry::exists(imagePath)) {
                coverPng = Texture::loadFile(renderer, imagePath);
            } else {
                coverPng = Texture();
#ifdef AB_DEBUG_HOST
                if ((*this)->internal) {
                    GameMetadata md;
                    if (App::get().library().covers().findBySerial((*this)->serial, md) && !md.bytes.empty()) {
                        coverPng = Texture::loadMemory(renderer, md.bytes.data(), md.bytes.size());
                    }
                }
#endif
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
                auto makeBoxArtPath = [&] (const string& boxartDir) -> string
                        { return Env::getPathToRetroarchDir() + sep + "thumbnails" + sep +
                        DirEntry::getFileNameWithoutExtension((*this)->db_name) + sep +
                        boxartDir + sep + RetroArchService::escapeName((*this)->title) + ".png";
                        };

                imagePath = makeBoxArtPath("Named_Boxarts");
                string imagePath2 = makeBoxArtPath("Named_Titles");
                string imagePath3 = makeBoxArtPath("Named_Snaps");
                if (DirEntry::exists(imagePath)) {
                    coverPng = Texture::loadFile(renderer, imagePath);
                } else if (DirEntry::exists(imagePath2)) {
                    imagePath = imagePath2;
                    coverPng = Texture::loadFile(renderer, imagePath);
                } else if (DirEntry::exists(imagePath3)) {
                    imagePath = imagePath3;
                    coverPng = Texture::loadFile(renderer, imagePath);
                } else {
                    // use default
                    cout << "boxart image NOT found for " << imagePath << endl;
                    coverPng = Texture::loadFile(renderer, Env::getWorkingPath() + sep + "evoimg/ra-cover.png");
                }
            } else
            {
                imagePath = (*this)->image_path;

                if (DirEntry::exists(imagePath)) {
                    coverPng = Texture::loadFile(renderer, imagePath);
                } else {
                    // use default
                    cout << "boxart image NOT found for " << imagePath << endl;
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
            float aspectRatio = (fullRect.w*1.0f)/(fullRect.h*1.0f);
            (void)aspectRatio;   // computed but unused, kept to match the original for now
            Rect outputRect;

            // calculate output rect with aspect ratio
            int biggerSize = fullRect.w>fullRect.h ? fullRect.w : fullRect.h;


            outputRect.x = 0;
            outputRect.y = 0;
            outputRect.h = (226*fullRect.h)/biggerSize;
            outputRect.w = (226*fullRect.w)/biggerSize;
            outputRect.x = (226-outputRect.w)/2;
            outputRect.y = (226-outputRect.h)/2;


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
PsScreenpoint PsCarousel::createCoverPoint(int x, int shade, int side) {
    shade = 255;
    if (side == 0) {
        PsScreenpoint point;
        point.x = 405 - SLOT_SIZE * x;
        point.y = 100;
        point.scale = 0.5f;
        point.shade = shade;
        return point;
    } else {
        PsScreenpoint point;
        point.x = 405 + 357 + SLOT_SIZE * x;
        point.y = 100;
        point.scale = 0.5f;
        point.shade = shade;
        return point;
    }
}

//*******************************
// PsCarousel::initCoverPositions
//*******************************
void PsCarousel::initCoverPositions() {
    //405 x 100
    coverPositions.clear();

    coverPositions.push_back(createCoverPoint(5, 40, 0));
    coverPositions.push_back(createCoverPoint(4, 70, 0));
    coverPositions.push_back(createCoverPoint(3, 90, 0));
    coverPositions.push_back(createCoverPoint(2, 100, 0));
    coverPositions.push_back(createCoverPoint(1, 128, 0));
    coverPositions.push_back(createCoverPoint(0, 150, 0));

    PsScreenpoint point;
    point.x = 640 - 113;
    point.y = 180;
    point.scale = 1;
    point.shade = 255;
    coverPositions.push_back(point);

    coverPositions.push_back(createCoverPoint(0, 150, 1));
    coverPositions.push_back(createCoverPoint(1, 128, 1));
    coverPositions.push_back(createCoverPoint(2, 100, 1));
    coverPositions.push_back(createCoverPoint(3, 90, 1));
    coverPositions.push_back(createCoverPoint(4, 70, 1));
    coverPositions.push_back(createCoverPoint(5, 40, 1));

    // special point to move it up
}
