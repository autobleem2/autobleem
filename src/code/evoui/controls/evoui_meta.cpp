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
#include <ableem/engine/strings.h>
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

            // the core is the CORE row's; the PUBLISHER row only shows what the database gave the game
            // Options -> "Clean RetroArch game names": the shown name without its (region) [!] tags; the title
            // itself (box art lookup, sorting) and the file stay as they are
            const bool clean = App::get().config().inifile.values["cleannames"] != "false";
            updateTexts(clean ? ableem::Strings::stripBracketTags(psGame->title) : psGame->title, psGame->publisher,
                        to_string(psGame->year), psGame->serial, psGame->region,
                        to_string(psGame->players) + " " + appendText, psGame->internal, psGame->hd, psGame->locked,
                        psGame->cds, psGame->favorite, psGame->play_using_ra, psGame->foreign, psGame->app,
                        App::get().clock().displayTime(psGame->last_played), _textColor);
            coreName = psGame->core_name;
            trim(coreName);
            if (coreName == "DETECT")
                coreName = _("Unknown Core (AutoDetect)");
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
        const MetaLayout::Metrics &m = metrics;
        // the row's own icons at their size on the 1280x720 section, at the section's icon size on another (4:3)
        const int ownIcon = m.iconSize == MetaLayout::IconSize ? 0 : m.iconSize;
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
            // the whole picture when the section draws its icons at another size than 30 (the 4:3 layout)
            fullRect =
                m.iconSize == MetaLayout::IconSize ? ableem::Rect(0, 0, rect.w, rect.h) : ableem::Rect(0, 0, s.w, s.h);
            copyWithOutline(icon);
        };

        // the title - a name too long for the space up to screenRight is drawn in the largest size that fits, and
        // elided there when even the smallest does not
        auto nameFont = fixed.boldAtSize(m.titleSize);
        if (x + nameFont.width(gameName) > screenRight)
            nameFont = text.fittingFont(FONT_BOLD, m.titleSize, m.titleMinSize, gameName, screenRight - x);
        text.renderText_WithColor(nameFont, text.elide(nameFont, gameName, screenRight - x), x, y, style.text,
                                  XALIGN_LEFT);

        // the rule under it: the `edge` role at 78 %
        renderer.setBlendMode(ableem::BlendMode::Blend);
        renderer.setDrawColor(ableem::Color(style.edge.r, style.edge.g, style.edge.b, 200));
        renderer.fillRect(ableem::Rect(x, y + m.ruleY, m.ruleWidth, 1));

        // the facts grid: the label in `secondary` (bold capitals, a little lower), the value in `text`; a RetroArch
        // game shows its core in the CORE row (updateTexts); an App's description is a wrapped, unlabelled row
#ifdef AB_PLATFORM_PSC
        // the stock console has no clock to have known the time: only the AutoBleem kernel gives it one
        const bool canShowLastPlayed = Env::autobleemKernel;
#else
        // every other machine keeps time (Clock::displayTime blanks a time it could not have known)
        const bool canShowLastPlayed = true;
#endif
        const MetaLayout::Kind kind =
            !foreign ? MetaLayout::Kind::Ps1 : (app ? MetaLayout::Kind::App : MetaLayout::Kind::RetroArch);
        const ableem::Font &valueFont = fixed.atSize(FONT_MED, m.valueSize);
        int rowY = y + m.gridY;
        for (const MetaLayout::Fact &fact :
             MetaLayout::facts(kind, publisher, year, serial, region, last_played, coreName, canShowLastPlayed)) {
            if (fact.wrapped) {
                // a description: no label, wrapped to the area, the last line elided when it still does not fit
                vector<string> lines =
                    MetaLayout::capLines(text.wrapLines(valueFont, fact.value, m.ruleWidth), m.descriptionLines());
                for (const string &line : lines) {
                    text.renderText_WithColor(valueFont, text.elide(valueFont, line, m.ruleWidth), x, rowY, style.text,
                                              XALIGN_LEFT);
                    rowY += m.rowPitch;
                }
                continue;
            }
            // a label longer than its column (German, Polish) shrinks to fit
            const string label = _(fact.label);
            const ableem::Font labelFont = text.fittingFont(FONT_BOLD, m.labelSize, 8, label, m.labelWidth());
            text.renderText_WithColor(labelFont, label, x, rowY + m.labelDrop, style.secondary, XALIGN_LEFT);
            text.renderText_WithColor(valueFont, text.elide(valueFont, fact.value, m.valueWidth()), x + m.valueX, rowY,
                                      style.text, XALIGN_LEFT);
            rowY += m.rowPitch;
        }

        // the icon row: text centred on the icons' height
        const ableem::Font &rowFont = m.infoSize == 15 ? fonts[FONT_15_BOLD] : fixed.boldAtSize(m.infoSize);
        const int iconY = y + m.iconRowY;
        const int textY = iconY + (m.iconSize - rowFont.lineHeight()) / 2;
        vector<string> badges;
        if (kind == MetaLayout::Kind::Ps1) {
            text.renderText_WithColor(rowFont, players, x + m.playersTextX, textY, style.text, XALIGN_LEFT);
            drawIcon("players", x, iconY, ownIcon);
            drawIcon("disc", x + m.discX, iconY, ownIcon);
            text.renderText_WithColor(rowFont, to_string(discs), x + m.discCountX, textY, style.text, XALIGN_LEFT);

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
                text.renderText_WithColor(rowFont, players, x + m.playersTextX, textY, style.text, XALIGN_LEFT);
                drawIcon("players", x, iconY, ownIcon);
            }
            badges.push_back("retroarch");
            if (lightgun)
                badges.push_back("lightgun");
        }
        const int count = static_cast<int>(badges.size());
        for (int i = 0; i < count; i++)
            drawIcon(badges[i], x + m.badgeX(count, i), iconY, m.iconSize);
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
