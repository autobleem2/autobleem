//
// Created by screemer on 2019-02-22.
//

#include "evoui_stateselector.h"
#include "gui/gui.h"
#include "../screens/evoui_launcher.h"
#include "../../app.h"
#include "resume_layout.h"

#include <algorithm>

using namespace std;

//*******************************
// PsStateSelector::cleanSaveStateImages
//*******************************
void PsStateSelector::cleanSaveStateImages() {
    for (auto &i : slotImg)
        i = ableem::Texture();
}

//*******************************
// PsStateSelector::loadSaveStateImages
//*******************************
// UIREV-37: also reads each slot's time (the kept state file's) and works out the NEWEST slot. The pictures are
// drawn as they are - the card's well clips them to its cut corners, not the theme's resume picture mask (that one
// is for the game menu's icon).
void PsStateSelector::loadSaveStateImages(PsGamePtr &game, bool saving) {
    operation = saving ? OP_SAVE : OP_LOAD;
    gameTitle = game->title;
    ResumePointService &resume = App::get().resumePoints();
    const string format = ResumeLayout::dateFormat(App::get().config().inifile.values["datetimeformat"]);
    time_t times[ResumeLayout::SlotCount] = {};
    for (int i = 0; i < ResumeLayout::SlotCount; i++) {
        slotImg[i] = ableem::Texture();
        slotDate[i].clear();
        slotUsed[i] = resume.slotIsActive(*game, i);
        slotActive[i] = saving || slotUsed[i]; // a save may go to any slot
        if (slotUsed[i]) {
            slotImg[i] = ableem::Texture::loadFile(renderer, resume.pictureForSlot(*game, i));
            times[i] = resume.timeForSlot(*game, i);
            slotDate[i] = App::get().clock().displayTime(times[i], format);
        }
    }
    newest = ResumeLayout::newestSlot(times, slotUsed);
}

//*******************************
// PsStateSelector::drawPicture
//*******************************
// The slot's picture in `box`, clipped to the well's shape: the cut corners are drawn row by row (each of the six
// top and bottom rows a little narrower), the rows between in one copy.
void PsStateSelector::drawPicture(const ableem::Texture &picture, const ResumeLayout::Box &box) {
    const ableem::Size s = picture.size();
    if (s.w <= 0 || s.h <= 0)
        return;
    auto strip = [&](int row, int rows) {
        const ResumeLayout::RowInset in = ResumeLayout::rowInset(row, box.h);
        const int srcY = row * s.h / box.h;
        const int srcH = max(1, (row + rows) * s.h / box.h - srcY);
        const int srcL = in.left * s.w / box.w;
        const int srcW = max(1, s.w - srcL - in.right * s.w / box.w);
        const ableem::Rect from(srcL, srcY, srcW, srcH);
        const ableem::Rect to(box.x + in.left, box.y + row, box.w - in.left - in.right, rows);
        renderer.copy(picture, &from, &to);
    };
    const int cut = ResumeLayout::CutCorner;
    for (int row = 0; row < cut; row++)
        strip(row, 1);
    strip(cut, box.h - 2 * cut);
    for (int row = box.h - cut; row < box.h; row++)
        strip(row, 1);
}

