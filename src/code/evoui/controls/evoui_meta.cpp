//
// Created by screemer on 2/12/19.
//

#include "evoui_meta.h"
#include "core/model/timing.h"
#include "core/model/ps_game.h"
#include "core/services/system.h"
#include "../../app.h"
#include "core/main.h"
#include "core/main.h"
#include "core/services/environment.h"
#include "gui/panel_style.h" // PanelStyle::outlineOf, the icons' dark halo (UIREV-27)

using namespace std;

//*******************************
// PsMeta::updateTexts
//*******************************
void PsMeta::updateTexts(const string &gameNameTxt, const string &publisherTxt, const string &yearTxt,
                         const string &serial, const string &region, const string &playersTxt, bool internal, bool hd,
                         bool locked, int discs, bool favorite, bool play_using_ra, bool foreign, bool app,
                         const string &last_played, ableem::Color _textColor) {
    this->discs = discs;
    this->internal = internal;
    this->hd = hd;
    this->locked = locked;
    this->favorite = favorite;
    this->play_using_ra = play_using_ra;
    this->gameName = gameNameTxt;
    this->publisher = publisherTxt;
    this->year = yearTxt;
    this->serial = serial;
    this->region = region;
    this->players = playersTxt;
    this->foreign = foreign;
    this->app = app;
    this->last_played = last_played;
    coreName = "";
    playersKnown = false;
    textColor = _textColor;
    textColor.a = 255; // if you're rendering with a different color you need this or it will be transparent

    if (foreign) {
        trim(publisher);
        if (publisher == "DETECT")
            publisher = _("Unknown Core (AutoDetect)");
    }
}

//*******************************
// PsMeta::updateTexts
//*******************************
void PsMeta::updateTexts(PsGamePtr &psGame, ableem::Color _textColor) {
    string appendText = psGame->players == 1 ? _("Player") : _("Players");
    lightgun = App::get().lightguns().isLightgun(*psGame);
    if (!psGame->foreign) {
        if (psGame->serial == "") {
            IniFile iniFile;
            iniFile.load(psGame->folder + sep + "Game.ini");
            psGame->serial = iniFile.values["serial"];
            psGame->region = iniFile.values["region"];
        }
        updateTexts(psGame->title, psGame->publisher, to_string(psGame->year), psGame->serial, psGame->region,
                    to_string(psGame->players) + " " + appendText, psGame->internal, psGame->hd, psGame->locked,
                    psGame->cds, psGame->favorite, psGame->play_using_ra, psGame->foreign, psGame->app,
                    App::get().clock().displayTime(psGame->last_played), _textColor);
    } else {
        if (psGame->app) {
            psGame->serial = "";
            psGame->region = "";

            updateTexts(psGame->title, psGame->publisher, to_string(psGame->year), psGame->serial, psGame->region,
                        to_string(psGame->players) + " " + appendText, psGame->internal, psGame->hd, psGame->locked,
                        psGame->cds, psGame->favorite, psGame->play_using_ra, psGame->foreign, psGame->app,
                        App::get().clock().displayTime(psGame->last_played), _textColor);
        } else {
            psGame->serial = "";
            psGame->region = "";

            // the publisher line is the core's name unless the database gave the game a publisher; the
            // core then gets a line of its own
            const bool hasPublisher = !psGame->publisher.empty();
            updateTexts(psGame->title, hasPublisher ? psGame->publisher : psGame->core_name, to_string(psGame->year),
                        psGame->serial, psGame->region, to_string(psGame->players) + " " + appendText, psGame->internal,
                        psGame->hd, psGame->locked, psGame->cds, psGame->favorite, psGame->play_using_ra,
                        psGame->foreign, psGame->app, App::get().clock().displayTime(psGame->last_played), _textColor);
            if (hasPublisher)
                coreName = psGame->core_name;
            playersKnown = psGame->players > 0;
        }
    }
}

//*******************************
// PsMeta::destroy
//*******************************
void PsMeta::destroy() {}

