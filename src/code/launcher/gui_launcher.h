//
// Created by screemer on 2/8/19.
//
#pragma once

#include "gui_NotificationLine.h"
#include "../gui/gui_screen.h"
#include "ps_obj.h"
#include "ps_settings_back.h"
#include "ps_zoom_btn.h"
#include "ps_meta.h"
#include "carousel.h"
#include "ps_move_bnt.h"
#include "ps_menu.h"
#include "ps_centerlabel.h"
#include "ps_stateselector.h"
#include "../core/main.h"
#include <vector>
#include <memory>
#include "../gui/gui.h"

// which sub-screen of the launcher is showing
enum class LauncherScreenState : int { Games = 0, Set, Resume, Info };

// the four icons of the settings overlay, in the order PsMenu lays them out. PsMenu::selOption stays a plain
// index into that row (it also animates the zoom per option), so it is compared through selOptionIs.
enum class LauncherMenuOption : int { AbSettings = 0, EditGameSettings, EditMemcardInfo, ResumeFromSavestate };
inline bool selOptionIs(int selOption, LauncherMenuOption option) { return selOption == static_cast<int>(option); }

extern const ableem::Color brightWhite;

//******************
// GuiLauncher
//******************
// The EvolutionUI launcher. One class in three files: launcher_screen.cpp (assets, sets, the metadata panel,
// state transitions, render), launcher_input.cpp (the event loop and the per-button handlers) and
// launcher_actions.cpp (what Cross does: start the game, open the editors and choosers, reconcile after).
class GuiLauncher : public GuiScreen {
public:
    void init();
    ~GuiLauncher();
    void render();

    // these variables are used by the loop routines
    long motionStart = 0;
    long timespeed = 0;
    int motionDir = 0;
    vector<string> headers;
    vector<string> texts;
    long time = 0;
    ableem::Event e;
    // for prev/next first letter fast forwarding
    unsigned int prevNextFastForwardTimeLimit = 200;
    bool L1_isPressedForFastForward = false;
    unsigned int L1_fastForwardTimeStart = 0;
    bool R1_isPressedForFastForward = false;
    unsigned int R1_fastForwardTimeStart = 0;

    void loop();

    void loop_joyMoveLeft();
    void loop_joyMoveRight();
    void loop_joyMoveUp();
    void loop_joyMoveDown();

    // a button is pressed
    void loop_joyButton_Pressed();
    void loop_chooseGameDir();
    void loop_chooseRAPlaylist();
    void loop_selectButton_Pressed();
    void loop_startButton_Pressed();
    void loop_circleButton_Pressed();
    void loop_triangleButton_Pressed();
    void loop_squareButton_Pressed();
    void loop_crossButton_Pressed();
    void loop_crossButtonPressed_STATE_GAMES();
    void loop_crossButtonPressed_STATE_SET();
    void loop_crossButtonPressed_STATE_SET__OPT_AB_SETTINGS();
    void loop_crossButtonPressed_STATE_SET__OPT_EDIT_GAME_SETTINGS();
    void loop_crossButtonPressed_STATE_SET__OPT_EDIT_MEMCARD();
    void loop_crossButtonPressed_STATE_SET__OPT_RESUME_FROM_SAVESTATE();
    void loop_crossButtonPressed_STATE_RESUME();

    // a button is released
    void loop_joyButtonReleased();
    void loop_prevNextGameFirstLetter(bool next);   // false is prev, true is next
    void loop_prevGameFirstLetter();
    void loop_nextGameFirstLetter();

    void nextCarouselGame(int speed);
    void prevCarouselGame(int speed);
    void updateMeta();
    void loadAssets();
    void freeAssets();

    // what the carousel is showing. loadAssets() seeds it from the Session, rememberSelection() writes it
    // back when a game starts.
    GameSetSelection selection;
    void rememberSelection();
    void switchSet(GameSet newSet, bool noForce);
    void showSetName();

    NotificationLines notificationLines; // top two lines of the screen

    bool powerOffShift=false;

    // the row of covers: the games it shows, the selected one, the scroll animation
    Carousel carousel{*gui};

    // the screen elements are owned by staticElements / frontElemets (created in loadAssets, freed in freeAssets).
    // the named pointers below are non-owning shortcuts into those vectors.
    PsSettingsBack *settingsBack = nullptr;
    PsObj *playButton = nullptr;
    PsZoomBtn *playText = nullptr;
    PsMeta *meta = nullptr;

    PsObj *background = nullptr;
    PsMoveBtn *arrow = nullptr;
    PsObj *xButton = nullptr;
    PsObj *oButton = nullptr;
    PsObj *tButton = nullptr;
    std::unique_ptr<PsMenu> menu;
    PsStateSelector * sselector= nullptr;

    ableem::Color fgColor { 255, 255, 255, 255 };
    ableem::Color secColor { 100, 100, 100, 255 };

    std::vector<std::unique_ptr<PsObj>> staticElements;
    std::vector<std::unique_ptr<PsObj>> frontElemets;

    // adds an element to one of the vectors above and returns the non-owning pointer for the shortcut members
    template <typename T> T *addStaticElement(T *obj) { staticElements.emplace_back(obj); return obj; }
    template <typename T> T *addFrontElement(T *obj) { frontElemets.emplace_back(obj); return obj; }

    PsCenterLabel * menuHead = nullptr;
    PsCenterLabel * menuText = nullptr;

    std::string gameName;
    std::string publisher;
    std::string year;
    std::string serial;
    std::string region;
    std::string players;

    bool staticMeta=false;
    bool gameInfoVisible = true;
    using GuiScreen::GuiScreen;

    std::vector<std::string> raPlaylists;

    LauncherScreenState state = LauncherScreenState::Games;
    void switchState(LauncherScreenState state, int time);
    void forceSettingsOnly();
    void showAllOptions();
};
