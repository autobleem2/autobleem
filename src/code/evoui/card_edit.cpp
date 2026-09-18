//
// CardEdit: a memory card as the memory-card manager shows it.
//
#include "card_edit.h"
#include "../core/services/environment.h"

#include <fstream>
#include <iostream>
#include <ableem/engine/log.h>

using namespace std;
using ableem::MemcardImage;
using ableem::Texture;

//*******************************
// CardEdit::CardEdit
//*******************************
CardEdit::CardEdit(ableem::Renderer &renderer) : renderer_(renderer) {
    vector<uint8_t> table(25088, 0);
    ifstream is(Env::getWorkingPath() + sep + "shiftjis.dat", ios::binary);
    if (is.is_open()) {
        is.read(reinterpret_cast<char *>(table.data()), table.size());
        image_.setShiftJisTable(std::move(table));
    } else {
        PLOG_WARNING << "shiftjis.dat not found, japanese memory card titles will not be converted";
    }

    for (int slot = 0; slot < MemcardImage::Slots; slot++) {
        for (int frame = 0; frame < MemcardImage::IconFrames; frame++) {
            icons_[slot][frame] = Texture::createStreaming(renderer_, MemcardImage::IconSize, MemcardImage::IconSize);
        }
    }

    // a new empty card
    load(Env::getPathToMemcardTemplateDir() + sep + "card1.mcd");
}

//*******************************
// CardEdit::load
//*******************************
bool CardEdit::load(const string &filename) {
    bool ok = image_.load(filename);
    refreshIcons();
    return ok;
}

//*******************************
// CardEdit::deleteGame / importGame
//*******************************
void CardEdit::deleteGame(int startSlot) {
    image_.deleteGame(startSlot);
    refreshIcons();
}

void CardEdit::importGame(const uint8_t *buffer, int length) {
    image_.importGame(buffer, length);
    refreshIcons();
}

//*******************************
// CardEdit::title
//*******************************
string CardEdit::title(int slot) const {
    if (!image_.isUsed(slot))
        return _("Free");
    if (image_.isDeleted(slot))
        return "(" + image_.title(slot) + ")";
    switch (image_.blockType(slot)) {
    case MemcardImage::BlockType::Link:
        return _("Link Block");
    case MemcardImage::BlockType::LinkEnd:
        return _("Link end Block");
    default:
        return image_.title(slot);
    }
}

//*******************************
// CardEdit::refreshIcons
//*******************************
void CardEdit::refreshIcons() {
    MemcardImage::Pixel pixels[MemcardImage::IconSize * MemcardImage::IconSize];
    for (int slot = 0; slot < MemcardImage::Slots; slot++) {
        for (int frame = 0; frame < MemcardImage::IconFrames; frame++) {
            image_.iconPixels(slot, frame, pixels);
            auto lock = icons_[slot][frame].lock();
            for (int y = 0; y < MemcardImage::IconSize; y++) {
                for (int x = 0; x < MemcardImage::IconSize; x++) {
                    const MemcardImage::Pixel &p = pixels[y * MemcardImage::IconSize + x];
                    lock.set(x, y, ableem::Color(p.r, p.g, p.b, p.a));
                }
            }
        }
    }
}