//*******************************
// PsStateSelector::render
//*******************************
// UIREV-37 (autobleem-design themes/ab2.0.0/design/uirev37/README.md): the band, the heading and the game's name,
// four framed cards - each its picture in a field-framed well, "Slot n", the slot's date and (on the newest of
// two or more) a NEWEST chip - the selected card in `keySelected`, an empty slot in load mode under the `disabled`
// veil. Every piece is a theme frame through the Style, which draws today's code-drawn shape where the theme has none.
void PsStateSelector::render() {
    if (!visible)
        return;
    shared_ptr<Gui> gui(Gui::getInstance());
    abgui::Context &ctx = gui->uiContext();
    const abgui::Style &style = ctx.style();
    Fonts &fonts = ThemeAssets::fixedFonts();
    const ableem::Font &titleFont = font30; // bold 28
    const ableem::Font &nameFont = fonts.atSize(FONT_MED, 20);
    const ableem::Font &slotFont = fonts.boldAtSize(24);
    const ableem::Font &emptyFont = fonts[FONT_22_MED];
    const ableem::Font &dateFont = fonts.atSize(FONT_MED, 18);
    const ableem::Font &chipFont = fonts.boldAtSize(13);

    // the band: the theme's `band` frame (G5i), else the black strip
    const ableem::Rect band(0, ResumeLayout::BandY, SCREEN_WIDTH, ResumeLayout::BandH);
    if (!style.drawFrame(ctx, "band", band)) {
        renderer.setDrawColor(ableem::Color(0, 0, 0, 200));
        renderer.fillRect(band);
    }

    // the theme's own text colour (white where the theme says nothing); the selection colour names the selected slot
    // (the text colour where the theme has none)
    const LauncherTheme &theme = App::get().theme().launcher();
    const ableem::Color textColor =
        theme.colors.text.set ? TextRenderer::toColor(theme.colors.text, 255) : ableem::Color(255, 255, 255, 255);
    const ableem::Color selectionColor =
        theme.colors.selection.set ? TextRenderer::toColor(theme.colors.selection, 255) : textColor;
    const ableem::Color secondary = style.secondary;

    const string title = operation == OP_SAVE ? _("Select slot to save state") : _("Select resume slot to load");
    gui->text().renderText_WithColor(titleFont, title, 0, ResumeLayout::TitleMidY - titleFont.lineHeight() / 2,
                                     textColor, XALIGN_CENTER);
    gui->text().renderText_WithColor(nameFont, gameTitle, 0, ResumeLayout::NameMidY - nameFont.lineHeight() / 2,
                                     secondary, XALIGN_CENTER);

    for (int i = 0; i < ResumeLayout::SlotCount; i++) {
        const ResumeLayout::Box card = ResumeLayout::cardBox(i);
        const ResumeLayout::Box well = ResumeLayout::wellBox(card);
        const bool selected = selSlot == i;
        const bool veiled = operation == OP_LOAD && !slotUsed[i];
        const ableem::Rect cardRect(card.x, card.y, card.w, card.h);

        style.key(ctx, cardRect, selected ? abgui::KeyState::Selected : abgui::KeyState::Normal);
        style.field(ctx, ableem::Rect(well.x, well.y, well.w, well.h));

        if (slotImg[i].valid()) {
            drawPicture(slotImg[i], ResumeLayout::pictureBox(card));
        } else {
            const string empty = _("Empty");
            const int w = gui->text().textWidth(emptyFont, empty);
            gui->text().renderText_WithColor(emptyFont, empty, well.x + (well.w - w) / 2,
                                             well.y + (well.h - emptyFont.lineHeight()) / 2, secondary);
        }

        const int textX = card.x + ResumeLayout::TextInset;
        gui->text().renderText_WithColor(slotFont, _("Slot") + " " + to_string(i + 1), textX,
                                         card.y + ResumeLayout::SlotNameY, selected ? selectionColor : textColor);
        const string date = slotUsed[i] ? slotDate[i] : (operation == OP_LOAD ? _("No resume point") : _("Free"));
        gui->text().renderText_WithColor(dateFont, date, textX, card.y + ResumeLayout::DateY, secondary);

        if (i == newest) {
            const string word = _("NEWEST");
            const ResumeLayout::Box chip = ResumeLayout::chipBox(card, gui->text().textWidth(chipFont, word));
            const ableem::Rect chipRect(chip.x, chip.y, chip.w, chip.h);
            if (!style.drawFrame(ctx, "chip", chipRect)) {
                renderer.setBlendMode(ableem::BlendMode::Blend);
                renderer.setDrawColor(ableem::Color(255, 255, 255, 24));
                renderer.fillRect(chipRect);
                renderer.setDrawColor(ableem::Color(style.edge.r, style.edge.g, style.edge.b, 200));
                renderer.drawRect(chipRect);
            }
            gui->text().renderText_WithColor(chipFont, word, chip.x + ResumeLayout::ChipPadding,
                                             chip.y + (chip.h - chipFont.lineHeight()) / 2, textColor);
        }

        if (veiled)
            style.disabled(ctx, cardRect);
    }

    // no hint line of its own: GuiLauncher::buildHintLines() shows the Resume state's hints in the launcher's hint bar
}
