//
// ThemeConverter: an old theme folder becomes theme.json + role-named files, and nothing else.
//
#include "doctest/doctest.h"

#include "../support/string_maker.h"
#include "../support/temp_dir.h"

#include "core/services/theme_converter.h"

#include <string>

using std::string;

namespace {

// the shipped aergb theme.ini, complete with the dead keys, plus default's extra ones
const char *FULL_INI =
    "[Theme]\n"
    "Music=mel.ogg\n"
    "Loop=1\n"
    "\n"
    "Logo=ab.png\n"
    "Font=zrnic.ttf\n"
    "Background=background.jpg\n"
    "\n"
    "# button icons\n"
    "Circle=circle.png\n"
    "Cross=cross.png\n"
    "Square=square.png\n"
    "Triangle=triangle.png\n"
    "Start=start.png\n"
    "Select=select.png\n"
    "L1=l1.png\n"
    "R1=r1.png\n"
    "\n"
    "Iconw=30\n"
    "Iconh=30\n"
    "IconRescan=705\n"
    "IconExit=600\n"
    "\n"
    "Lpositionx=520\n"
    "Lpositiony=0\n"
    "Lw=240\n"
    "Lh=180\n"
    "\n"
    "Textx=-0\n"
    "Texty=-670\n"
    "Textw=1280\n"
    "Texth=30\n"
    "\n"
    "Textalpha=170\n"
    "Text_fg=255,255,255\n"
    "Text_bg=0,0,0\n"
    "Key_bg=120,120,120\n"
    "Keyalpha=170\n"
    "Label_bg=180,180,180\n"
    "\n"
    "Lspositionx=400\n"
    "Lspositiony=20\n"
    "Lsw=240\n"
    "Lsh=180\n"
    "\n"
    "Opscreenx=30\n"
    "Opscreeny=10\n"
    "Opscreenw=1220\n"
    "Opscreenh=530\n"
    "\n"
    "Fsize=24\n"
    "Ttop=662\n"
    "Maxw=1140\n"
    "\n"
    "Fsposx=180\n"
    "Fsposy=35\n"
    "\n"
    "Lines=13\n"
    "\n"
    "Ecoverx=95;\n"
    "Ecovery=130;\n";

// an old theme folder: theme.ini, the launcher images under their PSC names among the ~300 others,
// the fonts and sounds, and a credit file at the root
struct OldTheme {
    explicit OldTheme(TempDir &tmp, const string &name, bool withAbVariants = false) : dir(tmp.at(name)) {
        tmp.makeSubDir(name + "/images/GR");
        tmp.makeSubDir(name + "/images/CB");
        tmp.makeSubDir(name + "/images/MC");
        tmp.makeSubDir(name + "/images/BMP_Text");
        tmp.makeSubDir(name + "/images/BMP_TXT_SST");
        tmp.makeSubDir(name + "/images/SET");
        tmp.makeSubDir(name + "/font");
        tmp.makeSubDir(name + "/sounds");
        tmp.writeFile(name + "/theme.ini", FULL_INI);
        tmp.writeFile(name + "/credit.txt", "Theme by someone\n");
        tmp.writeFile(name + "/mel.ogg", "ogg");
        tmp.writeFile(name + "/ab.png", "png");
        tmp.writeFile(name + "/background.jpg", "jpg");
        tmp.writeFile(name + "/zrnic.ttf", "ttf");
        for (const char *b : { "circle", "cross", "square", "triangle", "start", "select", "l1", "r1" })
            tmp.writeFile(name + "/" + b + ".png", "png");

        const char *used[] = { "GR/JP_US_BG.png", "GR/Footer.png", "GR/Acid_C_Btn.png", "BMP_Text/Play_Text.png",
                               "CB/Function_BG.png", "CB/PlayerOne.png", "GR/arrow.png", "GR/X_Btn_ICN.png",
                               "GR/Circle_Btn_ICN.png", "GR/Tri_Btn_ICN.png", "CB/Setting_ICN.png",
                               "CB/Manual_ICN.png", "CB/MemoryCard_ICN.png", "CB/Resume.png",
                               "MC/Dot_Matrix.png", "MC/Pencil_Carsor.png" };
        for (const char *f : used) tmp.writeFile(name + "/images/" + f, string("stock ") + f);
        if (withAbVariants) {
            tmp.writeFile(name + "/images/GR/AB_BG.png", "ab bg");
            tmp.writeFile(name + "/images/GR/Footer_AB.png", "ab footer");
            tmp.writeFile(name + "/images/CB/Function_AB.png", "ab function");
        }
        // the SonyUI-only leftovers
        tmp.writeFile(name + "/images/BMP_TXT_SST/msg_cant_change_discs_now_E.png", "x");
        tmp.writeFile(name + "/images/SET/Check.png", "x");
        tmp.writeFile(name + "/images/GR/Squere_Btn_ICN.png", "x");
        tmp.writeFile(name + "/images/Rectangle.png", "x");
        tmp.writeFile(name + "/images/Game_Guide_QR.png", "x");

        tmp.writeFile(name + "/font/SST-Medium.ttf", "ttf");
        tmp.writeFile(name + "/font/SST-Bold.ttf", "ttf");
        tmp.writeFile(name + "/font/SSTJapanese-Bold.ttf", "ttf");
        for (const char *s : { "cursor", "cancel", "home_up", "home_down", "resume_new", "resume_old", "decide", "end", "error" })
            tmp.writeFile(name + "/sounds/" + s + ".wav", "wav");
    }

