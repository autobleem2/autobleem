//
// Created by screemer on 2019-01-24.
//

#include "gui_about.h"
#include <algorithm>
#include <string>
#include "../gui.h"
#include "../../core/services/environment.h"
#include "core/version.h" // generated into the build tree

const std::string GuiAbout::HeadingMark = "";

void GuiAbout::init() {
    std::shared_ptr<Gui> gui(Gui::getInstance());
    fx.renderer = &renderer;
    // the credits' small font: the launcher's medium face (about.ttf, an SST copy, went with the Sony fonts)
    font = Fonts::openNewSharedCachedFont(Env::getPathToFontsDir() + sep + "OpenSans-Medium.ttf", 15, renderer);
    logo = ableem::Texture::loadFile(renderer, Env::getWorkingPath() + sep + "ablogo.png");

    string sdir = Env::getWorkingPath() + sep + "surprise_game" + sep;
    sprites.ship = ableem::Texture::loadFile(renderer, sdir + "ship.png");
    sprites.enemy1 = ableem::Texture::loadFile(renderer, sdir + "enemy1.png");
    sprites.enemy2 = ableem::Texture::loadFile(renderer, sdir + "enemy2.png");
    sprites.ufo = ableem::Texture::loadFile(renderer, sdir + "ufo.png");
    sprites.laserPlayer = ableem::Texture::loadFile(renderer, sdir + "laser_player.png");
    sprites.laserEnemy = ableem::Texture::loadFile(renderer, sdir + "laser_enemy.png");
    sprites.powerupRapid = ableem::Texture::loadFile(renderer, sdir + "powerup_rapid.png");
    sprites.powerupSpread = ableem::Texture::loadFile(renderer, sdir + "powerup_spread.png");
    sprites.powerupPower = ableem::Texture::loadFile(renderer, sdir + "powerup_power.png");

    game.sounds.playerShoot = ableem::Sound::load(sdir + "sfx_player_shoot.ogg");
    game.sounds.enemyShoot = ableem::Sound::load(sdir + "sfx_enemy_shoot.ogg");
    game.sounds.explosion = ableem::Sound::load(sdir + "sfx_explosion.ogg");
    game.sounds.playerHit = ableem::Sound::load(sdir + "sfx_player_hit.ogg");
    game.sounds.waveClear = ableem::Sound::load(sdir + "sfx_wave_clear.ogg");
    game.sounds.powerup = ableem::Sound::load(sdir + "sfx_powerup.ogg");
    surpriseMusic = ableem::Music::load(sdir + "music.ogg");

    savedHighScore = Strings::toInt(app.config().inifile.values["surprisehighscore"]);
    game.seedHighScore(savedHighScore);

    if (credits.empty()) {
        credits = autobleemCredits();
        foot = autobleemFoot();
    }
}

//*******************************
// GuiAbout::autobleemCredits
//*******************************
vector<string> GuiAbout::autobleemCredits() {
    auto heading = [](const string &text) { return HeadingMark + text; }; // render() draws it as a heading
    return {
        heading(_("Code C++ and shell scripts")),
        "screemer, Axanar, mGGk, nex, genderbent",
        heading(_("Graphics")),
        "KaonashiFTW, GeekAndy, rubixcube6, NewbornfromHell",
        heading(_("Testing")),
        "MagnusRC, xboxiso, Azazel, Solidius, SupaSAIAN, Kingherb, saptis",
        heading(_("Database maintenance")),
        "Screemer,Kingherb",
        heading(_("Localization support")),
        "nex(German), Azazel(Polish), gadsby(Turkish), GeekAndy(Dutch), Pardubak(Slovak), SupaSAIAN(Spanish), "
        "Mate(Czech)",
        "Sasha(Italian), Jakejj(BR_Portuguese), jolny(Swedish), StepJefli(Danish), alucard73 / MagnusRC(French), "
        "Quenti(Occitan), ",
        heading(_("RetroArch and emulation cores")),
        "genderbent, KMFDManic",
        heading(_("Ported from AutoBleem-NG")),
        "cornelk (AutoBleem-NG), Axanar - lightgun games, RDB metadata, libretro-thumbnails covers, multi-disc merge",
        heading(_("Game data")),
        "libretro-database (Sony - PlayStation.rdb), libretro-thumbnails",
    };
}

