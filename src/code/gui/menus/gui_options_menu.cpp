#include "gui_options_menu.h"
#include <algorithm>
#include <ableem/engine/theme_spec.h>
#include <ableem/ui/canvas.h>
#include "core/services/system.h"
#include "core/services/environment.h"
#include "core/services/theme_converter.h"
#include "core/services/theme_zip_cache.h"
#include "core/services/output_mode.h"

using namespace std;

string GuiOptions::getStatusLine() {
    auto id = lines[selected].id;
    string hints = "|@L1/R1| " + _("First/last") + "   |@L2/R2| " + _("Page");
    hints += "   |@Left+Right| " + _("Choose") + "   |@O| " + _("Back");
    if (id == CFG_THEME || id == CFG_MUSIC)
        hints += "  |@Start| " + _("Random");
    return hints + "|";
}

//*******************************
// GuiOptions::getThemes
//*******************************
vector<string> GuiOptions::getThemes() {
    vector<string> list;
    string uiThemePath = Env::getPathToThemesDir();
    DirEntries uiThemeFolders = DirEntry::diru_DirsOnly(uiThemePath);
    for (const DirEntry &entry : uiThemeFolders) {
        // a theme.json, or an old-layout folder that Theme::load() will convert when it is picked
        const string dir = uiThemePath + sep + entry.name;
        // a theme.json with "hidden": true stays installed (and loads when config.ini names it) but is not offered
        if (ThemeConverter::isThemeFolder(dir) && !ableem::loadThemeHidden(dir)) {
            list.push_back(entry.name); // add the theme dir name
        }
    }
    // a <name>.zip is listed as <name> without being unpacked (it is, when picked); a folder of that name wins
    for (const string &name : ThemeZipCache::listZipThemes(uiThemePath))
        list.push_back(name);

    // in the CRT 4:3 mode only the themes with a 4:3 layout (a zip theme is not unpacked here: it has none to show)
    return OutputMode::themesFor(
        OutputMode::parse(app.config().inifile.values[OutputMode::ConfigKey]), list, [&](const string &name) {
            return ableem::ThemeSpec::supports4x3(uiThemePath + sep + name + sep + "theme.json");
        });
}

//*******************************
// GuiOptions::getJewels
//*******************************
vector<string> GuiOptions::getJewels() {
    vector<string> list;
    DirEntries folders = DirEntry::diru_FilesOnly(Env::getWorkingPath() + sep + "evoimg/frames");
    for (const DirEntry &entry : folders) {
        if (DirEntry::getFileExtension(entry.name) == "png") {
            list.push_back(entry.name);
        }
    }

    return list;
}

//*******************************
// GuiOptions::getMusic
//*******************************
vector<string> GuiOptions::getMusic() {
    vector<string> list;
    list.push_back("--");
    DirEntries folders = DirEntry::diru_FilesOnly(Env::getWorkingPath() + sep + "music");
    for (const DirEntry &entry : folders) {
        if (DirEntry::getFileExtension(entry.name) == "ogg") {
            list.push_back(entry.name);
        }
    }

    return list;
}

//*******************************
// GuiOptions::getTimeoutValues
//*******************************
vector<string> GuiOptions::getTimeoutValues() {
    vector<string> list;
    for (int i = 0; i <= 20; ++i) {
        list.push_back(to_string(i));
    }

    return list;
}

