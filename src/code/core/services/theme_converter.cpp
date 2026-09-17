//
// ThemeConverter: theme.ini + the PSC data tree -> theme.json + role-named files.
//

#include "theme_converter.h"

#include <cstdlib>
#include <iostream>
#include <set>

using namespace std;

namespace {

const char *THEME_JSON = "theme.json";
const char *THEME_INI = "theme.ini";
const char *COLORS_INI = "colors.ini";

// the sub-directories the cleanup is confined to
const char *CLEANED_DIRS[] = { "images", "sounds", "font" };

//*******************************
// ini helpers
//*******************************
bool has(const IniFile &ini, const string &key) {
    return ini.values.find(key) != ini.values.end();
}

bool hasAll(const IniFile &ini, const vector<string> &keys) {
    for (const string &key : keys)
        if (!has(ini, key)) return false;
    return true;
}

// atoi's leniency is what the old code had: "95;" is 95
int intOf(const IniFile &ini, const string &key) {
    return atoi(ini.values.at(key).c_str());
}

string strOf(const IniFile &ini, const string &key) {
    return Strings::trim(ini.values.at(key));
}

void colorOf(const IniFile &ini, const string &key, ThemeColor &out) {
    if (!has(ini, key)) return;
    if (!ThemeColor::parseRgb(ini.values.at(key), out))
        cout << "theme.ini: " << key << "=" << ini.values.at(key) << " is not r,g,b - ignored" << endl;
}

// an ini rect: the four keys together or not at all
bool rectOf(const IniFile &ini, const string &x, const string &y, const string &w, const string &h,
            int &ox, int &oy, int &ow, int &oh) {
    if (!hasAll(ini, {x, y, w, h})) {
        if (has(ini, x) || has(ini, y) || has(ini, w) || has(ini, h))
            cout << "theme.ini: " << x << "/" << y << "/" << w << "/" << h << " incomplete - ignored" << endl;
        return false;
    }
    ox = intOf(ini, x); oy = intOf(ini, y); ow = intOf(ini, w); oh = intOf(ini, h);
    return true;
}

//*******************************
// file helpers
//*******************************
// every file under `dir`, as paths relative to `dir`, depth first
void listFiles(const string &dir, const string &prefix, vector<string> &out) {
    for (const DirEntry &entry : DirEntry::diru(dir)) {
        string rel = prefix.empty() ? entry.name : prefix + sep + entry.name;
        if (entry.isDir)
            listFiles(dir + sep + entry.name, rel, out);
        else
            out.push_back(rel);
    }
}

// removes the directories under `dir` that are left empty, deepest first, and `dir` itself if it ends up empty
void removeEmptyDirs(const string &dir) {
    for (const DirEntry &entry : DirEntry::diru_DirsOnly(dir))
        removeEmptyDirs(dir + sep + entry.name);
    if (DirEntry::diru(dir).empty())
        DirEntry::removeDirAndContents(dir);
}

} // namespace

//*******************************
// ThemeConverter::launcherRoles
//*******************************
// The PSC names GuiLauncher, PsMenu and GuiMcManager used to load, in the order they preferred them: an
// "_AB" variant, when a theme had one, won over the stock file.
const vector<ThemeConverter::Role> &ThemeConverter::launcherRoles() {
    static const vector<Role> roles = {
        { {"GR/AB_BG.png", "GR/JP_US_BG.png"},          "launcher_background.png", [](ThemeSpec &s) { return &s.launcher.background; } },
        { {"GR/Footer_AB.png", "GR/Footer.png"},        "launcher_footer.png",     [](ThemeSpec &s) { return &s.launcher.footer; } },
        { {"GR/Acid_C_Btn.png"},                        "play_button.png",         [](ThemeSpec &s) { return &s.launcher.playButton; } },
        { {"BMP_Text/Play_Text.png"},                   "play_text.png",           [](ThemeSpec &s) { return &s.launcher.playText; } },
        { {"CB/Function_AB.png", "CB/Function_BG.png"}, "settings_panel.png",      [](ThemeSpec &s) { return &s.launcher.settingsPanel; } },
        { {"CB/PlayerOne.png"},                         "meta_panel.png",          [](ThemeSpec &s) { return &s.launcher.metaPanel; } },
        { {"GR/arrow.png"},                             "arrow.png",               [](ThemeSpec &s) { return &s.launcher.arrow; } },
        { {"GR/X_Btn_ICN.png"},                         "hint_cross.png",          [](ThemeSpec &s) { return &s.launcher.hints.cross; } },
        { {"GR/Circle_Btn_ICN.png"},                    "hint_circle.png",         [](ThemeSpec &s) { return &s.launcher.hints.circle; } },
        { {"GR/Tri_Btn_ICN.png"},                       "hint_triangle.png",       [](ThemeSpec &s) { return &s.launcher.hints.triangle; } },
        { {"CB/Setting_ICN.png"},                       "menu_settings.png",       [](ThemeSpec &s) { return &s.launcher.menuIcons.settings; } },
        { {"CB/Manual_ICN.png"},                        "menu_guide.png",          [](ThemeSpec &s) { return &s.launcher.menuIcons.guide; } },
        { {"CB/MemoryCard_ICN.png"},                    "menu_memcard.png",        [](ThemeSpec &s) { return &s.launcher.menuIcons.memcard; } },
        { {"CB/Resume.png"},                            "menu_resume.png",         [](ThemeSpec &s) { return &s.launcher.menuIcons.resume; } },
        { {"MC/Dot_Matrix.png"},                        "memcard_grid.png",        [](ThemeSpec &s) { return &s.launcher.memcardManager.grid; } },
        { {"MC/Pencil_Carsor.png"},                     "memcard_pencil.png",      [](ThemeSpec &s) { return &s.launcher.memcardManager.pencil; } },
    };
    return roles;
}

