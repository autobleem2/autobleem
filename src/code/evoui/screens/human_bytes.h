//
// "42.1 MB": a byte count for a status line. Pure.
//
#pragma once

#include <cstdint>
#include <cstdio>
#include <string>

inline std::string humanBytes(uint64_t bytes) {
    char buf[32];
    if (bytes >= 1000000)
        snprintf(buf, sizeof(buf), "%.1f MB", bytes / 1000000.0);
    else if (bytes >= 1000)
        snprintf(buf, sizeof(buf), "%.0f KB", bytes / 1000.0);
    else
        snprintf(buf, sizeof(buf), "%u B", static_cast<unsigned>(bytes));
    return buf;
}
