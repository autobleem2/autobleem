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
#include <ab_gui/context.h>
#include <ab_gui/style.h>

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
    // the slide's position at this frame (a hidden panel did not move before either)
    if (visible && sliding_)
        y = evomotion::slidInt(prevPos, nextPos, progress_);

    if (gameName == "") {
        return;
    }

    if (visible) {
        int w, h;
        ableem::Rect rect;
        ableem::Rect fullRect;

        // the icons come from the Context's icon set, asked at draw time (the display release drops them); a theme
        // without launcher.icons and without a "badge" frame gets today's files and today's calls
        abgui::Context &ctx = gui->uiContext();
        const bool badgeFrame = ctx.frame("badge").valid();
        abgui::Style badgeStyle;
        if (badgeFrame)
            badgeStyle = ctx.style();

        // a meta icon plus its dark halo (UIREV-27), drawn at the same +2px margin Play's outline uses; a badge
        // has the theme's "badge" frame behind it when there is one (32 x 32 round a 30 x 30 icon)
        auto copyWithOutline = [&](const string &icon, bool badge = true) {
            const ableem::Texture outline = ctx.iconHalo(icon);
            if (badge && badgeFrame)
                badgeStyle.drawFrame(ctx, "badge", ableem::Rect(rect.x - 1, rect.y - 1, rect.w + 2, rect.h + 2));
            if (outline.valid()) {
                ableem::Rect outlineRect(rect.x - 2, rect.y - 2, rect.w + 5, rect.h + 5);
                renderer.copy(outline, nullptr, &outlineRect);
            }
            renderer.copy(ctx.icon(icon), &fullRect, &rect);
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
            // serial number line - a field with no value is left out, and with neither there is no line at all
            string serialLine;
            if (!serial.empty())
                serialLine = _("Serial:") + " " + serial;
            if (!region.empty())
                serialLine += (serialLine.empty() ? "" : ", ") + _("Region:") + " " + region;
            if (!serialLine.empty()) {
                yOffset += 21;
                gui->text().renderText(otherFont, serialLine, x, y + yOffset);
            }

            // last played line - skipped entirely (no row reserved) when there is no value to show, the
            // same way the coreName line above skips its yOffset when there is no core name
#ifdef AB_PLATFORM_PSC
            // the stock console has no clock to have known the time: only the AutoBleem kernel gives it one
            bool canShowLastPlayed = Env::autobleemKernel;
#else
            // every other machine keeps time (Clock::displayTime blanks a time it could not have known)
            bool canShowLastPlayed = true;
#endif
            if (canShowLastPlayed && !last_played.empty()) {
                yOffset += 21;
                gui->text().renderText(otherFont, _("Last played:") + " " + last_played, x, y + yOffset);
            }
        }

        yOffset += 22;
        if (!foreign) {
            // PS1 icons line
            gui->text().renderText(otherFont, players, x + 35, y + yOffset);

            const ableem::Texture playersIcon = ctx.icon("players");
            ableem::Size s = playersIcon.size();
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
            copyWithOutline("players", false);

            int xoffset = 190, spread = 40;
            // render internal icon
            rect.x = x + 135;
            copyWithOutline("disc", false);

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
                copyWithOutline("internal");
            } else {
                copyWithOutline("usb");
            }

            int spreadCount = 1;
            rect.x = x + xoffset + (spread * spreadCount);
            if (hd) {
                copyWithOutline("hd");
            } else {
                copyWithOutline("sd");
            }
            ++spreadCount;
            rect.x = x + xoffset + (spread * spreadCount);
            if (locked) {
                copyWithOutline("lock");
            } else {
                copyWithOutline("unlock");
            }
            if (favorite) {
                ++spreadCount;
                rect.x = x + xoffset + (spread * spreadCount);
                copyWithOutline("favorite");
            }
            if (play_using_ra) {
                ++spreadCount;
                rect.x = x + xoffset + (spread * spreadCount);
                copyWithOutline("retroarch");
            }
            if (lightgun) {
                ++spreadCount;
                rect.x = x + xoffset + (spread * spreadCount);
                bool onePlayer = players.rfind("1 ", 0) == 0;
                copyWithOutline(onePlayer ? "lightgun" : "lightgun2");
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
                ableem::Size s = ctx.icon("retroarch").size();
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
                copyWithOutline("retroarch");

                if (lightgun) {
                    rect.x += 40;
                    rect.w = 30;
                    rect.h = 30;
                    fullRect.w = 30;
                    fullRect.h = 30;
                    copyWithOutline("lightgun");
                }
            }
        }
    }
}

//*******************************
// PsMeta::update
//*******************************
void PsMeta::update(long time) {
    lastTime = time;
}

//*******************************
// PsMeta::slideTo
//*******************************
// from where it is now to `pos`; a slide running is replaced (it stops where it is and the new one starts there)
void PsMeta::slideTo(int pos) {
    owner_.cancel();
    prevPos = y;
    nextPos = pos;
    progress_ = 0;
    sliding_ = true;
    gui->uiContext().stack().tweens().start(abgui::Tween(progress_, 0.0f, 1.0f, evomotion::MetaSlideMs).onEnd([this]() {
        sliding_ = false;
        y = nextPos;
    }),
                                            owner_);
}
