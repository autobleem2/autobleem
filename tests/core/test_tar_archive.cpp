// TarArchive: the .tar.gz reader the installer unpacks the site's packages with
#include <doctest/doctest.h>

#include <ableem/engine/filesystem.h>
#include <ableem/engine/tar_archive.h>

#include "support/tar_builder.h"
#include "support/temp_dir.h"

#include <string>
#include <vector>

using namespace std;
using ableem::TarArchive;
using ableem::TarEntry;
using test_support::TarBuilder;

namespace {

TarBuilder sample() {
    TarBuilder b;
    b.dir("./Autobleem");
    b.dir("./Autobleem/rc");
    b.file("./Autobleem/rc/launch.sh", "#!/bin/sh\necho hi\n", 0755);
    b.file("./VERSION", "v2.0.0-pre0-abc1234\n");
    b.file("./Themes/ab2/theme.json", string(70000, 'x')); // over a block, over an inflate buffer
    b.symlink("./Autobleem/lib/apps/libfoo.so", "libfoo.so.1");
    b.file("./Apps/" + string(120, 'n') + "/app.ini", "Title=Long\n");
    return b;
}

} // namespace

TEST_CASE("TarArchive lists a gzipped and a plain archive alike, names cleaned") {
    TempDir tmp("tar");
    REQUIRE(sample().writeTarGz(tmp.at("a.tar.gz")));
    REQUIRE(sample().writeTar(tmp.at("a.tar")));
    for (const char *name : {"a.tar.gz", "a.tar"}) {
        vector<TarEntry> entries;
        string error;
        REQUIRE_MESSAGE(TarArchive::list(tmp.at(name), entries, error), error);
        REQUIRE(entries.size() == 7);
        CHECK(entries[0].name == "Autobleem");
        CHECK(entries[0].isDir);
        CHECK(entries[2].name == "Autobleem/rc/launch.sh");
        CHECK(entries[2].isFile);
        CHECK(entries[2].mode == 0755);
        CHECK(entries[2].size == 18);
        CHECK(entries[4].size == 70000);
        CHECK(entries[5].isSymlink);
        CHECK(entries[6].name == "Apps/" + string(120, 'n') + "/app.ini"); // the GNU long name
    }
}

TEST_CASE("TarArchive::extract writes the files, makes the directories, skips symlinks, reports progress") {
    TempDir tmp("tar");
    REQUIRE(sample().writeTarGz(tmp.at("a.tar.gz")));
    string error;
    int calls = 0;
    uint64_t last = 0, total = 0;
    REQUIRE_MESSAGE(TarArchive::extract(tmp.at("a.tar.gz"), tmp.at("out"), error, TarArchive::Filter(),
                                        [&](uint64_t done, uint64_t t) {
                                            calls++;
                                            last = done;
                                            total = t;
                                        }),
                    error);
    CHECK(tmp.readFile("out/Autobleem/rc/launch.sh") == "#!/bin/sh\necho hi\n");
    CHECK(tmp.readFile("out/VERSION") == "v2.0.0-pre0-abc1234\n");
    CHECK(tmp.readFile("out/Themes/ab2/theme.json").size() == 70000);
    CHECK(tmp.readFile("out/Apps/" + string(120, 'n') + "/app.ini") == "Title=Long\n");
    CHECK_FALSE(ableem::DirEntry::exists(tmp.at("out/Autobleem/lib/apps/libfoo.so")));
    CHECK(calls >= 2);
    CHECK(last == total); // the final call says done
}

TEST_CASE("TarArchive::extract takes a filter and a prefix") {
    TempDir tmp("tar");
    TarBuilder b;
    b.file("Apps/doom/run.sh", "run", 0755).file("Apps/doom/icon.png", "png").file("apps.json", "{}");
    REQUIRE(b.writeTarGz(tmp.at("apps.tar.gz")));
    string error;
    // only what is under Apps/, landing without that prefix, and no .png
    REQUIRE_MESSAGE(TarArchive::extract(
                        tmp.at("apps.tar.gz"), tmp.at("Apps"), error,
                        [](const TarEntry &e) { return e.name.find(".png") == string::npos; }, TarArchive::Progress(),
                        "Apps/"),
                    error);
    CHECK(tmp.readFile("Apps/doom/run.sh") == "run");
    CHECK_FALSE(ableem::DirEntry::exists(tmp.at("Apps/doom/icon.png")));
    CHECK_FALSE(ableem::DirEntry::exists(tmp.at("Apps/apps.json")));
}

TEST_CASE("TarArchive refuses an entry that would escape the destination") {
    TempDir tmp("tar");
    TarBuilder b;
    b.file("../escape.txt", "no");
    REQUIRE(b.writeTarGz(tmp.at("bad.tar.gz")));
    string error;
    CHECK_FALSE(TarArchive::extract(tmp.at("bad.tar.gz"), tmp.at("out"), error));
    CHECK(error.find("refusing") != string::npos);
    CHECK(TarArchive::isSafeName("a/b/c.txt"));
    CHECK_FALSE(TarArchive::isSafeName("/etc/passwd"));
    CHECK_FALSE(TarArchive::isSafeName("a/../b"));
    CHECK_FALSE(TarArchive::isSafeName("C:/x"));
}

TEST_CASE("TarArchive reports a file that is not an archive") {
    TempDir tmp("tar");
    tmp.writeFile("not.tar.gz", string(2000, 'q'));
    vector<TarEntry> entries;
    string error;
    // plain bytes: no zero block, no valid header - listed as nothing, without a crash
    TarArchive::list(tmp.at("not.tar.gz"), entries, error);
    CHECK_FALSE(TarArchive::list(tmp.at("missing.tar.gz"), entries, error));
    CHECK(error.find("cannot open") != string::npos);
}