//*******************************
// ThemeConverter::needsConversion
//*******************************
bool ThemeConverter::needsConversion(const string &themeDir) {
    if (DirEntry::exists(themeDir + sep + THEME_JSON)) return false;
    if (DirEntry::exists(themeDir + sep + THEME_INI)) return true;
    const string images = themeDir + sep + "images" + sep;
    for (const string &name : launcherRoles().front().oldNames)
        if (DirEntry::exists(images + name)) return true;
    return false;
}

//*******************************
// ThemeConverter::specFromIni
//*******************************
ThemeSpec ThemeConverter::specFromIni(const IniFile &ini) {
    ThemeSpec spec;
    ClassicTheme &c = spec.classic;

    if (has(ini, "loop") && strOf(ini, "loop") == "-1") {
        spec.music.set = true;
        spec.music.none = true;
    } else if (has(ini, "music")) {
        spec.music.set = true;
        spec.music.file = strOf(ini, "music");
        spec.music.loop = !has(ini, "loop") || strOf(ini, "loop") == "1";
    }

    if (has(ini, "background")) c.background = strOf(ini, "background");
    if (has(ini, "logo")) c.logo.file = strOf(ini, "logo");
    c.logo.set = rectOf(ini, "lpositionx", "lpositiony", "lw", "lh", c.logo.x, c.logo.y, c.logo.w, c.logo.h);
    if (has(ini, "font")) c.font.file = strOf(ini, "font");
    if (has(ini, "fsize")) c.font.size = intOf(ini, "fsize");
    if (has(ini, "lines")) c.menuLines = intOf(ini, "lines");

    c.menuPanel.set = rectOf(ini, "opscreenx", "opscreeny", "opscreenw", "opscreenh",
                             c.menuPanel.x, c.menuPanel.y, c.menuPanel.w, c.menuPanel.h);
    colorOf(ini, "main_bg", c.menuPanel.color);
    if (has(ini, "mainalpha")) c.menuPanel.alpha = intOf(ini, "mainalpha");

    c.statusBar.set = rectOf(ini, "textx", "texty", "textw", "texth",
                             c.statusBar.x, c.statusBar.y, c.statusBar.w, c.statusBar.h);
    colorOf(ini, "text_bg", c.statusBar.color);
    if (has(ini, "textalpha")) c.statusBar.alpha = intOf(ini, "textalpha");
    if (has(ini, "ttop")) c.statusBar.textY = intOf(ini, "ttop");

    colorOf(ini, "text_fg", c.textColor);
    colorOf(ini, "key_bg", c.keyboardKey.color);
    if (has(ini, "keyalpha")) c.keyboardKey.alpha = intOf(ini, "keyalpha");
    colorOf(ini, "label_bg", c.labelColor);

    if (hasAll(ini, {"fsposx", "fsposy"})) {
        c.freeSpaceText.set = true;
        c.freeSpaceText.x = intOf(ini, "fsposx");
        c.freeSpaceText.y = intOf(ini, "fsposy");
    }
    if (hasAll(ini, {"ecoverx", "ecovery"})) {
        c.editorCover.set = true;
        c.editorCover.x = intOf(ini, "ecoverx");
        c.editorCover.y = intOf(ini, "ecovery");
    }

    struct { const char *key; string *field; } buttons[] = {
        { "cross", &c.buttons.cross }, { "circle", &c.buttons.circle }, { "square", &c.buttons.square },
        { "triangle", &c.buttons.triangle }, { "start", &c.buttons.start }, { "select", &c.buttons.select },
        { "l1", &c.buttons.l1 }, { "r1", &c.buttons.r1 }, { "l2", &c.buttons.l2 }, { "r2", &c.buttons.r2 },
        { "check", &c.buttons.check }, { "uncheck", &c.buttons.uncheck }, { "esc", &c.buttons.esc },
        { "enter", &c.buttons.enter }, { "tab", &c.buttons.tab },
    };
    for (const auto &b : buttons)
        if (has(ini, b.key)) *b.field = strOf(ini, b.key);

    return spec;
}