//*******************************
// GuiAbout::autobleemFoot
//*******************************
// the lines under the credits: support, and - GPLv3 5(d), the "Appropriate Legal Notices" an interactive
// program shows - the copyright and the licence
vector<string> GuiAbout::autobleemFoot() {
    return {
        _("Support via Discord:") + " https://discord.gg/AHUS3RM",
        "Copyright (C) 2018-2026 screemer and the AutoBleem contributors",
        _("Free software under the GNU GPL v3 or later - no warranty. Source and licence:") +
            " github.com/autobleem/AutoBleem2",
    };
}

//*******************************
// GuiAbout::render
//*******************************
void GuiAbout::render() {
    std::shared_ptr<Gui> gui(Gui::getInstance());

    if (surpriseMode) {
        renderSurprise();
        return;
    }

    gui->renderBackground();

    renderer.setDrawColor(ableem::Color(0, 0, 0, 235));
    renderer.setBlendMode(ableem::BlendMode::Blend);

    ableem::Rect rect2;
    rect2.x = 0;
    rect2.y = 0;
    rect2.w = SCREEN_WIDTH;
    rect2.h = SCREEN_HEIGHT;

    renderer.fillRect(rect2);

    fx.render(gui->platform().ticks());

    // the logo and the version at the top, centred
    ableem::Rect rect;
    rect.x = SCREEN_WIDTH / 2 - 100;
    rect.y = 5;
    rect.w = 200;
    rect.h = 141;
    renderer.copy(logo, nullptr, &rect);
    PanelStyle style = gui->panelStyle();
    Fonts &fonts = gui->assets().themeFonts;
    auto centred = [&](const ableem::Font &f, const string &text, int y, const ableem::Color &color) {
        gui->text().renderText_WithColor(f, text, SCREEN_WIDTH / 2 - gui->text().textWidth(f, text) / 2, y, color,
                                         XALIGN_LEFT);
    };
    centred(fonts[FONT_15_BOLD], Version::FULL_VERSION, rect.y + rect.h + 4, style.secondary);

    // the credits as sections - a heading (the launcher's bold) and its names wrapped under it - flowed
    // into two columns, the left one first
    struct Section {
        string heading;
        vector<string> lines;
    };
    vector<Section> sections;
    for (const string &s : credits) {
        if (s.compare(0, HeadingMark.size(), HeadingMark) == 0)
            sections.push_back({s.substr(HeadingMark.size()), {}});
        else if (!sections.empty())
            sections.back().lines.push_back(s);
        else
            sections.push_back({"", {s}});
    }
    const ableem::Font &headingFont = fonts[FONT_20_BOLD];
    const ableem::Font &textFont = fonts[FONT_15_BOLD];
    const int columnGap = 40;
    const int columnWidth = (SCREEN_WIDTH - 2 * 80 - columnGap) / 2;
    const int top = rect.y + rect.h + 34;
    const int footTop = SCREEN_HEIGHT - PanelStyle::FooterHeight - 24 - static_cast<int>(foot.size()) * 20;
    auto sectionHeight = [&](const Section &sec) {
        int h = sec.heading.empty() ? 0 : 26;
        for (const string &line : sec.lines)
            h += max(20, gui->text().wrappedHeight(textFont, line, columnWidth));
        return h + 14;
    };
    int total = 0;
    for (const Section &sec : sections)
        total += sectionHeight(sec);
    int x = 80, y = top;
    int placed = 0;
    for (const Section &sec : sections) {
        // the right column starts once the left holds half the height
        if (x == 80 && placed >= (total + 1) / 2) {
            x = 80 + columnWidth + columnGap;
            y = top;
        }
        if (!sec.heading.empty()) {
            gui->text().renderText_WithColor(headingFont, sec.heading, x, y, style.text, XALIGN_LEFT);
            y += 26;
        }
        for (const string &line : sec.lines)
            y += max(20, gui->text().renderWrappedText(textFont, line, x, y, columnWidth, style.secondary));
        y += 14;
        placed += sectionHeight(sec);
    }

    // the foot: support, copyright, licence - centred above the footer
    y = footTop;
    for (const string &line : foot) {
        centred(textFont, line, y, style.text);
        y += 20;
    }

    // the footer's hints without its rule: the starfield is the panel here
    style.footer(*gui, gui->classicFooter(), "|@O| " + _("Back") + " |@Start| " + _("Surprise"), false);
    renderer.present();
}

