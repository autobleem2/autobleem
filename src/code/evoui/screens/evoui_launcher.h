//
// Created by screemer on 2/8/19.
//
#pragma once

#include "../controls/evoui_notification_line.h"
#include "../controls/evoui_notification_bubble.h"
#include "gui/gui_screen.h"
#include "../../app.h"
#include "core/services/scan_service.h"
#include "../controls/evoui_obj.h"
#include "../controls/evoui_settings_back.h"
#include "../controls/evoui_zoom_btn.h"
#include "../controls/evoui_meta.h"
#include "../carousel.h"
#include "../controls/evoui_move_btn.h"
#include "../controls/evoui_menu.h"
#include "../controls/evoui_centerlabel.h"
#include "../controls/evoui_stateselector.h"
#include "core/main.h"
#include "core/model/timing.h"
#include <vector>
#include <memory>
#include "gui/gui.h"

// which sub-screen of the launcher is showing
enum class LauncherScreenState : int { Games = 0, Set, Resume, Info };

// the four icons of the settings overlay, in the order PsMenu lays them out. PsMenu::selOption stays a plain
// index into that row (it also animates the zoom per option), so it is compared through selOptionIs.
enum class LauncherMenuOption : int { AbSettings = 0, EditGameSettings, EditMemcardInfo, ResumeFromSavestate };
inline bool selOptionIs(int selOption, LauncherMenuOption option) {
    return selOption == static_cast<int>(option);
}

extern const ableem::Color brightWhite;

//******************
// GuiLauncher
//******************
// The EvolutionUI launcher. One class in three files: launcher_screen.cpp (assets, sets, the metadata panel,
// state transitions, render), launcher_input.cpp (the event loop and the per-button handlers) and
// launcher_actions.cpp (what Cross does: start the game, open the editors and choosers, reconcile after).
class GuiLauncher : public GuiScreen {
public:
    App &app = App::get(); // the game model, over GuiScreen's AppBase (see gui_screen.h)
    // spelled out rather than inherited (using GuiScreen::GuiScreen): the console's GCC 6 cannot combine an
    // inherited constructor with a member initialised from another member, which `carousel` is
    explicit GuiLauncher(ableem::GuiBase &g) : GuiScreen(g), carousel(*gui) {}
    void init() override;
    ~GuiLauncher() override;
    void render() override;

    // these variables are used by the loop routines
    long motionStart = 0; // when the stick went left/right and stayed; 0 once it is centred again
    int motionDir = 0;    // 0 right (next game), 1 left
    // a tap that landed while a scroll was running: -1 previous, +1 next, run when the scroll ends,
    // so a quick second tap is not lost
    int queuedScroll = 0;
    vector<string> headers;
    vector<string> texts;
    string headersLanguage; // the language headers/texts were translated into
    long time = 0;
    ableem::Event e;
    // for prev/next first letter fast forwarding
    unsigned int prevNextFastForwardTimeLimit = 200;
    bool L1_isPressedForFastForward = false;
    unsigned int L1_fastForwardTimeStart = 0;
    bool R1_isPressedForFastForward = false;
    unsigned int R1_fastForwardTimeStart = 0;

    void loop() override;
    // set when the window's own close button fires an SDL Quit event - AutoBleem::run() checks this after
    // show() returns to actually stop, rather than looping back into a fresh GuiLauncher (see loop()'s comment)
    bool quitRequested = false;

    void loop_joyMoveLeft();
    void loop_joyMoveRight();
    void loop_joyMoveUp();
    void loop_joyMoveDown();

    // a button is pressed
    void loop_joyButton_Pressed();
    void loop_chooseSet(); // Select: the set picker (tabs PlayStation / RetroArch / Apps, the groups inside)
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
    void reloadFavoritesAfterRemoval();
    void reloadLightgunSetAfterEdit();
    void loop_crossButtonPressed_STATE_SET__OPT_RESUME_FROM_SAVESTATE();
    void loop_crossButtonPressed_STATE_RESUME();
    // the system menu: Re-Scan, RetroArch, Memory Cards, Game Manager, Options, About, Power Off, ... -
    // reached with L2+R2 (loop_joyButton_Pressed's powerOffShift branch)
    void loop_openSystemMenu();
#ifdef AB_ONLINE_UPDATE
    // the online update: the check's result once a frame (it asks when one lands), the system menu's
    // "Software Update" item (a check now, then the same question), and the download that ends in
    // MENU_OPTION_UPDATE on a Pi
    void pollUpdates();
    void loop_softwareUpdate();
    void offerUpdate(bool fromMenu);
#endif

    // a button is released
    void loop_joyButtonReleased();

    // A sub-screen opened while L2 (or L1/R1) is held runs its own event loop, so the release of that button
    // reaches the sub-screen, not us - and the launcher would come back believing it is still held. Called
    // after every show() that can be reached with a modifier down.
    void forgetHeldModifiers();
    void loop_prevNextGameFirstLetter(bool next); // false is prev, true is next
    void loop_prevGameFirstLetter();
    void loop_nextGameFirstLetter();

