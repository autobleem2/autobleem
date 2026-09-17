//
// Theme: theme.json merged over the default theme, every file resolved, old folders converted on load.
//
#include "doctest/doctest.h"

#include "../support/env_fixture.h"
#include "../support/string_maker.h"
#include "../support/temp_dir.h"

#include "core/services/theme.h"

#include <memory>
#include <string>

using std::string;

namespace {

// A themes directory with a complete default theme, a partial theme on top of it, an old-layout theme and
// a folder that is no theme at all. config.ini names the theme; Config reads it from the working path.
struct Themes {
    Themes() : tmp("theme") {
        env.setWorkingPath(tmp.path());
        env.setThemesDir(tmp.makeSubDir("themes"));
        env.setSonyDataPath(tmp.makeSubDir("sony"));

        tmp.makeSubDir("themes/default/images");
        tmp.makeSubDir("themes/default/font");
        tmp.writeFile("themes/default/theme.json",
                      "{ \"format\": 1,\n"
                      "  \"music\": { \"file\": \"4.wav\", \"loop\": true },\n"
                      "  \"classic\": { \"background\": \"bg.png\", \"logo\": { \"file\": \"logo.png\", \"x\": 1, \"y\": 2, \"w\": 3, \"h\": 4 },\n"
                      "                \"font\": { \"file\": \"font.ttf\", \"size\": 20 }, \"menuLines\": 12,\n"
                      "                \"menuPanel\": { \"x\": 30, \"y\": 10, \"w\": 1220, \"h\": 585, \"color\": \"#000000\", \"alpha\": 170 },\n"
                      "                \"textColor\": \"#ffffff\", \"buttons\": { \"cross\": \"cross.png\", \"l2\": \"l2.png\" } },\n"
                      "  \"launcher\": { \"background\": \"images/launcher_background.png\", \"metaPanelSlides\": true,\n"
                      "                 \"fonts\": { \"medium\": \"font/SST-Medium.ttf\", \"bold\": \"font/SST-Bold.ttf\" },\n"
                      "                 \"colors\": { \"text\": \"#ffffff\", \"secondary\": \"#646464\" } },\n"
                      "  \"sounds\": { \"cursor\": \"sounds/cursor.wav\" } }\n");
        for (const char *f : { "4.wav", "bg.png", "logo.png", "font.ttf", "cross.png", "l2.png",
                               "images/launcher_background.png", "font/SST-Medium.ttf", "font/SST-Bold.ttf" })
            tmp.writeFile(string("themes/default/") + f, "x");
        // sounds/cursor.wav is named but not there

        // aergb overrides a few keys and has some files of its own: its own background and cross, a font
        // it names but does not have, and no launcher images at all
        tmp.writeFile("themes/aergb/theme.json",
                      "{ \"classic\": { \"background\": \"aergb.png\", \"font\": { \"file\": \"missing.ttf\", \"size\": 24 },\n"
                      "                \"menuPanel\": { \"x\": 5, \"y\": 6, \"w\": 7, \"h\": 8 },\n"
                      "                \"buttons\": { \"cross\": \"x.png\" } },\n"
                      "  \"launcher\": { \"metaPanelSlides\": false } }\n");
        tmp.writeFile("themes/aergb/aergb.png", "x");
        tmp.writeFile("themes/aergb/x.png", "x");

        // an old-layout theme: theme.ini and a launcher image under its PSC name
        tmp.makeSubDir("themes/old/images/GR");
        tmp.writeFile("themes/old/theme.ini", "[Theme]\nBackground=old.jpg\nFsize=30\nLines=9\n");
        tmp.writeFile("themes/old/old.jpg", "x");
        tmp.writeFile("themes/old/images/GR/JP_US_BG.png", "x");
        tmp.writeFile("themes/old/images/GR/Squere_Btn_ICN.png", "x");

        tmp.makeSubDir("themes/bare");   // a theme directory with nothing in it
    }

    void configure(const string &themeName) {
        tmp.writeFile("config.ini", "Theme=" + themeName + "\n");
        config.reset(new Config);
        theme.reset(new Theme(*config));
    }

    EnvFixture env;
    TempDir tmp;
    std::unique_ptr<Config> config;
    std::unique_ptr<Theme> theme;
};

} // namespace

TEST_CASE("a partial theme is merged over the default theme, so every key has a value") {
    Themes t;
    t.configure("aergb");

    t.theme->load();

    const ClassicTheme &c = t.theme->classic();
    CHECK(int(c.font.size) == 24);                       // the theme's own
    CHECK(c.menuPanel.x == 5);
    CHECK(c.menuPanel.h == 8);
    CHECK_FALSE(bool(t.theme->launcher().metaPanelSlides));
    CHECK(int(c.menuLines) == 12);                       // from default
    CHECK(c.logo.w == 3);
    CHECK(c.menuPanel.color.toHex() == "#000000");       // a rect of its own, the default's fill
    CHECK(int(c.menuPanel.alpha) == 170);
    CHECK(c.textColor.toHex() == "#ffffff");
    CHECK(t.theme->music().file == t.tmp.at("themes/default/4.wav"));
    CHECK(t.theme->launcher().colors.secondary.toHex() == "#646464");
    CHECK(t.theme->loadedPath() == t.tmp.at("themes/aergb"));
}