//*******************************
// ThemeConverter::specFor
//*******************************
ThemeSpec ThemeConverter::specFor(const string &themeDir) {
    ThemeSpec spec;
    // a theme.json already there (a conversion that did not get to the end) is the starting point
    if (DirEntry::exists(themeDir + sep + THEME_JSON))
        spec.load(themeDir + sep + THEME_JSON);
    if (DirEntry::exists(themeDir + sep + THEME_INI)) {
        IniFile ini;
        ini.load(themeDir + sep + THEME_INI);
        spec = specFromIni(ini);
    }

    if (DirEntry::exists(themeDir + sep + COLORS_INI)) {
        IniFile colors;
        colors.load(themeDir + sep + COLORS_INI);
        colorOf(colors, "fg", spec.launcher.colors.text);
        colorOf(colors, "sec", spec.launcher.colors.secondary);
    }

    // the launcher images: whichever old name the theme has, or the new name if it is already renamed
    const string images = themeDir + sep + "images" + sep;
    bool hasLauncherImages = false;
    for (const Role &role : launcherRoles()) {
        bool found = DirEntry::exists(images + role.newName);
        for (const string &oldName : role.oldNames)
            found = found || DirEntry::exists(images + oldName);
        if (found) {
            *role.field(spec) = string("images") + sep + role.newName;
            hasLauncherImages = true;
        }
    }
    // GuiLauncher took an AB_BG.png as "the meta panel does not move"
    if (hasLauncherImages)
        spec.launcher.metaPanelSlides = !DirEntry::exists(images + "GR/AB_BG.png");

    struct { const char *rel; string *field; } named[] = {
        { "font/SST-Medium.ttf", &spec.launcher.fonts.medium },
        { "font/SST-Bold.ttf", &spec.launcher.fonts.bold },
        { "sounds/cursor.wav", &spec.sounds.cursor },
        { "sounds/cancel.wav", &spec.sounds.cancel },
        { "sounds/home_up.wav", &spec.sounds.homeUp },
        { "sounds/home_down.wav", &spec.sounds.homeDown },
        { "sounds/resume_new.wav", &spec.sounds.resume },
    };
    for (const auto &n : named)
        if (DirEntry::exists(themeDir + sep + n.rel)) *n.field = n.rel;

    return spec;
}

//*******************************
// ThemeConverter::convert
//*******************************
bool ThemeConverter::convert(const string &themeDir) {
    cout << "Converting theme folder to theme.json: " << themeDir << endl;
    ThemeSpec spec = specFor(themeDir);

    // 1. theme.json first: from here on the folder is readable by the new code whatever happens next
    if (!spec.save(themeDir + sep + THEME_JSON)) {
        cout << "Theme not converted, could not write " << THEME_JSON << endl;
        return false;
    }

    // 2. the launcher images to their role names
    const string images = themeDir + sep + "images" + sep;
    for (const Role &role : launcherRoles()) {
        if (DirEntry::exists(images + role.newName)) continue;
        for (const string &oldName : role.oldNames) {
            if (!DirEntry::exists(images + oldName)) continue;
            if (!DirEntry::renameFile(images + oldName, images + role.newName))
                cout << "Could not rename " << images + oldName << " to " << role.newName << endl;
            break;
        }
    }

    // 3. everything the json does not name goes, but only under the three data directories
    set<string> keep;
    for (const string &file : spec.referencedFiles()) keep.insert(file);
    DirEntry::removeFile(themeDir + sep + THEME_INI);
    DirEntry::removeFile(themeDir + sep + COLORS_INI);
    int removed = 0;
    for (const char *sub : CLEANED_DIRS) {
        const string dir = themeDir + sep + sub;
        if (!DirEntry::isDirectory(dir)) continue;
        vector<string> files;
        listFiles(dir, sub, files);
        for (const string &rel : files) {
            if (keep.count(rel)) continue;
            if (DirEntry::removeFile(themeDir + sep + rel)) removed++;
        }
        removeEmptyDirs(dir);
    }
    cout << "Theme converted: " << keep.size() << " files kept, " << removed << " removed" << endl;
    return true;
}
