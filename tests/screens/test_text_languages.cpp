//
// Every translation of every language file through the Rust text engine (autobleem-core lib_ableem/rust/ab_text),
// in the fonts the launcher draws that language with: the theme's Red Hat Text, or Noto Sans SC for Chinese, with
// the fallback chain behind it (Fonts::openAllFonts). No character may come out as the empty box. And each line's
// width against SDL_ttf's (what SDL_FontCache measured, glyph by glyph, no kerning): the lines that grew by more
// than 3% are listed (a message, and text-widths.csv in the build directory) for the screenshot review - the text
// shapes differently now, so a little change is expected and not a failure.
//
#include "doctest/doctest.h"

#ifdef ABLEEM_TEXT_RUST
#include "ab_text.h"
#include <SDL2/SDL_ttf.h>

#include <algorithm>
#include <cstdio>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

using namespace std;

namespace {

const string fontsDir = string(AB_LAUNCHER_RESOURCES) + "/fonts/";
const string langDir = string(AB_LAUNCHER_RESOURCES) + "/lang/";

const vector<string> languages = {"English",  "Chinese_Simplified",
                                  "Czech",    "Danish",
                                  "Deutsch",  "Dutch",
                                  "Finnish",  "French",
                                  "Italiano", "Occitan",
                                  "Polski",   "Portuguese_BR",
                                  "Romanian", "Slovak",
                                  "Spanish",  "Swedish",
                                  "Turkish"};

// the translations of a language file ("English text=Translation"; English: the keys themselves)
vector<string> translations(const string &language) {
    vector<string> out;
    ifstream in(langDir + language + ".txt");
    string line;
    while (getline(in, line)) {
        if (!line.empty() && line.back() == '\r')
            line.pop_back();
        if (line.empty() || line[0] == '#')
            continue;
        const size_t eq = line.find('=');
        if (eq == string::npos)
            continue;
        string text = language == "English" ? line.substr(0, eq) : line.substr(eq + 1);
        if (!text.empty())
            out.push_back(text);
    }
    return out;
}

struct RustFont {
    AbtFont *font;
    explicit RustFont(const string &file, float px) : font(abt_font_load((fontsDir + file).c_str(), px)) {}
    ~RustFont() { abt_font_free(font); }
};

int ttfWidth(TTF_Font *font, const string &text) {
    int w = 0, h = 0;
    return TTF_SizeUTF8(font, text.c_str(), &w, &h) == 0 ? w : 0;
}

} // namespace

TEST_CASE("every language file has all 17 languages with text in them") {
    for (const string &language : languages) {
        INFO(language);
        CHECK(translations(language).size() > 400);
    }
}

TEST_CASE("no translation draws an empty box, and the lines that got wider are listed") {
    REQUIRE(abt_add_fallback((fontsDir + "NotoSansSC-Regular.otf").c_str()) > 0);
    REQUIRE(abt_add_fallback((fontsDir + "OpenSans-Medium.ttf").c_str()) > 0);
    REQUIRE(TTF_Init() == 0);
    ofstream csv("text-widths.csv");
    csv << "language,font,old,new,growth %,text\n";
    struct Grown {
        double growth;
        string line;
    };
    vector<Grown> grown;
    for (const string &language : languages) {
        const bool chinese = language == "Chinese_Simplified";
        // FONT_22_MED and FONT_20_BOLD, the sizes most of the launcher's text is in
        const vector<pair<string, int>> faces =
            chinese ? vector<pair<string, int>>{{"NotoSansSC-Regular.otf", 22}}
                    : vector<pair<string, int>>{{"RedHatText-Medium.ttf", 22}, {"RedHatText-SemiBold.ttf", 20}};
        for (const auto &face : faces) {
            RustFont rust(face.first, static_cast<float>(face.second));
            REQUIRE(rust.font != nullptr);
            unique_ptr<TTF_Font, void (*)(TTF_Font *)> ttf(TTF_OpenFont((fontsDir + face.first).c_str(), face.second),
                                                           TTF_CloseFont);
            REQUIRE(ttf != nullptr);
            // the same line height as SDL_FontCache's (TTF_FontHeight), give or take a rounding
            CHECK(abs(abt_font_line_height(rust.font) - TTF_FontHeight(ttf.get())) <= 1);
            size_t missing = 0;
            for (const string &text : translations(language)) {
                const size_t m = abt_missing_glyphs(rust.font, text.data(), text.size());
                if (m) {
                    missing += m;
                    MESSAGE(language << " (" << face.first << "): " << m << " missing in \"" << text << "\"");
                }
                int32_t w = 0;
                abt_measure(rust.font, text.data(), text.size(), 0, &w, nullptr);
                const int old = ttfWidth(ttf.get(), text);
                const double growth = old > 0 ? 100.0 * (w - old) / old : 0.0;
                char pct[16];
                snprintf(pct, sizeof(pct), "%.1f", growth);
                string quoted = text;
                for (size_t q = 0; (q = quoted.find('"', q)) != string::npos; q += 2)
                    quoted.insert(q, "\"");
                csv << language << "," << face.first << "@" << face.second << "," << old << "," << w << "," << pct
                    << ",\"" << quoted << "\"\n";
                if (growth > 3.0 && old >= 40)
                    grown.push_back({growth, language + " " + pct + "% (" + to_string(old) + " -> " + to_string(w) +
                                                 " px): " + text});
            }
            INFO(language << " in " << face.first);
            CHECK(missing == 0);
        }
    }
    sort(grown.begin(), grown.end(), [](const Grown &a, const Grown &b) { return a.growth > b.growth; });
    MESSAGE(grown.size() << " lines grew by more than 3% (text-widths.csv has them all); the widest growths:");
    for (size_t i = 0; i < grown.size() && i < 15; i++)
        MESSAGE("  " << grown[i].line);
    TTF_Quit();
}

TEST_CASE("Chinese wraps between characters inside the width") {
    RustFont font("NotoSansSC-Regular.otf", 22);
    REQUIRE(font.font != nullptr);
    for (const string &text : translations("Chinese_Simplified")) {
        int32_t w = 0, lines = 0;
        abt_measure(font.font, text.data(), text.size(), 0, &w, nullptr);
        if (w <= 300)
            continue;
        abt_measure(font.font, text.data(), text.size(), 300, &w, &lines);
        INFO(text);
        CHECK(lines >= 2);
        CHECK(w <= 300);
    }
}
#else
TEST_CASE("the Rust text engine is not in this build (no cargo): nothing to check") {
    MESSAGE("ABLEEM_TEXT_RUST is off - text is drawn by SDL_FontCache");
}
#endif