    bool has(const string &rel) const { return DirEntry::exists(dir + sep + rel); }

    string dir;
};

} // namespace

TEST_CASE("specFromIni: every live key lands in its typed place, the dead ones are dropped") {
    TempDir tmp("theme_conv");
    tmp.writeFile("theme.ini", FULL_INI);
    IniFile ini;
    ini.load(tmp.at("theme.ini"));

    ThemeSpec s = ThemeConverter::specFromIni(ini);

    CHECK(s.music.set);
    CHECK(s.music.file == "mel.ogg");
    CHECK(s.music.loop);
    CHECK_FALSE(s.music.none);
    CHECK(s.classic.background == "background.jpg");
    CHECK(s.classic.logo.file == "ab.png");
    CHECK(s.classic.logo.set);
    CHECK(s.classic.logo.x == 520);
    CHECK(s.classic.logo.h == 180);
    CHECK(s.classic.font.file == "zrnic.ttf");
    CHECK(int(s.classic.font.size) == 24);
    CHECK(int(s.classic.menuLines) == 13);
    CHECK(s.classic.menuPanel.set);
    CHECK(s.classic.menuPanel.w == 1220);
    CHECK_FALSE(s.classic.menuPanel.color.set);     // no Main_bg in this ini: inherits the default's
    CHECK_FALSE(s.classic.menuPanel.alpha.set);
    CHECK(s.classic.statusBar.set);
    CHECK(s.classic.statusBar.y == -670);
    CHECK(s.classic.statusBar.color.toHex() == "#000000");
    CHECK(int(s.classic.statusBar.alpha) == 170);
    CHECK(int(s.classic.statusBar.textY) == 662);
    CHECK(s.classic.textColor.toHex() == "#ffffff");
    CHECK(s.classic.keyboardKey.color.toHex() == "#787878");
    CHECK(int(s.classic.keyboardKey.alpha) == 170);
    CHECK(s.classic.labelColor.toHex() == "#b4b4b4");
    CHECK(s.classic.freeSpaceText.x == 180);
    CHECK(s.classic.editorCover.x == 95);            // "95;" the way atoi read it
    CHECK(s.classic.editorCover.y == 130);
    CHECK(s.classic.buttons.cross == "cross.png");
    CHECK(s.classic.buttons.r1 == "r1.png");
    CHECK(s.classic.buttons.l2.empty());             // not in this ini
    CHECK(s.classic.buttons.tab.empty());
    CHECK(s.launcher.background.empty());            // the ini knows nothing about the launcher
    CHECK(s.sounds.cursor.empty());
}

TEST_CASE("specFromIni: Loop=-1 is no music; a group with a key missing is left to the default") {
    TempDir tmp("theme_conv");
    tmp.writeFile("theme.ini", "[Theme]\nMusic=x.ogg\nLoop=-1\nLpositionx=1\nLpositiony=2\nLw=3\nFsposx=5\n");
    IniFile ini;
    ini.load(tmp.at("theme.ini"));

    ThemeSpec s = ThemeConverter::specFromIni(ini);
    CHECK(s.music.set);
    CHECK(s.music.none);
    CHECK_FALSE(s.classic.logo.set);
    CHECK_FALSE(s.classic.freeSpaceText.set);

    tmp.writeFile("once.ini", "[Theme]\nMusic=x.ogg\nLoop=0\n");
    IniFile once;
    once.load(tmp.at("once.ini"));
    ThemeSpec o = ThemeConverter::specFromIni(once);
    CHECK_FALSE(o.music.loop);
    CHECK_FALSE(o.music.none);
}

