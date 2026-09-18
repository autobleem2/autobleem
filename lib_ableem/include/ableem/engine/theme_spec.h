// lib_ableem - engine: a UI theme's description, theme.json. What a theme consists of - its music, the
// classic UI's colours, rects and button markers, the launcher's images and fonts, the UI sounds - as a
// typed struct with JSON load/save. The library knows nothing about which UI reads which field; it only
// keeps the file honest (a bad file is reported, never thrown) and merges a partial theme over a base one.
#pragma once

#include <string>
#include <vector>

namespace ableem {

//******************
// Opt
//******************
// A scalar a theme may leave out. Reads convert to T; an assignment marks it set. mergeOver() takes the
// base theme's value for anything not set.
template <class T> struct Opt {
    T value{};
    bool set = false;

    Opt() = default;
    Opt(const T &v) : value(v), set(true) {} // NOLINT: implicit on purpose, `spec.menuLines = 13`
    Opt &operator=(const T &v) {
        value = v;
        set = true;
        return *this;
    }
    operator const T &() const { return value; } // NOLINT: implicit on purpose, `int n = spec.menuLines`
};

//******************
// ThemeColor
//******************
// "#rrggbb" in theme.json. fromRgb() reads the old theme.ini "r,g,b" form.
struct ThemeColor {
    int r = 0, g = 0, b = 0;
    bool set = false;

    ThemeColor() = default;
    ThemeColor(int r_, int g_, int b_) : r(r_), g(g_), b(b_), set(true) {}

    static bool parseHex(const std::string &hex, ThemeColor &out); // "#rrggbb", case-insensitive
    static bool parseRgb(const std::string &rgb, ThemeColor &out); // "255,255,255"
    std::string toHex() const;
};

//******************
// ThemePoint / ThemeRect
//******************
struct ThemePoint {
    int x = 0, y = 0;
    bool set = false;
};

struct ThemeRect {
    int x = 0, y = 0, w = 0, h = 0;
    bool set = false;
};

//******************
// ThemeMusic
//******************
// "music": null is a theme that plays no music at all (theme.ini's Loop=-1): set with none.
struct ThemeMusic {
    std::string file;
    bool loop = true;
    bool none = false;
    bool set = false;
};

//******************
// ThemeLogo / ThemeFont / ThemeFill / ThemePanel
//******************
// Each part of these is optional on its own (the file, the rect, the colour, the alpha), so a theme that
// gives only a rect still inherits the default's fill. `set` is the rect's flag.
struct ThemeLogo { // the classic logo and where it is drawn
    std::string file;
    int x = 0, y = 0, w = 0, h = 0;
    bool set = false;
};

struct ThemeFont { // a ttf and a point size
    std::string file;
    Opt<int> size;
};

struct ThemeFill { // a translucent fill: colour + alpha
    ThemeColor color;
    Opt<int> alpha;
};

struct ThemePanel { // a filled rect
    int x = 0, y = 0, w = 0, h = 0;
    bool set = false;
    ThemeColor color;
    Opt<int> alpha;
};

struct ThemeStatusBar : ThemePanel { // the classic status line: its bar, and the y the text is drawn at
    Opt<int> textY;
};

//******************
// ClassicTheme
//******************
// The classic UI (menus, splash, dialogs, keyboard). Every file is relative to the theme dir until
// resolveFiles() makes it absolute.
struct ClassicTheme {
    std::string background;
    ThemeLogo logo;
    ThemeFont font;
    Opt<int> menuLines;   // visible rows in the list menus
    ThemePanel menuPanel; // the translucent panel behind a menu
    ThemeStatusBar statusBar;
    ThemeColor textColor;
    Opt<bool> textShadow;     // false: no dark halo under the classic UI's text (unset counts as true)
    ThemeFill keyboardKey;    // on-screen keyboard key
    ThemeColor labelColor;    // label box fill
    ThemePoint freeSpaceText; // where "Free space: ..." is drawn
    ThemePoint editorCover;   // the game editor's cover art

    // the |@X| marker textures, by marker name
    struct Buttons {
        std::string cross, circle, square, triangle, start, select, l1, r1, l2, r2;
        std::string check, uncheck, esc, enter, tab;
    } buttons;
};

//******************
// LauncherTheme
//******************
// The EvolutionUI launcher.
struct LauncherTheme {
    std::string background;
    std::string footer;
    std::string playButton;
    std::string playText;
    std::string settingsPanel;
    std::string metaPanel;
    Opt<bool> metaPanelSlides; // false: the meta panel stays put when the menu opens (a static layout)
    Opt<bool> textShadow;      // false: no dark halo under the launcher's text (unset counts as true)
    ThemeRect snapPanel;       // where the selected game's screenshot is drawn (aspect-fit); unset: not drawn
    std::string arrow;

    struct Hints {
        std::string cross, circle, triangle;
    } hints; // the button hints in the footer
    struct MenuIcons {
        std::string settings, guide, memcard, resume;
    } menuIcons; // the launcher's menu row
    struct MemcardManager {
        std::string grid, pencil;
    } memcardManager;
    struct Fonts {
        std::string medium, bold;
    } fonts;
    // hint: the footer's "Enter" / "Cancel" / "Button Guide" labels next to the button icons; unset means
    // they take the secondary colour (light hint text gets the dark halo like every other light text)
    struct Colors {
        ThemeColor text, secondary, hint;
    } colors;
};

//******************
// ThemeSounds
//******************
struct ThemeSounds {
    std::string cursor, cancel, homeUp, homeDown, resume;
};

//******************
// ThemeSpec
//******************
struct ThemeSpec {
    enum { currentFormat = 1 };

    int format = currentFormat;
    ThemeMusic music;
    ClassicTheme classic;
    LauncherTheme launcher;
    ThemeSounds sounds;

    // reads theme.json. False (and *this untouched) when the file is missing or not valid JSON; a key that
    // is missing or of the wrong type is simply not set.
    bool load(const std::string &path);
    // writes theme.json; only what is set is written, so a partial theme stays partial
    bool save(const std::string &path) const;

    // every value this theme does not set is taken from `base` - a partial theme over the default one
    void mergeOver(const ThemeSpec &base);

    // Turns every relative file name into an absolute path: `dir`'s file when it exists, otherwise
    // `fallback`'s file for the same role under `fallbackDir`, otherwise "". After this the spec is what
    // a UI loads from.
    void resolveFiles(const std::string &dir, const ThemeSpec &fallback, const std::string &fallbackDir);

    // the (relative or resolved) file names the theme sets, in field order; "" entries are skipped
    std::vector<std::string> referencedFiles() const;

    // a mutable view of every file field, for the loops above. Kept public for the same reason as
    // referencedFiles(): a converter enumerates them too.
    std::vector<std::string *> fileFields();
    std::vector<const std::string *> fileFields() const;
};

} // namespace ableem
