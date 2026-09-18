#include "ableem/engine/memcard_image.h"
#include "ableem/engine/filesystem.h"

#include <cstring>
#include <fstream>
#include <iostream>
#include "ableem/engine/log.h"

using namespace std;

namespace ableem {

namespace {

const int DirectoryFree = 0xA0;
const int DirectoryBusy = 0x50;
const int BlockSize = 0x2000;
const int FrameSize = 0x80;

int dirPosition(int slot) {
    return FrameSize + slot * FrameSize;
}
int blockPosition(int slot) {
    return BlockSize + slot * BlockSize;
}

} // namespace

//*******************************
// MemcardImage::MemcardImage
//*******************************
MemcardImage::MemcardImage() {
    memset(card_, 0, sizeof(card_));
    for (int i = 0; i < Slots; i++) {
        slotIsUsed_[i] = slotIsDeleted_[i] = slotHasIcon_[i] = false;
        blockType_[i] = BlockType::Free;
        nextSlotMap_[i] = 0xFF;
    }
}

//*******************************
// MemcardImage::load
//*******************************
bool MemcardImage::load(const string &filename) {
    ifstream f(filename, ifstream::ate | ifstream::binary);
    if (!f.is_open()) {
        PLOG_WARNING << "Cannot open memory card: " << filename;
        return false;
    }
    if (f.tellg() < Size) {
        PLOG_INFO << "Memory card file is too small: " << filename;
        return false;
    }
    if (f.tellg() == 134976) {
        f.seekg(3904); // a DexDrive file: skip its header, the card image follows
    } else {
        f.seekg(0);
    }
    f.read(reinterpret_cast<char *>(card_), Size);
    reparse();
    return true;
}

//*******************************
// MemcardImage::save
//*******************************
bool MemcardImage::save(const string &filename) const {
    ofstream f(filename, ios::binary);
    if (!DirEntry::checkWritable(f, filename))
        return false;
    f.write(reinterpret_cast<const char *>(card_), Size);
    f.close();
    return f.good();
}

//*******************************
// MemcardImage::setBytes
//*******************************
void MemcardImage::setBytes(const uint8_t *data) {
    memcpy(card_, data, Size);
    reparse();
}

//*******************************
// MemcardImage::reparse
//*******************************
void MemcardImage::reparse() {
    // order is important here.
    parseUsed();    // is it used, and which kind of block; also the next-slot map
    parseDeleted(); // a freed slot whose data is still there can be undeleted
    parseHasIcon();
    parseProductCodes();
    parseTitles();
    parseGameIds();
}

//*******************************
// MemcardImage::parseUsed
//*******************************
void MemcardImage::parseUsed() {
    for (int i = 0; i < Slots; i++) {
        int pos = dirPosition(i);
        if ((card_[pos] & DirectoryBusy) == DirectoryBusy) {
            slotIsUsed_[i] = true;
            // the low bits say what kind of block
            blockType_[i] = BlockType::Free;
            if ((card_[pos] & 0x01) == 0x01)
                blockType_[i] = BlockType::Top;
            if ((card_[pos] & 0x03) == 0x02)
                blockType_[i] = BlockType::Link;
            if ((card_[pos] & 0x03) == 0x03)
                blockType_[i] = BlockType::LinkEnd;
        } else {
            slotIsUsed_[i] = false;
            blockType_[i] = BlockType::Free;
        }
        nextSlotMap_[i] = card_[pos + 8];
    }
}

//*******************************
// MemcardImage::parseDeleted
//*******************************
void MemcardImage::parseDeleted() {
    for (int i = 0; i < Slots; i++) {
        // the directory says free, but the block still starts with "SC": the save is still there
        bool freed = (card_[dirPosition(i)] & DirectoryFree) == DirectoryFree;
        int block = blockPosition(i);
        slotIsDeleted_[i] = freed && card_[block] == 'S' && card_[block + 1] == 'C';
    }
}

//*******************************
// MemcardImage::parseHasIcon
//*******************************
void MemcardImage::parseHasIcon() {
    for (int i = 0; i < Slots; i++) {
        slotHasIcon_[i] = (blockType_[i] == BlockType::Top) || slotIsDeleted_[i];
    }
}

//*******************************
// MemcardImage::parseProductCodes
//*******************************
void MemcardImage::parseProductCodes() {
    for (int i = 0; i < Slots; i++) {
        productCodes_[i] = "";
        if (!slotIsUsed_[i])
            continue;
        int pos = dirPosition(i) + 12; // the product code is the 12th byte
        for (int n = 0; n < 10 && card_[pos] != 0; n++, pos++) {
            productCodes_[i] += static_cast<char>(card_[pos]);
        }
    }
}

//*******************************
// MemcardImage::parseGameIds
//*******************************
void MemcardImage::parseGameIds() {
    for (int i = 0; i < Slots; i++) {
        gameIds_[i] = "";
        if (!slotIsUsed_[i])
            continue;
        int pos = dirPosition(i) + 22; // the game ID is the 22nd byte
        while (card_[pos] != 0) {
            gameIds_[i] += static_cast<char>(card_[pos]);
            pos++;
        }
    }
}

//*******************************
// MemcardImage::parseTitles
//*******************************
void MemcardImage::parseTitles() {
    for (int i = 0; i < Slots; i++) {
        titles_[i] = "";
        if (slotIsUsed_[i] && (blockType_[i] == BlockType::Top || slotIsDeleted_[i])) {
            //  04h-43h  Title in Shift-JIS format (64 bytes = max 32 characters)
            const char *jisTitle = reinterpret_cast<const char *>(&card_[blockPosition(i) + 4]);
            titles_[i] = shiftJisToUtf8(string(jisTitle, strnlen(jisTitle, 64)));
        }
    }
}

//*******************************
// MemcardImage::shiftJisToUtf8
//*******************************
string MemcardImage::shiftJisToUtf8(const string &input) const {
    if (convTable_.size() < 25088) {
        return string(input.length(), '\0'); // no table: what an unconverted card has always shown
    }
    string output(3 * input.length(), ' '); // ShiftJis won't give 4-byte UTF-8, so at most 3 bytes per input char
    size_t indexInput = 0, indexOutput = 0;

    while (indexInput < input.length()) {
        char arraySection = (static_cast<uint8_t>(input[indexInput])) >> 4;

        size_t arrayOffset;
        if (arraySection == 0x8)
            arrayOffset = 0x100; // these are two-byte shiftjis
        else if (arraySection == 0x9)
            arrayOffset = 0x1100;
        else if (arraySection == 0xE)
            arrayOffset = 0x2100;
        else
            arrayOffset = 0; // this is one byte shiftjis

        if (arrayOffset) {
            arrayOffset += ((static_cast<uint8_t>(input[indexInput])) & 0xf) << 8;
            indexInput++;
            if (indexInput >= input.length())
                break;
        }
        arrayOffset += static_cast<uint8_t>(input[indexInput++]);
        arrayOffset <<= 1;

        uint16_t unicodeValue = (convTable_[arrayOffset] << 8) | convTable_[arrayOffset + 1];

        if (unicodeValue < 0x80) {
            output[indexOutput++] = unicodeValue;
        } else if (unicodeValue < 0x800) {
            output[indexOutput++] = 0xC0 | (unicodeValue >> 6);
            output[indexOutput++] = 0x80 | (unicodeValue & 0x3f);
        } else {
            output[indexOutput++] = 0xE0 | (unicodeValue >> 12);
            output[indexOutput++] = 0x80 | ((unicodeValue & 0xfff) >> 6);
            output[indexOutput++] = 0x80 | (unicodeValue & 0x3f);
        }
    }

    output.resize(indexOutput);
    return output;
}

//*******************************
// MemcardImage::fixChecksum
//*******************************
// the last byte of a directory frame is the xor of the other 127
void MemcardImage::fixChecksum(int slot) {
    int pos = dirPosition(slot);
    uint8_t xorCode = 0x00;
    for (int j = 0; j < 126; j++) {
        xorCode = xorCode ^ card_[j + pos];
    }
    card_[pos + 127] = xorCode;
}

//*******************************
// MemcardImage::setProductCode / setGameId
//*******************************
// The product code is a 10-byte field at 0x0C of the frame, the game id an ASCIIZ string right after it.
// (CardEdit, which this replaces, wrote neither the padding nor the terminator, so a shorter value left the
// tail of the old one behind; nothing in the app called either setter.)
void MemcardImage::setProductCode(int slot, const string &code) {
    int pos = dirPosition(slot) + 0x0C;
    for (int i = 0; i < 10; i++) {
        card_[pos + i] = i < static_cast<int>(code.size()) ? code[i] : 0;
    }
    fixChecksum(slot);
    reparse();
}

void MemcardImage::setGameId(int slot, const string &id) {
    int pos = dirPosition(slot) + 0x0C + 10;
    int n = id.size() < 102 ? id.size() : 102; // the frame has room for 102 characters and the terminator
    memcpy(card_ + pos, id.c_str(), n);
    card_[pos + n] = 0;
    fixChecksum(slot);
    reparse();
}

//*******************************
// MemcardImage::deleteSlot / undeleteSlot / deleteGame
//*******************************
void MemcardImage::deleteSlot(int slot) {
    int pos = dirPosition(slot);
    card_[pos] = (card_[pos] | 0xF0) ^ 0xF0 ^ DirectoryFree;
    fixChecksum(slot);
    reparse();
}

void MemcardImage::undeleteSlot(int slot) {
    int pos = dirPosition(slot);
    card_[pos] = (card_[pos] | 0xF0) ^ 0xF0 ^ DirectoryBusy;
    fixChecksum(slot);
    reparse();
}

void MemcardImage::deleteGame(int startSlot) {
    for (int slot : gameSlots(startSlot)) {
        deleteSlot(slot);
    }
}

//*******************************
// MemcardImage::gameSlots
//*******************************
vector<int> MemcardImage::gameSlots(int startSlot) const {
    vector<int> slots;
    int current = startSlot;
    slots.push_back(current);
    int next;
    int iteration = 0;
    while ((next = nextSlotMap_[current]) != 0xFF) {
        slots.push_back(next);
        current = next;
        if (++iteration == Slots) {
            break; // a broken chain that loops
        }
    }
    return slots;
}

//*******************************
// MemcardImage::findEmptySlots
//*******************************
vector<int> MemcardImage::findEmptySlots(int requested) const {
    vector<int> slots;
    for (int i = 0; i < Slots; i++) {
        if (isFree(i)) {
            slots.push_back(i);
        }
        if (static_cast<int>(slots.size()) == requested) {
            break;
        }
    }
    return slots;
}

//*******************************
// MemcardImage::getSlotData / setSlotData
//*******************************
void MemcardImage::getSlotData(int slot, uint8_t *block, uint8_t *dirEntry) const {
    memcpy(dirEntry, card_ + dirPosition(slot), FrameSize);
    memcpy(block, card_ + blockPosition(slot), BlockSize);
}

void MemcardImage::setSlotData(int slot, const uint8_t *block, const uint8_t *dirEntry) {
    memcpy(card_ + dirPosition(slot), dirEntry, FrameSize);
    memcpy(card_ + blockPosition(slot), block, BlockSize);
    reparse();
}

//*******************************
// MemcardImage::exportSize / exportGame / importGame
//*******************************
int MemcardImage::exportSize(int startSlot) const {
    return FrameSize + gameSlots(startSlot).size() * BlockSize;
}

void MemcardImage::exportGame(int startSlot, uint8_t *buffer) const {
    memcpy(buffer, card_ + dirPosition(startSlot), FrameSize);
    int n = 0;
    for (int slot : gameSlots(startSlot)) {
        memcpy(buffer + FrameSize + n * BlockSize, card_ + blockPosition(slot), BlockSize);
        n++;
    }
}

void MemcardImage::importGame(const uint8_t *buffer, int length) {
    int slotCount = (length - FrameSize) / BlockSize;
    int numberOfBytes = slotCount * BlockSize;
    vector<int> destSlots = findEmptySlots(slotCount);
    if (static_cast<int>(destSlots.size()) != slotCount) {
        return;
    }

    // the directory frame goes to the first slot, with the size updated
    int dir = dirPosition(destSlots[0]);
    memcpy(card_ + dir, buffer, FrameSize);
    card_[dir + 4] = static_cast<uint8_t>(numberOfBytes & 0xFF);
    card_[dir + 5] = static_cast<uint8_t>((numberOfBytes & 0xFF00) >> 8);
    card_[dir + 6] = static_cast<uint8_t>((numberOfBytes & 0xFF0000) >> 16);

    // the blocks
    int n = 0;
    for (int slot : destSlots) {
        memcpy(card_ + blockPosition(slot), buffer + FrameSize + n * BlockSize, BlockSize);
        n++;
    }

    // relink the chain: every slot points at the next, the last at 0xFF, the first is the top block
    for (int i = 0; i < slotCount; i++) {
        int d = dirPosition(destSlots[i]);
        card_[d + 0] = 0x52;
        card_[d + 8] = static_cast<uint8_t>(i + 1 < slotCount ? destSlots[i + 1] : 0);
        card_[d + 9] = 0x00;
    }
    int last = dirPosition(destSlots.back());
    card_[last + 0] = 0x53;
    card_[last + 8] = 0xFF;
    card_[last + 9] = 0xFF;
    card_[dirPosition(destSlots[0]) + 0] = 0x51;

    for (int slot : destSlots) {
        fixChecksum(slot);
    }
    reparse();
}

//*******************************
// MemcardImage::topSlotOf
//*******************************
int MemcardImage::topSlotOf(int slot) const {
    for (int i = 0; i < Slots; i++) {
        if (!isTop(i))
            continue;
        for (int s : gameSlots(i)) {
            if (s == slot)
                return i;
        }
    }
    return -1;
}

//*******************************
// MemcardImage::ownIconPixels
//*******************************
// the frame's 16 palette entries (15-bit BGR) and 4-bit pixels, out of the top block
void MemcardImage::ownIconPixels(int slot, int frame, Pixel *out) const {
    int paletteAddr = blockPosition(slot) + 0x60;
    int dataAddr = blockPosition(slot) + 0x80 + frame * 128;

    Pixel palette[16];
    for (int p = 0; p < 16; p++) {
        uint8_t lo = card_[paletteAddr + p * 2];
        uint8_t hi = card_[paletteAddr + p * 2 + 1];
        uint8_t blue = hi >> 2;
        uint8_t green = (((lo >> 5) | 0xF8) ^ 0xF8) + (((hi | 0xFC) ^ 0xFC) << 3);
        uint8_t red = (lo | 0xE0) ^ 0xE0;
        if (slotIsDeleted_[slot]) {
            palette[p] = Pixel{static_cast<uint8_t>(red * 4 + 127), static_cast<uint8_t>(green * 4 + 127),
                               static_cast<uint8_t>(blue * 4 + 127), 255};
        } else {
            palette[p] = Pixel{static_cast<uint8_t>(red * 8), static_cast<uint8_t>(green * 8),
                               static_cast<uint8_t>(blue * 8), 255};
        }
    }

    int pos = 0;
    for (int y = 0; y < IconSize; y++) {
        for (int x = 0; x < IconSize; x += 2) {
            uint8_t two = card_[dataAddr + pos];
            out[y * IconSize + x] = palette[two & 0x0F];
            out[y * IconSize + x + 1] = palette[(two >> 4) & 0x0F];
            pos++;
        }
    }
}

//*******************************
// MemcardImage::iconPixels
//*******************************
void MemcardImage::iconPixels(int slot, int frame, Pixel *out) const {
    if (slotHasIcon_[slot]) {
        ownIconPixels(slot, frame, out);
        return;
    }
    int top = isUsed(slot) ? topSlotOf(slot) : -1;
    if (top != -1 && top != slot) {
        // a link block shows its save's first frame, dimmed
        ownIconPixels(top, 0, out);
        for (int i = 0; i < IconSize * IconSize; i++) {
            out[i] = Pixel{static_cast<uint8_t>(out[i].r / 3), static_cast<uint8_t>(out[i].g / 3),
                           static_cast<uint8_t>(out[i].b / 3), 255};
        }
        return;
    }
    for (int i = 0; i < IconSize * IconSize; i++) {
        out[i] = Pixel{0, 0, 0, 127};
    }
}

} // namespace ableem
