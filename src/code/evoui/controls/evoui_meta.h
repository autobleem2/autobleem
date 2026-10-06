//
// Created by screemer on 2/12/19.
//

#pragma once

#include <ab_gui/tween.h>
#include "evoui_motion.h"
#include "evoui_meta_layout.h"
#include "evoui_obj.h"
#include "core/model/ps_game.h"
#include "gui/gui_font.h"

class PsGame;

//******************
// PsMeta
//******************
class PsMeta : public PsObj {
public:
    std::string gameName;
    std::string publisher;
    std::string year;
    std::string players;
    std::string serial;
    std::string region;
    std::string last_played;
    Fonts fonts;
    ableem::Color textColor;

    // the row's icons and their dark halos (UIREV-27) come from the Context's icon set by name (ab_gui G5b:
    // "players", "disc", "usb"/"internal", "hd"/"sd", "lock"/"unlock", "favorite", "retroarch", "lightgun"/
    // "lightgun2"), so a theme's launcher.icons replaces any of them; the badges are bare since UIREV-35 (no plate).
    // The section is the compact facts grid of evoui_meta_layout.h (132 px high, colours from the Context's Style)

    // the panel's slide to another y (200 ms, easeOutCubic): a non-ambient tween (ab_gui G5o3) started here; a new
    // one starts from where the panel is. `nextPos`/`prevPos` are its ends.
    void slideTo(int pos);
    bool sliding() const { return sliding_; }
    int nextPos = 0;
    int prevPos = 0;

    bool internal = false;
    bool hd = false;
    bool locked = false;
    int discs = 1;
    bool favorite = false;
    bool play_using_ra = false;
    bool lightgun = false; // set from LightgunService by updateTexts(PsGamePtr)
    bool foreign = false;
    bool app = false;
    // the section's numbers (the 1280x720 ones; the 4:3 layout's, evoui_layout.h) and the right edge a long
    // title is fitted to
    MetaLayout::Metrics metrics;
    int screenRight = 1280;
    std::string coreName;      // a RetroArch game's core, shown under the publisher when there is one
    bool playersKnown = false; // the players line is drawn for a RetroArch game only when the database said

    void updateTexts(const std::string &gameNameTxt, const std::string &publisherTxt, const std::string &yearTxt,
                     const std::string &serial, const std::string &region, const std::string &playersTxt, bool internal,
                     bool hd, bool locked, int discs, bool favorite, bool play_using_ra, bool foreign, bool app,
                     const std::string &last_played, ableem::Color _textColor);

    void updateTexts(PsGamePtr &game, ableem::Color _textColor);

    void destroy() override;

    void render() override;

    void update(long time) override;

    using PsObj::PsObj;

private:
    float progress_ = 0; // the slide's eased progress, written by its tween
    bool sliding_ = false;
    abgui::TweenOwner owner_; // the tween stops with the panel
};