    // one step of the carousel: `speed` milliseconds, eased for a tap, linear for a held stick
    void nextCarouselGame(int speed, bool eased = true);
    void prevCarouselGame(int speed, bool eased = true);
    // the meta panel for the selected game; withSnap=false leaves the snap (a PNG decode) to
    // finishSettleLoads() once the carousel has stopped
    void updateMeta(bool withSnap = true);
    // the loads a scroll defers to the frame the carousel comes to rest in: the snap and the resume
    // picture, both PNG decodes off the SD card that used to cost the scroll its first frame
    bool settleLoadsPending = false;
    void finishSettleLoads();
    void loadAssets();
    void freeAssets();

    // what the carousel is showing. loadAssets() seeds it from the Session, rememberSelection() writes it
    // back when a game starts.
    GameSetSelection selection;
    void rememberSelection();
    void switchSet(GameSet newSet, bool noForce);
    void showSetName();
    // re-runs the current set's query and re-selects the same game by id (falling back to the first game,
    // or none) - what a scan finishing, or the Game Manager/Options changing the roster, needs: the list
    // itself may have gained, lost or reordered entries, so the old carousel index cannot be trusted.
    void reloadGames();

    NotificationLines notificationLines; // the messages, bubbles under the scan's at the top-right

    // the scan's progress, one line at the very bottom of the screen: applyScanUpdate() (called from loop(),
    // once a frame) turns each ScanUpdate from app.scans().poll() into this line's text, and reloads the
    // carousel via reloadGames() whenever the PS1 roster changed and no scroll animation is in the way.
    // the scan's progress, in the top-right corner while it runs; its summary a while after, then gone
    NotificationBubble scanBubble;
    void applyScanUpdate(const ScanUpdate &update);
    // the bubble's title and detail for a scan update
    void scanStatusText(const ScanUpdate &update, std::string &title, std::string &detail) const;
    bool scanRosterChangedSinceReload = false; // set by applyScanUpdate, cleared once reloadGames() runs

    // the extensions (docs/extensions-plan.md): their bubble under the scan's, and what they asked the
    // launcher for (App::takeExtensionRequests) acted on once a frame, after their poll()
    NotificationBubble extensionBubble;
    void applyExtensionRequests();
    // the system menu's Extensions item: the list, then the chosen one run
    void loop_openExtensions();

    bool powerOffShift = false;

    bool r2Held = false; // with L2 held too the system menu opens, whichever was pressed first

    // the row of covers: the games it shows, the selected one, the scroll animation
    Carousel carousel;

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
    // the footer's hint row: an icon (a theme image for X/O/T, chips for the rest) and a label each, laid
    // out by layoutHints() in the theme's hintBar at the largest font that fits the language
    struct Hint {
        PsObj *icon;         // the theme's hint image, positioned by layoutHints(); or
        std::string markers; // the chips, "|@L2+R2|" - drawn by render()
        std::string label;
        int labelX = 0, chipX = 0;
    };
    std::vector<Hint> hints;
    ableem::Font hintFont;
    int hintLabelY = 0, hintChipY = 0;
    void layoutHints();
    std::unique_ptr<PsMenu> menu;
    PsStateSelector *sselector = nullptr;

    ableem::Color fgColor{255, 255, 255, 255};
    ableem::Color secColor{100, 100, 100, 255};
    ableem::Color hintColor{100, 100, 100, 255}; // theme launcher.colors.hint, else secColor

    std::vector<std::unique_ptr<PsObj>> staticElements;
    std::vector<std::unique_ptr<PsObj>> frontElemets;

    // adds an element to one of the vectors above and returns the non-owning pointer for the shortcut members
    template <typename T> T *addStaticElement(T *obj) {
        staticElements.emplace_back(obj);
        return obj;
    }
    template <typename T> T *addFrontElement(T *obj) {
        frontElemets.emplace_back(obj);
        return obj;
    }

    PsCenterLabel *menuHead = nullptr;
    PsCenterLabel *menuText = nullptr;

    std::string gameName;
    std::string publisher;
    std::string year;
    std::string serial;
    std::string region;
    std::string players;

    bool staticMeta = false;
    bool textShadow = true; // theme launcher.textShadow: the dark halo under this screen's text
    bool gameInfoVisible = true;

    // a black overlay fading from fully opaque to transparent over LauncherFadeInDuration, so the launcher
    // eases in rather than cutting straight in - from black after the splash, or from whatever was on
    // screen (a sub-screen, PCSX) the rest of the time. loadAssets() restarts it; render() draws it.
    int fadeAlpha = 255;
    long fadeStart = 0;
    std::vector<std::string> raPlaylists;
    void refreshPlaylistNames(); // after the scan rewrote playlists

    LauncherScreenState state = LauncherScreenState::Games;
    void switchState(LauncherScreenState state, int time);
    // an empty set has nothing to play: the icon row (the settings icon) is the only place, the play
    // button hidden and Up refused until the roster has a game again - see settleEmptyRoster()
    void settleEmptyRoster();
    // the options row for the selected game: every icon for a PS1 game, settings + game editor for a
    // RetroArch game, settings alone for an App or an empty carousel (was forceSettingsOnly/showAllOptions)
    void showOptions();
    // the selected game as the emulator sees it: a PS1 game, even from the Lightgun set
    bool selectedIsPs1() const;
    // the selected game's screenshot in the theme's launcher.snapPanel, when the theme has one
    void loadSnap();
    void renderSnap();
    ableem::Texture snapTex;
    int snapForGameId = -1;
    bool snapForInternal = false;
};
