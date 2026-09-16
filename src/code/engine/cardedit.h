//
// CardEdit: a PS1 memory card (.mcd) block editor.
//
// Code based on https://github.com/raphnet/psxmemcardmgr
// Raphael Assenat   Licenced as: GPLv3
//
#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <iconv.h>
#include <ableem/ui/renderer.h>
#include <ableem/ui/texture.h>

//******************
// RGB
//******************
// One colour of a memory card save icon's 16-colour palette.
class RGB {
public:
    RGB() : RGB(0, 0, 0, 0) {}
    RGB(unsigned char r, unsigned char g, unsigned char b) : RGB(r, g, b, 0xFF) {}
    RGB(unsigned char r, unsigned char g, unsigned char b, unsigned char a) : r(r), g(g), b(b), a(a) {}

    unsigned char r, g, b, a;
};

//******************
// CardEdit
//******************
// A whole 128K memory card held in memory: load_file() reads it, the update_* routines parse the directory
// frame into the per-slot arrays below, and save_file() writes it back. A card has 15 slots; a save may span
// several of them, so a "game" is a top block (block_type 1) plus the link blocks next_slot_map chains to.
//
// This stays in the app rather than moving to lib_ableem's engine because it also renders each save's icon
// into an ableem::Texture (the little animated pictures in the memory card manager).
//
// Slot numbers are 0..14 throughout. The editing calls do not touch the file until save_file() is called.
class CardEdit {
public:
    CardEdit(ableem::Renderer &renderer1);
    ~CardEdit();

    // whole card
    int load_file(std::string filename);
    int save_file(std::string filename);
    int clearData();                 // format: empty every slot
    void update_data();              // re-parse the card into the per-slot arrays (after editing the bytes)

    // slots
    void delete_game(int startslot); // deletes the top block and every link block after it
    int delete_slot(int slot);
    int undelete_slot(int slot);

    // slot information
    std::string get_slot_Pcode(int slot);      // the product code, e.g. "BASCUS-94163"
    std::string get_slot_title(int slot);      // the save's title, converted from Shift-JIS
    bool is_slot_top(int slot);                // true for the first block of a save (not a link block)
    std::string get_slot_gameID(int slot);
    ableem::Texture get_slot_icon(int slot, int frame);   // frame 0..2 of the animated icon

    void set_slot_gameID(int slot, std::string newID);
    void set_slot_Pcode(int slot, std::string newPcode);

    bool get_slot_is_used(int slot);
    bool get_slot_is_free(int slot);

    // raw block access, used to copy saves between two cards
    void getSlotData(int slot, unsigned char *buffer, unsigned char *direntry);
    void setSlotData(int slot, unsigned char *buffer, unsigned char *direntry);
    std::vector<int> getGameSlots(int startslot);        // every slot of the save starting at startslot
    std::vector<int> findEmptySlot(int requested);       // `requested` free slots, empty if there are not enough

    int getExportSize(int startslot);                    // bytes one save occupies
    void exportGame(int slot, unsigned char *buffer);
    void importGame(unsigned char *buffer, int length);

    int next_slot_map[15];   // slot -> next slot of the same save, 0xFF at the end of the chain

private:
    ableem::Renderer &renderer;
    char memoryCard[131072];   // a memory card can hold 128K

    bool slot_is_used[15];
    bool slot_is_deleted[15];  // deleted, but SC still there
    bool slot_has_icon[15];
    unsigned char block_type[15];   // 0 not used, 1 top block, 2 link, 3 link end block
    std::string slot_Pcodes[15];
    std::string slot_gameID[15];
    std::string slot_titles[15];
    ableem::Texture slot_icons[15][3];

    // each of these re-reads one thing out of the card's directory frame; update() runs them all
    void update();
    void update_slot_is_used();    // also updates block_type and next slot map
    void update_slot_is_deleted();
    void update_slot_has_icon();
    void update_slot_Pcodes();
    void update_slot_gameIDs();
    void update_slot_titles();
    void update_slot_iconImages();

    std::string sj2utf8(const std::string &input);
    std::vector<uint8_t> convTable;   // shift-jis conversion table (shiftjis.dat)
};
