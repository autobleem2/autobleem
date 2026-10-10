//
// ra_glibc_check.h (pure): the glibc versions a RetroArch binary names against the libc the console runs.
//
#include "doctest/doctest.h"

#include "ra_glibc_check.h"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
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

namespace {

void put(string &b, uint64_t v, int width) {
    for (int i = 0; i < width; ++i)
        b.push_back(static_cast<char>((v >> (8 * i)) & 0xff));
}

// a tiny ELF: header, then the string table and the version needs, then three section headers
// (null, .dynstr, .gnu.version_r linked to it). Names: libc.so.6 with GLIBC_2.17, GLIBC_2.28, GLIBC_PRIVATE, and
// a text "GLIBC_2.99" in a place the loader does not look at.
string tinyElf(bool is64, bool bigEndian = false) {
    const string strings = string("\0libc.so.6\0GLIBC_2.17\0GLIBC_2.28\0GLIBC_PRIVATE\0", 47);
    // offsets in strings: libc.so.6 = 1, 2.17 = 11, 2.28 = 22, PRIVATE = 33
    const size_t headerSize = is64 ? 64 : 52;
    const size_t shentsize = is64 ? 64 : 40;
    const size_t stringsAt = headerSize;
    const size_t needAt = stringsAt + strings.size();
    string needs;
    put(needs, 1, 2); // vn_version
    put(needs, 3, 2); // vn_cnt
    put(needs, 1, 4); // vn_file
    put(needs, 16, 4); // vn_aux
    put(needs, 0, 4); // vn_next
    const uint32_t nameAt[3] = {11, 22, 33};
    for (int i = 0; i < 3; ++i) {
        put(needs, 0, 4); // vna_hash
        put(needs, 0, 2); // vna_flags
        put(needs, 2, 2); // vna_other
        put(needs, nameAt[i], 4);
        put(needs, i == 2 ? 0 : 16, 4);
    }
    const size_t shoff = needAt + needs.size();

    string f;
    f += string("\x7f" "ELF", 4);
    f.push_back(is64 ? 2 : 1);
    f.push_back(bigEndian ? 2 : 1);
    f.push_back(1);
    f.append(9, '\0');
    put(f, 3, 2); // e_type
    put(f, 0x28, 2); // e_machine
    put(f, 1, 4); // e_version
    put(f, 0, is64 ? 8 : 4); // e_entry
    put(f, 0, is64 ? 8 : 4); // e_phoff
    put(f, shoff, is64 ? 8 : 4); // e_shoff
    put(f, 0, 4); // e_flags
    put(f, headerSize, 2);
    put(f, 0, 2); // e_phentsize
    put(f, 0, 2); // e_phnum
    put(f, shentsize, 2);
    put(f, 3, 2); // e_shnum
    put(f, 0, 2); // e_shstrndx
    f += strings;
    f += needs;
    f += "GLIBC_2.99"; // after the needs: only a file scan would find it
    f.resize(shoff, '\0');
    auto header = [&](uint32_t type, uint64_t offset, uint64_t size, uint32_t link) {
        const size_t start = f.size();
        put(f, 0, 4); // sh_name
        put(f, type, 4);
        put(f, 0, is64 ? 8 : 4); // sh_flags
        put(f, 0, is64 ? 8 : 4); // sh_addr
        put(f, offset, is64 ? 8 : 4);
        put(f, size, is64 ? 8 : 4);
        put(f, link, 4);
        put(f, 0, 4); // sh_info
        f.resize(start + shentsize, '\0');
    };
    header(0, 0, 0, 0);
    header(3, stringsAt, strings.size(), 0); // SHT_STRTAB
    header(0x6ffffffe, needAt, needs.size(), 1); // SHT_GNU_verneed, linked to the strings
    return f;
}

// a file in the temp folder; the caller removes it
string writeTemp(const char *name, const string &bytes) {
    const char *folder = std::getenv("TMPDIR");
    const string path = string(folder != nullptr && *folder != '\0' ? folder : "/tmp") + "/" + name;
    std::ofstream out(path, std::ios::binary);
    out.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    return path;
}

GlibcVersion checkBytes(const char *name, const string &bytes) {
    const string path = writeTemp(name, bytes);
    const GlibcVersion v = highestGlibcNeededInFile(path);
    std::remove(path.c_str());
    return v;
}

} // namespace

TEST_CASE("a 64 bit ELF names the highest GLIBC_ version of its version needs") {
    const GlibcVersion v = checkBytes("ab_ra_glibc_elf64.bin", tinyElf(true));
    CHECK(v.major == 2);
    CHECK(v.minor == 28);
}

TEST_CASE("a 32 bit ELF names the highest GLIBC_ version of its version needs") {
    const GlibcVersion v = checkBytes("ab_ra_glibc_elf32.bin", tinyElf(false));
    CHECK(v.major == 2);
    CHECK(v.minor == 28);
}

TEST_CASE("a big-endian ELF is unknown") {
    CHECK_FALSE(checkBytes("ab_ra_glibc_elfbe.bin", tinyElf(true, true)).known());
}

TEST_CASE("a file that is not an ELF is unknown, even with GLIBC_ text in it") {
    CHECK_FALSE(checkBytes("ab_ra_glibc_text.bin", string(200, 'x') + "GLIBC_2.28").known());
    CHECK_FALSE(checkBytes("ab_ra_glibc_empty.bin", "").known());
}

TEST_CASE("a truncated ELF is unknown and does not crash") {
    for (bool is64 : {true, false}) {
        const string whole = tinyElf(is64);
        for (size_t keep = 0; keep < whole.size(); keep += 7) {
            const GlibcVersion v = checkBytes("ab_ra_glibc_cut.bin", whole.substr(0, keep));
            CHECK_FALSE(v.known());
        }
    }
}

TEST_CASE("a second look at an unchanged file does not read it again, a changed one is read") {
    const string whole = tinyElf(true);
    const string path = writeTemp("ab_ra_glibc_cache.bin", whole);
    const int before = glibcFileParses();
    CHECK(highestGlibcNeededInFile(path).minor == 28);
    CHECK(highestGlibcNeededInFile(path).minor == 28);
    CHECK(glibcFileParses() == before + 1);
    writeTemp("ab_ra_glibc_cache.bin", whole.substr(0, 40)); // other size
    CHECK_FALSE(highestGlibcNeededInFile(path).known());
    CHECK(glibcFileParses() == before + 2);
    std::remove(path.c_str());
}

TEST_CASE("a missing file is unknown, so the program is let start") {
    CHECK_FALSE(highestGlibcNeededInFile("/no/such/retroarch").known());
}
