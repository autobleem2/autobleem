//
// Created by screemer on 2/12/19.
//

#include "evoui_meta.h"
#include "evoui_meta_layout.h"
#include "gui/theme_assets.h"
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
        ableem::Rect rect;
        ableem::Rect fullRect;

        // the icons come from the Context's icon set, asked at draw time (the display release drops them)
        abgui::Context &ctx = gui->uiContext();
        const abgui::Style &style = ctx.style();
        TextRenderer &text = gui->text();
        Fonts &fixed = ThemeAssets::fixedFonts();

        // a meta icon plus its dark halo (UIREV-27), drawn at the same +2px margin Play's outline uses; the badges
        // are bare since UIREV-35 (no plate behind them)
        auto copyWithOutline = [&](const string &icon) {
            const ableem::Texture outline = ctx.iconHalo(icon);
            if (outline.valid()) {
                ableem::Rect outlineRect(rect.x - 2, rect.y - 2, rect.w + 5, rect.h + 5);
                renderer.copy(outline, nullptr, &outlineRect);
            }
            renderer.copy(ctx.icon(icon), &fullRect, &rect);
        };
        // an icon at (ix, iy), at its own size, or square when `size` is given
        auto drawIcon = [&](const string &icon, int ix, int iy, int size) {
            ableem::Size s = ctx.icon(icon).size();
            rect = ableem::Rect(ix, iy, size > 0 ? size : s.w, size > 0 ? size : s.h);
            fullRect = ableem::Rect(0, 0, rect.w, rect.h);
            copyWithOutline(icon);
        };

        // the title - a name too long for the screen is drawn in the largest size that fits
        auto nameFont = fixed.boldAtSize(MetaLayout::TitleSize);
        if (x + nameFont.width(gameName) > SCREEN_WIDTH)
            nameFont = text.fittingFont(FONT_BOLD, MetaLayout::TitleSize, MetaLayout::TitleMinSize, gameName,
                                        SCREEN_WIDTH - x);
        text.renderText_WithColor(nameFont, gameName, x, y, style.text, XALIGN_LEFT);

        // the rule under it: the `edge` role at 78 %
        renderer.setBlendMode(ableem::BlendMode::Blend);
        renderer.setDrawColor(ableem::Color(style.edge.r, style.edge.g, style.edge.b, 200));
        renderer.fillRect(ableem::Rect(x, y + MetaLayout::RuleY, MetaLayout::RuleWidth, 1));

        // the facts grid: the label in `secondary` (bold capitals, a little lower), the value in `text`; a RetroArch
        // game the database does not know shows its core in the publisher row (updateTexts)
#ifdef AB_PLATFORM_PSC
        // the stock console has no clock to have known the time: only the AutoBleem kernel gives it one
        const bool canShowLastPlayed = Env::autobleemKernel;
#else
        // every other machine keeps time (Clock::displayTime blanks a time it could not have known)
        const bool canShowLastPlayed = true;
#endif
        const MetaLayout::Kind kind =
            !foreign ? MetaLayout::Kind::Ps1 : (app ? MetaLayout::Kind::App : MetaLayout::Kind::RetroArch);
        const ableem::Font &valueFont = fixed.atSize(FONT_MED, MetaLayout::ValueSize);
        int rowY = y + MetaLayout::GridY;
        for (const MetaLayout::Fact &fact : MetaLayout::facts(kind, publisher, year, serial, region, last_played,
                                                              coreName, canShowLastPlayed)) {
            // a label longer than its column (German, Polish) shrinks to fit
            const string label = _(fact.label);
            const ableem::Font labelFont = text.fittingFont(FONT_BOLD, MetaLayout::LabelSize, 8, label,
                                                            MetaLayout::LabelWidth);
            text.renderText_WithColor(labelFont, label, x, rowY + MetaLayout::LabelDrop, style.secondary,
                                      XALIGN_LEFT);
            text.renderText_WithColor(valueFont, fact.value, x + MetaLayout::ValueX, rowY, style.text, XALIGN_LEFT);
            rowY += MetaLayout::RowPitch;
        }

        // the icon row: text centred on the icons' height
        const ableem::Font &rowFont = fonts[FONT_15_BOLD];
        const int iconY = y + MetaLayout::IconRowY;
        const int textY = iconY + (MetaLayout::IconSize - rowFont.lineHeight()) / 2;
        vector<string> badges;
        if (kind == MetaLayout::Kind::Ps1) {
            text.renderText_WithColor(rowFont, players, x + MetaLayout::PlayersTextX, textY, style.text, XALIGN_LEFT);
            drawIcon("players", x, iconY, 0);
            drawIcon("disc", x + MetaLayout::DiscX, iconY, 0);
            text.renderText_WithColor(rowFont, to_string(discs), x + MetaLayout::DiscCountX, textY, style.text,
                                      XALIGN_LEFT);

            if (internal) {
                locked = true;
                hd = false;
            }
            badges.push_back(internal ? "internal" : "usb");
            badges.push_back(hd ? "hd" : "sd");
            badges.push_back(locked ? "lock" : "unlock");
            if (favorite)
                badges.push_back("favorite");
            if (play_using_ra)
                badges.push_back("retroarch");
            if (lightgun) {
                bool onePlayer = players.rfind("1 ", 0) == 0;
                badges.push_back(onePlayer ? "lightgun" : "lightgun2");
            }
        } else if (kind == MetaLayout::Kind::RetroArch) {
            // the players when the database knows, then the RA icon (and the light gun) as badges
            if (playersKnown) {
                text.renderText_WithColor(rowFont, players, x + MetaLayout::PlayersTextX, textY, style.text,
                                          XALIGN_LEFT);
                drawIcon("players", x, iconY, 0);
            }
            badges.push_back("retroarch");
            if (lightgun)
                badges.push_back("lightgun");
        }
        const int count = static_cast<int>(badges.size());
        for (int i = 0; i < count; i++)
            drawIcon(badges[i], x + MetaLayout::badgeX(count, i), iconY, MetaLayout::IconSize);
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
