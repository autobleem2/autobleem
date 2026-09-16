// doctest's main(), compiled once into its own library so every test executable links it without paying
// the compile cost of the implementation again.
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest/doctest.h"
