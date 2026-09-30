//
// Created by screemer on 2/12/19.
//

#pragma once

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
    // "lightgun2"), so a theme's launcher.icons replaces any of them; the optional "badge" frame goes behind each
    // badge but the players icon and the disc

    int nextPos = 0;
    int prevPos = 0;
    long animEndTime = 0;
    long animStarted = 0;

    bool internal = false;
    bool hd = false;
    bool locked = false;
    int discs = 1;
    bool favorite = false;
    bool play_using_ra = false;
    bool lightgun = false; // set from LightgunService by updateTexts(PsGamePtr)
    bool foreign = false;
    bool app = false;
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
};
