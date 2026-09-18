//
// ZipWriter (the write side of the vendored miniz) and Md5::ofFile - what abflashkit's backup is made of.
//
#include "doctest/doctest.h"

#include "../support/temp_dir.h"

#include <ableem/engine/md5.h>
#include <ableem/engine/zip_archive.h>
#include <ableem/engine/zip_writer.h>

#include <algorithm>
#include <string>
#include <vector>

using std::string;
using std::vector;

TEST_CASE("a zip written entry by entry lists and extracts back to the same bytes") {
    TempDir tmp("zip_writer");
    // one file bigger than miniz's 64 KB read buffer, so the streamed path takes several chunks
    string big(200 * 1024, 'x');
    for (size_t i = 0; i < big.size(); i += 7)
        big[i] = static_cast<char>('a' + i % 26);
    tmp.writeFile("big.bin", big);
    tmp.writeFile("small.txt", "hello");

    string zipPath = tmp.at("out.zip");
    {
        ableem::ZipWriter writer;
        REQUIRE(writer.open(zipPath));
        CHECK(writer.isOpen());
        CHECK(writer.addFile(tmp.at("big.bin"), "images/big.bin"));
        CHECK(writer.addFile(tmp.at("small.txt"), "small.txt"));
        CHECK(writer.addBytes("note.txt", "from memory"));
        CHECK(writer.close());
        CHECK_FALSE(writer.isOpen());
    }

    vector<string> names;
    REQUIRE(ableem::ZipArchive::list(zipPath, names));
    CHECK(std::find(names.begin(), names.end(), "images/big.bin") != names.end());
    CHECK(std::find(names.begin(), names.end(), "small.txt") != names.end());
    CHECK(std::find(names.begin(), names.end(), "note.txt") != names.end());

    string dest = tmp.makeSubDir("unpacked");
    REQUIRE(ableem::ZipArchive::extract(zipPath, dest));
    CHECK(tmp.readFile("unpacked/images/big.bin") == big);
    CHECK(tmp.readFile("unpacked/small.txt") == "hello");
    CHECK(tmp.readFile("unpacked/note.txt") == "from memory");
}

TEST_CASE("ZipWriter refuses what it cannot do and says so") {
    TempDir tmp("zip_writer_bad");
    ableem::ZipWriter writer;
    CHECK_FALSE(writer.addBytes("x", "y")); // not open
    CHECK_FALSE(writer.close());
    CHECK_FALSE(writer.open(tmp.at("no/such/dir/out.zip")));
    REQUIRE(writer.open(tmp.at("out.zip")));
    CHECK_FALSE(writer.addFile(tmp.at("missing.bin"), "missing.bin"));
    CHECK(writer.close());
}

TEST_CASE("Md5 of a file streams and matches the string digest") {
    TempDir tmp("md5");
    CHECK(ableem::Md5::ofString("") == "d41d8cd98f00b204e9800998ecf8427e");
    CHECK(ableem::Md5::ofString("The quick brown fox jumps over the lazy dog") == "9e107d9d372bb6826bd81d3542a419d6");
    string big(150 * 1024, 'q');
    tmp.writeFile("big.bin", big);
    CHECK(ableem::Md5::ofFile(tmp.at("big.bin")) == ableem::Md5::ofString(big));
    CHECK(ableem::Md5::ofFile(tmp.at("absent.bin")).empty());
}
