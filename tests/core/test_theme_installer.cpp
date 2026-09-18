//
// ZipArchive and ThemeInstaller: a <name>.zip dropped next to the theme folders becomes <name>/.
//
#include "doctest/doctest.h"

#include "../support/env_fixture.h"
#include "../support/string_maker.h"
#include "../support/temp_dir.h"
#include "../support/zip_fixtures.h"

#include "core/services/theme_installer.h"
#include "core/services/theme_converter.h"
#include "core/services/theme.h"

#include <memory>
#include <string>

using std::string;
using std::vector;

//*******************************
// ZipArchive
//*******************************
TEST_CASE("ZipArchive lists and extracts, creating the directories on the way") {
    TempDir tmp("zip");
    tmp.writeFile("nested.zip", ZIP_FIXTURE(NESTED_ZIP));

    vector<string> names;
    REQUIRE(ZipArchive::list(tmp.at("nested.zip"), names));
    CHECK(names.size() == 7);
    CHECK(names[0] == "mytheme/");
    CHECK(names[1] == "mytheme/theme.ini");
    CHECK(names[3] == "mytheme/images/GR/JP_US_BG.png");

    REQUIRE(ZipArchive::extract(tmp.at("nested.zip"), tmp.at("out")));
    CHECK(tmp.readFile("out/mytheme/theme.ini") == "[Theme]\nBackground=old.jpg\nLines=9\n");
    CHECK(tmp.readFile("out/mytheme/images/GR/JP_US_BG.png") == "stock bg");
    CHECK(DirEntry::isDirectory(tmp.at("out/__MACOSX")));

    // deflated content comes back byte for byte
    tmp.writeFile("flat.zip", ZIP_FIXTURE(FLAT_ZIP));
    REQUIRE(ZipArchive::extract(tmp.at("flat.zip"), tmp.at("flat")));
    CHECK(tmp.readFile("flat/bg.png").size() == 9 * 40);
    CHECK(tmp.readFile("flat/theme.json").find("bg.png") != string::npos);
}

TEST_CASE("ZipArchive refuses names that would land outside the destination, before writing anything") {
    TempDir tmp("zip");
    tmp.writeFile("evil.zip", ZIP_FIXTURE(EVIL_ZIP));

    CHECK_FALSE(ZipArchive::extract(tmp.at("evil.zip"), tmp.at("out")));
    CHECK_FALSE(DirEntry::exists(tmp.at("evil.txt")));
    CHECK_FALSE(DirEntry::exists(tmp.at("out/theme.json"))); // the good entry was not written either
    CHECK_FALSE(DirEntry::exists(tmp.at("out")));

    CHECK(ZipArchive::isSafeName("a/b.png"));
    CHECK(ZipArchive::isSafeName("theme.json"));
    CHECK_FALSE(ZipArchive::isSafeName("../x"));
    CHECK_FALSE(ZipArchive::isSafeName("a/../../x"));
    CHECK_FALSE(ZipArchive::isSafeName("a/./x"));
    CHECK_FALSE(ZipArchive::isSafeName("/etc/passwd"));
    CHECK_FALSE(ZipArchive::isSafeName("C:/x"));
    CHECK_FALSE(ZipArchive::isSafeName("a\\b"));
    CHECK_FALSE(ZipArchive::isSafeName(""));
}

TEST_CASE("ZipArchive: a file that is not a zip is reported, not extracted") {
    TempDir tmp("zip");
    tmp.writeFile("text.zip", "this is not an archive at all, just text with a zip name");
    vector<string> names;
    CHECK_FALSE(ZipArchive::list(tmp.at("text.zip"), names));
    CHECK(names.empty());
    CHECK_FALSE(ZipArchive::extract(tmp.at("text.zip"), tmp.at("out")));
    CHECK_FALSE(ZipArchive::extract(tmp.at("missing.zip"), tmp.at("out")));
}

//*******************************
// ThemeInstaller
//*******************************
TEST_CASE("a flat zip (theme.json at the root) installs as <name>/ and the zip goes") {
    TempDir tmp("installer");
    tmp.makeSubDir("themes");
    tmp.writeFile("themes/Neon.zip", ZIP_FIXTURE(FLAT_ZIP));

    vector<string> installed = ThemeInstaller::installZips(tmp.at("themes"));

    CHECK(installed == vector<string>{"Neon"});
    CHECK(DirEntry::exists(tmp.at("themes/Neon/theme.json")));
    CHECK(DirEntry::exists(tmp.at("themes/Neon/images/launcher_background.png")));
    CHECK_FALSE(DirEntry::exists(tmp.at("themes/Neon.zip")));
    CHECK_FALSE(DirEntry::exists(tmp.at("themes/.Neon.unzip")));
    CHECK(ThemeInstaller::installZips(tmp.at("themes")).empty()); // nothing left to do
}

