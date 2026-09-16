//
// ableem::MemcardImage: a PlayStation memory card image and its fifteen save slots.
//
#include "doctest/doctest.h"

#include "../support/string_maker.h"
#include "../support/temp_dir.h"

#include <ableem/engine/memcard_image.h>

#include <cstring>
#include <string>
#include <vector>

using ableem::MemcardImage;
using std::string;
using std::vector;

namespace {

const int Frame = 0x80;
const int Block = 0x2000;

// A card built in memory. Slot layout (see memcard_image.h): a directory frame per slot after the header
// frame, a block per slot after the first block.
struct CardBytes {
    CardBytes() : bytes(MemcardImage::Size, 0) {
        // every slot free
        for (int slot = 0; slot < MemcardImage::Slots; slot++) {
            dir(slot)[0] = 0xA0;
            dir(slot)[8] = 0xFF;
            checksum(slot);
        }
    }
    uint8_t *dir(int slot) { return &bytes[Frame + slot * Frame]; }
    uint8_t *block(int slot) { return &bytes[Block + slot * Block]; }

    void checksum(int slot) {
        uint8_t x = 0;
        for (int i = 0; i < 126; i++) x ^= dir(slot)[i];
        dir(slot)[127] = x;
    }

    // a save occupying `slots` (first is the top block), with its title, product code and game id, and an
    // icon whose first frame is solid palette colour 1 (pure red)
    void addSave(const vector<int> &slots, const string &title, const string &productCode, const string &gameId) {
        for (size_t i = 0; i < slots.size(); i++) {
            uint8_t *d = dir(slots[i]);
            bool last = i + 1 == slots.size();
            d[0] = i == 0 ? 0x51 : (last ? 0x53 : 0x52);
            int size = slots.size() * Block;
            d[4] = size & 0xFF; d[5] = (size >> 8) & 0xFF; d[6] = (size >> 16) & 0xFF;
            d[8] = last ? 0xFF : slots[i + 1];
            d[9] = last ? 0xFF : 0x00;
            if (i == 0) {
                memcpy(d + 12, productCode.c_str(), productCode.size());
                memcpy(d + 22, gameId.c_str(), gameId.size());
            }
            checksum(slots[i]);
        }
        uint8_t *b = block(slots[0]);
        b[0] = 'S'; b[1] = 'C';
        memcpy(b + 4, title.c_str(), title.size());
        // palette entry 1: 15-bit BGR with red = 31 -> bytes 0x1F 0x00
        b[0x60 + 2] = 0x1F; b[0x60 + 3] = 0x00;
        // frame 0: every pixel index 1 (two pixels per byte)
        memset(b + 0x80, 0x11, 128);
    }

    vector<uint8_t> bytes;
};

// shiftjis.dat maps a Shift-JIS code to Unicode as big-endian 16-bit entries; this table has the ASCII
// range and nothing else, which is all a title in these tests uses
vector<uint8_t> asciiShiftJisTable() {
    vector<uint8_t> table(25088, 0);
    for (int c = 0; c < 128; c++) {
        table[c * 2] = 0;
        table[c * 2 + 1] = c;
    }
    return table;
}

MemcardImage imageOf(CardBytes &card) {
    MemcardImage image;
    image.setShiftJisTable(asciiShiftJisTable());
    image.setBytes(card.bytes.data());
    return image;
}

bool checksumValid(const MemcardImage &image, int slot) {
    const uint8_t *d = image.bytes() + Frame + slot * Frame;
    uint8_t x = 0;
    for (int i = 0; i < 126; i++) x ^= d[i];
    return d[127] == x;
}

} // namespace

TEST_CASE("an empty card is fifteen free slots with nothing to say") {
    CardBytes card;
    MemcardImage image = imageOf(card);
    for (int slot = 0; slot < MemcardImage::Slots; slot++) {
        CHECK(image.isFree(slot));
        CHECK_FALSE(image.isUsed(slot));
        CHECK(image.title(slot) == "");
        CHECK(image.nextSlot(slot) == 0xFF);
    }
    CHECK(image.findEmptySlots(3) == vector<int>{0, 1, 2});
}

TEST_CASE("a save is parsed from its directory frames: kind of block, chain, codes and title") {
    CardBytes card;
    card.addSave({0, 1}, "Hello", "BASCUS-941", "63TEKKEN3");
    MemcardImage image = imageOf(card);

    CHECK(image.isUsed(0));
    CHECK(image.isTop(0));
    CHECK(image.blockType(1) == MemcardImage::BlockType::LinkEnd);
    CHECK(image.nextSlot(0) == 1);
    CHECK(image.nextSlot(1) == 0xFF);
    CHECK(image.gameSlots(0) == vector<int>{0, 1});
    CHECK(image.productCode(0) == "BASCUS-941");
    CHECK(image.gameId(0) == "63TEKKEN3");
    CHECK(image.title(0) == "Hello");
    CHECK(image.title(1) == "");        // a link block has no title of its own
    CHECK(image.hasIcon(0));
    CHECK_FALSE(image.hasIcon(1));
    CHECK(image.isFree(2));
    CHECK(image.findEmptySlots(2) == vector<int>{2, 3});
}