//*******************************
// GuiOptions::getOutputModes
//*******************************
// The console: 720p, 1080p or 720x480 (CRT 4:3) - Weston's mode, set by rc/boot.sh (its HDMI driver reads no EDID,
// so there is nothing to list). Elsewhere: the display's own mode (auto) and every mode its EDID lists at 50 Hz or
// more; the CRT 4:3 mode, when it is listed, comes right after 720p and 1080p.
vector<string> GuiOptions::getOutputModes() {
#ifdef AB_PLATFORM_PSC
    vector<string> list{"720", "1080", OutputMode::CrtToken()};
#else
    vector<string> list{"auto"};
    const vector<ableem::DisplayMode> modes = ableem::Platform::displayModes();
    for (const ableem::DisplayMode &m : modes) {
        OutputMode mode;
        mode.w = m.w;
        mode.h = m.h;
        list.push_back(mode.token());
    }
    list = OutputMode::placeCrt(list);
#endif
    // "Auto (1080p)": the mode Auto would really pick - the display's own, its biggest listed one - and not the one
    // the window runs in now (800x600 after a live switch); asked once here, never per drawn frame. No list (the
    // console, which has none): the window's mode, else the desktop's
    ableem::Size desktop;
#ifndef AB_PLATFORM_PSC
    desktop = ableem::Platform::largestMode(modes);
#endif
    if (desktop.w <= 0 || desktop.h <= 0)
        desktop = gui->platform().windowDisplaySize();
    if (desktop.w <= 0 || desktop.h <= 0)
        desktop = ableem::Platform::desktopDisplaySize();
    OutputMode own;
    own.w = desktop.w;
    own.h = desktop.h;
    autoLabel = own.isAuto() ? _("Auto") : _("Auto") + " (" + own.label() + ")";
    // a mode chosen on another display stays listed, so the row does not jump away from it by itself
    const string current = OutputMode::parse(app.config().inifile.values[OutputMode::ConfigKey]).token();
    if (find(list.begin(), list.end(), current) == list.end())
        list.push_back(current);
    return list;
}

