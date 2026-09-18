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
// Was a third of GuiLauncher. Thirteen screen positions (PsCarousel), one game per position and the selected
// one in the middle at full size; scrolling moves every visible cover one position along and drops the one
// that falls off the end. Covers are textures loaded when a game becomes visible and freed when it stops
// being, because thirteen at a time is what the console has memory for.
//
// The duplicated-games rule: with fewer than thirteen games the list is repeated until there are enough to
// fill every position, so the same PsGamePtr can sit in several PsCarouselGames. `numberOfNonDuplicatedGames`
// is the real count, for the "Showing: ... (N games)" line.
class Carousel {
public:
    explicit Carousel(ableem::GuiBase &gui) : gui_(gui) {}

    // the games the carousel shows, in display order. Frees the old covers, applies the duplicated-games
    // rule, selects the first game (or none: `selected` is -1 for an empty list) and places the covers.
    void setGames(const PsGames &gamesList);
    void freeTextures();
    void initPositions() { positions.initCoverPositions(); }

    std::vector<PsCarouselGame> games;
    int selected = 0; // index into `games`; -1 when there are none
    bool selectedIsValid() const { return selected >= 0 && selected < static_cast<int>(games.size()); }
    // move the selection one game along, wrapping; the caller starts the matching scroll animation
    void selectNext();
    void selectPrevious();

    int numberOfNonDuplicatedGames = 0;
    bool scrolling = false; // an animation is in progress; input that would start another waits
    PsCarousel positions;

    // places the covers around `selectedIndex` with no animation, loading and freeing textures to match
    void setInitialPositions(int selectedIndex);
    // start the scroll animation towards the next / previous game, `speed` milliseconds long
    void scrollLeft(int speed);
    void scrollRight(int speed);
    // the selected cover moves up to make room for the game menu, and back down when it closes
    void moveMainCover(bool toGamesRow);
    // advances every cover's animation; call once per frame before render()
    void updatePositions();
    void render();

private:
    int getNextId(int id) const;
    int getPreviousId(int id) const;
    // when every animation has finished a scroll, settle the covers on their final positions
    void updateVisibility();

    ableem::GuiBase &gui_;
};
