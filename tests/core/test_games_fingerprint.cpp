//
// GamesFingerprint: a cheap size-only snapshot of the games directory, used by ScanService to notice a
// change without doing a full scan. No mtime on purpose - see the header comment.
//
#include "doctest/doctest.h"

#include "../support/temp_dir.h"

#include <ableem/engine/games_fingerprint.h>

using ableem::GamesFingerprint;
using std::string;

TEST_CASE("an unchanged games directory fingerprints the same both times") {
    TempDir tmp("fp");
    tmp.writeFile("Crash Bandicoot/Crash Bandicoot.bin", "bin-contents");
    tmp.writeFile("Crash Bandicoot/Crash Bandicoot.cue", "cue-contents");

    GamesFingerprint a = GamesFingerprint::take(tmp.path());
    GamesFingerprint b = GamesFingerprint::take(tmp.path());
    CHECK(a == b);
}

TEST_CASE("a new game directory changes the fingerprint") {
    TempDir tmp("fp");
    tmp.writeFile("Crash Bandicoot/Crash Bandicoot.bin", "bin-contents");

    GamesFingerprint before = GamesFingerprint::take(tmp.path());
    tmp.writeFile("Spyro/Spyro.bin", "bin-contents");
    GamesFingerprint after = GamesFingerprint::take(tmp.path());

    CHECK(before != after);
}

TEST_CASE("a removed game directory changes the fingerprint") {
    TempDir tmp("fp");
    tmp.writeFile("Crash Bandicoot/Crash Bandicoot.bin", "bin-contents");
    tmp.writeFile("Spyro/Spyro.bin", "bin-contents");
    GamesFingerprint before = GamesFingerprint::take(tmp.path());

    ableem::DirEntry::removeDirAndContents(tmp.at("Spyro"));
    GamesFingerprint after = GamesFingerprint::take(tmp.path());

    CHECK(before != after);
}

TEST_CASE("a resized game file changes the fingerprint") {
    TempDir tmp("fp");
    tmp.writeFile("Crash Bandicoot/Crash Bandicoot.bin", "short");
    GamesFingerprint before = GamesFingerprint::take(tmp.path());

    tmp.writeFile("Crash Bandicoot/Crash Bandicoot.bin", "a much longer replacement");
    GamesFingerprint after = GamesFingerprint::take(tmp.path());

    CHECK(before != after);
}

TEST_CASE("a renamed game directory changes the fingerprint even though its contents did not") {
    TempDir tmp("fp");
    tmp.writeFile("Crash Bandicoot/Crash Bandicoot.bin", "bin-contents");
    GamesFingerprint before = GamesFingerprint::take(tmp.path());

    ableem::DirEntry::renameFile(tmp.at("Crash Bandicoot"), tmp.at("Crash Bandicoot 2"));
    GamesFingerprint after = GamesFingerprint::take(tmp.path());

    CHECK(before != after);
}

TEST_CASE("editing Game.ini does not change the fingerprint") {
    TempDir tmp("fp");
    tmp.writeFile("Crash Bandicoot/Crash Bandicoot.bin", "bin-contents");
    tmp.writeFile("Crash Bandicoot/Game.ini", "[Game]\nAutomation=1\n");
    GamesFingerprint before = GamesFingerprint::take(tmp.path());

    tmp.writeFile("Crash Bandicoot/Game.ini", "[Game]\nAutomation=0\nFavorite=1\n");
    GamesFingerprint after = GamesFingerprint::take(tmp.path());

    CHECK(before == after);
}

TEST_CASE("!SaveStates and !MemCards are not part of the fingerprint") {
    TempDir tmp("fp");
    tmp.writeFile("Crash Bandicoot/Crash Bandicoot.bin", "bin-contents");
    GamesFingerprint before = GamesFingerprint::take(tmp.path());

    tmp.writeFile("!SaveStates/Crash Bandicoot/slot0.state", "some save data");
    tmp.writeFile("!MemCards/card1.mcd", "some card data");
    GamesFingerprint after = GamesFingerprint::take(tmp.path());

    CHECK(before == after);
}

TEST_CASE("GamesFingerprint::save/load roundtrips") {
    TempDir tmp("fp");
    tmp.writeFile("Crash Bandicoot/Crash Bandicoot.bin", "bin-contents");
    tmp.writeFile("Spyro/Spyro.bin", "other-contents");
    GamesFingerprint original = GamesFingerprint::take(tmp.path());

    string savedTo = tmp.at("games.fingerprint");
    REQUIRE(original.save(savedTo));

    GamesFingerprint loaded;
    REQUIRE(loaded.load(savedTo));
    CHECK(loaded == original);
}

TEST_CASE("loading a missing fingerprint file fails and leaves it comparable to an empty scan") {
    TempDir tmp("fp");
    GamesFingerprint loaded;
    CHECK_FALSE(loaded.load(tmp.at("does_not_exist")));
}