//*******************************
// GuiOptions::fill
//*******************************
void GuiOptions::fill() {
    // this is filled once and not on every render.
    // save the current lang and switch to English.  we need the "Prefix:" to be scanned in English for English.txt
    // getLineText() will do the translation
    string saveCurrentLang = app.lang().currentLanguage();
    app.lang().load(Env::getPathToLangDir(), "English");

    // the rows in groups, each under a heading row (CFG_HEADING - drawn as a band, skipped by the cursor)
    auto heading = [&](const string &name) { lines.emplace_back(CFG_HEADING, name); };
    // the display mode first, then the look, then the set splash; the fonts under a heading of their own
    // (the owner's order, 2026-09-29)
    heading(_("Interface"));
    // the display mode (OutputMode) - the launcher's and the PS1 emulator's; only where the launcher is full
    // screen (a dev host's window has no mode to change)
    if (Gui::fullscreen())
        lines.emplace_back(CFG_DISPLAY, _("Display:"), OutputMode::ConfigKey, false, getOutputModes());
    // the safe area (overscan): on any 4:3 output - the tube (720x480, its own setting, 5 % unless set) and a VGA mode
    // (640x480, 800x600 ..., a setting of its own, 0 unless set); hidden on a wide one; applied at once
    if (Gui::fullscreen() && renderer.fourByThreeOutput()) {
        const ableem::Size window = gui->platform().windowSize();
        OutputMode shown;
        shown.w = window.w;
        shown.h = window.h;
        lines.emplace_back(CFG_CRT_MARGIN, _("CRT margin:"), OutputMode::marginKeyFor(shown), false,
                           vector<string>({"0", "1", "2", "3", "4", "5", "6", "7", "8", "9", "10"}));
        // the picture height, one value for every 4:3 output (CRT and VGA): the frame taller or shorter by that many
        // output pixels, centred (a taller one is cropped top and bottom, for a tube's overscan); applied at once
        vector<string> heights;
        for (int px = -ableem::MaxVerticalAdjust; px <= ableem::MaxVerticalAdjust; px += ableem::VerticalAdjustStep)
            heights.push_back(to_string(px));
        lines.emplace_back(CFG_VSIZE, _("Picture height:"), OutputMode::VsizeKey, false, heights);
    }
    // how the PS1 emulator fits a game's picture to the screen: the emulator's own menu's Scaler (pcsx-abnxt's
    // g_scaler, AB_SCALER); the classic pcsx-ab and RetroArch know only full and 4:3 (LaunchService). Under the
    // display mode, the owner's place for it (2026-09-29); it replaced the Widescreen switch
    lines.emplace_back(CFG_SCALER, _("Emulator screen scaling:"), "scaler", false,
                       vector<string>({"1x1", "2x", "4:3", "4:3i", "full"}));
    lines.emplace_back(CFG_THEME, _("AutoBleem theme:"), "theme", false, getThemes());
    lines.emplace_back(CFG_JEWEL, _("Cover style:"), "jewel", false, getJewels());
    // the shine that crosses the selected cover when the row comes to rest (Carousel::drawShine)
    lines.emplace_back(CFG_COVER_SHINE, _("Cover shine:"), "covershine", true, vector<string>({"false", "true"}));
    lines.emplace_back(CFG_LANG, _("Language:"), "language", false, Lang::listLanguages(Env::getPathToLangDir()));
    // how long the informational bubbles ("Showing: <set>", the scan's summary, ...) stay: 0 = not shown at all
    // (valueText: Off). Errors keep their own fixed time
    lines.emplace_back(CFG_SHOWINGTIMEOUT, _("Notification timeout:"), "showingtimeout", false, getTimeoutValues());
    // the boot splash (Gui::display); off goes straight to the launcher
    lines.emplace_back(CFG_SPLASH_SCREEN, _("Splash screen:"), "splashscreen", true, vector<string>({"false", "true"}));
    // the screen transitions (Gui::loadAssets -> ScreenStack::setAnimations): off, every screen change is instant
    lines.emplace_back(CFG_ANIMATIONS, _("Animations:"), "animations", true, vector<string>({"false", "true"}));

    heading(_("Fonts"));
    // "themefont" on: the theme's font, else the default (Red Hat Text, Fonts::DefaultClassicFont) - the key kept its
    // name when a theme's own classic font stopped being read (2026-09-29)
    lines.emplace_back(CFG_THEME_FONT, _("Use default font:"), "themefont", true, vector<string>({"false", "true"}));
    lines.emplace_back(CFG_FONT, _("Font:"), "font", false, getFonts());

    heading(_("Sound"));
    lines.emplace_back(CFG_MUSIC, _("Music:"), "music", false, getMusic());
    lines.emplace_back(CFG_ENABLE_BACKGROUND_MUSIC, _("Background music:"), "nomusic", true,
                       vector<string>({"true", "false"}));

    heading(_("Emulation"));
    // the PS1 emulator a game starts in: the one AutoBleem has always shipped, or the next one (see Config)
    lines.emplace_back(CFG_EMULATOR, _("PS1 emulator:"), "emulator", false, vector<string>({"pcsx-abnxt", "pcsx-ab"}));
    // C11: a positional swap of the first two SDL pads' PS1 ports (core/model/pad_assignment.h,
    // LaunchService's AB_PAD_ORDER) - PS1 only, RetroArch is unaffected, hence the row saying so; next to the
    // PS1 emulator it applies to
    lines.emplace_back(CFG_PAD_SWAP, _("Swap Player 1 / Player 2 (PS1 emulators):"), "padswap", true,
                       vector<string>({"false", "true"}));
    // the three RetroArch rows only where the RetroArch program is installed (Env::retroArchInstalled); their
    // saved values are keyed by name and stay as they are while the rows are hidden
    if (Env::retroArchInstalled()) {
        lines.emplace_back(CFG_PLAY_ALL_PSX_WITH_RA, _("Play all PSX games with RA:"), "play_all_psx_with_ra", true,
                           vector<string>({"false", "true"}));
        lines.emplace_back(CFG_RACONFIG, _("Update RA config:"), "raconfig", true, vector<string>({"false", "true"}));
        // RetroArch's config_save_on_exit (see Config): whether a change made in RetroArch is kept
        lines.emplace_back(CFG_RA_PERSIST, _("Persist RetroArch config:"), "rapersist", true,
                           vector<string>({"false", "true"}));
        // the RetroArch game lists without the (region) [!] tags; the files and the PS1 titles are not touched
        lines.emplace_back(CFG_CLEAN_NAMES, _("Clean RetroArch game names:"), "cleannames", true,
                           vector<string>({"false", "true"}));
    }

    heading(_("Library"));
#ifdef AB_HAS_INTERNAL_GAMES
    // an appliance or a Windows PC has no built-in games to show (GameQueryService::showInternalGames is hard
    // false there)
    lines.emplace_back(CFG_SHOW_ORIGAMES, _("Show internal games:"), "origames", true,
                       vector<string>({"false", "true"}));
#endif
    // only where the platform can fetch at all (download_command in its ini)
    if (!Env::downloadCommand().empty())
        lines.emplace_back(CFG_ONLINE, _("Fetch box art online:"), "online", true, vector<string>({"true", "false"}));
    if (lines.back().id == CFG_HEADING)
        lines.pop_back(); // a console: nothing under it
#if defined(AB_ONLINE_UPDATE) && (defined(AB_APPLIANCE) || defined(AB_PLATFORM_WIN) || defined(AB_PLATFORM_PSC))
    // the online update's channel (UpdateService): the download site's releases, its pre-release (testing),
    // its newest development build (nightly), or off. The real targets only (the owner's call, 2026-09-20;
    // the console since 2026-09-23 - it checks only with a network, see App::applyUpdateSetting) -
    // a dev host tests the flow with the default
    heading(_("Updates"));
    lines.emplace_back(CFG_UPDATES, _("Updates:"), "updates", false,
                       vector<string>({"release", "testing", "nightly", "off"}));
#endif

    // the logs live in RAM and reach the stick only after a crash (autobleem-main's docs/archive/quiet-stick-plan.md);
    // a tester keeps them all - from the next start, which is when the logs dir is chosen. The same switch as the
    // System/Logs/keep marker, which this row makes and removes.
    heading(_("Diagnostics"));
    lines.emplace_back(CFG_KEEPLOGS, _("Keep logs on the stick:"), "keeplogs", true, vector<string>({"false", "true"}));
    // the renderer's overlay: frame rate, CPU load, threads, memory in the bottom-left corner
    lines.emplace_back(CFG_PERFOVERLAY, _("Show performance:"), "perfoverlay", true, vector<string>({"false", "true"}));

    app.lang().load(Env::getPathToLangDir(), saveCurrentLang);
}

