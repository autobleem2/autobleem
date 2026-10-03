//
// CardEdit: a memory card as the memory-card manager shows it - the image plus its icons as textures.
//
#pragma once

#include "core/main.h"

#include <ableem/ableem.h>

#include <string>

//******************
// CardEdit
//******************
// The card itself is ableem::MemcardImage, portable and in the engine. This adds what the screen needs:
// the forty-five icon frames as textures, kept in step with the image after every edit, the translated
// "Free" / "Link Block" placeholders in the title column, and where the blank template card and the
// Shift-JIS table are (the working path). Reads go straight to image(); edits go through here so the
// textures follow.
class CardEdit {
public:
    explicit CardEdit(ableem::Renderer &renderer); // a blank card (the template card1.mcd) to start with

    bool load(const std::string &filename);
    bool save(const std::string &filename) const { return image_.save(filename); }

    const ableem::MemcardImage &image() const { return image_; }

    void deleteGame(int startSlot);
    void importGame(const uint8_t *buffer, int length);

    // the save's title, "(title)" for a deleted one, or what kind of block it is when there is no save
    std::string title(int slot) const;
    ableem::Texture icon(int slot, int frame) const { return icons_[slot][frame]; }

private:
    void refreshIcons();

    ableem::Renderer &renderer_;
    ableem::MemcardImage image_;
    ableem::Texture icons_[ableem::MemcardImage::Slots][ableem::MemcardImage::IconFrames];
};
