//
// Created by screemer on 2/8/19.
//
#pragma once

#include "../controls/evoui_notification_line.h"
#include "../controls/evoui_notification_bubble.h"
#include "gui/gui_screen.h"
#include <ab_gui/hint_bar.h>
#include <ab_gui/tween.h>
#include "../../app.h"
#include "core/services/scan_service.h"
#include "../controls/evoui_obj.h"
#include "../controls/evoui_settings_back.h"
#include "../controls/evoui_zoom_btn.h"
#include "../controls/evoui_meta.h"
#include "../carousel.h"
#include "../evoui_layout.h"
#include "../controls/evoui_move_btn.h"
#include "../controls/evoui_menu.h"
#include "../controls/evoui_centerlabel.h"
#include "../controls/evoui_stateselector.h"
#include "core/main.h"
#include "core/model/timing.h"
#include "core/model/pad_assignment.h"
#include "core/services/pad_battery.h"
#include "core/model/pad_battery_alert.h"
#include "core/model/pad_battery_match.h"
#include "core/services/game_query.h"
#include <functional>
#include <vector>
#include <memory>
#include <map>
#include <set>
#include "gui/gui.h"

enum class SystemMenuAction; // evoui_system_menu.h

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
    // every frame cleared to transparent black, the draw colour from then on (docs/ab-gui-plan.md, G3e)
    // no screen transition of its own (UIREV-48): after a game or a display change it fades in from black by itself
    // (startFadeIn); only at start-up after the splash does the stack drop it in from the top (Gui::display)
    explicit GuiLauncher(ableem::GuiBase &g) : GuiScreen(g), carousel(*gui) {
        frameColor = abgui::OptionalColor(ableem::Color(0x00, 0x00, 0x00, 0x00));
        declareTransitions(abgui::ScreenTransitions(abgui::Transition::none()));
        if (ctx.hasStack())
            ctx.stack().declareFrameCanvas(*this, [this] { useFrameCanvas(); });
    }
    void init() override;
    ~GuiLauncher() override;
    bool prepareFrame() override;
    void useFrameCanvas(); // the 4:3 layout's 640x480, else the 1280x720 one (also for a transition's picture of it)
                           // before each frame: the busy state ends, the state selector follows the menu
    void draw() override;  // the frame's picture: the stack clears before and presents after

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

    // a pad connected or disconnected: what PS1 port each one now lands on (C9), a NotificationLine -
    // quiet, only on the change, not shown at startup/loadAssets. seedPadAssignment() (loadAssets(), and
    // every time a game returns the display) records the current assignment as already "shown" without
    // popping the notice, so SDL's start-up PadAdded burst and the flush/reopen around a launch stay quiet;
    // showPadAssignment() (a live PadAdded/PadRemoved) only pops it when ableem::PadAssignment actually
    // differs from padAssignmentState.lastShown (decidePadAssignmentChange(), core/model/pad_assignment.h).
    // C16: unplugging the *only* connected pad leaves an empty assignment that decidePadAssignmentChange()
    // never shows directly any more (a lone pad's unplug is one PadRemoved event, never a second one to
    // confirm the pads are really gone rather than mid re-enumeration) - pollPadAssignmentEmptyNotice(),
    // called every frame like pollPadBattery(), shows "Controllers: None" once PadEmptyNoticeDelay has
    // passed with nothing reconnecting (checkPadAssignmentEmptyNotice(), core/model/pad_assignment.h).
    void seedPadAssignment();
    void showPadAssignment();
    void pollPadAssignmentEmptyNotice();
    PadAssignment currentPadAssignment() const;
    PadAssignmentState padAssignmentState;

    // C8: a small battery indicator per wireless pad, top-left corner - PadBatteryService (ab_core) reads
    // the kernel's power_supply sysfs tree, same as PSC-Bios's pairing screen. Polled at most every
    // PadBatteryPollInterval (core/model/timing.h), never every frame - a handful of sysfs reads is cheap,
    // but there is no reason to do it 60 times a second. lowBatteryAlert (core/model/pad_battery_alert.h) decides
    // when a pad gets the one-time "battery low" NotificationLine and when it is taken down again (the pad on a
    // charger, back over the reset level, gone) - so a reading sitting at 12% for ten minutes says it once.
    // lowBatteryText is the text each pad's notice was shown with, so a dismissal only hides the bubble while it
    // still shows that very text (line 1 is shared with every other message).
    // C12: each reading is also matched to the SDL pad it belongs to (matchPadBatteries(),
    // core/model/pad_battery_match.h - by the pad's own serial against the sysfs address), so the label is
    // "Player 1"/"Player 2" when that match succeeds; padBatteryLabelsFor() falls back to the old generic
    // "Wireless pad N" when it does not (an address with no matching pad still gets a stable number,
    // counted among the unmatched entries only). padBatteryIconTags is the short form of the same match
    // ("P1"/"P2", "" when unmatched) the icon row draws next to a matched pad's icon.
    PadBatteryService padBatteryService;
    std::vector<PadBatteryInfo> padBatteries;
    std::vector<std::string> padBatteryLabels;   // padBatteryLabels[i] is padBatteries[i]'s label (for the
                                                 // low-battery notice), recomputed with it each poll
    std::vector<std::string> padBatteryIconTags; // padBatteryIconTags[i] is padBatteries[i]'s short icon
                                                 // tag ("P1"/"P2"/""), recomputed together with the above
    long lastPadBatteryPoll = 0;
    PadBatteryAlert lowBatteryAlert;
    std::map<std::string, std::string> lowBatteryText;
    void pollPadBattery();
    int renderPadBatteries(); // returns the plate's bottom edge (0: no plate drawn)
    void renderChannelWatermark(int plateBottom);
    // UIREV-43: a fresh install's welcome card, drawn where the covers would be when the PS1 "all games" set is empty
    bool welcomeCardShows() const;
    void renderWelcomeCard();
    // the layout profile: the 1280x720 launcher, or the 4:3 canvas's (evoui_layout.h - chosen in loadAssets)
    EvoLayout layout;
    // a part laid out for 1280x720 only: drawn as it is, or on the 4:3 canvas into wideLayer and letterboxed there
    void drawWide(const std::function<void()> &draw);
    ableem::Texture wideLayer;
    // the rect the hint grid is laid out in: the hint bar, or on the 4:3 layout the bar 1 / hintScale its size at 0, 0
    // (drawn into hintLayer, which goes into the bar scaled)
    ableem::Rect hintLayoutRect() const;
    ableem::Texture hintLayer;
    unsigned long hintLayerAt = 0;
    unsigned long wideLayerAt = 0;
    // fills both padBatteryLabels and padBatteryIconTags from one pass of matchPadBatteries() - out params
    // rather than a struct-of-two-vectors to keep the call site in pollPadBattery() simple
    std::vector<std::string> padBatteryLabelsFor(const std::vector<PadBatteryInfo> &batteries,
                                                 std::vector<std::string> &iconTagsOut) const;

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
    // an App with Uses= and the game data it starts with (none: false after a message; one; the picker)
    bool chooseGameData(const PsGamePtr &game);
    void loop_openPackageInfo(const PsGame &game);
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
    // the Quick menu: Re-Scan, Store, Network & Controllers, System menu... - d-pad Up in the Games state (and
    // on an empty set); the gear icon of the game's icon row is Options
    void loop_openQuickMenu();
    // what an item of either menu does
    void runMenuAction(SystemMenuAction action);
    // Options (the System menu's): the screen, then the theme, the sets and the covers reloaded
    void loop_openOptions();
    // an extension provides the "network" entry here: the Network & Controllers item shows
    bool networkProvided();
    // one does, but cannot run (switched off, another AutoBleem, not built for this system): why, as the
    // greyed item's description ("PSC-Bios is switched off - enable it in Extensions"), and which one - the
    // item then opens the Extensions list at it. "" when a provider can run, or none is installed
    std::string networkUnavailable(std::string *extension = nullptr);
    // an extension run from a menu: by name, or at `entry` by whichever provides it (name ""); the refusal
    // reported on the notification line
    void runExtensionEntry(const std::string &name, const std::string &entry);
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

    // UIREV-26 (G5r5): every screen opened from the launcher draws over ONE snapshot of it, not over the theme's plain
    // background. takeBackdrop() draws the launcher once without its hint band and bubbles (snapshotFrame, draw()),
    // keeps that frame and hands it to Gui::setLauncherBackdrop - Gui's renderBackground(), the Context's
    // backdropDrawer (the ab_gui screens, the extensions') and renderer.lastCapture() all give it while it is held.
    // Returns true when no snapshot was held before: the caller then owns it and drops it. BackdropScope is that, for
    // one opening: take in the constructor, drop in the destructor (or release() earlier, e.g. before the theme
    // reloads). An opening inside another (the System menu's Memory Cards) keeps the outer frame and leaves the outer
    // one the drop, unless `fresh`: then the frame is taken again (an extension reads renderer.lastCapture(), which
    // must be this one).
    bool takeBackdrop(bool fresh = false);
    void dropBackdrop();
    bool snapshotFrame = false; // draw(): the frame is the backdrop's - no footer band, hints, bubbles, fade
    class BackdropScope {
    public:
        explicit BackdropScope(GuiLauncher &launcher, bool fresh = false)
            : launcher_(launcher), owns_(launcher.takeBackdrop(fresh)) {}
        BackdropScope(const BackdropScope &) = delete;
        BackdropScope &operator=(const BackdropScope &) = delete;
        ~BackdropScope() { release(); }
        void release() {
            if (owns_)
                launcher_.dropBackdrop();
            owns_ = false;
        }

    private:
        GuiLauncher &launcher_;
        bool owns_;
    };

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
    // an animation is running or input is held: the loop draws every frame, else at the ambient rate
    bool somethingMoves() const;
    // measuring on a device (not for users): AB_BENCH_SCROLL=1 runs the row back and forth by itself as a held
    // stick would, =tap by single taps (350 ms apart), =menu opens and closes the game menu (700 ms apart);
    // AB_SKIP=part,part leaves parts of the frame undrawn (a static element's name, carousel, menu, hints,
    // bubbles, front) - the frame statistics then say what each part costs
    enum BenchMode { BenchOff, BenchHold, BenchTap, BenchMenu };
    static int benchMode();
    static bool benchSkips(const std::string &part);
    void benchStep();
    int benchDir = 0;
    long benchLast = 0;
    void finishSettleLoads();
    void loadAssets();
    void freeAssets();

    // what the carousel is showing. loadAssets() seeds it from the Session, rememberSelection() writes it
    // back when a game starts.
    GameSetSelection selection;
    void rememberSelection();
    void switchSet(GameSet newSet, bool noForce);
    void showSetName();
    // the informational bubbles (the set, a saved resume point, the controllers, the scan summary) hold for
    // Options' "Notification timeout" seconds and are not shown at 0; the error ones keep their fixed time
    long infoTimeout() const;
    void showInfo(const std::string &text);
    // re-runs the current set's query and re-selects the same game by id (falling back to the first game,
    // or none) - what a scan finishing, or the Game Manager/Options changing the roster, needs: the list
    // itself may have gained, lost or reordered entries, so the old carousel index cannot be trusted.
    void reloadGames();
    void afterRetroArchJob(); // the RetroArch manager's job ran: see its definition
    // which game is highlighted, in a form that survives the list being queried again: a library game by id,
    // a playlist game by its image path (its id is only its position) - and finding it in the new list
    struct GameKey {
        int gameId = -1;
        bool internal = false;
        bool foreign = false;
        std::string imagePath;
    };
    GameKey selectedGameKey() const;
    // the set picker's (Select) row counts, kept between openings so Select opens at once (its tab icons are the
    // Context's); forgetSetCounts() whenever the library may have changed - a scan, an edit, a game played, a menu
    // action (Game Manager, the Store, an extension), the screen reloaded
    GameQueryService::SetCounts setCounts;
    bool setCountsValid = false; // C++14 on the console: no std::optional
    void forgetSetCounts() { setCountsValid = false; }
    int findGame(const GameKey &key) const; // its index in carousel.games, -1 when it is gone

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
    // select: the extension the list opens at ("" = the first)
    void loop_openExtensions(const std::string &select = "");
    // the system menu's Scanner processors item: the sequences sorted, a scan when anything changed
    void loop_openProcessors();

    bool powerOffShift = false;

    bool r2Held = false; // with L2 held too the system menu opens, whichever was pressed first

    // the row of covers: the games it shows, the selected one, the scroll animation
    Carousel carousel;

    // the screen elements are owned by staticElements / frontElemets (created in loadAssets, freed in freeAssets).
    // the named pointers below are non-owning shortcuts into those vectors.
    PsSettingsBack *settingsBack = nullptr;
    PsObj *playButton = nullptr;
    PsZoomBtn *playText = nullptr;
    // Play's outline: the launcher text's dark halo around its images, made once per theme (makePlayOutline) -
    // one for the button, one for the text that pulses with it (drawn at the text's zoom each frame)
    ableem::Texture playOutline;
    ableem::Rect playOutlineRect;
    ableem::Texture playTextOutline;
    int playTextOutlineW = 0,
        playTextOutlineH = 0; // the text outline's size at zoom 1 (the text + 2 px each side, +1 down-right)
    void makePlayOutline(const LauncherTheme &theme);
    // a theme with the `play` frame (G5j) draws Play as that still frame + the `play` icon + this label, in place of
    // the two images and their outline: the label is the hint line's word in capitals, in the largest bold font (28
    // down to 14) that fits the button, ellipsized when even that one is too wide - refitted when the word changes
    std::string playLabel;
    ableem::Font playLabelFont;
    // icon + label drawn once into this texture (renderPlayFrame), pulsed over the still frame
    ableem::Texture playContent;
    int playContentW = 0, playContentH = 0;
    unsigned long playContentAt = 0; // Renderer::targetsLost() it was drawn at
    void renderPlayFrame();
    PsMeta *meta = nullptr;

    PsObj *background = nullptr;
    PsMoveBtn *arrow = nullptr;
    // the footer's hint bar (UIREV-36): a fixed grid of 4 columns x 2 lines, every item at its home slot
    // (evoui/controls/hint_slots.h: line 1 is what acts on the current selection, a slot with nothing in this state
    // stays empty; line 2 is always Select/Start/Guide/System, an item that does nothing here is dimmed). Each hint
    // is a marker string ("|@X|", "|@L2+R2|", "|@Left+Right|" - drawn through PanelStyle::buttons(), the launcher's
    // own X/O/T images included: see PanelStyle::faceIcon) and its label; an empty `markers` is an empty slot. The
    // columns and the one font are laid out once per language and bar by layoutHints() (abgui::HintBar::layoutGrid)
    // over the theme's `hintBar` frame when it has one. updateHintsIfNeeded() rebuilds the items from a signature of
    // what they depend on and calls layoutHints() only when that signature changes - the "cache the layout" rule -
    // so render() can call it every frame for free.
    struct Hint {
        std::string markers; // the chips, e.g. "|@L2+R2|" - drawn by render(); empty = an empty slot
        std::string label;
        bool dim = false; // drawn at HintSlots::DimAlpha
        int labelX = 0, chipX = 0;
    };
    std::vector<Hint> hints;  // line 1, one per column
    std::vector<Hint> hints2; // line 2, one per column
    ableem::Font hintFont;    // one font for both lines
    int hintLabelY = 0, hintChipY = 0;
    int hintLabelY2 = 0, hintChipY2 = 0;
    bool hintsOneLineOnly = false; // the theme's hintBar is under 48 px tall: line 2 is not drawn at all
    abgui::HintGridLayout hintGrid;
    std::string hintGridKey; // the language and bar hintGrid was laid out for
    std::string lastHintSignature;
    // the two hint lines for the current state/selection, in the language it was built in
    void buildHintLines(std::vector<Hint> &line1, std::vector<Hint> &line2) const;
    // a short summary of everything buildHintLines() depends on - state, the resume slot's use and operation,
    // the selected game's kind, RetroArch availability, language - so layoutHints() runs only when it changes
    std::string hintSignature() const;
    // Env::retroArchInstalled() for the per-frame footer: re-checked every 2 s, not a stat per binary a frame
    bool retroArchInstalledCached() const;
    mutable bool raInstalled_ = false;
    mutable unsigned int raCheckedAt_ = 0;
    void updateHintsIfNeeded();
    // lays the grid out in hintBarRect() through abgui::HintBar::layoutGrid (pure: font, columns) and puts the items
    // in their slots
    void layoutHints();
    // the theme's launcher.hintBar, else the default pill (560, 624, 680 x 72): the lines' bar and the `hintBar`
    // frame's box
    ableem::Rect hintBarRect() const;
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
    int captionOption = -1;  // the icon the caption was last set for (-1: none yet)
    bool menuForApp = false; // the icon row was last set up for an App (its Game icon is "Game settings")

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
    // screen (a sub-screen, PCSX) the rest of the time. loadAssets() restarts it (startFadeIn); render() draws it.
    // The time gone is a tween's (non-ambient: the DebugDriver is busy while it runs), the alpha is worked out from it.
    void startFadeIn(unsigned int durationMs);
    int fadeAlpha = 255;
    float fadeMs = 0; // milliseconds into the fade - the tween's value
    unsigned int fadeDuration = LauncherFadeInDuration;
    abgui::TweenOwner fadeOwner; // after the float it writes: it goes first and stops the tween
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
    // the game menu's caption (menuHead/menuText) is the one of the icon the cursor is on - or is moving to, while
    // an icon move runs - set whenever that is not what it shows, so it can never lag or miss the selection
    void syncMenuCaption();
    // headers/texts translated again when the language changed since they were (also resets the caption)
    void retranslateMenu();
    // any of the game's resume slots active (UIREV-13: greys the Resume icon and refuses opening the
    // picker when this is false)
    bool gameHasResumePoints(const PsGamePtr &game) const;
    // the selected game as the emulator sees it: a PS1 game, even from the Lightgun set
    bool selectedIsPs1() const;
    // the selected game is a PSN PS1 Classic still under its licence's DRM (the scan found it): it is not
    // started - pcsx-ab would read noise - and this says why; true when the start was refused
    bool refuseLicenceProtected();
    // the selected game's screenshot in the theme's launcher.snapPanel, when the theme has one
    void loadSnap();
    void renderSnap();
    // where the selected game's screenshot is (empty: none)
    std::string snapPathFor(const PsGame &game);
    ableem::Texture snapTex;
    int snapForGameId = -1;
    bool snapForInternal = false;

    // the snap and the resume picture finishSettleLoads() asked for, decoded in the background and put on
    // the GPU by pollSettleLoads() when they are ready - the picture shown until then is the last one
    CoverLoader extrasLoader;
    std::string pendingSnapPath, pendingResumePath;
    void pollSettleLoads();
    // the snap and the resume picture belong to the game the row rests on: gone while it moves
    void hideSettlePictures();
};