//*******************************
// PsMeta::render
//*******************************
void PsMeta::render() {
    if (gameName == "") {
        return;
    }

    if (!internalOffTex.valid()) {
        string curPath = Env::getWorkingPath() + sep;
        internalOnTex = ableem::Texture::loadFile(renderer, curPath + "evoimg/ps1.png");
        internalOffTex = ableem::Texture::loadFile(renderer, curPath + "evoimg/usb.png");
        hdOnTex = ableem::Texture::loadFile(renderer, curPath + "evoimg/hd.png");
        hdOffTex = ableem::Texture::loadFile(renderer, curPath + "evoimg/sd.png");
        lockOnTex = ableem::Texture::loadFile(renderer, curPath + "evoimg/lock.png");
        lockOffTex = ableem::Texture::loadFile(renderer, curPath + "evoimg/unlock.png");
        cdTex = ableem::Texture::loadFile(renderer, curPath + "evoimg/cd.png");
        favoriteTex = ableem::Texture::loadFile(renderer, curPath + "evoimg/favorite.png");
        raTex = ableem::Texture::loadFile(renderer, curPath + "evoimg/ra.png");
        lightgunTex = ableem::Texture::loadFile(renderer, curPath + "evoimg/lightgun.png");
        lightgun2Tex = ableem::Texture::loadFile(renderer, curPath + "evoimg/lightgun2.png");

        // each icon's own dark halo (UIREV-27): built once here from a fresh Image decode of the same
        // file - a loaded Texture cannot be read back pixel by pixel
        internalOnOutline = PanelStyle::outlineOf(renderer, ableem::Image::loadFile(curPath + "evoimg/ps1.png"));
        internalOffOutline = PanelStyle::outlineOf(renderer, ableem::Image::loadFile(curPath + "evoimg/usb.png"));
        hdOnOutline = PanelStyle::outlineOf(renderer, ableem::Image::loadFile(curPath + "evoimg/hd.png"));
        hdOffOutline = PanelStyle::outlineOf(renderer, ableem::Image::loadFile(curPath + "evoimg/sd.png"));
        lockOnOutline = PanelStyle::outlineOf(renderer, ableem::Image::loadFile(curPath + "evoimg/lock.png"));
        lockOffOutline = PanelStyle::outlineOf(renderer, ableem::Image::loadFile(curPath + "evoimg/unlock.png"));
        cdOutline = PanelStyle::outlineOf(renderer, ableem::Image::loadFile(curPath + "evoimg/cd.png"));
        favoriteOutline = PanelStyle::outlineOf(renderer, ableem::Image::loadFile(curPath + "evoimg/favorite.png"));
        raOutline = PanelStyle::outlineOf(renderer, ableem::Image::loadFile(curPath + "evoimg/ra.png"));
        lightgunOutline = PanelStyle::outlineOf(renderer, ableem::Image::loadFile(curPath + "evoimg/lightgun.png"));
        lightgun2Outline =
            PanelStyle::outlineOf(renderer, ableem::Image::loadFile(curPath + "evoimg/lightgun2.png"));
    }

    if (visible) {
        int w, h;
        ableem::Rect rect;
        ableem::Rect fullRect;

        // a meta icon plus its dark halo (UIREV-27), drawn at the same +2px margin Play's outline uses
        auto copyWithOutline = [&](const ableem::Texture &texture, const ableem::Texture &outline) {
            if (outline.valid()) {
                ableem::Rect outlineRect(rect.x - 2, rect.y - 2, rect.w + 5, rect.h + 5);
                renderer.copy(outline, nullptr, &outlineRect);
            }
            renderer.copy(texture, &fullRect, &rect);
        };

        auto nameFont = fonts[FONT_28_BOLD];
        auto otherFont = fonts[FONT_15_BOLD];

        int yOffset = 0;
        // game name line - a name too long for the screen is drawn in the largest size that fits
        if (x + nameFont.width(gameName) > SCREEN_WIDTH)
            nameFont = gui->text().fittingFont(FONT_BOLD, 28, 12, gameName, SCREEN_WIDTH - x);
        gui->text().renderText(nameFont, gameName, x, y + yOffset);

        yOffset += 35;
        // publisher line - with the year when known (a RetroArch game the database does not know shows its
        // core name here instead)
        if (!year.empty() && year != "0")
            gui->text().renderText(otherFont, publisher + ", " + year, x, y + yOffset);
        else
            gui->text().renderText(otherFont, publisher, x, y + yOffset);
        if (!coreName.empty()) {
            yOffset += 21;
            gui->text().renderText(otherFont, coreName, x, y + yOffset);
        }

        // the serial/region and last-played lines are a PS1 game's; a RetroArch game or an App has neither
        if (!foreign) {
            yOffset += 21;
            // serial number line
            gui->text().renderText(otherFont, _("Serial:") + " " + serial + ", " + _("Region:") + " " + region, x,
                                   y + yOffset);

            yOffset += 21;
            // last played line
#ifdef AB_PLATFORM_PSC
            // the stock console has no clock to have known the time: only the AutoBleem kernel gives it one
            if (Env::autobleemKernel)
                gui->text().renderText(otherFont, _("Last Played:") + " " + last_played, x, y + yOffset);
#else
            // every other machine keeps time (Clock::displayTime blanks a time it could not have known)
            gui->text().renderText(otherFont, _("Last Played:") + " " + last_played, x, y + yOffset);
#endif
        }

        yOffset += 22;
        if (!foreign) {
            // PS1 icons line
            gui->text().renderText(otherFont, players, x + 35, y + yOffset);

            ableem::Size s = tex.size();
            w = s.w;
            h = s.h;
            rect.x = x;
            rect.y = y + yOffset - 2;
            rect.w = w;
            rect.h = h;

            fullRect.x = 0;
            fullRect.y = 0;
            fullRect.w = w;
            fullRect.h = h;
            renderer.copy(tex, &fullRect, &rect);

            int xoffset = 190, spread = 40;
            // render internal icon
            rect.x = x + 135;
            copyWithOutline(cdTex, cdOutline);

            gui->text().renderText(otherFont, to_string(discs), x + 170, y + yOffset);

            rect.x = x + xoffset;
            rect.y = y + yOffset - 2;
            rect.w = 30;
            rect.h = 30;

            fullRect.x = 0;
            fullRect.y = 0;
            fullRect.w = 30;
            fullRect.h = 30;
            if (internal) {
                locked = true;
                hd = false;
                copyWithOutline(internalOnTex, internalOnOutline);
            } else {
                copyWithOutline(internalOffTex, internalOffOutline);
            }

            int spreadCount = 1;
            rect.x = x + xoffset + (spread * spreadCount);
            if (hd) {
                copyWithOutline(hdOnTex, hdOnOutline);
            } else {
                copyWithOutline(hdOffTex, hdOffOutline);
            }
            ++spreadCount;
            rect.x = x + xoffset + (spread * spreadCount);
            if (locked) {
                copyWithOutline(lockOnTex, lockOnOutline);
            } else {
                copyWithOutline(lockOffTex, lockOffOutline);
            }
            if (favorite) {
                ++spreadCount;
                rect.x = x + xoffset + (spread * spreadCount);
                copyWithOutline(favoriteTex, favoriteOutline);
            }
            if (play_using_ra) {
                ++spreadCount;
                rect.x = x + xoffset + (spread * spreadCount);
                copyWithOutline(raTex, raOutline);
            }
            if (lightgun) {
                ++spreadCount;
                rect.x = x + xoffset + (spread * spreadCount);
                bool onePlayer = players.rfind("1 ", 0) == 0;
                copyWithOutline(onePlayer ? lightgunTex : lightgun2Tex, onePlayer ? lightgunOutline : lightgun2Outline);
            }
        } else {
            // RetroArch game: the players line when the database knows, then the RA icon on the row the
            // serial line left free
            if (!app) {
                if (playersKnown) {
                    gui->text().renderText(otherFont, players, x, y + yOffset);
                    yOffset += 21;
                }
                yOffset += 21;
                ableem::Size s = raTex.size();
                w = s.w;
                h = s.h;
                rect.x = x;
                rect.y = y + yOffset - 2;
                rect.w = w;
                rect.h = h;

                fullRect.x = 0;
                fullRect.y = 0;
                fullRect.w = w;
                fullRect.h = h;
                copyWithOutline(raTex, raOutline);

                if (lightgun) {
                    rect.x += 40;
                    rect.w = 30;
                    rect.h = 30;
                    fullRect.w = 30;
                    fullRect.h = 30;
                    copyWithOutline(lightgunTex, lightgunOutline);
                }
            }
        }
    }
}

//*******************************
// PsMeta::update
//*******************************
void PsMeta::update(long time) {
    if (visible)
        if (animEndTime != 0) {
            if (animStarted == 0) {
                animStarted = time;
            }

            if (animStarted != 0) {
                // calculate length for point in time
                long currentAnim = time - animStarted;
                long totalAnimTime = animEndTime - animStarted;
                float position = easeOutCubic(currentAnim * 1.0f / totalAnimTime);
                int newPos = prevPos + ((nextPos - prevPos) * position);
                y = newPos;
            }

            if (time >= animEndTime) {
                animStarted = 0;
                animEndTime = 0;
                y = nextPos;
            }
        }
    lastTime = time;
}