TEST_CASE("a zip with one folder inside installs that folder, under the zip's name, junk folders ignored") {
    TempDir tmp("installer");
    tmp.makeSubDir("themes");
    tmp.writeFile("themes/retro.zip", ZIP_FIXTURE(NESTED_ZIP)); // holds mytheme/ and __MACOSX/

    REQUIRE(ThemeInstaller::installZip(tmp.at("themes/retro.zip"), tmp.at("themes")));

    CHECK(DirEntry::exists(tmp.at("themes/retro/theme.ini"))); // the old layout is left for the converter
    CHECK(DirEntry::exists(tmp.at("themes/retro/images/GR/JP_US_BG.png")));
    CHECK_FALSE(DirEntry::exists(tmp.at("themes/retro/mytheme")));
    CHECK_FALSE(DirEntry::exists(tmp.at("themes/mytheme")));
    CHECK_FALSE(DirEntry::exists(tmp.at("themes/__MACOSX")));
    CHECK_FALSE(DirEntry::exists(tmp.at("themes/.retro.unzip")));
    CHECK(ThemeConverter::needsConversion(tmp.at("themes/retro")));
}

TEST_CASE("a zip replaces a folder of the same name") {
    TempDir tmp("installer");
    tmp.makeSubDir("themes/Neon/images");
    tmp.writeFile("themes/Neon/theme.json", "{ \"classic\": { \"background\": \"old.png\" } }");
    tmp.writeFile("themes/Neon/old.png", "x");
    tmp.writeFile("themes/Neon.zip", ZIP_FIXTURE(FLAT_ZIP));

    REQUIRE(ThemeInstaller::installZip(tmp.at("themes/Neon.zip"), tmp.at("themes")));

    CHECK(tmp.readFile("themes/Neon/theme.json").find("bg.png") != string::npos);
    CHECK_FALSE(DirEntry::exists(tmp.at("themes/Neon/old.png")));
}

TEST_CASE("a zip that is no theme, or no zip, is renamed .bad and nothing is installed") {
    TempDir tmp("installer");
    tmp.makeSubDir("themes");
    tmp.writeFile("themes/pics.zip", ZIP_FIXTURE(NOTHEME_ZIP));
    tmp.writeFile("themes/two.zip", ZIP_FIXTURE(TWO_ZIP)); // two folders, neither is "the" theme
    tmp.writeFile("themes/text.zip", "not an archive");
    tmp.writeFile("themes/evil.zip", ZIP_FIXTURE(EVIL_ZIP));

    CHECK(ThemeInstaller::installZips(tmp.at("themes")).empty());

    for (const char *name : {"pics", "two", "text", "evil"}) {
        CHECK_FALSE(DirEntry::exists(tmp.at(string("themes/") + name)));
        CHECK_FALSE(DirEntry::exists(tmp.at(string("themes/") + name + ".zip")));
        CHECK(DirEntry::exists(tmp.at(string("themes/") + name + ".zip.bad")));
        CHECK_FALSE(DirEntry::exists(tmp.at(string("themes/.") + name + ".unzip")));
    }
    CHECK_FALSE(DirEntry::exists(tmp.at("evil.txt")));
    CHECK(ThemeInstaller::installZips(tmp.at("themes")).empty()); // .bad files are not retried
}

TEST_CASE("Theme::load() installs a dropped zip and can then load it, converting it if it is old") {
    EnvFixture env;
    TempDir tmp("installer");
    env.setWorkingPath(tmp.path());
    env.setThemesDir(tmp.makeSubDir("themes"));
    env.setSonyDataPath(tmp.makeSubDir("sony"));
    tmp.writeFile("themes/default/theme.json", "{ \"classic\": { \"background\": \"bg.png\", \"menuLines\": 12 } }");
    tmp.writeFile("themes/default/bg.png", "x");
    tmp.writeFile("themes/retro.zip", ZIP_FIXTURE(NESTED_ZIP));
    tmp.writeFile("config.ini", "Theme=retro\n");

    Config config;
    Theme theme(config);
    theme.load();

    CHECK(theme.loadedPath() == tmp.at("themes/retro"));
    CHECK(theme.classic().background == tmp.at("themes/retro/old.jpg"));
    CHECK(int(theme.classic().menuLines) == 9);
    CHECK(DirEntry::exists(tmp.at("themes/retro/theme.json")));
    CHECK(DirEntry::exists(tmp.at("themes/retro/images/launcher_background.png")));
    CHECK_FALSE(DirEntry::exists(tmp.at("themes/retro.zip")));
    CHECK(config.inifile.values["theme"] == "retro");
}
