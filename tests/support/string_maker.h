//
// Teaches doctest to print the containers the tests compare, so a failure names the difference.
//
#pragma once

#include "doctest/doctest.h"

#include <sstream>
#include <string>
#include <vector>

// Without this a failed vector comparison reports `{?} == {?}`, which says nothing about what went wrong.
namespace doctest {

template <> struct StringMaker<std::vector<std::string>> {
    static String convert(const std::vector<std::string> &values) {
        std::ostringstream out;
        out << "{";
        for (size_t i = 0; i < values.size(); ++i) {
            if (i != 0)
                out << ", ";
            out << '"' << values[i] << '"';
        }
        out << "}";
        return out.str().c_str();
    }
};

template <> struct StringMaker<std::vector<int>> {
    static String convert(const std::vector<int> &values) {
        std::ostringstream out;
        out << "{";
        for (size_t i = 0; i < values.size(); ++i) {
            if (i != 0)
                out << ", ";
            out << values[i];
        }
        out << "}";
        return out.str().c_str();
    }
};

} // namespace doctest
