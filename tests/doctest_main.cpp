// doctest's main(), compiled once into its own library so every test executable links it without paying
// the compile cost of the implementation again. It also brings the engine's log up, console only: the
// PLOG_* lines the engine writes are what a failing test's --output-on-failure shows, and a plog macro
// with no logger initialised is a silent no-op.
#define DOCTEST_CONFIG_IMPLEMENT
#include "doctest/doctest.h"

#include <ableem/engine/log.h>

int main(int argc, char **argv) {
    ableem::Log::initConsoleOnly();
    return doctest::Context(argc, argv).run();
}