//*******************************
// GuiAbout::renderSurprise
//*******************************
void GuiAbout::renderSurprise() {
    std::shared_ptr<Gui> gui(Gui::getInstance());

    gui->renderBackground();

    renderer.setDrawColor(ableem::Color(0, 0, 0, 235));
    renderer.setBlendMode(ableem::BlendMode::Blend);

    ableem::Rect rect2;
    rect2.x = 0;
    rect2.y = 0;
    rect2.w = SCREEN_WIDTH;
    rect2.h = SCREEN_HEIGHT;
    renderer.fillRect(rect2);

    fx.render(gui->platform().ticks());

    game.render(renderer, gui->text(), font, sprites);

    // Start restarts, Circle leaves the game (the footer without its rule: no panel on the play field)
    gui->panelStyle().footer(*gui, gui->classicFooter(),
                             "|@Start| " + _("Restart") + "  |@O| " + (game.gameOver() ? _("Back") : _("Exit game")),
                             false);
    renderer.present();
}

//*******************************
// GuiAbout::loop
//*******************************
void GuiAbout::loop() {
    std::shared_ptr<Gui> gui(Gui::getInstance());
    menuVisible = true;
    surpriseMode = false;
    crossHeld = false;
    while (menuVisible) {
        unsigned int ticks = gui->platform().ticks();

        if (surpriseMode) {
            game.update(ticks, gui->input().dpadLeft(), gui->input().dpadRight(), crossHeld);

            if (game.currentHighScore() > savedHighScore) {
                savedHighScore = game.currentHighScore();
                app.config().inifile.values["surprisehighscore"] = to_string(savedHighScore);
                app.config().save();
            }
        }

        render();
        Event e;
        while (gui->input().poll(e)) {
            // this is for pc Only
            if (e.type == Event::Type::Quit) {
                menuVisible = false;
            }
            switch (e.type) {
            case Event::Type::ButtonDown:
                if (e.button == Button::Start) {
                    if (surpriseMode) {
                        // in the game Start restarts it
                        game.reset(ticks);
                        crossHeld = false;
                        app.audio().cursor.play();
                    } else {
                        surpriseMode = true;
                        crossHeld = false;
                        game.reset(ticks);
                        // a faster, darker field with more comets: the lasers and pickups have to read
                        // against it, and it should feel like flying rather than drifting
                        StarFx::Style flying;
                        flying.speedScale = 2.5f;
                        flying.brightnessScale = 0.55f;
                        flying.cometOdds = 150;
                        flying.maxComets = 3;
                        fx.setStyle(flying);
                        if (app.audio().music.isPlaying()) {
                            // something is already playing (the theme's track or a custom one) -
                            // just duck it to 50% behind the game
                            app.audio().music.setVolume(64);
                            duckedThemeMusic = true;
                        } else {
                            // a silent theme or "nomusic": Surprise mode still gets some music
                            surpriseMusic.play(-1);
                            surpriseMusic.setVolume(96);
                            playingFallbackMusic = true;
                        }
                        app.audio().cursor.play();
                    }
                } else if (surpriseMode && e.button == Button::Cross) {
                    crossHeld = true;
                } else if (surpriseMode && e.button == Button::Circle) {
                    // Circle leaves the game, back to the credits
                    surpriseMode = false;
                    fx.setStyle(StarFx::Style()); // back to the About screen's calm backdrop
                    if (duckedThemeMusic)
                        app.audio().music.setVolume(128);
                    if (playingFallbackMusic) {
                        surpriseMusic.halt();
                        app.audio().playMusic(); // resume whatever the theme/config normally plays
                    }
                    duckedThemeMusic = false;
                    playingFallbackMusic = false;
                    app.audio().cancel.play();
                } else if (!surpriseMode && e.button == Button::Circle) {
                    app.audio().cancel.play();
                    menuVisible = false;
                }
                break;
            case Event::Type::ButtonUp:
                if (e.button == Button::Cross)
                    crossHeld = false;
                break;
            default:
                break;
            }
        }
    }
}
