//
// SevenZipArchive: the vendored 7z reader over small archives made with 7-Zip 25 - LZMA2 (7z's default),
// BCJ2 + LZMA (what an archive of Windows executables gets, RetroArch.7z included) and PPMd. Each holds
// hello.txt, sub/pattern.txt (3000 lines), sub/zeros.bin (200000 zero bytes) and sub/tool.exe.
//
#include "doctest/doctest.h"

#include "../support/temp_dir.h"

#include <ableem/engine/filesystem.h>
#include <ableem/engine/seven_zip_archive.h>

#include <string>
#include <vector>

using ableem::DirEntry;
using ableem::SevenZipArchive;
using ableem::SevenZipEntry;
using std::string;
using std::vector;

namespace {
const string dataDir = AB_TEST_DATA_DIR;

bool hasEntry(const vector<SevenZipEntry> &entries, const string &name, uint64_t size) {
    for (const SevenZipEntry &e : entries)
        if (e.name == name && e.size == size)
            return true;
    return false;
}
} // namespace

TEST_CASE("list names every entry with its unpacked size, directories with a slash") {
    vector<SevenZipEntry> entries;
    REQUIRE(SevenZipArchive::list(dataDir + "/test_lzma2.7z", entries));
    CHECK(entries.size() == 5);
    CHECK(hasEntry(entries, "hello.txt", 9));
    CHECK(hasEntry(entries, "sub/", 0));
    CHECK(hasEntry(entries, "sub/zeros.bin", 200000));
    CHECK(hasEntry(entries, "sub/tool.exe", 2 + 256 * 40));
}

TEST_CASE("extract unpacks every method 7-Zip uses for a program folder") {
    for (const char *name : {"test_lzma2.7z", "test_bcj2.7z", "test_ppmd.7z"}) {
        CAPTURE(name);
        TempDir tmp("seven_zip");
        string error;
        uint64_t lastDone = 0, lastTotal = 0;
        REQUIRE_MESSAGE(SevenZipArchive::extract(dataDir + "/" + name, tmp.at("out"), error, SevenZipArchive::Filter(),
                                                 [&](uint64_t done, uint64_t total) {
                                                     lastDone = done;
                                                     lastTotal = total;
                                                 }),
                        error);
        CHECK(tmp.readFile("out/hello.txt") == "hello 7z\n");
        CHECK(DirEntry::fileSize(tmp.at("out/sub/zeros.bin")) == 200000);
        string pattern = tmp.readFile("out/sub/pattern.txt");
        CHECK(pattern.compare(0, 26, "line 0 of the pattern file") == 0); // (CRLF lines - made on Windows)
        CHECK(pattern.find("line 2999 of the pattern file") != string::npos);
        string exe = tmp.readFile("out/sub/tool.exe");
        CHECK(exe.size() == 2 + 256 * 40);
        CHECK(exe.compare(0, 2, "MZ") == 0);
        CHECK(static_cast<unsigned char>(exe[2 + 255]) == 255);
        CHECK(lastTotal == 9 + 200000 + (2 + 256 * 40) + DirEntry::fileSize(tmp.at("out/sub/pattern.txt")));
        CHECK(lastDone == lastTotal);
    }
}

TEST_CASE("a filter and a prefix pick a folder out of the archive") {
    TempDir tmp("seven_zip_prefix");
    string error;
    REQUIRE(SevenZipArchive::extract(
        dataDir + "/test_lzma2.7z", tmp.at("out"), error,
        [](const SevenZipEntry &e) { return e.name != "sub/zeros.bin"; }, SevenZipArchive::Progress(), "sub/"));
    CHECK(DirEntry::exists(tmp.at("out/pattern.txt")));
    CHECK(DirEntry::exists(tmp.at("out/tool.exe")));
    CHECK_FALSE(DirEntry::exists(tmp.at("out/zeros.bin")));
    CHECK_FALSE(DirEntry::exists(tmp.at("out/hello.txt")));
    CHECK_FALSE(DirEntry::exists(tmp.at("out/sub")));
}

TEST_CASE("not a 7z: false with a reason, nothing written") {
    TempDir tmp("seven_zip_bad");
    tmp.writeFile("bad.7z", "this is not an archive at all, not even close");
    vector<SevenZipEntry> entries;
    CHECK_FALSE(SevenZipArchive::list(tmp.at("bad.7z"), entries));
    string error;
    CHECK_FALSE(SevenZipArchive::extract(tmp.at("bad.7z"), tmp.at("out"), error));
    CHECK_FALSE(error.empty());
    CHECK_FALSE(SevenZipArchive::extract(tmp.at("missing.7z"), tmp.at("out"), error));
    CHECK(error.find("cannot open") != string::npos);
}