//*******************************
// GuiOptions::getFonts
//*******************************
vector<string> GuiOptions::getFonts() {
    vector<string> list;
    for (const string &dir : Fonts::userFontDirs()) {
        for (const DirEntry &entry : DirEntry::diru_FilesOnly(dir)) {
            string ext = ableem::toLowerCopy(DirEntry::getFileExtension(entry.name));
            // an empty file is no font (one in retroarch/fonts crashed the screen that tried to draw with it)
            if ((ext == "ttf" || ext == "otf") && DirEntry::fileSize(dir + sep + entry.name) > 0 &&
                find(list.begin(), list.end(), entry.name) == list.end())
                list.push_back(entry.name);
        }
    }
    return list;
}

//*******************************
// GuiOptions::prepareFrame
//*******************************
bool GuiOptions::prepareFrame() {
    publishToDriver(); // the DebugDriver's rows and cursor (Options draws its rows itself, not via renderLines)
    holdTick();
    return true;
}

//*******************************
// GuiOptions::draw
//*******************************
// what the stack's frame holds (docs/ab-gui-plan.md, G3d)
void GuiOptions::draw() {
    gui->renderBackground();
    gui->renderTextBar();
    yoffset = gui->renderHeader(getTitle());

    // the rows pack under the header at the font's height, as many as the panel holds (init()), and
    // scroll a row at a time through the base's paging (computePagePosition/adjustPageBy)
    const int fontHeight = font.lineHeight();
    const int firstLineY = yoffset + fontHeight * firstRow;
    if (firstRender) {
        // the rows that fit the canvas of this frame: init() ran from the launcher, whose 4:3 canvas is smaller
        maxVisible = gui->classicRowsThatFit(font);
        lastVisibleIndex = firstVisibleIndex + maxVisible - 1;
        computePagePosition();
        firstRender = false;
    }
    const int count = getVerticalSize();
    // a theme's selection frame goes under every row's text, so it is drawn before all of them - its bleed would
    // cover the row above otherwise (G4d); without a frame the band is drawn with its row, as before
    const bool framed = gui->text().selectionFramed(gui->uiContext());
    if (framed && selected >= firstVisibleIndex && selected <= lastVisibleIndex && selected < count)
        gui->text().renderSelectionBox(gui->uiContext(), 0, firstLineY + fontHeight * (selected - firstVisibleIndex),
                                       selectionBoxXOffset, font);
    for (int i = firstVisibleIndex, row = 0; i <= lastVisibleIndex && i < count; i++, row++) {
        if (i < 0)
            continue;
        const int y = firstLineY + fontHeight * row;
        if (lines[i].id == CFG_HEADING) {
            gui->text().renderLabelBox(gui->uiContext(), 0, y);
            TextRenderer::RowRoleScope role(gui->text(), TextRenderer::RowRole::Heading);
            gui->text().renderTextLine(app.lang().translate(lines[i].descriptionToTranslate), -y, 0, XALIGN_LEFT, 0,
                                       font);
            continue;
        }
        if (i == selected && !framed)
            gui->text().renderSelectionBox(gui->uiContext(), 0, y, selectionBoxXOffset, font);
        // the theme's roles (UIREV-29): the selected row bright, the others dim
        TextRenderer::RowRoleScope role(gui->text(),
                                        i == selected ? TextRenderer::RowRole::Selected : TextRenderer::RowRole::Row);
        renderOptionRow(lines[i], y);
    }
    gui->renderScrollMarkers(firstVisibleIndex > 0, lastVisibleIndex < count - 1);

    gui->renderStatus(getStatusLine());
}

