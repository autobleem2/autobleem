//
// ThemeSpec: theme.json in and out, a partial theme merged over a full one, file resolution.
//
#include "doctest/doctest.h"

#include "../support/string_maker.h"
#include "../support/temp_dir.h"

#include <ableem/engine/theme_spec.h>

#include <string>

using ableem::ThemeColor;
using ableem::ThemeSpec;
using std::string;

namespace {

// a theme that sets everything, the way a converted full theme.ini does
ThemeSpec fullSpec() {
    ThemeSpec s;
    s.music.set = true;
    s.music.file = "mel.ogg";
    s.music.loop = true;

    s.classic.background = "background.jpg";
    s.classic.logo.set = true;
    s.classic.logo.file = "ab.png";
    s.classic.logo.x = 520;
    s.classic.logo.y = 0;
    s.classic.logo.w = 240;
    s.classic.logo.h = 180;
    s.classic.font.file = "zrnic.ttf";
    s.classic.font.size = 24;
    s.classic.menuLines = 13;
    s.classic.menuPanel.set = true;
    s.classic.menuPanel.x = 30;
    s.classic.menuPanel.y = 10;
    s.classic.menuPanel.w = 1220;
    s.classic.menuPanel.h = 530;
    s.classic.menuPanel.color = ThemeColor(0, 0, 0);
    s.classic.menuPanel.alpha = 170;
    s.classic.statusBar.set = true;
    s.classic.statusBar.x = 0;
    s.classic.statusBar.y = -670;
    s.classic.statusBar.w = 1280;
    s.classic.statusBar.h = 30;
    s.classic.statusBar.color = ThemeColor(0, 0, 0);
    s.classic.statusBar.alpha = 170;
    s.classic.statusBar.textY = 662;
    s.classic.textColor = ThemeColor(255, 255, 255);
    s.classic.textShadow = true;
    s.classic.keyboardKey.color = ThemeColor(120, 120, 120);
    s.classic.keyboardKey.alpha = 170;
    s.classic.labelColor = ThemeColor(180, 180, 180);
    s.classic.freeSpaceText.set = true;
    s.classic.freeSpaceText.x = 180;
    s.classic.freeSpaceText.y = 35;
    s.classic.editorCover.set = true;
    s.classic.editorCover.x = 95;
    s.classic.editorCover.y = 130;
    auto &b = s.classic.buttons;
    b.cross = "cross.png";
    b.circle = "circle.png";
    b.square = "square.png";
    b.triangle = "triangle.png";
    b.start = "start.png";
    b.select = "select.png";
    b.l1 = "l1.png";
    b.r1 = "r1.png";
    b.l2 = "l2.png";
    b.r2 = "r2.png";
    b.check = "on.png";
    b.uncheck = "off.png";
    b.esc = "esc.png";
    b.enter = "enter.png";
    b.tab = "tab.png";

    auto &l = s.launcher;
    l.background = "images/launcher_background.png";
    l.footer = "images/launcher_footer.png";
    l.playButton = "images/play_button.png";
    l.playText = "images/play_text.png";
    l.settingsPanel = "images/settings_panel.png";
    l.metaPanel = "images/meta_panel.png";
    l.metaPanelSlides = true;
    l.textShadow = false;
    l.arrow = "images/arrow.png";
    l.hints.cross = "images/hint_cross.png";
    l.hints.circle = "images/hint_circle.png";
    l.hints.triangle = "images/hint_triangle.png";
    l.menuIcons.settings = "images/menu_settings.png";
    l.menuIcons.guide = "images/menu_guide.png";
    l.menuIcons.memcard = "images/menu_memcard.png";
    l.menuIcons.resume = "images/menu_resume.png";
    l.memcardManager.grid = "images/memcard_grid.png";
    l.memcardManager.pencil = "images/memcard_pencil.png";
    l.fonts.medium = "font/SST-Medium.ttf";
    l.fonts.bold = "font/SST-Bold.ttf";
    l.colors.text = ThemeColor(255, 255, 255);
    l.colors.secondary = ThemeColor(100, 100, 100);
    l.colors.hint = ThemeColor(255, 255, 255);
    l.snapPanel.x = 260;
    l.snapPanel.y = 225;
    l.snapPanel.w = 240;
    l.snapPanel.h = 180;
    l.snapPanel.set = true;
    l.menuIcons.resumePicture.x = 25;
    l.menuIcons.resumePicture.y = 22;
    l.menuIcons.resumePicture.w = 68;
    l.menuIcons.resumePicture.h = 52;
    l.menuIcons.resumePicture.set = true;

    s.sounds.cursor = "sounds/cursor.wav";
    s.sounds.cancel = "sounds/cancel.wav";
    s.sounds.homeUp = "sounds/home_up.wav";
    s.sounds.homeDown = "sounds/home_down.wav";
    s.sounds.resume = "sounds/resume_new.wav";
    return s;
}

} // namespace

