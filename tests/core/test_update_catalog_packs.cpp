// PackCatalog / PscRetroArchCatalog: the site's psc/<kind>/latest.json files as the installer reads them
#include <doctest/doctest.h>

#include <ableem/engine/update_catalog.h>

using namespace std;
using ableem::PackCatalog;
using ableem::PscRetroArchCatalog;

TEST_CASE("PackCatalog reads a dated pack's latest.json") {
    PackCatalog cores;
    REQUIRE(cores.parse(R"({"name": "cores-psc-20260920.tar.gz", "size": 123, "sha256": "ab", "date": "20260920",
                            "url": "https://site/psc/cores/cores-psc-20260920.tar.gz",
                            "manifest": "https://site/psc/cores/cores-psc-20260920.json", "count": 171})"));
    CHECK(cores.file.name == "cores-psc-20260920.tar.gz");
    CHECK(cores.file.size == 123);
    CHECK(cores.file.url == "https://site/psc/cores/cores-psc-20260920.tar.gz");
    CHECK(cores.manifestUrl == "https://site/psc/cores/cores-psc-20260920.json");
    CHECK(cores.date == "20260920");
    CHECK(cores.count == 171);

    // the BIOS list: no date, no manifest, the total of what it names
    PackCatalog bios;
    REQUIRE(bios.parse(
        R"({"name": "biospack.txt", "size": 158027, "sha256": "8b", "url": "https://site/psc/bios/biospack.txt",
                           "retrobios_ref": "73be130", "count": 719, "total_bytes": 316784588})"));
    CHECK(bios.count == 719);
    CHECK(bios.totalBytes == 316784588);
    CHECK(bios.date.empty());

    CHECK_FALSE(PackCatalog().parse("not json"));
    CHECK_FALSE(PackCatalog().parse(R"({"name": "x"})")); // no url/sha256
}

TEST_CASE("PscRetroArchCatalog reads psc/retroarch/latest.json") {
    PscRetroArchCatalog ra;
    REQUIRE(ra.parse(R"({"version": "v1.22.2-4", "zip": {"name": "retroarch-psc-v1.22.2-4.zip", "size": 5129326,
                         "sha256": "f0", "url": "https://site/psc/retroarch/v1.22.2-4/retroarch-psc-v1.22.2-4.zip"},
                         "manifest": "https://site/psc/retroarch/v1.22.2-4/manifest.json"})"));
    CHECK(ra.version == "v1.22.2-4");
    CHECK(ra.zip.name == "retroarch-psc-v1.22.2-4.zip");
    CHECK(ra.zip.size == 5129326);
    CHECK(ra.manifestUrl == "https://site/psc/retroarch/v1.22.2-4/manifest.json");
    CHECK_FALSE(PscRetroArchCatalog().parse(R"({"version": "v1"})"));
}
