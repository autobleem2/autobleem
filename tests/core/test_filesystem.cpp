//
// DirEntry: the filesystem helpers the engine builds on. Only what has bitten so far is covered here.
//
#include "doctest/doctest.h"

#include "../support/temp_dir.h"

#include <ableem/engine/filesystem.h>

#include <string>

using ableem::DirEntry;
using std::string;

namespace {

// the .m3u is written in text mode, so on the Windows dev host its lines end in CRLF
string lines(const TempDir &tmp, const string &relative) {
    string text = tmp.readFile(relative), out;
    for (char c : text) {
        if (c != 13) out += c;   // CR
    }
    return out;
}

} // namespace

TEST_CASE("generateM3UForDirectory lists every disc image, sorted, under the first disc's base name") {
    TempDir tmp("m3u");
    tmp.makeSubDir("Game");
    tmp.writeFile("Game/Game (Disc 2).cue", "");
    tmp.writeFile("Game/Game (Disc 2).bin", "");
    tmp.writeFile("Game/Game (Disc 1).cue", "");
    tmp.writeFile("Game/Game (Disc 1).bin", "");

    DirEntry::generateM3UForDirectory(tmp.at("Game"), "Game (Disc 1)");
    CHECK(lines(tmp, "Game/Game (Disc 1).m3u") == "Game (Disc 1).cue\nGame (Disc 2).cue\n");
}

TEST_CASE("generateM3UForDirectory strips the image extension a PBP or CHD base name carries") {
    // the scanner names a PBP's / CHD's disc by its whole file name; the old code kept the *last four*
    // characters of a PBP name instead of dropping them, and did not list .chd files at all
    TempDir tmp("m3u");
    tmp.makeSubDir("Pbp");
    tmp.writeFile("Pbp/Pbp (Disc 1).pbp", "");
    tmp.writeFile("Pbp/Pbp (Disc 2).pbp", "");
    DirEntry::generateM3UForDirectory(tmp.at("Pbp"), "Pbp (Disc 1).pbp");
    CHECK(DirEntry::exists(tmp.at("Pbp/Pbp (Disc 1).m3u")));
    CHECK_FALSE(DirEntry::exists(tmp.at("Pbp/.pbp.m3u")));

    tmp.makeSubDir("Chd");
    tmp.writeFile("Chd/Chd (Disc 1).chd", "");
    tmp.writeFile("Chd/Chd (Disc 2).chd", "");
    DirEntry::generateM3UForDirectory(tmp.at("Chd"), "Chd (Disc 1).chd");
    CHECK(lines(tmp, "Chd/Chd (Disc 1).m3u") == "Chd (Disc 1).chd\nChd (Disc 2).chd\n");
}

TEST_CASE("generateM3UForDirectory writes nothing for a single disc and replaces a stale .m3u") {
    TempDir tmp("m3u");
    tmp.makeSubDir("Single");
    tmp.writeFile("Single/Single.cue", "");
    DirEntry::generateM3UForDirectory(tmp.at("Single"), "Single");
    CHECK_FALSE(DirEntry::exists(tmp.at("Single/Single.m3u")));

    tmp.makeSubDir("Renamed");
    tmp.writeFile("Renamed/Old Name.m3u", "stale\n");
    tmp.writeFile("Renamed/New (Disc 1).cue", "");
    tmp.writeFile("Renamed/New (Disc 2).cue", "");
    DirEntry::generateM3UForDirectory(tmp.at("Renamed"), "New (Disc 1)");
    CHECK_FALSE(DirEntry::exists(tmp.at("Renamed/Old Name.m3u")));
    CHECK(lines(tmp, "Renamed/New (Disc 1).m3u") == "New (Disc 1).cue\nNew (Disc 2).cue\n");
}