//*******************************
// GuiOptions::init
//*******************************
void GuiOptions::init() {
    GuiOptionsMenuBase::init(); // call the base class init()
    lines.clear();
    fill();
    string &mode = app.config().inifile.values[OutputMode::ConfigKey];
    mode = OutputMode::parse(mode).token(); // "1920x1080" is "1080", as the row lists it
#ifdef AB_PLATFORM_PSC
    if (mode != "1080" && mode != OutputMode::CrtToken())
        mode = "720"; // what rc/boot.sh runs Weston in for anything but 1080 and the CRT mode
#endif
    outputModeOnEntry = mode;
    newOutputMode.clear();
    selected = 0;
    settleOnOption(1);
}

//*******************************
// GuiOptions::settleOnOption / doKeyDown / doKeyUp
//*******************************
// the cursor never rests on a heading: after a move it goes on in the same direction, round the ends
void GuiOptions::settleOnOption(int direction) {
    const int count = getVerticalSize();
    for (int i = 0; i < count && lines[selected].id == CFG_HEADING; i++)
        selected = (selected + direction + count) % count;
    if (selected < firstVisibleIndex || selected > lastVisibleIndex)
        computePagePosition();
}

void GuiOptions::doKeyDown() {
    GuiOptionsMenuBase::doKeyDown();
    settleOnOption(1);
}

void GuiOptions::doKeyUp() {
    GuiOptionsMenuBase::doKeyUp();
    settleOnOption(-1);
}

//*******************************
// GuiOptions::valueText
//*******************************
std::string GuiOptions::valueText(const OptionsInfo &info, const std::string &value) {
    if (info.id == CFG_SHOWINGTIMEOUT)
        return Strings::toInt(value, 0) <= 0 ? _("Off") : value + "s";
    if (info.id == CFG_FONT) {
        // the font really drawing the classic screens (BUG-45): the theme's, the shipped Red Hat Text, a Chinese
        // UI's Noto, or the user's own - not the stored choice, which "Use default font" leaves unused. While a
        // change of it is waiting to be applied the row shows the choice
        const string &file = gui->assets().classicFontFile();
        if (pendingReload || file.empty())
            return value;
        const size_t cut = file.find_last_of("/\\");
        return cut == string::npos ? file : file.substr(cut + 1);
    }
    if (info.id == CFG_DISPLAY) {
        const OutputMode mode = OutputMode::parse(value);
        if (!mode.isAuto())
            return mode.label();
        return autoLabel;
    }
    if (info.id == CFG_VSIZE) // signed, in output pixels
        return (OutputMode::vsize(value) > 0 ? "+" : "") + to_string(OutputMode::vsize(value)) + " px";
    if (info.id == CFG_CRT_MARGIN)
        return value + "%";
    if (info.id == CFG_SCALER) { // the emulator menu's names, shortened: "integer scaled 2x" is "2x (integer)"
        if (value == "2x")
            return _("2x (integer)");
        if (value == "4:3i")
            return _("4:3 (integer)");
        if (value == "full")
            return _("Full screen");
    }
    return value;
}

//*******************************
// GuiOptions::doPrevNextOption
//*******************************
string GuiOptions::doPrevNextOption(OptionsInfo &info, bool next) {
    int id = info.id;

    // do the default action
    const string was = app.config().inifile.values[info.iniKey];
    string nextValue = GuiOptionsMenuBase::doPrevNextOption(info, next);

    // after doing the default these need special action afterwards (a repeat that found the last value changed
    // nothing: nothing to reload)
    if (!valueRepeat || nextValue != was)
        reloadFor(id, nextValue);
    return nextValue;
}

