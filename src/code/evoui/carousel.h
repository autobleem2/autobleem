//
// Carousel: the row of covers in the EvolutionUI launcher - which games are in it, which one is selected,
// where each cover is on its way to, and the drawing of them.
//
#pragma once

#include "carousel_game.h"
#include "../core/model/ps_game.h"

#include <ableem/ableem.h>

#include <vector>

//******************
// Carousel
//******************
// Was a third of GuiLauncher. PsCarousel::Slots screen positions (SideCovers a side and the selected one in the
// middle at full size), one game per position; scrolling moves every visible cover one position along and
// drops the one that falls off the end. Covers are textures loaded when a game becomes visible and freed
// when it stops being, to keep the number held at once (each 226x226 RGBA) to what the console can spare.
//
// The row is bounded: it holds exactly the games given, the first game has nothing to its left and the
// last nothing to its right, and a scroll past either end is refused (canSelectNext()/canSelectPrevious()).
// Until 2026-09-18 a short list was repeated to fill every slot and the row wrapped around, which showed
// the same few games several times over.
class Carousel {
public:
    explicit Carousel(ableem::GuiBase &gui) : gui_(gui) {}

    // the games the carousel shows, in display order. Frees the old covers, selects the first game (or
    // none: `selected` is -1 for an empty list) and places the covers.
    void setGames(const PsGames &gamesList);
    void freeTextures();
    void initPositions() { positions.initCoverPositions(); }

    std::vector<PsCarouselGame> games;
    int selected = 0; // index into `games`; -1 when there are none
    bool selectedIsValid() const { return selected >= 0 && selected < static_cast<int>(games.size()); }
    // move the selection one game along; a no-op at either end of the row. The caller checks canSelect*()
    // first and starts the matching scroll animation
    void selectNext();
    void selectPrevious();
    bool canSelectNext() const { return selectedIsValid() && selected + 1 < static_cast<int>(games.size()); }
    bool canSelectPrevious() const { return selectedIsValid() && selected > 0; }

    bool scrolling = false; // an animation is in progress; input that would start another waits
    PsCarousel positions;

    // places the covers around `selectedIndex` with no animation, loading and freeing textures to match:
    // every visible cover is loaded before this returns; the Lookahead games beyond each end of the row
    // keep theirs and get them from loadOneMissingTexture() when the screen has a frame to spare
    void setInitialPositions(int selectedIndex);
    // how many games beyond each end of the row keep a texture ready for the scroll that brings them in.
    // A cover is a PNG decode of 10-40 ms on a Pi, and doing it on the frame the scroll ended was a hitch
    // at the end of every scroll.
    static const int Lookahead = 2;
    // decodes one wanted-but-missing cover (the nearest first); true if it did. For the launcher's idle
    // frames: one a frame, so the covers just past the ends are ready before the next scroll.
    bool loadOneMissingTexture();
    // start the scroll animation towards the next / previous game, `speed` milliseconds long; eased
    // (easeOutCubic) for a tap, linear for a held stick so that one step runs into the next
    void scrollLeft(int speed, bool eased = true);
    void scrollRight(int speed, bool eased = true);
    // the selected cover moves up to make room for the game menu, and back down when it closes
    void moveMainCover(bool toGamesRow);
    // the same two places, taken at once with no animation - for a screen that comes back with the menu
    // still open, or a reload that must not drop the cover while the menu shows
    void snapMainCover(bool toGamesRow);
    // advances every cover's animation; call once per frame before render()
    void updatePositions();
    void render();

private:
    // the game one along from `id`, -1 past either end of the row
    int getNextId(int id) const;
    int getPreviousId(int id) const;
    // when every animation has finished a scroll, settle the covers on their final positions
    void updateVisibility();

    ableem::GuiBase &gui_;
};
