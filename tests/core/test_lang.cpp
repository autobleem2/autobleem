//
// ableem::Lang: UI string translation from a directory of <Language>.txt files.
//
#include "doctest/doctest.h"

#include "../support/string_maker.h"
#include "../support/temp_dir.h"

#include "core/main.h"

#include <string>
#include <vector>

using std::string;
using std::vector;

namespace {

// a lang directory with one translation file: pairs of lines, source then translation
struct LangDir {
    LangDir() : tmp("lang") {
        tmp.makeSubDir("lang");
        tmp.writeFile("lang/Polish.txt", "Re/Scan\nSkanuj\nAbout\nO programie\n|@X| Select\n|@X| Wybierz\n");
        tmp.writeFile("lang/German.txt", "\xEF\xBB\xBFRe/Scan\nNeu scannen\n");   // with a UTF-8 BOM
        tmp.writeFile("lang/readme.md", "not a language");
    }
    string dir() const { return tmp.at("lang"); }
    TempDir tmp;
};

} // namespace

TEST_CASE("the source language translates nothing, and neither does a Lang that has loaded nothing") {
    ableem::Lang lang;
    CHECK(lang.currentLanguage() == "English");
    CHECK(lang.translate("Re/Scan") == "Re/Scan");

    LangDir d;
    lang.load(d.dir(), "English");
    CHECK(lang.translate("Re/Scan") == "Re/Scan");
}

TEST_CASE("a loaded language translates the strings its file has and passes the rest through") {
    LangDir d;
    ableem::Lang lang;
    lang.load(d.dir(), "Polish");

    CHECK(lang.currentLanguage() == "Polish");
    CHECK(lang.translate("Re/Scan") == "Skanuj");
    CHECK(lang.translate("About") == "O programie");
    CHECK(lang.translate("|@X| Select") == "|@X| Wybierz");   // the button markers travel with the string
    CHECK(lang.translate("Options") == "Options");            // no translation: unchanged
    CHECK(lang.translate("") == "");
}

TEST_CASE("a UTF-8 BOM on the first line is not part of the first string") {
    LangDir d;
    ableem::Lang lang;
    lang.load(d.dir(), "German");
    CHECK(lang.translate("Re/Scan") == "Neu scannen");
}

TEST_CASE("a language with no file leaves everything untranslated rather than failing") {
    LangDir d;
    ableem::Lang lang;
    lang.load(d.dir(), "Klingon");
    CHECK(lang.currentLanguage() == "Klingon");
    CHECK(lang.translate("Re/Scan") == "Re/Scan");
}

TEST_CASE("listLanguages is English first, then every other .txt in the directory, alphabetically") {
    LangDir d;
    d.tmp.writeFile("lang/Czech.txt", "");     // written last, listed first: directory order is not the order
    vector<string> names = ableem::Lang::listLanguages(d.dir());
    REQUIRE(names.size() == 4);
    CHECK(names[0] == "English");
    CHECK(names[1] == "Czech");
    CHECK(names[2] == "German");
    CHECK(names[3] == "Polish");
}

TEST_CASE("dumpUntranslated writes the strings a translator still has to do, in order of first use") {
    LangDir d;
    ableem::Lang lang;
    lang.load(d.dir(), "Polish");
    lang.translate("Options");
    lang.translate("Re/Scan");    // translated: not listed
    lang.translate("Memory Cards");
    lang.translate("Options");    // seen again: listed once

    REQUIRE(lang.dumpUntranslated(d.tmp.at("todo.txt")));
    string todo = d.tmp.readFile("todo.txt");
    // pairs of lines, translation left equal to the source; the line ending is the platform's
    CHECK(todo.find("Options") < todo.find("Memory Cards"));
    CHECK(todo.find("Re/Scan") == string::npos);
    CHECK(todo.find("Options", todo.find("Options") + 1) != string::npos);   // twice: source, then translation
}

TEST_CASE("the app's _() goes through the registered Lang, and through none is the identity") {
    LangDir d;
    CHECK(_("Re/Scan") == "Re/Scan");

    ableem::Lang lang;
    lang.load(d.dir(), "Polish");
    ableem::Lang::setCurrent(&lang);
    CHECK(_("Re/Scan") == "Skanuj");
    ableem::Lang::setCurrent(nullptr);
    CHECK(_("Re/Scan") == "Re/Scan");
}