//*******************************
// GuiOptions::reloadFor
//*******************************
// what a changed row makes the screen reload: the theme (everything), the language (the fonts may change
// with it - Chinese), a font choice, the music; with the spinner over the panel while it happens
void GuiOptions::reloadFor(int id, const string &nextValue) {
    if (id == CFG_KEEPLOGS)
        Env::setKeepLogsMarker(nextValue == "true"); // what the rc scripts look at, from the next boot
    if (id == CFG_PERFOVERLAY)
        renderer.setPerfOverlay(nextValue == "true"); // at once, from the next frame
    if (id == CFG_CRT_MARGIN)
        renderer.setSafeMargin(OutputMode::crtMargin(nextValue)); // at once, from the next frame
    if (id == CFG_VSIZE)
        renderer.setVerticalAdjust(OutputMode::vsize(nextValue)); // at once, from the next frame
    const bool theme = id == CFG_THEME || id == CFG_MUSIC || id == CFG_ENABLE_BACKGROUND_MUSIC;
    const bool fonts = id == CFG_LANG || id == CFG_THEME_FONT || (id == CFG_FONT && userFontInUse());
    if (!theme && !fonts)
        return;
    if (pendingReload && pendingReloadId != id)
        flushPendingReload(); // another row's change still waiting: load it first
    if (valueHold.held()) {   // Left/Right still down: the row only shows the values; the last one loads once the
                              // row has rested after the release (holdTick)
        pendingReload = true;
        pendingReloadAt = 0;
        pendingReloadId = id;
        pendingReloadValue = nextValue;
        return;
    }
    pendingReload = false; // this row's own waiting value is superseded (Start's random pick)
    pendingReloadAt = 0;
    loadFor(id, nextValue);
}

//*******************************
// GuiOptions::loadFor
//*******************************
void GuiOptions::loadFor(int id, const string &nextValue) {
    const bool theme = id == CFG_THEME || id == CFG_MUSIC || id == CFG_ENABLE_BACKGROUND_MUSIC;
    // the launcher's snapshot under this screen is of the OLD theme: dropped once, so the panel now draws over the
    // new theme's own background (renderBackground() falls back to it) - no readback, no per-frame cost
    if (id == CFG_THEME)
        gui->clearLauncherBackdrop();
    gui->beginBusy(_("Loading..."), [this]() { render(); });
    if (id == CFG_LANG)
        app.lang().load(Env::getPathToLangDir(), nextValue);
    // the music with a theme change, and with a language change: the theme may have a track for the language
    gui->loadAssets(theme || id == CFG_LANG);
    // the new font (and the new theme's panel) decide how many rows fit: take the font and re-count the
    // rows as init() did, then page to the cursor again - rendering with the old count overran the panel
    GuiOptionsMenuBase::init();
    computePagePosition();
    gui->endBusy();
    // the language and the fonts change what the launcher under the panel shows: the snapshot is taken again, once,
    // into a render target (no readback); after endBusy(), as the launcher's frame ends the busy state itself
    if (id != CFG_THEME && backdropRefresh && gui->hasLauncherBackdrop())
        backdropRefresh();
}

//*******************************
// string GuiOptions::doRandomOption()
// only a few lines will use this.  most will just return.
//*******************************
string GuiOptions::doRandomOption() {
    int id = lines[selected].id;
    if (id == CFG_THEME || id == CFG_MUSIC || id == CFG_FONT) {
        auto &choices = lines[selected].choices;
        unsigned int size = choices.size();
        if (size > 1)
            return doOptionIndex(System::getRandomIndex(size));
    }
    return "";
}

//*******************************
// string GuiOptions::doOptionIndex()
//*******************************
string GuiOptions::doOptionIndex(unsigned int index) {
    if (validSelectedIndex()) {
        int id = lines[selected].id;
        // do the default action
        string nextValue = GuiOptionsMenuBase::doOptionIndex(index);

        // after doing the default these need special action afterwards
        reloadFor(id, nextValue);
        return nextValue;
    } else
        return "";
}

//*******************************
// GuiOptions::doCircle_Pressed
//*******************************
// Circle leaves with the settings as they are on screen, as every other screen does - there is no
// "back without saving" here any more (it used to be Cross = save, Circle = discard)
void GuiOptions::doCircle_Pressed() {
    app.audio().cancel.play();
    endHold();
    flushPendingReload(); // a change still waiting to load is loaded before leaving
    // a new display mode is only tried here (the launcher leaves for it) and kept once confirmed
    // config.ini is written with the old one; in memory the row keeps showing the new one while the settings
    // reload under it (it blinked back to the old value) - the launcher puts the old one back afterwards
    string &mode = app.config().inifile.values[OutputMode::ConfigKey];
    if (mode != outputModeOnEntry) {
        newOutputMode = mode;
        mode = outputModeOnEntry;
        app.config().save();
        mode = newOutputMode;
    } else {
        app.config().save();
    }
    menuVisible = false;
    exitCode = 0;
}