TEST_CASE("a save exported from one card imports into the free slots of another, relinked") {
    CardBytes a;
    a.addSave({0, 1}, "Hello", "BASCUS-941", "63TEKKEN3");
    MemcardImage from = imageOf(a);

    CardBytes b;
    b.addSave({0}, "Other", "BASCUS-000", "00OTHER");   // slot 0 is taken; the import lands on 1 and 2
    MemcardImage to = imageOf(b);

    int size = from.exportSize(0);
    CHECK(size == Frame + 2 * Block);
    vector<uint8_t> buffer(size);
    from.exportGame(0, buffer.data());
    to.importGame(buffer.data(), size);

    CHECK(to.isTop(1));
    CHECK(to.blockType(2) == MemcardImage::BlockType::LinkEnd);
    CHECK(to.nextSlot(1) == 2);
    CHECK(to.nextSlot(2) == 0xFF);
    CHECK(to.title(1) == "Hello");
    CHECK(to.productCode(1) == "BASCUS-941");
    CHECK(to.title(0) == "Other");   // untouched
    CHECK(checksumValid(to, 1));
    CHECK(checksumValid(to, 2));
    CHECK(to.findEmptySlots(1) == vector<int>{3});
}

TEST_CASE("a save that does not fit is not imported at all") {
    CardBytes a;
    a.addSave({0, 1, 2}, "Big", "BASCUS-941", "63BIG");
    MemcardImage from = imageOf(a);

    CardBytes b;
    for (int slot = 0; slot < 13; slot++) b.addSave({slot}, "S", "BASCUS-000", "0");   // two slots left
    MemcardImage to = imageOf(b);

    vector<uint8_t> buffer(from.exportSize(0));
    from.exportGame(0, buffer.data());
    to.importGame(buffer.data(), buffer.size());

    CHECK(to.isFree(13));
    CHECK(to.isFree(14));
}

TEST_CASE("deleting a save frees its blocks but leaves the data, so it can be undeleted") {
    CardBytes card;
    card.addSave({0, 1}, "Hello", "BASCUS-941", "63TEKKEN3");
    MemcardImage image = imageOf(card);

    image.deleteGame(0);
    CHECK(image.isFree(0));
    CHECK(image.isFree(1));
    CHECK(image.isDeleted(0));       // the block still starts with "SC"
    CHECK_FALSE(image.isUsed(0));
    CHECK(checksumValid(image, 0));

    image.undeleteSlot(0);
    CHECK(image.isTop(0));
    CHECK(image.title(0) == "Hello");
    CHECK(checksumValid(image, 0));
}

TEST_CASE("the product code and game id can be rewritten, with the frame's checksum kept right") {
    CardBytes card;
    card.addSave({0}, "Hello", "BASCUS-941", "63TEKKEN3");
    MemcardImage image = imageOf(card);

    image.setProductCode(0, "BESLES-123");
    image.setGameId(0, "45NEW");
    CHECK(image.productCode(0) == "BESLES-123");
    CHECK(image.gameId(0) == "45NEW");
    CHECK(checksumValid(image, 0));
}

TEST_CASE("icon frames come back as pixels: the save's own, dimmed for a link block, translucent for a free slot") {
    CardBytes card;
    card.addSave({0, 1}, "Hello", "BASCUS-941", "63TEKKEN3");
    MemcardImage image = imageOf(card);
    MemcardImage::Pixel px[MemcardImage::IconSize * MemcardImage::IconSize];

    image.iconPixels(0, 0, px);
    CHECK(px[0].r == 248);   // palette red 31 * 8
    CHECK(px[0].g == 0);
    CHECK(px[0].a == 255);
    CHECK(px[255].r == 248);

    image.iconPixels(1, 0, px);   // the link block: the top's frame, a third as bright
    CHECK(px[0].r == 82);
    CHECK(px[0].a == 255);

    image.iconPixels(2, 0, px);   // free
    CHECK(px[0].r == 0);
    CHECK(px[0].a == 127);

    image.deleteGame(0);          // deleted: lightened
    image.iconPixels(0, 0, px);
    CHECK(px[0].r == 251);        // 31 * 4 + 127
}

TEST_CASE("a card is written and read back whole; a DexDrive file is read past its header; a short file is refused") {
    TempDir tmp("mcd");
    CardBytes card;
    card.addSave({0}, "Hello", "BASCUS-941", "63TEKKEN3");
    MemcardImage image = imageOf(card);

    REQUIRE(image.save(tmp.at("card.mcd")));
    MemcardImage back;
    back.setShiftJisTable(asciiShiftJisTable());
    REQUIRE(back.load(tmp.at("card.mcd")));
    CHECK(back.title(0) == "Hello");
    CHECK(memcmp(back.bytes(), image.bytes(), MemcardImage::Size) == 0);

    string dex(3904, '\0');
    dex.append(reinterpret_cast<const char *>(image.bytes()), MemcardImage::Size);
    tmp.writeFile("card.gme", dex);
    MemcardImage fromDex;
    fromDex.setShiftJisTable(asciiShiftJisTable());
    REQUIRE(fromDex.load(tmp.at("card.gme")));
    CHECK(fromDex.title(0) == "Hello");

    tmp.writeFile("short.mcd", "not a card");
    CHECK_FALSE(MemcardImage().load(tmp.at("short.mcd")));
    CHECK_FALSE(MemcardImage().load(tmp.at("missing.mcd")));
}

TEST_CASE("without a Shift-JIS table a title is as many U+0000s as it has bytes") {
    CardBytes card;
    card.addSave({0}, "Hi", "BASCUS-941", "63TEKKEN3");
    MemcardImage image;
    image.setBytes(card.bytes.data());
    CHECK(image.title(0) == string(2, '\0'));
}