TEST_CASE("convert: theme.json names every used file by role, the rest is gone, the root is left alone") {
    TempDir tmp("theme_conv");
    OldTheme theme(tmp, "aergb");
    REQUIRE(ThemeConverter::needsConversion(theme.dir));

    REQUIRE(ThemeConverter::convert(theme.dir));

    CHECK(theme.has("theme.json"));
    CHECK_FALSE(theme.has("theme.ini"));
    CHECK_FALSE(ThemeConverter::needsConversion(theme.dir));

    ThemeSpec s;
    REQUIRE(s.load(theme.dir + sep + "theme.json"));
    CHECK(s.classic.background == "background.jpg");
    CHECK(s.classic.buttons.l1 == "l1.png");
    CHECK(s.launcher.background == "images/launcher_background.png");
    CHECK(s.launcher.footer == "images/launcher_footer.png");
    CHECK(s.launcher.playText == "images/play_text.png");
    CHECK(s.launcher.settingsPanel == "images/settings_panel.png");
    CHECK(s.launcher.metaPanel == "images/meta_panel.png");
    CHECK(s.launcher.metaPanelSlides.set);
    CHECK(bool(s.launcher.metaPanelSlides));
    CHECK(s.launcher.hints.triangle == "images/hint_triangle.png");
    CHECK(s.launcher.menuIcons.memcard == "images/menu_memcard.png");
    CHECK(s.launcher.memcardManager.pencil == "images/memcard_pencil.png");
    CHECK(s.launcher.fonts.medium == "font/SST-Medium.ttf");
    CHECK(s.launcher.fonts.bold == "font/SST-Bold.ttf");
    CHECK(s.sounds.resume == "sounds/resume_new.wav");
    CHECK(s.referencedFiles().size() == 1 + 3 + 8 + 16 + 2 + 5);   // music, bg/logo/font, 8 buttons, launcher, fonts, sounds

    // renamed, with the right content, and the old names gone
    CHECK(tmp.readFile("aergb/images/launcher_background.png") == "stock GR/JP_US_BG.png");
    CHECK(tmp.readFile("aergb/images/memcard_pencil.png") == "stock MC/Pencil_Carsor.png");
    CHECK_FALSE(theme.has("images/GR/JP_US_BG.png"));
    CHECK_FALSE(theme.has("images/MC/Pencil_Carsor.png"));

    // the leftovers and their directories
    CHECK_FALSE(theme.has("images/BMP_TXT_SST/msg_cant_change_discs_now_E.png"));
    CHECK_FALSE(theme.has("images/BMP_TXT_SST"));
    CHECK_FALSE(theme.has("images/SET"));
    CHECK_FALSE(theme.has("images/GR"));
    CHECK_FALSE(theme.has("images/CB"));
    CHECK_FALSE(theme.has("images/Rectangle.png"));
    CHECK_FALSE(theme.has("images/Game_Guide_QR.png"));
    CHECK_FALSE(theme.has("font/SSTJapanese-Bold.ttf"));
    CHECK_FALSE(theme.has("sounds/decide.wav"));
    CHECK_FALSE(theme.has("sounds/resume_old.wav"));
    CHECK(theme.has("font/SST-Bold.ttf"));
    CHECK(theme.has("sounds/home_down.wav"));

    // the root: everything named is there, and so is what is not
    CHECK(theme.has("credit.txt"));
    CHECK(theme.has("mel.ogg"));
    CHECK(theme.has("cross.png"));

    // exactly what the new layout says
    std::vector<string> files;
    for (const DirEntry &e : DirEntry::diru(theme.dir + sep + "images")) files.push_back(e.name);
    CHECK(files.size() == 16);
    CHECK(DirEntry::diru(theme.dir + sep + "font").size() == 2);
    CHECK(DirEntry::diru(theme.dir + sep + "sounds").size() == 5);
}

TEST_CASE("convert: the _AB variants win, and make the meta panel static") {
    TempDir tmp("theme_conv");
    OldTheme theme(tmp, "evolution", true);

    REQUIRE(ThemeConverter::convert(theme.dir));

    ThemeSpec s;
    REQUIRE(s.load(theme.dir + sep + "theme.json"));
    CHECK_FALSE(bool(s.launcher.metaPanelSlides));
    CHECK(tmp.readFile("evolution/images/launcher_background.png") == "ab bg");
    CHECK(tmp.readFile("evolution/images/launcher_footer.png") == "ab footer");
    CHECK(tmp.readFile("evolution/images/settings_panel.png") == "ab function");
    CHECK(tmp.readFile("evolution/images/play_button.png") == "stock GR/Acid_C_Btn.png");
    CHECK_FALSE(theme.has("images/GR"));    // the stock JP_US_BG.png etc. went with the directory
}