TEST_CASE("every file is resolved: the theme's own, else the default theme's, else nothing") {
    Themes t;
    t.configure("aergb");

    t.theme->load();

    const ClassicTheme &c = t.theme->classic();
    CHECK(c.background == t.tmp.at("themes/aergb/aergb.png"));           // its own
    CHECK(c.buttons.cross == t.tmp.at("themes/aergb/x.png"));
    CHECK(c.font.file == t.tmp.at("themes/default/font.ttf"));            // names missing.ttf: the default's file
    CHECK(c.logo.file == t.tmp.at("themes/default/logo.png"));            // not named: the default's
    CHECK(c.buttons.l2 == t.tmp.at("themes/default/l2.png"));
    CHECK(t.theme->launcher().background == t.tmp.at("themes/default/images/launcher_background.png"));
    CHECK(t.theme->launcher().fonts.bold == t.tmp.at("themes/default/font/SST-Bold.ttf"));
    CHECK(t.theme->sounds().cursor.empty());                              // named by default, on disk nowhere
    CHECK(t.theme->sounds().cancel.empty());                              // named by nobody
}

TEST_CASE("a theme that does not exist at all is the default theme") {
    Themes t;
    t.configure("nosuchtheme");

    CHECK(t.theme->path() == t.tmp.at("themes/default"));
    t.theme->load();
    CHECK(t.theme->classic().background == t.tmp.at("themes/default/bg.png"));
    CHECK(t.config->inifile.values["theme"] == "nosuchtheme");   // the name is kept: the folder may turn up
}

TEST_CASE("a theme directory that is no theme falls back to default and says so in config.ini") {
    Themes t;
    t.configure("bare");

    t.theme->load();

    CHECK(t.theme->loadedPath() == t.tmp.at("themes/default"));
    CHECK(t.theme->classic().background == t.tmp.at("themes/default/bg.png"));
    CHECK(t.config->inifile.values["theme"] == "default");
    CHECK(t.tmp.readFile("config.ini").find("Theme=default") != string::npos);   // saved, not just in memory
}

TEST_CASE("an old-layout theme is converted in place the first time it is loaded") {
    Themes t;
    t.configure("old");
    REQUIRE_FALSE(DirEntry::exists(t.tmp.at("themes/old/theme.json")));

    t.theme->load();

    CHECK(DirEntry::exists(t.tmp.at("themes/old/theme.json")));
    CHECK_FALSE(DirEntry::exists(t.tmp.at("themes/old/theme.ini")));
    CHECK(DirEntry::exists(t.tmp.at("themes/old/images/launcher_background.png")));
    CHECK_FALSE(DirEntry::exists(t.tmp.at("themes/old/images/GR")));
    CHECK(t.theme->classic().background == t.tmp.at("themes/old/old.jpg"));
    CHECK(int(t.theme->classic().font.size) == 30);
    CHECK(int(t.theme->classic().menuLines) == 9);
    CHECK(t.theme->classic().font.file == t.tmp.at("themes/default/font.ttf"));
    CHECK(t.theme->launcher().background == t.tmp.at("themes/old/images/launcher_background.png"));
    CHECK(bool(t.theme->launcher().metaPanelSlides));
    CHECK(t.config->inifile.values["theme"] == "old");
}

TEST_CASE("the default theme is converted too when it is still the old layout") {
    Themes t;
    DirEntry::removeFile(t.tmp.at("themes/default/theme.json"));
    t.tmp.writeFile("themes/default/theme.ini", "[Theme]\nBackground=bg.png\nLines=7\n");
    t.configure("aergb");

    t.theme->load();

    CHECK(DirEntry::exists(t.tmp.at("themes/default/theme.json")));
    CHECK(int(t.theme->classic().menuLines) == 7);
    CHECK(t.theme->classic().background == t.tmp.at("themes/aergb/aergb.png"));
}

TEST_CASE("load() re-reads: a theme change in config.ini takes effect on the next load") {
    Themes t;
    t.configure("aergb");
    t.theme->load();
    REQUIRE(t.theme->classic().background == t.tmp.at("themes/aergb/aergb.png"));

    t.config->inifile.values["theme"] = "default";
    t.theme->load();
    CHECK(t.theme->classic().background == t.tmp.at("themes/default/bg.png"));
    CHECK(int(t.theme->classic().font.size) == 20);
    CHECK(bool(t.theme->launcher().metaPanelSlides));   // aergb's false did not linger
}
