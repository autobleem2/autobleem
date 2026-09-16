//
// Theme: the merged theme.ini and the theme's directories, with their fallbacks.
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

// A themes directory with the default theme, a partial theme on top of it, and a Sony data dir to fall back
// to. config.ini names the theme; Config reads it from the working path.
struct Themes {
    Themes() : tmp("theme") {
        env.setWorkingPath(tmp.path());
        env.setThemesDir(tmp.makeSubDir("themes"));
        env.setSonyDataPath(tmp.makeSubDir("sony"));
        tmp.makeSubDir("sony/font");
        tmp.makeSubDir("sony/images");

        tmp.writeFile("themes/default/theme.ini", "[Theme]\nBackground=bg.png\nLogo=logo.png\nFont=font.ttf\nFsize=20\nText_fg=255,255,255\n");
        tmp.makeSubDir("themes/default/images");
        tmp.makeSubDir("themes/default/font");
        tmp.makeSubDir("themes/default/sounds");

        // aergb overrides two keys and has its own images, but no font or sounds of its own
        tmp.writeFile("themes/aergb/theme.ini", "[Theme]\nBackground=aergb.png\nFsize=24\n");
        tmp.makeSubDir("themes/aergb/images");

        tmp.makeSubDir("themes/bare");   // a theme directory with no theme.ini at all
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

    CHECK(t.theme->value("background") == "aergb.png");   // the theme's own
    CHECK(t.theme->intValue("fsize") == 24);
    CHECK(t.theme->value("logo") == "logo.png");          // from default
    CHECK(t.theme->value("text_fg") == "255,255,255");
    CHECK(t.theme->defaults.values["background"] == "bg.png");   // the defaults are kept apart, for per-file fallback
    CHECK(t.theme->loadedPath() == t.tmp.at("themes/aergb") + "/");
    CHECK(t.theme->defaultsPath() == t.tmp.at("themes/default") + "/");
}

TEST_CASE("a theme sub-directory the theme lacks comes from the Sony data dir") {
    Themes t;
    t.configure("aergb");

    CHECK(t.theme->imagePath() == t.tmp.at("themes/aergb/images"));   // has its own
    CHECK(t.theme->fontPath() == t.tmp.at("sony/font"));             // has none: Sony's
    CHECK(t.theme->soundPath() == t.tmp.at("sony/sounds"));          // Sony has none either; the path is still Sony's
}

TEST_CASE("a theme that does not exist at all is the Sony data dir") {
    Themes t;
    t.configure("nosuchtheme");

    CHECK(t.theme->path() == t.tmp.at("sony"));
    CHECK(t.theme->imagePath() == t.tmp.at("sony/images"));
}

TEST_CASE("a theme directory with no theme.ini falls back to default and says so in config.ini") {
    Themes t;
    t.configure("bare");

    t.theme->load();

    CHECK(t.theme->loadedPath() == t.tmp.at("themes/default") + "/");
    CHECK(t.theme->value("background") == "bg.png");
    CHECK(t.config->inifile.values["theme"] == "default");
    CHECK(t.tmp.readFile("config.ini").find("Theme=default") != string::npos);   // saved, not just in memory
}

TEST_CASE("load() re-reads: a theme change in config.ini takes effect on the next load") {
    Themes t;
    t.configure("aergb");
    t.theme->load();
    REQUIRE(t.theme->value("background") == "aergb.png");

    t.config->inifile.values["theme"] = "default";
    t.theme->load();
    CHECK(t.theme->value("background") == "bg.png");
    CHECK(t.theme->intValue("fsize") == 20);
}