TEST_CASE("a colour is #rrggbb in the file and r,g,b in the old ini") {
    ThemeColor c;
    CHECK(ThemeColor::parseHex("#78A0ff", c));
    CHECK(c.r == 120);
    CHECK(c.g == 160);
    CHECK(c.b == 255);
    CHECK(c.set);
    CHECK(c.toHex() == "#78a0ff");

    CHECK(ThemeColor::parseRgb("255, 0,17", c));
    CHECK(c.toHex() == "#ff0011");

    ThemeColor bad;
    CHECK_FALSE(ThemeColor::parseHex("ffffff", bad)); // no '#'
    CHECK_FALSE(ThemeColor::parseHex("#fff", bad));   // short form is not accepted
    CHECK_FALSE(ThemeColor::parseHex("#gg0000", bad));
    CHECK_FALSE(ThemeColor::parseRgb("255,255", bad));
    CHECK_FALSE(ThemeColor::parseRgb("256,0,0", bad));
    CHECK_FALSE(ThemeColor::parseRgb("1,2,3,4", bad));
    CHECK_FALSE(bad.set);
}

TEST_CASE("a full theme survives a save/load round trip") {
    TempDir tmp("theme_spec");
    ThemeSpec out = fullSpec();
    REQUIRE(out.save(tmp.at("theme.json")));

    ThemeSpec in;
    REQUIRE(in.load(tmp.at("theme.json")));

    CHECK(in.format == ThemeSpec::currentFormat);
    CHECK(in.music.set);
    CHECK_FALSE(in.music.none);
    CHECK(in.music.file == "mel.ogg");
    CHECK(in.music.loop);
    CHECK(in.classic.logo.x == 520);
    CHECK(in.classic.logo.h == 180);
    CHECK(in.classic.font.size == 24);
    CHECK(int(in.classic.menuLines) == 13);
    CHECK(in.classic.statusBar.y == -670);
    CHECK(in.classic.statusBar.textY == 662);
    CHECK(in.classic.statusBar.alpha == 170);
    CHECK(in.classic.textShadow.set);
    CHECK(bool(in.classic.textShadow));
    CHECK(in.classic.keyboardKey.color.toHex() == "#787878");
    CHECK(in.classic.labelColor.toHex() == "#b4b4b4");
    CHECK(in.classic.editorCover.y == 130);
    CHECK(in.classic.buttons.tab == "tab.png");
    CHECK(in.launcher.metaPanel == "images/meta_panel.png");
    CHECK(in.launcher.metaPanelSlides.set);
    CHECK(bool(in.launcher.metaPanelSlides));
    CHECK(in.launcher.textShadow.set);
    CHECK_FALSE(bool(in.launcher.textShadow));
    CHECK(in.launcher.menuIcons.resume == "images/menu_resume.png");
    CHECK(in.launcher.fonts.bold == "font/SST-Bold.ttf");
    CHECK(in.launcher.colors.secondary.toHex() == "#646464");
    CHECK(in.launcher.colors.hint.toHex() == "#ffffff");
    CHECK(in.launcher.snapPanel.set);
    CHECK(in.launcher.snapPanel.w == 240);
    CHECK(in.launcher.menuIcons.resumePicture.set);
    CHECK(in.launcher.menuIcons.resumePicture.y == 22);
    CHECK(in.sounds.resume == "sounds/resume_new.wav");

    // every file field made the trip - the one list in fileFields() is what everything else iterates
    CHECK(in.referencedFiles() == out.referencedFiles());
    CHECK(in.referencedFiles().size() == 42);

    // the file reads in section order, not alphabetically
    string text = tmp.readFile("theme.json");
    CHECK(text.find("\"format\"") < text.find("\"music\""));
    CHECK(text.find("\"music\"") < text.find("\"classic\""));
    CHECK(text.find("\"classic\"") < text.find("\"launcher\""));
    CHECK(text.find("\"launcher\"") < text.find("\"sounds\""));
    CHECK(text.find("\"#000000\"") != string::npos);
}

