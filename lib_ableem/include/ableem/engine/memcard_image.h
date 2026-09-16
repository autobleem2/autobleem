// lib_ableem - engine: a PlayStation memory card image (.mcd, 128 KB) and its fifteen save slots.
//
// The directory frame at 0x80 + slot*0x80 says whether a slot is free, the top block of a save, or a link
// block continuing one; each slot's 8 KB of data starts at 0x2000 + slot*0x2000, with the Shift-JIS title
// and the three 16x16 icon frames at the front of a top block. Editing calls change the in-memory image
// only; save() writes it. The icon frames come back as RGBA pixels - turning them into textures is the UI's.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace ableem {

//******************
// MemcardImage
//******************
class MemcardImage {
public:
    static const int Slots = 15;
    static const int Size = 131072;          // a memory card holds 128K
    static const int IconSize = 16;          // an icon frame is 16x16
    static const int IconFrames = 3;

    enum class BlockType { Free = 0, Top = 1, Link = 2, LinkEnd = 3 };

    struct Pixel {
        uint8_t r = 0, g = 0, b = 0, a = 0;
    };

    MemcardImage();   // all zeroes: load() or a template card first

    // shiftjis.dat, the Shift-JIS -> Unicode table Japanese titles are converted through. Without it every
    // character of a title converts to U+0000, which is what an unconverted card has always shown.
    void setShiftJisTable(std::vector<uint8_t> table) { convTable_ = std::move(table); }

    // .mcd files, and DexDrive files (a 3904-byte header before the same image). false if it cannot be read.
    bool load(const std::string &filename);
    bool save(const std::string &filename) const;
    // the raw image (exactly Size bytes); the setter re-parses
    const uint8_t *bytes() const { return card_; }
    void setBytes(const uint8_t *data);
    void reparse();

    // --- slots ---
    bool isUsed(int slot) const { return slotIsUsed_[slot]; }
    bool isFree(int slot) const { return blockType_[slot] == BlockType::Free; }
    bool isDeleted(int slot) const { return slotIsDeleted_[slot]; }   // freed, but the data still starts with "SC": can be undeleted
    bool isTop(int slot) const { return blockType_[slot] == BlockType::Top; }
    bool hasIcon(int slot) const { return slotHasIcon_[slot]; }
    BlockType blockType(int slot) const { return blockType_[slot]; }
    int nextSlot(int slot) const { return nextSlotMap_[slot]; }    // 0xFF at the end of a save's chain

    std::string productCode(int slot) const { return productCodes_[slot]; }   // e.g. "BASCUS-94163"
    std::string gameId(int slot) const { return gameIds_[slot]; }
    // the save's title from its top block, converted from Shift-JIS; "" for a free slot or a link block
    std::string title(int slot) const { return titles_[slot]; }

    void setProductCode(int slot, const std::string &code);
    void setGameId(int slot, const std::string &id);

    void deleteSlot(int slot);
    void undeleteSlot(int slot);
    void deleteGame(int startSlot);   // the top block and every link block after it

    // every slot of the save starting at startSlot, in chain order
    std::vector<int> gameSlots(int startSlot) const;
    // `requested` free slots, or fewer if there are not that many
    std::vector<int> findEmptySlots(int requested) const;

    // a save as one buffer: its 128-byte directory frame, then each of its blocks. importGame puts one into
    // free slots, relinking the chain; it does nothing if there are not enough.
    int exportSize(int startSlot) const;
    void exportGame(int startSlot, uint8_t *buffer) const;
    void importGame(const uint8_t *buffer, int length);

    // raw block access: the directory frame (128 bytes) and the block (8 KB) of one slot
    void getSlotData(int slot, uint8_t *block, uint8_t *dirEntry) const;
    void setSlotData(int slot, const uint8_t *block, const uint8_t *dirEntry);

    // one icon frame as IconSize*IconSize RGBA pixels, row by row: a top block's own frame, a deleted
    // block's lightened, a link block's the dimmed first frame of its save, and a free slot's a translucent
    // black square.
    void iconPixels(int slot, int frame, Pixel *out) const;

private:
    void parseUsed();
    void parseDeleted();
    void parseHasIcon();
    void parseProductCodes();
    void parseGameIds();
    void parseTitles();
    void fixChecksum(int slot);
    int topSlotOf(int slot) const;   // the top block a link block belongs to, or -1
    void ownIconPixels(int slot, int frame, Pixel *out) const;
    std::string shiftJisToUtf8(const std::string &input) const;

    uint8_t card_[Size];
    bool slotIsUsed_[Slots];
    bool slotIsDeleted_[Slots];
    bool slotHasIcon_[Slots];
    BlockType blockType_[Slots];
    int nextSlotMap_[Slots];
    std::string productCodes_[Slots];
    std::string gameIds_[Slots];
    std::string titles_[Slots];
    std::vector<uint8_t> convTable_;
};

} // namespace ableem