TEST_CASE("convert: a folder with launcher images but no theme.ini is a theme too") {
    TempDir tmp("theme_conv");
    OldTheme theme(tmp, "autobleem");
    DirEntry::removeFile(theme.dir + sep + "theme.ini");
    REQUIRE(ThemeConverter::needsConversion(theme.dir));

    REQUIRE(ThemeConverter::convert(theme.dir));

    ThemeSpec s;
    REQUIRE(s.load(theme.dir + sep + "theme.json"));
    CHECK_FALSE(s.music.set);
    CHECK(s.classic.background.empty());
    CHECK_FALSE(s.classic.logo.set);
    CHECK(s.launcher.metaPanel == "images/meta_panel.png");
    CHECK(theme.has("images/meta_panel.png"));
    CHECK(theme.has("ab.png"));      // a root file the json does not name is not the converter's business
}

TEST_CASE("convert: colors.ini becomes launcher.colors and is removed") {
    TempDir tmp("theme_conv");
    OldTheme theme(tmp, "sony");
    tmp.writeFile("sony/colors.ini", "[colors]\nfg=255,255,255\nsec=60,60,60\n");

    REQUIRE(ThemeConverter::convert(theme.dir));

    ThemeSpec s;
    REQUIRE(s.load(theme.dir + sep + "theme.json"));
    CHECK(s.launcher.colors.text.toHex() == "#ffffff");
    CHECK(s.launcher.colors.secondary.toHex() == "#3c3c3c");
    CHECK_FALSE(theme.has("colors.ini"));
}

TEST_CASE("convert: a partial theme stays partial, and a launcher image that is missing is not named") {
    TempDir tmp("theme_conv");
    tmp.makeSubDir("mini/images/GR");
    tmp.writeFile("mini/theme.ini", "[Theme]\nBackground=bg.png\nFsize=30\n");
    tmp.writeFile("mini/bg.png", "x");
    tmp.writeFile("mini/images/GR/JP_US_BG.png", "x");

    REQUIRE(ThemeConverter::convert(tmp.at("mini")));

    ThemeSpec s;
    REQUIRE(s.load(tmp.at("mini/theme.json")));
    CHECK(s.classic.background == "bg.png");
    CHECK(int(s.classic.font.size) == 30);
    CHECK(s.classic.font.file.empty());
    CHECK_FALSE(s.classic.menuPanel.set);
    CHECK(s.launcher.background == "images/launcher_background.png");
    CHECK(s.launcher.footer.empty());
    CHECK(s.launcher.fonts.bold.empty());
    CHECK(s.sounds.cursor.empty());
    CHECK(DirEntry::exists(tmp.at("mini/images/launcher_background.png")));
    CHECK_FALSE(DirEntry::exists(tmp.at("mini/font")));    // never there, not created either
}

TEST_CASE("convert twice: the second run has nothing to do and changes nothing") {
    TempDir tmp("theme_conv");
    OldTheme theme(tmp, "twice");
    REQUIRE(ThemeConverter::convert(theme.dir));
    string json = tmp.readFile("twice/theme.json");

    CHECK_FALSE(ThemeConverter::needsConversion(theme.dir));
    REQUIRE(ThemeConverter::convert(theme.dir));   // forced anyway: still fine
    CHECK(tmp.readFile("twice/theme.json") == json);
    CHECK(theme.has("images/launcher_background.png"));
    CHECK(theme.has("credit.txt"));
}

TEST_CASE("a folder that is neither is not a theme") {
    TempDir tmp("theme_conv");
    tmp.makeSubDir("junk/images");
    tmp.writeFile("junk/readme.txt", "x");
    CHECK_FALSE(ThemeConverter::needsConversion(tmp.at("junk")));

    tmp.makeSubDir("done");
    tmp.writeFile("done/theme.json", "{}");
    tmp.writeFile("done/theme.ini", "[Theme]\n");   // a leftover next to a theme.json is not a reason to convert again
    CHECK_FALSE(ThemeConverter::needsConversion(tmp.at("done")));
}