TEST_CASE("a partial theme writes only what it sets and reads back as partial") {
    TempDir tmp("theme_spec");
    ThemeSpec out;
    out.classic.background = "bg.png";
    out.classic.menuLines = 12;
    out.launcher.colors.text = ThemeColor(1, 2, 3);
    REQUIRE(out.save(tmp.at("theme.json")));

    string text = tmp.readFile("theme.json");
    CHECK(text.find("\"sounds\"") == string::npos); // an empty section is not written
    CHECK(text.find("\"logo\"") == string::npos);
    CHECK(text.find("\"music\"") == string::npos);

    ThemeSpec in;
    REQUIRE(in.load(tmp.at("theme.json")));
    CHECK(in.classic.background == "bg.png");
    CHECK(in.classic.menuLines.set);
    CHECK_FALSE(in.classic.logo.set);
    CHECK_FALSE(in.classic.font.size.set);
    CHECK_FALSE(in.music.set);
    CHECK(in.launcher.background.empty());
    CHECK(in.launcher.colors.text.set);
    CHECK_FALSE(in.launcher.colors.secondary.set);
    CHECK_FALSE(in.launcher.colors.hint.set);             // stays unset: the launcher falls back to secondary
    CHECK_FALSE(in.launcher.snapPanel.set);               // no pane unless a theme asks
    CHECK_FALSE(in.launcher.menuIcons.resumePicture.set); // the launcher's own default window then
    CHECK_FALSE(in.launcher.textShadow.set);              // a theme that says nothing gets the default
    CHECK_FALSE(in.classic.textShadow.set);
}

TEST_CASE("\"music\": null is a theme with no music") {
    TempDir tmp("theme_spec");
    ThemeSpec out;
    out.music.set = true;
    out.music.none = true;
    REQUIRE(out.save(tmp.at("theme.json")));
    CHECK(tmp.readFile("theme.json").find("\"music\": null") != string::npos);

    ThemeSpec in;
    REQUIRE(in.load(tmp.at("theme.json")));
    CHECK(in.music.set);
    CHECK(in.music.none);

    // merging over a theme with music keeps it silent
    in.mergeOver(fullSpec());
    CHECK(in.music.none);
    CHECK(in.music.file.empty());
}

TEST_CASE("mergeOver takes the base's value for everything the theme leaves out") {
    ThemeSpec partial;
    partial.classic.background = "aergb.png";
    partial.classic.font.file = "other.ttf";
    partial.classic.font.size = 30;
    partial.classic.buttons.cross = "x.png";
    partial.launcher.metaPanelSlides = false;
    partial.classic.textShadow = false;

    partial.mergeOver(fullSpec());

    CHECK(partial.classic.background == "aergb.png"); // its own
    CHECK(partial.classic.font.size == 30);
    CHECK(partial.classic.buttons.cross == "x.png");
    CHECK_FALSE(bool(partial.launcher.metaPanelSlides));
    CHECK_FALSE(bool(partial.classic.textShadow)); // its own false survives the base's true
    CHECK(partial.launcher.textShadow.set);        // the base's
    CHECK_FALSE(bool(partial.launcher.textShadow));
    CHECK(partial.classic.logo.file == "ab.png"); // the base's
    CHECK(partial.classic.logo.w == 240);
    CHECK(int(partial.classic.menuLines) == 13);
    CHECK(partial.classic.buttons.circle == "circle.png");
    CHECK(partial.classic.textColor.toHex() == "#ffffff");
    CHECK(partial.music.file == "mel.ogg");
    CHECK(partial.launcher.metaPanel == "images/meta_panel.png");
    CHECK(partial.sounds.cursor == "sounds/cursor.wav");
    CHECK(partial.launcher.colors.hint.toHex() == "#ffffff"); // the base's, like any other colour
    CHECK(partial.referencedFiles().size() == 42);
}

TEST_CASE("a hint colour the theme and the base both leave out stays unset after the merge") {
    // the launcher then draws the footer labels in the secondary colour - that fallback is the
    // screen's, not the spec's, so the spec must not invent a value here
    ThemeSpec base = fullSpec();
    base.launcher.colors.hint = ThemeColor();
    ThemeSpec partial;
    partial.launcher.colors.secondary = ThemeColor(10, 20, 30);
    partial.mergeOver(base);
    CHECK_FALSE(partial.launcher.colors.hint.set);
    CHECK(partial.launcher.colors.secondary.toHex() == "#0a141e");
}

