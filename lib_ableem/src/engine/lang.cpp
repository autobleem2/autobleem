#include "ableem/engine/lang.h"
#include "ableem/engine/filesystem.h"
#include "ableem/engine/strings.h"

#include <fstream>
#include <iostream>

using namespace std;

namespace ableem {

const char *const Lang::SourceLanguage = "English";
Lang *Lang::current_ = nullptr;

//*******************************
// translate
//*******************************
string translate(const string &input) {
    Lang *lang = Lang::current();
    return lang ? lang->translate(input) : input;
}

//*******************************
// Lang::translate
//*******************************
// The string is looked up exactly as given - not trimmed - which is how it has always been: the original
// called a copying trim and discarded the result.
string Lang::translate(const string &input) {
    if (currentLanguage_ == SourceLanguage) return input;
    if (input.empty()) return "";
    string translated = translations_[input];
    if (translated == "") {
        translations_[input] = input;
        translated = input;
        untranslated_.push_back(input);
    }
    return translated;
}

//*******************************
// Lang::load
//*******************************
void Lang::load(const string &langDir, const string &languageName) {
    translations_.clear();
    untranslated_.clear();
    currentLanguage_ = languageName;
    if (languageName == SourceLanguage) return;

    string path = langDir + sep + languageName + ".txt";
    ifstream is(path);
    string line;
    vector<string> lines;
    int lineNum = 0;
    while (std::getline(is, line)) {
        // strip the UTF-8 BOM some editors put on the first line
        if (lineNum == 0 && line.size() >= 3) {
            unsigned char *p = (unsigned char *) line.c_str();
            if ((p[0] == 0xEF) && (p[1] == 0xBB) && (p[2] == 0xBF)) {
                line = (char *) p + 3;
            }
        }
        trim(line);
        lines.push_back(line);
        ++lineNum;
    }
    for (size_t i = 0; i + 1 < lines.size(); i += 2) {
        translations_[lines[i]] = lines[i + 1];
    }
}

//*******************************
// Lang::listLanguages
//*******************************
vector<string> Lang::listLanguages(const string &langDir) {
    vector<string> languages;
    languages.push_back(SourceLanguage);
    for (const DirEntry &entry : DirEntry::diru(langDir)) {
        // every *.txt but the source language's own
        if (DirEntry::matchExtension(entry.name, ".txt") &&
            !Strings::compareCaseInsensitive(entry.name, string(SourceLanguage) + ".txt")) {
            languages.push_back(entry.name.substr(0, entry.name.size() - 4));
        }
    }
    return languages;
}

//*******************************
// Lang::dumpUntranslated
//*******************************
bool Lang::dumpUntranslated(const string &path) const {
    ofstream os(path);
    if (!DirEntry::checkWritable(os, path)) return false;
    for (const string &source : untranslated_) {
        os << source << endl << source << endl;
    }
    os.flush();
    return true;
}

} // namespace ableem
