//
// Lang: the gettext-style string translation used by every screen.
//
#pragma once

#include <map>
#include <string>
#include <memory>
#include <vector>

// Translate one UI string: `_("Re/Scan")`. English is the source language, so it is returned unchanged.
// Strings may contain emoji markers such as |@X| or |@L1|, which Gui::renderText replaces with the theme's
// button textures - keep them in the translation.
std::string _(const std::string &input);

//******************
// Lang
//******************
// One language loaded from resources/lang/<Language>.txt (tab separated original/translation pairs, with a
// UTF-8 BOM tolerated on the first line). A string with no translation is passed through unchanged and
// remembered, so dump() can write out a file listing everything still untranslated.
class Lang {
public:
    std::string currentLang;                        // "English" means no lookup at all

    std::string translate(std::string input);
    void load(std::string languageName);            // the filename without the .txt
    void dump(std::string fileName);                // writes the strings seen with no translation
    std::vector<std::string> getListOfLanguages();  // the .txt files in resources/lang

    Lang(Lang const &) = delete;
    Lang &operator=(Lang const &) = delete;

    static std::shared_ptr<Lang> getInstance() {
        static std::shared_ptr<Lang> s{new Lang};
        return s;
    }

private:
    Lang() {}

    std::map<std::string, std::string> langData;    // original -> translation
    std::vector<std::string> newData;               // originals seen with no translation, for dump()
};
