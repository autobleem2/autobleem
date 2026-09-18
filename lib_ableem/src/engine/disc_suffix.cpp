#include "ableem/engine/disc_suffix.h"

#include <cctype>

namespace ableem {

namespace {

bool isSpace(char c) {
    return std::isspace(static_cast<unsigned char>(c)) != 0;
}

// Match `keyword` against the start of `s` case-insensitively. On success,
// returns the offset past the keyword; on failure, returns 0.
size_t matchKeyword(const std::string &s, size_t pos, const char *keyword) {
    size_t i = 0;
    while (keyword[i] != '\0') {
        if (pos + i >= s.size())
            return 0;
        char a = static_cast<char>(std::tolower(static_cast<unsigned char>(s[pos + i])));
        if (a != keyword[i])
            return 0;
        i++;
    }
    return pos + i;
}

// Try to parse "(Disc|Disk|CD)[ ]N" starting at `pos`, ending at `endExclusive`.
// On success returns the disc number; otherwise returns 0.
int parseKeywordAndNumber(const std::string &s, size_t pos, size_t endExclusive) {
    // Tolerate leading whitespace inside the marker, e.g. "( Disc 1 )".
    while (pos < endExclusive && isSpace(s[pos]))
        pos++;

    size_t after = matchKeyword(s, pos, "disc");
    if (after == 0)
        after = matchKeyword(s, pos, "disk");
    if (after == 0)
        after = matchKeyword(s, pos, "cd");
    if (after == 0)
        return 0;

    // Skip whitespace between keyword and number.
    while (after < endExclusive && isSpace(s[after]))
        after++;

    int n = 0;
    bool sawDigit = false;
    while (after < endExclusive && std::isdigit(static_cast<unsigned char>(s[after]))) {
        n = n * 10 + (s[after] - '0');
        after++;
        sawDigit = true;
    }
    if (!sawDigit)
        return 0;

    // Allow trailing whitespace only.
    while (after < endExclusive && isSpace(s[after]))
        after++;
    if (after != endExclusive)
        return 0;
    return n;
}

// Trim trailing whitespace; return the new end index (exclusive).
size_t rtrimEnd(const std::string &s, size_t end) {
    while (end > 0 && isSpace(s[end - 1]))
        end--;
    return end;
}

} // namespace

//*******************************
// DiscSuffix::parse
//*******************************
DiscSuffix DiscSuffix::parse(const std::string &name) {
    DiscSuffix result;
    result.base = name;

    const size_t end = rtrimEnd(name, name.size());
    if (end == 0)
        return result;

    // Form 1: "... (Disc N)" — locate the opening paren of the final group.
    if (name[end - 1] == ')') {
        size_t open = std::string::npos;
        for (size_t i = end - 1; i-- > 0;) {
            if (name[i] == '(') {
                open = i;
                break;
            }
            if (name[i] == ')')
                return result; // nested ')' before '(' — bail
        }
        if (open != std::string::npos) {
            const int n = parseKeywordAndNumber(name, open + 1, end - 1);
            if (n > 0) {
                result.base.assign(name, 0, rtrimEnd(name, open));
                result.disc = n;
                return result;
            }
        }
    }

    // Form 2: "... - Disc N" / "... - Disk N" / "... - CD N"
    for (size_t i = end; i-- > 0;) {
        if (name[i] != '-')
            continue;
        size_t start = i + 1;
        while (start < end && isSpace(name[start]))
            start++;
        const int n = parseKeywordAndNumber(name, start, end);
        if (n > 0) {
            result.base.assign(name, 0, rtrimEnd(name, i));
            result.disc = n;
            return result;
        }
        // Only check the rightmost '-'; if it didn't yield a disc match,
        // assume there's no disc marker.
        break;
    }

    return result;
}

} // namespace ableem