TEST_CASE("mergeOver keeps a rect, a colour and an alpha apart: a theme with only the rect inherits the fill") {
    ThemeSpec partial;
    partial.classic.menuPanel.set = true;
    partial.classic.menuPanel.x = 1;
    partial.classic.menuPanel.y = 2;
    partial.classic.menuPanel.w = 3;
    partial.classic.menuPanel.h = 4;
    partial.classic.statusBar.color = ThemeColor(9, 9, 9); // a colour without a rect
    partial.classic.font.file = "mine.ttf";                // a file without a size
    partial.classic.logo.file = "mylogo.png";              // a file without a rect

    partial.mergeOver(fullSpec());

    CHECK(partial.classic.menuPanel.x == 1);
    CHECK(partial.classic.menuPanel.h == 4);
    CHECK(partial.classic.menuPanel.color.toHex() == "#000000");
    CHECK(int(partial.classic.menuPanel.alpha) == 170);
    CHECK(partial.classic.statusBar.y == -670);
    CHECK(partial.classic.statusBar.color.toHex() == "#090909");
    CHECK(int(partial.classic.statusBar.textY) == 662);
    CHECK(partial.classic.font.file == "mine.ttf");
    CHECK(int(partial.classic.font.size) == 24);
    CHECK(partial.classic.logo.file == "mylogo.png");
    CHECK(partial.classic.logo.w == 240);
}

TEST_CASE("a file that is not valid JSON, or not there, is reported and leaves the spec alone") {
    TempDir tmp("theme_spec");
    tmp.writeFile("bad.json", "{ \"classic\": { \"background\": ");
    tmp.writeFile("array.json", "[1, 2, 3]");

    ThemeSpec spec;
    spec.classic.background = "keep.png";
    CHECK_FALSE(spec.load(tmp.at("bad.json")));
    CHECK_FALSE(spec.load(tmp.at("array.json")));
    CHECK_FALSE(spec.load(tmp.at("missing.json")));
    CHECK(spec.classic.background == "keep.png");
}

TEST_CASE("a key of the wrong type is ignored, not an error") {
    TempDir tmp("theme_spec");
    tmp.writeFile("theme.json", "{ \"classic\": { \"background\": 7, \"menuLines\": \"twelve\", \"textColor\": \"red\","
                                " \"logo\": { \"file\": \"ab.png\", \"x\": \"1\" } }, \"launcher\": \"none\" }");

    ThemeSpec spec;
    REQUIRE(spec.load(tmp.at("theme.json")));
    CHECK(spec.classic.background.empty());
    CHECK_FALSE(spec.classic.menuLines.set);
    CHECK_FALSE(spec.classic.textColor.set);
    CHECK(spec.classic.logo.file == "ab.png");
    CHECK_FALSE(spec.classic.logo.set); // "x" is a string, so the rect is not set
    CHECK(spec.classic.logo.x == 0);
    CHECK(spec.launcher.background.empty());
}

TEST_CASE("resolveFiles: the theme's own file, else the fallback's, else nothing") {
    TempDir tmp("theme_spec");
    tmp.makeSubDir("mine/images");
    tmp.makeSubDir("base/images");
    tmp.writeFile("mine/background.jpg", "x");
    tmp.writeFile("base/background.jpg", "x");
    tmp.writeFile("base/ab.png", "x");
    tmp.writeFile("base/images/arrow.png", "x");

    ThemeSpec base;
    base.classic.background = "background.jpg";
    base.classic.logo.set = true;
    base.classic.logo.file = "ab.png";
    base.launcher.arrow = "images/arrow.png";
    base.launcher.footer = "images/footer.png"; // named but not on disk

    ThemeSpec mine;
    mine.classic.background = "background.jpg"; // has its own
    mine.classic.logo.set = true;
    mine.classic.logo.file = "logo.png"; // names a file it does not have -> base's
    mine.mergeOver(base);                // arrow inherited by name, resolved against `mine` first
    mine.resolveFiles(tmp.at("mine"), base, tmp.at("base"));

    CHECK(mine.classic.background == tmp.at("mine/background.jpg"));
    CHECK(mine.classic.logo.file == tmp.at("base/ab.png"));
    CHECK(mine.launcher.arrow == tmp.at("base/images/arrow.png"));
    CHECK(mine.launcher.footer.empty());
    CHECK(mine.sounds.cursor.empty());
}
