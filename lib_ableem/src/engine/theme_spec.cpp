#include "ableem/engine/theme_spec.h"
#include "ableem/engine/filesystem.h"
#include "ableem/engine/strings.h"

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <json.h>
#include <fifo_map.h>
#include "ableem/engine/log.h"

using namespace std;
using namespace nlohmann;

namespace ableem {

namespace {

// A workaround to use fifo_map as the json map so keys keep their insertion order; the 'less' compare is ignored
template<class K, class V, class dummy_compare, class A>
using fifo_map_workaround = fifo_map<K, V, fifo_map_compare<K>, A>;
using ordered_json = basic_json<fifo_map_workaround>;

//*******************************
// reading: each helper sets `out` only when the key is there with the right type
//*******************************
const json *child(const json &j, const char *key) {
    if (!j.is_object()) return nullptr;
    auto it = j.find(key);
    return it == j.end() ? nullptr : &*it;
}

void readStr(const json &j, const char *key, string &out) {
    const json *v = child(j, key);
    if (v && v->is_string()) out = v->get<string>();
}

void readInt(const json &j, const char *key, int &out, bool &set) {
    const json *v = child(j, key);
    if (v && v->is_number_integer()) { out = v->get<int>(); set = true; }
}

void readInt(const json &j, const char *key, int &out) {
    bool set = false;
    readInt(j, key, out, set);
}

void readOptInt(const json &j, const char *key, Opt<int> &out) {
    readInt(j, key, out.value, out.set);
}

void readOptBool(const json &j, const char *key, Opt<bool> &out) {
    const json *v = child(j, key);
    if (v && v->is_boolean()) out = v->get<bool>();
}

void readColor(const json &j, const char *key, ThemeColor &out) {
    const json *v = child(j, key);
    if (v && v->is_string()) ThemeColor::parseHex(v->get<string>(), out);
}

// x/y/w/h (set when x is there), colour and alpha, each optional on its own
void readRect(const json &j, int &x, int &y, int &w, int &h, bool &set) {
    readInt(j, "x", x, set);
    readInt(j, "y", y);
    readInt(j, "w", w);
    readInt(j, "h", h);
}

void readPanel(const json &j, ThemePanel &out) {
    readRect(j, out.x, out.y, out.w, out.h, out.set);
    readColor(j, "color", out.color);
    readOptInt(j, "alpha", out.alpha);
}

//*******************************
// writing
//*******************************
ordered_json rectJson(int x, int y, int w, int h) {
    ordered_json o = ordered_json::object();
    o["x"] = x;
    o["y"] = y;
    o["w"] = w;
    o["h"] = h;
    return o;
}

void putStr(ordered_json &o, const char *key, const string &s) {
    if (!s.empty()) o[key] = s;
}

void putColor(ordered_json &o, const char *key, const ThemeColor &c) {
    if (c.set) o[key] = c.toHex();
}

void putOptInt(ordered_json &o, const char *key, const Opt<int> &v) {
    if (v.set) o[key] = v.value;
}

ordered_json panelJson(const ThemePanel &p) {
    ordered_json o = ordered_json::object();
    if (p.set) o = rectJson(p.x, p.y, p.w, p.h);
    putColor(o, "color", p.color);
    putOptInt(o, "alpha", p.alpha);
    return o;
}

void putPoint(ordered_json &o, const char *key, const ThemePoint &p) {
    if (!p.set) return;
    ordered_json pt = ordered_json::object();
    pt["x"] = p.x;
    pt["y"] = p.y;
    o[key] = pt;
}

// an object is written only if something in it is set, so an untouched section stays out of the file
void putObject(ordered_json &o, const char *key, const ordered_json &obj) {
    if (!obj.empty()) o[key] = obj;
}

//*******************************
// merging
//*******************************
void mergeStr(string &mine, const string &base) {
    if (mine.empty()) mine = base;
}

void mergeColor(ThemeColor &mine, const ThemeColor &base) {
    if (!mine.set) mine = base;
}

template<class T>
void mergeSet(T &mine, const T &base) {   // anything with a `set` member and nothing else optional in it
    if (!mine.set) mine = base;
}

void mergePanel(ThemePanel &mine, const ThemePanel &base) {
    if (!mine.set) {
        mine.x = base.x; mine.y = base.y; mine.w = base.w; mine.h = base.h;
        mine.set = base.set;
    }
    mergeColor(mine.color, base.color);
    mergeSet(mine.alpha, base.alpha);
}

} // namespace

//*******************************
// ThemeColor::parseHex / parseRgb / toHex
//*******************************
bool ThemeColor::parseHex(const string &hex, ThemeColor &out) {
    if (hex.size() != 7 || hex[0] != '#') return false;
    for (size_t i = 1; i < hex.size(); i++)
        if (!isxdigit(static_cast<unsigned char>(hex[i]))) return false;
    unsigned int rgb = static_cast<unsigned int>(strtoul(hex.c_str() + 1, nullptr, 16));
    out = ThemeColor((rgb >> 16) & 0xff, (rgb >> 8) & 0xff, rgb & 0xff);
    return true;
}

bool ThemeColor::parseRgb(const string &rgb, ThemeColor &out) {
    int r, g, b;
    char extra;
    if (sscanf(rgb.c_str(), " %d , %d , %d %c", &r, &g, &b, &extra) != 3) return false;
    if (r < 0 || r > 255 || g < 0 || g > 255 || b < 0 || b > 255) return false;
    out = ThemeColor(r, g, b);
    return true;
}

string ThemeColor::toHex() const {
    char buf[8];
    snprintf(buf, sizeof(buf), "#%02x%02x%02x", r & 0xff, g & 0xff, b & 0xff);
    return buf;
}

//*******************************
// ThemeSpec::load
//*******************************
bool ThemeSpec::load(const string &path) {
    ifstream in(path, ifstream::binary);
    if (!in.is_open()) {
        PLOG_WARNING << "Could not open theme file: " << path;
        return false;
    }

    // a hand-edited theme must not take the whole UI down (nlohmann throws on bad input)
    json j;
    try {
        in >> j;
    } catch (const json::exception &e) {
        PLOG_INFO << "Theme " << path << " is not valid JSON: " << e.what();
        return false;
    }
    if (!j.is_object()) {
        PLOG_INFO << "Theme " << path << " is not a JSON object";
        return false;
    }

    readInt(j, "format", format);

    if (const json *m = child(j, "music")) {
        music.set = true;
        if (m->is_null()) {
            music.none = true;
        } else {
            readStr(*m, "file", music.file);
            const json *loop = child(*m, "loop");
            if (loop && loop->is_boolean()) music.loop = loop->get<bool>();
        }
    }

    if (const json *c = child(j, "classic")) {
        readStr(*c, "background", classic.background);
        if (const json *l = child(*c, "logo")) {
            readStr(*l, "file", classic.logo.file);
            readRect(*l, classic.logo.x, classic.logo.y, classic.logo.w, classic.logo.h, classic.logo.set);
        }
        if (const json *f = child(*c, "font")) {
            readStr(*f, "file", classic.font.file);
            readOptInt(*f, "size", classic.font.size);
        }
        readOptInt(*c, "menuLines", classic.menuLines);
        if (const json *p = child(*c, "menuPanel")) readPanel(*p, classic.menuPanel);
        if (const json *s = child(*c, "statusBar")) {
            readPanel(*s, classic.statusBar);
            readOptInt(*s, "textY", classic.statusBar.textY);
        }
        readColor(*c, "textColor", classic.textColor);
        readOptBool(*c, "textShadow", classic.textShadow);
        if (const json *k = child(*c, "keyboardKey")) {
            readColor(*k, "color", classic.keyboardKey.color);
            readOptInt(*k, "alpha", classic.keyboardKey.alpha);
        }
        readColor(*c, "labelColor", classic.labelColor);
        if (const json *p = child(*c, "freeSpaceText")) {
            readInt(*p, "x", classic.freeSpaceText.x);
            readInt(*p, "y", classic.freeSpaceText.y);
            classic.freeSpaceText.set = true;
        }
        if (const json *p = child(*c, "editorCover")) {
            readInt(*p, "x", classic.editorCover.x);
            readInt(*p, "y", classic.editorCover.y);
            classic.editorCover.set = true;
        }
        if (const json *b = child(*c, "buttons")) {
            auto &bt = classic.buttons;
            readStr(*b, "cross", bt.cross);
            readStr(*b, "circle", bt.circle);
            readStr(*b, "square", bt.square);
            readStr(*b, "triangle", bt.triangle);
            readStr(*b, "start", bt.start);
            readStr(*b, "select", bt.select);
            readStr(*b, "l1", bt.l1);
            readStr(*b, "r1", bt.r1);
            readStr(*b, "l2", bt.l2);
            readStr(*b, "r2", bt.r2);
            readStr(*b, "check", bt.check);
            readStr(*b, "uncheck", bt.uncheck);
            readStr(*b, "esc", bt.esc);
            readStr(*b, "enter", bt.enter);
            readStr(*b, "tab", bt.tab);
        }
    }

    if (const json *l = child(j, "launcher")) {
        readStr(*l, "background", launcher.background);
        readStr(*l, "footer", launcher.footer);
        readStr(*l, "playButton", launcher.playButton);
        readStr(*l, "playText", launcher.playText);
        readStr(*l, "settingsPanel", launcher.settingsPanel);
        readStr(*l, "metaPanel", launcher.metaPanel);
        readOptBool(*l, "metaPanelSlides", launcher.metaPanelSlides);
        readOptBool(*l, "textShadow", launcher.textShadow);
        readStr(*l, "arrow", launcher.arrow);
        if (const json *h = child(*l, "hints")) {
            readStr(*h, "cross", launcher.hints.cross);
            readStr(*h, "circle", launcher.hints.circle);
            readStr(*h, "triangle", launcher.hints.triangle);
        }
        if (const json *m = child(*l, "menuIcons")) {
            readStr(*m, "settings", launcher.menuIcons.settings);
            readStr(*m, "guide", launcher.menuIcons.guide);
            readStr(*m, "memcard", launcher.menuIcons.memcard);
            readStr(*m, "resume", launcher.menuIcons.resume);
        }
        if (const json *m = child(*l, "memcardManager")) {
            readStr(*m, "grid", launcher.memcardManager.grid);
            readStr(*m, "pencil", launcher.memcardManager.pencil);
        }
        if (const json *f = child(*l, "fonts")) {
            readStr(*f, "medium", launcher.fonts.medium);
            readStr(*f, "bold", launcher.fonts.bold);
        }
        if (const json *c = child(*l, "colors")) {
            readColor(*c, "text", launcher.colors.text);
            readColor(*c, "secondary", launcher.colors.secondary);
            readColor(*c, "hint", launcher.colors.hint);
        }
    }

    if (const json *s = child(j, "sounds")) {
        readStr(*s, "cursor", sounds.cursor);
        readStr(*s, "cancel", sounds.cancel);
        readStr(*s, "homeUp", sounds.homeUp);
        readStr(*s, "homeDown", sounds.homeDown);
        readStr(*s, "resume", sounds.resume);
    }
    return true;
}

//*******************************
// ThemeSpec::save
//*******************************
bool ThemeSpec::save(const string &path) const {
    ordered_json j = ordered_json::object();
    j["format"] = format;

    if (music.set) {
        if (music.none) {
            j["music"] = nullptr;
        } else {
            ordered_json m = ordered_json::object();
            m["file"] = music.file;
            m["loop"] = music.loop;
            j["music"] = m;
        }
    }

    {
        ordered_json c = ordered_json::object();
        putStr(c, "background", classic.background);
        {
            ordered_json l = ordered_json::object();
            putStr(l, "file", classic.logo.file);
            if (classic.logo.set) {
                l["x"] = classic.logo.x;
                l["y"] = classic.logo.y;
                l["w"] = classic.logo.w;
                l["h"] = classic.logo.h;
            }
            putObject(c, "logo", l);
        }
        {
            ordered_json f = ordered_json::object();
            putStr(f, "file", classic.font.file);
            putOptInt(f, "size", classic.font.size);
            putObject(c, "font", f);
        }
        putOptInt(c, "menuLines", classic.menuLines);
        putObject(c, "menuPanel", panelJson(classic.menuPanel));
        {
            ordered_json s = panelJson(classic.statusBar);
            putOptInt(s, "textY", classic.statusBar.textY);
            putObject(c, "statusBar", s);
        }
        putColor(c, "textColor", classic.textColor);
        if (classic.textShadow.set) c["textShadow"] = classic.textShadow.value;
        {
            ordered_json k = ordered_json::object();
            putColor(k, "color", classic.keyboardKey.color);
            putOptInt(k, "alpha", classic.keyboardKey.alpha);
            putObject(c, "keyboardKey", k);
        }
        putColor(c, "labelColor", classic.labelColor);
        putPoint(c, "freeSpaceText", classic.freeSpaceText);
        putPoint(c, "editorCover", classic.editorCover);
        {
            const auto &bt = classic.buttons;
            ordered_json b = ordered_json::object();
            putStr(b, "cross", bt.cross);
            putStr(b, "circle", bt.circle);
            putStr(b, "square", bt.square);
            putStr(b, "triangle", bt.triangle);
            putStr(b, "start", bt.start);
            putStr(b, "select", bt.select);
            putStr(b, "l1", bt.l1);
            putStr(b, "r1", bt.r1);
            putStr(b, "l2", bt.l2);
            putStr(b, "r2", bt.r2);
            putStr(b, "check", bt.check);
            putStr(b, "uncheck", bt.uncheck);
            putStr(b, "esc", bt.esc);
            putStr(b, "enter", bt.enter);
            putStr(b, "tab", bt.tab);
            putObject(c, "buttons", b);
        }
        putObject(j, "classic", c);
    }

    {
        ordered_json l = ordered_json::object();
        putStr(l, "background", launcher.background);
        putStr(l, "footer", launcher.footer);
        putStr(l, "playButton", launcher.playButton);
        putStr(l, "playText", launcher.playText);
        putStr(l, "settingsPanel", launcher.settingsPanel);
        putStr(l, "metaPanel", launcher.metaPanel);
        if (launcher.metaPanelSlides.set) l["metaPanelSlides"] = launcher.metaPanelSlides.value;
        if (launcher.textShadow.set) l["textShadow"] = launcher.textShadow.value;
        putStr(l, "arrow", launcher.arrow);
        {
            ordered_json h = ordered_json::object();
            putStr(h, "cross", launcher.hints.cross);
            putStr(h, "circle", launcher.hints.circle);
            putStr(h, "triangle", launcher.hints.triangle);
            putObject(l, "hints", h);
        }
        {
            ordered_json m = ordered_json::object();
            putStr(m, "settings", launcher.menuIcons.settings);
            putStr(m, "guide", launcher.menuIcons.guide);
            putStr(m, "memcard", launcher.menuIcons.memcard);
            putStr(m, "resume", launcher.menuIcons.resume);
            putObject(l, "menuIcons", m);
        }
        {
            ordered_json m = ordered_json::object();
            putStr(m, "grid", launcher.memcardManager.grid);
            putStr(m, "pencil", launcher.memcardManager.pencil);
            putObject(l, "memcardManager", m);
        }
        {
            ordered_json f = ordered_json::object();
            putStr(f, "medium", launcher.fonts.medium);
            putStr(f, "bold", launcher.fonts.bold);
            putObject(l, "fonts", f);
        }
        {
            ordered_json c = ordered_json::object();
            putColor(c, "text", launcher.colors.text);
            putColor(c, "secondary", launcher.colors.secondary);
            putColor(c, "hint", launcher.colors.hint);
            putObject(l, "colors", c);
        }
        putObject(j, "launcher", l);
    }

    {
        ordered_json s = ordered_json::object();
        putStr(s, "cursor", sounds.cursor);
        putStr(s, "cancel", sounds.cancel);
        putStr(s, "homeUp", sounds.homeUp);
        putStr(s, "homeDown", sounds.homeDown);
        putStr(s, "resume", sounds.resume);
        putObject(j, "sounds", s);
    }

    ofstream o(path, ofstream::binary);
    if (!DirEntry::checkWritable(o, path)) return false;
    o << setw(2) << j << "\n";
    o.flush();
    o.close();
    return o.good();
}

//*******************************
// ThemeSpec::mergeOver
//*******************************
void ThemeSpec::mergeOver(const ThemeSpec &base) {
    mergeSet(music, base.music);

    if (!classic.logo.set) {   // the rect; the file is merged with the other files below
        classic.logo.x = base.classic.logo.x; classic.logo.y = base.classic.logo.y;
        classic.logo.w = base.classic.logo.w; classic.logo.h = base.classic.logo.h;
        classic.logo.set = base.classic.logo.set;
    }
    mergeSet(classic.font.size, base.classic.font.size);
    mergeSet(classic.menuLines, base.classic.menuLines);
    mergePanel(classic.menuPanel, base.classic.menuPanel);
    mergePanel(classic.statusBar, base.classic.statusBar);
    mergeSet(classic.statusBar.textY, base.classic.statusBar.textY);
    mergeColor(classic.textColor, base.classic.textColor);
    mergeSet(classic.textShadow, base.classic.textShadow);
    mergeColor(classic.keyboardKey.color, base.classic.keyboardKey.color);
    mergeSet(classic.keyboardKey.alpha, base.classic.keyboardKey.alpha);
    mergeColor(classic.labelColor, base.classic.labelColor);
    mergeSet(classic.freeSpaceText, base.classic.freeSpaceText);
    mergeSet(classic.editorCover, base.classic.editorCover);

    mergeSet(launcher.metaPanelSlides, base.launcher.metaPanelSlides);
    mergeSet(launcher.textShadow, base.launcher.textShadow);
    mergeColor(launcher.colors.text, base.launcher.colors.text);
    mergeColor(launcher.colors.secondary, base.launcher.colors.secondary);
    mergeColor(launcher.colors.hint, base.launcher.colors.hint);

    // every file field, in one go
    vector<string *> mine = fileFields();
    vector<const string *> theirs = base.fileFields();
    for (size_t i = 0; i < mine.size(); i++)
        mergeStr(*mine[i], *theirs[i]);
    if (music.none) music.file = "";   // "music": null inherits no file
}

//*******************************
// ThemeSpec::resolveFiles
//*******************************
void ThemeSpec::resolveFiles(const string &dir, const ThemeSpec &fallback, const string &fallbackDir) {
    vector<string *> mine = fileFields();
    vector<const string *> theirs = fallback.fileFields();
    for (size_t i = 0; i < mine.size(); i++) {
        string &file = *mine[i];
        if (!file.empty() && DirEntry::exists(dir + sep + file)) {
            file = dir + sep + file;
        } else if (!theirs[i]->empty() && DirEntry::exists(fallbackDir + sep + *theirs[i])) {
            file = fallbackDir + sep + *theirs[i];
        } else {
            file = "";
        }
    }
}

//*******************************
// ThemeSpec::referencedFiles
//*******************************
vector<string> ThemeSpec::referencedFiles() const {
    vector<string> files;
    for (const string *f : fileFields())
        if (!f->empty()) files.push_back(*f);
    return files;
}

//*******************************
// ThemeSpec::fileFields
//*******************************
// The one list of file fields. Both overloads go through the mutable one so they cannot drift apart.
vector<string *> ThemeSpec::fileFields() {
    auto &b = classic.buttons;
    auto &l = launcher;
    return {
        &music.file,
        &classic.background, &classic.logo.file, &classic.font.file,
        &b.cross, &b.circle, &b.square, &b.triangle, &b.start, &b.select, &b.l1, &b.r1, &b.l2, &b.r2,
        &b.check, &b.uncheck, &b.esc, &b.enter, &b.tab,
        &l.background, &l.footer, &l.playButton, &l.playText, &l.settingsPanel, &l.metaPanel, &l.arrow,
        &l.hints.cross, &l.hints.circle, &l.hints.triangle,
        &l.menuIcons.settings, &l.menuIcons.guide, &l.menuIcons.memcard, &l.menuIcons.resume,
        &l.memcardManager.grid, &l.memcardManager.pencil,
        &l.fonts.medium, &l.fonts.bold,
        &sounds.cursor, &sounds.cancel, &sounds.homeUp, &sounds.homeDown, &sounds.resume,
    };
}

vector<const string *> ThemeSpec::fileFields() const {
    vector<string *> mutableFields = const_cast<ThemeSpec *>(this)->fileFields();
    return vector<const string *>(mutableFields.begin(), mutableFields.end());
}

} // namespace ableem
