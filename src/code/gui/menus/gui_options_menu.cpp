#include "gui_options_menu.h"
#include <algorithm>
#include "core/services/system.h"
#include "core/services/environment.h"
#include "core/services/theme_converter.h"
#include "core/services/theme_installer.h"
#include "core/services/output_mode.h"

using namespace std;

string GuiOptions::getStatusLine() {
    auto id = lines[selected].id;
    if (id == CFG_THEME || id == CFG_MUSIC)
        return "|@O| " + _("Back") + "  " + "|@Start|   " + _("Random") + "|";
    else
        return "|@O| " + _("Back") + "|";
}

//*******************************
// GuiOptions::getThemes
//*******************************
vector<string> GuiOptions::getThemes() {
    vector<string> list;
    string uiThemePath = Env::getPathToThemesDir();
    ThemeInstaller::installZips(uiThemePath); // a dropped <name>.zip is listed as <name>
    DirEntries uiThemeFolders = DirEntry::diru_DirsOnly(uiThemePath);
    for (const DirEntry &entry : uiThemeFolders) {
        // a theme.json, or an old-layout folder that Theme::load() will convert when it is picked
        if (ThemeConverter::isThemeFolder(uiThemePath + sep + entry.name)) {
            list.push_back(entry.name); // add the theme dir name
        }
    }

    return list;
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
// The console: 720p or 1080p - Weston's mode, set by rc/boot.sh (its HDMI driver reads no EDID, so there is
// nothing to list). Elsewhere: the display's own mode (auto) and every mode its EDID lists at 50 Hz or more.
vector<string> GuiOptions::getOutputModes() {
#ifdef AB_PLATFORM_PSC
    vector<string> list{"720", "1080"};
#else
    vector<string> list{"auto"};
    for (const ableem::DisplayMode &m : ableem::Platform::displayModes()) {
        OutputMode mode;
        mode.w = m.w;
        mode.h = m.h;
        list.push_back(mode.token());
    }
#endif
    // "Auto (1080p)": the display's own mode, asked once here - never per drawn frame
    const ableem::Size desktop = ableem::Platform::desktopDisplaySize();
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
    heading(_("Interface"));
    lines.emplace_back(CFG_THEME, _("AutoBleem Theme:"), "theme", false, getThemes());
    lines.emplace_back(CFG_JEWEL, _("Cover Style:"), "jewel", false, getJewels());
    lines.emplace_back(CFG_LANG, _("Language:"), "language", false, Lang::listLanguages(Env::getPathToLangDir()));
    lines.emplace_back(CFG_THEME_FONT, _("Use Font from Theme:"), "themefont", true, vector<string>({"false", "true"}));
    lines.emplace_back(CFG_FONT, _("Font:"), "font", false, getFonts());
    lines.emplace_back(CFG_SHOWINGTIMEOUT, _("Showing Timeout (0 for no timeout):"), "showingtimeout", false,
                       getTimeoutValues());
    // the display mode (OutputMode) - the launcher's and the PS1 emulator's; only where the launcher is full
    // screen (a dev host's window has no mode to change)
    if (Gui::fullscreen())
        lines.emplace_back(CFG_DISPLAY, _("Display:"), OutputMode::ConfigKey, false, getOutputModes());

    heading(_("Sound"));
    lines.emplace_back(CFG_MUSIC, _("Music:"), "music", false, getMusic());
    lines.emplace_back(CFG_ENABLE_BACKGROUND_MUSIC, _("Background Music:"), "nomusic", true,
                       vector<string>({"true", "false"}));

    heading(_("Emulation"));
    // the PS1 emulator a game starts in: the one AutoBleem has always shipped, or the next one (see Config)
    lines.emplace_back(CFG_EMULATOR, _("PS1 Emulator:"), "emulator", false, vector<string>({"pcsx-abnxt", "pcsx-ab"}));
    lines.emplace_back(CFG_WIDESCREEN, _("Widescreen:"), "aspect", true, vector<string>({"false", "true"}));
    lines.emplace_back(CFG_PLAY_ALL_PSX_WITH_RA, _("Play all PSX games with RA:"), "play_all_psx_with_ra", true,
                       vector<string>({"false", "true"}));
    lines.emplace_back(CFG_RACONFIG, _("Update RA Config:"), "raconfig", true, vector<string>({"false", "true"}));
    // RetroArch's config_save_on_exit (see Config): whether a change made in RetroArch is kept
    lines.emplace_back(CFG_RA_PERSIST, _("Persist RetroArch config:"), "rapersist", true,
                       vector<string>({"false", "true"}));
    // C11: a positional swap of the first two SDL pads' PS1 ports (core/model/pad_assignment.h,
    // LaunchService's AB_PAD_ORDER) - PS1 only, RetroArch is unaffected, hence the row saying so
    lines.emplace_back(CFG_PAD_SWAP, _("Swap Player 1 / Player 2 (PS1 emulators):"), "padswap", true,
                       vector<string>({"false", "true"}));

    heading(_("Library"));
#ifdef AB_HAS_INTERNAL_GAMES
    // an appliance or a Windows PC has no built-in games to show (GameQueryService::showInternalGames is hard
    // false there)
    lines.emplace_back(CFG_SHOW_ORIGAMES, _("Show Internal Games:"), "origames", true,
                       vector<string>({"false", "true"}));
#endif
    // only where the platform can fetch at all (download_command in its ini) - the console cannot
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
    vector<string> list{"--"};
    for (const string &dir : Fonts::userFontDirs(app.theme().path())) {
        for (const DirEntry &entry : DirEntry::diru_FilesOnly(dir)) {
            string ext = ableem::toLowerCopy(DirEntry::getFileExtension(entry.name));
            if ((ext == "ttf" || ext == "otf") && find(list.begin(), list.end(), entry.name) == list.end())
                list.push_back(entry.name);
        }
    }
    return list;
}

//*******************************
// GuiOptions::render
//*******************************
void GuiOptions::render() {
    renderer.clear();
    gui->renderBackground();
    gui->renderTextBar();
    yoffset = gui->renderHeader(getTitle());

    // the rows pack under the header at the font's height, as many as the panel holds (init()), and
    // scroll a row at a time through the base's paging (computePagePosition/adjustPageBy)
    const int fontHeight = font.lineHeight();
    const int firstLineY = yoffset + fontHeight * firstRow;
    if (firstRender) {
        computePagePosition();
        firstRender = false;
    }
    const int count = getVerticalSize();
    for (int i = firstVisibleIndex, row = 0; i <= lastVisibleIndex && i < count; i++, row++) {
        if (i < 0)
            continue;
        const int y = firstLineY + fontHeight * row;
        if (lines[i].id == CFG_HEADING) {
            gui->text().renderLabelBox(0, y);
            gui->text().renderTextLine(app.lang().translate(lines[i].descriptionToTranslate), -y, 0, XALIGN_LEFT, 0,
                                       font);
            continue;
        }
        if (i == selected)
            gui->text().renderSelectionBox(0, y, selectionBoxXOffset, font);
        renderOptionRow(lines[i], y);
    }
    gui->renderScrollMarkers(firstVisibleIndex > 0, lastVisibleIndex < count - 1);

    gui->renderStatus(getStatusLine());
    renderer.present();
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
    if (mode != "1080")
        mode = "720"; // what rc/boot.sh runs Weston in for anything but 1080
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
    if (info.id == CFG_FONT && (value.empty() || value == "--"))
        return _("Theme Default");
    if (info.id == CFG_DISPLAY) {
        const OutputMode mode = OutputMode::parse(value);
        if (!mode.isAuto())
            return mode.label();
        return autoLabel;
    }
    return value;
}

//*******************************
// GuiOptions::doPrevNextOption
//*******************************
string GuiOptions::doPrevNextOption(OptionsInfo &info, bool next) {
    int id = info.id;

    // do the default action
    string nextValue = GuiOptionsMenuBase::doPrevNextOption(info, next);

    // after doing the default these need special action afterwards
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
    const bool theme = id == CFG_THEME || id == CFG_MUSIC || id == CFG_ENABLE_BACKGROUND_MUSIC;
    const bool fonts = id == CFG_LANG || id == CFG_THEME_FONT || id == CFG_FONT;
    if (!theme && !fonts)
        return;
    gui->beginBusy(_("Loading..."), [this]() { render(); });
    if (id == CFG_LANG)
        app.lang().load(Env::getPathToLangDir(), nextValue);
    gui->loadAssets(theme); // the music only with a theme change
    // the new font (and the new theme's panel) decide how many rows fit: take the font and re-count the
    // rows as init() did, then page to the cursor again - rendering with the old count overran the panel
    GuiOptionsMenuBase::init();
    computePagePosition();
    gui->endBusy();
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
void GuiOptions::doJoyRight() {
    if (stepsOnePerPress()) {
        doKeyRight();
        render();
        return;
    }
    do {
        doKeyRight();
        render();
    } while (fastForwardUntilAnotherEvent());
}

//*******************************
// GuiOptions::stepsOnePerPress
//*******************************
// A row whose every step reloads (the theme, the music, the language, a font) or is tried on the screen (the
// display mode) moves one value a press, never on a held button: the repeat loop runs until the next pad
// event, and a release lost during a reload kept it stepping through every value - and on the Pi 400 left
// the launcher spinning with nothing on the screen
bool GuiOptions::stepsOnePerPress() {
    if (selected < 0 || selected >= static_cast<int>(lines.size()))
        return false;
    const int id = lines[selected].id;
    return id == CFG_THEME || id == CFG_MUSIC || id == CFG_ENABLE_BACKGROUND_MUSIC || id == CFG_LANG ||
           id == CFG_THEME_FONT || id == CFG_FONT || id == CFG_DISPLAY;
}

//*******************************
// GuiOptions::doJoyLeft
//*******************************
void GuiOptions::doJoyLeft() {
    if (stepsOnePerPress()) {
        doKeyLeft();
        render();
        return;
    }
    do {
        doKeyLeft();
        render();
    } while (fastForwardUntilAnotherEvent());
}

//*******************************
// GuiOptions::doKeyRight
//*******************************
void GuiOptions::doKeyRight() {
    app.audio().cursor.play();
    doPrevNextOption(true);
}

//*******************************
// GuiOptions::doKeyLeft
//*******************************
void GuiOptions::doKeyLeft() {
    app.audio().cursor.play();
    doPrevNextOption(false);
}