//*******************************
// GuiOptions::doCross_Pressed
//*******************************
// the rows are changed with Left/Right (and Start for a random theme or track); Cross does nothing, as in
// the game editor
void GuiOptions::doCross_Pressed() {}

//*******************************
// GuiOptions::doJoyRight
//*******************************
// Left/Right on a row, the same on every row: one step at the press; held past valueHoldTiming()'s delay it goes
// on, faster the longer it is held - a frame at a time from render() (holdTick), so the screen keeps drawing. A row
// that reloads (the theme, the music, the language, a font) only shows the values while held; the one it
// stops on is loaded once, reloadSettleTime() after the release - quick taps in a row count as one change,
// and so does a tap after a hold. The repeat used to be a loop
// (fastForwardUntilAnotherEvent) with no delay before the first repeat - a slow tap took two steps - and such
// rows could not repeat at all: a load per step, and a release lost during one kept it stepping
void GuiOptions::doJoyRight() {
    startHold(1);
}

//*******************************
// GuiOptions::doJoyLeft
//*******************************
void GuiOptions::doJoyLeft() {
    startHold(-1);
}

//*******************************
// GuiOptions::startHold / holdTick / doJoyCenter / endHold
//*******************************
void GuiOptions::startHold(int step) {
    if (valueHold.held() && valueHold.step() == step)
        return; // the same direction still down (another direction's event came and went)
    endHold();
    valueHold.press(step, gui->platform().ticks(), valueHoldTiming());
    step > 0 ? doKeyRight() : doKeyLeft();
    render();
}

void GuiOptions::holdTick() {
    if (!valueHold.held()) {
        // a reload waits for the row to rest; a new press before it is due puts it off again (reloadFor)
        if (pendingReload && pendingReloadAt != 0 &&
            static_cast<int32_t>(gui->platform().ticks() - pendingReloadAt) >= 0)
            flushPendingReload();
        return;
    }
    if (holdTicking)
        return;
    ableem::Input &input = gui->input();
    const bool stillDown = valueHold.step() > 0 ? input.dpadRight() : input.dpadLeft();
    if (!stillDown || input.dpadUp() || input.dpadDown()) {
        endHold(); // the release (or a move to another row) came while something else ran
        return;
    }
    holdTicking = true;
    valueRepeat = true; // a repeat stops at the last value, the press that started the hold wrapped
    for (int steps = valueHold.due(gui->platform().ticks()); steps != 0; steps -= valueHold.step())
        valueHold.step() > 0 ? doKeyRight() : doKeyLeft();
    valueRepeat = false;
    holdTicking = false;
}

void GuiOptions::doJoyCenter() {
    endHold();
}

void GuiOptions::endHold() {
    if (!valueHold.held())
        return;
    valueHold.release();
    if (pendingReload)
        pendingReloadAt = gui->platform().ticks() + reloadSettleTime(); // loaded by holdTick once due
}

void GuiOptions::flushPendingReload() {
    if (!pendingReload)
        return;
    pendingReload = false;
    pendingReloadAt = 0;
    loadFor(pendingReloadId, pendingReloadValue);
}

//*******************************
// GuiOptions::userFontInUse
//*******************************
// the Font row only matters with "Use Default Font" off: with it on, a change there is kept in config.ini for
// later and nothing is reloaded
bool GuiOptions::userFontInUse() {
    return app.config().inifile.values["themefont"] != "true";
}

//*******************************
// GuiOptions::doKeyRight
//*******************************
void GuiOptions::doKeyRight() {
    stepValue(true);
}

//*******************************
// GuiOptions::doKeyLeft
//*******************************
void GuiOptions::doKeyLeft() {
    stepValue(false);
}

// the click, then the row's next value - but a repeat that finds the last value stays put, silently
void GuiOptions::stepValue(bool next) {
    if (!validSelectedIndex())
        return;
    const string was = app.config().inifile.values[lines[selected].iniKey];
    doPrevNextOption(next);
    if (!valueRepeat || app.config().inifile.values[lines[selected].iniKey] != was)
        app.audio().cursor.play();
}
