// lib_ableem - engine: gettext-style UI string translation from a directory of <Language>.txt files.
//
// A language file is "English text=Translated text", one per line, with "#" comment lines (the first is a
// header naming the language; tools/lang_tools.py keeps the files in step with the source and counts
// what is untranslated). The older layout - pairs of lines, the source string then its translation - is
// still read when the first line is not a comment, so a file someone kept from before 2026-09 works. A
// UTF-8 BOM on the first line is tolerated. English is the source language, so loading it loads nothing
// and every string is returned as it is. A string with no translation (or an empty one) is passed through
// unchanged and remembered, so dumpUntranslated() can write a file listing what a translator still has to do.
#pragma once

#include <map>
#include <string>
#include <vector>

namespace ableem {

//******************
// Lang
//******************
class Lang {
public:
    static const char *const SourceLanguage;   // "English"

    // loads <langDir>/<languageName>.txt (nothing, for the source language). A missing file leaves every
    // string untranslated, which is what a misspelt language name in config.ini has always done.
    void load(const std::string &langDir, const std::string &languageName);

    std::string translate(const std::string &input);
    const std::string &currentLanguage() const { return currentLanguage_; }

    // "English" first, then every other <Name>.txt in the directory, as names without the extension
    static std::vector<std::string> listLanguages(const std::string &langDir);

    // writes every string seen since load() with no translation, as "English text=" lines ready to fill
    // in. false when the file cannot be written.
    bool dumpUntranslated(const std::string &path) const;

    // The instance translate() below consults. The application registers the one it owns; with none
    // registered every string is its own translation, which is what a unit test wants.
    static void setCurrent(Lang *lang) { current_ = lang; }
    static Lang *current() { return current_; }

private:
    std::string currentLanguage_ = SourceLanguage;
    std::map<std::string, std::string> translations_;   // source -> translation
    std::vector<std::string> untranslated_;             // sources seen with no translation, in order of first use
    static Lang *current_;
};

// translate through Lang::current(); the application's `_()` is this
std::string translate(const std::string &input);

} // namespace ableem
