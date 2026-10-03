// UIREV-9: "N <noun>" with a real singular for count == 1, everywhere the launcher shows a count of
// games or apps. `_()` is a flat English-text -> translation lookup (autobleem-core's ableem::Lang), so
// the simplest honest way to add a plural form is a second key: "game"/"app" for count == 1, "games"/
// "apps" otherwise, each translated independently - `tools/lang_tools.py extract` finds both keys here
// because the literal `_("game")` etc. calls stay in the source text. English and German (the languages
// this task's author reads) got a real singular in their lang files; every other language's new
// "game"/"app" key today holds the same text as its existing "games"/"apps" key (see
// src/resources/lang/*.txt), so a "1 ..." count reads exactly as it always has there until a translator
// who knows that language's plural grammar gives it a real one - this does not guess at grammar.
#pragma once

#include "core/main.h"

#include <string>

inline std::string pluralGames(size_t n) {
    return std::to_string(n) + " " + (n == 1 ? _("game") : _("games"));
}

inline std::string pluralApps(size_t n) {
    return std::to_string(n) + " " + (n == 1 ? _("app") : _("apps"));
}
