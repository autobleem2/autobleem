//
// The Memory Cards list's counter (BUG-41): "Card 3/12" - and "0/0" when there are no cards, not "Card 1/0".
// Pure, so tests/screens/test_memcard_counter holds it.
//
#pragma once

#include <string>

namespace MemcardCounter {

// selected: the cursor's 0-based row; count: how many cards there are
inline std::string text(const std::string &label, int selected, int count) {
    if (count <= 0)
        return "0/0";
    return label + " " + std::to_string(selected + 1) + "/" + std::to_string(count);
}

} // namespace MemcardCounter
