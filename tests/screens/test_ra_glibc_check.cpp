//
// ra_glibc_check.h (pure): the glibc versions a RetroArch binary names against the libc the console runs.
//
#include "doctest/doctest.h"

#include "ra_glibc_check.h"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>

using std::string;

namespace {

// what a binary's dynamic string table looks like around the version names
string fakeBinary(const string &names) {
    return "ELF libm.so.6 " + names + " __libc_start_main";
}

} // namespace

TEST_CASE("a libc version string is read to major and minor") {
    CHECK(parseGlibcVersion("2.28").major == 2);
    CHECK(parseGlibcVersion("2.28").minor == 28);
    CHECK(parseGlibcVersion("2.31.9000").minor == 31);
    CHECK_FALSE(parseGlibcVersion("").known());
    CHECK_FALSE(parseGlibcVersion("musl").known());
    CHECK_FALSE(parseGlibcVersion("2").known());
    CHECK_FALSE(parseGlibcVersion("2.").known());
}

TEST_CASE("the highest GLIBC_ name in the bytes is the one needed") {
    const GlibcVersion v = highestGlibcNeeded(fakeBinary("GLIBC_2.4 GLIBC_2.28 GLIBC_2.17 GLIBC_PRIVATE"));
    CHECK(v.major == 2);
    CHECK(v.minor == 28);
}

TEST_CASE("minor versions compare as numbers, not as text") {
    const GlibcVersion v = highestGlibcNeeded("GLIBC_2.9 GLIBC_2.10");
    CHECK(v.minor == 10);
}

TEST_CASE("a three-part name counts by its first two numbers") {
    CHECK(highestGlibcNeeded("GLIBC_2.2.5").minor == 2);
}

TEST_CASE("bytes with no versioned name are unknown, and so are GLIBC_PRIVATE and a bare tag") {
    CHECK_FALSE(highestGlibcNeeded("").known());
    CHECK_FALSE(highestGlibcNeeded("just some text, GLIBC_PRIVATE and GLIBC_").known());
}

TEST_CASE("a program needing a newer glibc than the console's is flagged") {
    const GlibcVersion console = parseGlibcVersion("2.23");
    CHECK(needsNewerGlibc(parseGlibcVersion("2.28"), console));
    CHECK_FALSE(needsNewerGlibc(parseGlibcVersion("2.23"), console));
    CHECK_FALSE(needsNewerGlibc(parseGlibcVersion("2.17"), console));
    CHECK(needsNewerGlibc(parseGlibcVersion("3.0"), console));
}

TEST_CASE("nothing is flagged when either side is unknown") {
    CHECK_FALSE(needsNewerGlibc(GlibcVersion(), parseGlibcVersion("2.23")));
    CHECK_FALSE(needsNewerGlibc(parseGlibcVersion("2.28"), GlibcVersion()));
}

TEST_CASE("a file is read in pieces and a name cut by a piece border is still found") {
    const std::filesystem::path path = std::filesystem::temp_directory_path() / "ab_ra_glibc_check.bin";
    {
        std::ofstream out(path, std::ios::binary);
        // the name starts 5 bytes before the 1 MiB border
        const string padding((1 << 20) - 5, 'x');
        out << padding << "GLIBC_2.28" << string(100, 'y');
    }
    const GlibcVersion v = highestGlibcNeededInFile(path.string());
    std::remove(path.string().c_str());
    CHECK(v.major == 2);
    CHECK(v.minor == 28);
}

TEST_CASE("a missing file is unknown, so the program is let start") {
    CHECK_FALSE(highestGlibcNeededInFile("/no/such/retroarch").known());
}
