//
// launch_picture.h (pure): the picture shown while a program starts.
//
#include "doctest/doctest.h"

#include "launch_picture.h"

#include <string>

using std::string;

TEST_CASE("an App gets AutoBleem's picture, not RetroArch's (it is a foreign entry like a RetroArch game)") {
    CHECK(string(waitingPictureName(true, true, false)) == "autobleem.jpg");
}

TEST_CASE("a RetroArch game gets RetroArch's picture") {
    CHECK(string(waitingPictureName(true, false, false)) == "retroarch.jpg");
}

TEST_CASE("a PS1 game gets AutoBleem's picture, another emulator RetroArch's") {
    CHECK(string(waitingPictureName(false, false, false)) == "autobleem.jpg");
    CHECK(string(waitingPictureName(false, false, true)) == "retroarch.jpg");
    CHECK(string(waitingPictureName(true, false, true)) == "retroarch.jpg");
}
