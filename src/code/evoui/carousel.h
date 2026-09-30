//
// Carousel: the row of covers in the EvolutionUI launcher - which games are in it, which one is selected,
// where each cover is on its way to, and the drawing of them.
//
#pragma once

#include "carousel_game.h"
#include "cover_loader.h"
#include "core/model/ps_game.h"

#include <ab_gui/tween.h>
#include <ableem/ableem.h>

#include <cstdint>
#include <vector>

//******************
// Carousel
//******************
// Was a third of GuiLauncher. PsCarousel::Slots screen positions (SideCovers a side and the selected one in the
// middle at full size), one game per position; scrolling moves every visible cover one position along and
// drops the one that falls off the end. Covers are textures decoded in the background (CoverLoader) as a game
// comes near the screen, and kept after it leaves up to a limit (coverCacheLimit) the console can spare.
//
// The row is bounded: it holds exactly the games given, the first game has nothing to its left and the
// last nothing to its right, and a scroll past either end is refused (canSelectNext()/canSelectPrevious()).
// Until 2026-09-18 a short list was repeated to fill every slot and the row wrapped around, which showed
// the same few games several times over. The slots past either end are not left bare, though: an empty
// box of the set's kind (`BoxKind`, greyed and see-through) stands in each, and scrolls along with the
// games - so the shelf runs to the edge of the screen at both ends, and an empty set is a shelf of empty
// boxes.
class Carousel {
public:
    explicit Carousel(ableem::GuiBase &gui) : gui_(gui) {}

    // the games the carousel shows, in display order, and the kind of box an empty slot shows when there
    // is no game to take it from (an empty set). Frees the old covers, selects the first game (or none:
    // `selected` is -1 for an empty list) and places the covers.
    void setGames(const PsGames &gamesList, BoxKind kind);
    void freeTextures();
    void initPositions() { positions.initCoverPositions(); }

    std::vector<PsCarouselGame> games;
    // the empty boxes: leftFill[k] stands k+1 places before the first game, rightFill[k] k places after
    // the last (index games.size() + k). Slots of them each: an empty row needs the middle and a whole
    // side from one of them.
    std::vector<PsCarouselGame> leftFill, rightFill;
    BoxKind boxKind = BoxKind::JewelCase;
    int selected = 0; // index into `games`; -1 when there are none
    bool selectedIsValid() const { return selected >= 0 && selected < static_cast<int>(games.size()); }
    // move the selection one game along; a no-op at either end of the row. The caller checks canSelect*()
    // first and starts the matching scroll animation
    void selectNext();
    void selectPrevious();
    bool canSelectNext() const { return selectedIsValid() && selected + 1 < static_cast<int>(games.size()); }
    bool canSelectPrevious() const { return selectedIsValid() && selected > 0; }

    bool scrolling = false; // an animation is in progress; input that would start another waits
    long chainEnd = 0;      // when the last held-stick step ends: the next one starts there (stepStart)
    long stepStart(int speed, bool eased);
    PsCarousel positions;

    // places the covers around `selectedIndex` with no animation. With waitForCovers every shown cover is
    // loaded before this returns (a new set, a return from a game); without it (the end of a scroll step)
    // nothing is loaded here - the missing covers are asked of the CoverLoader and arrive through
    // pumpCovers(), an empty box standing in for one until then. Covers no longer shown are kept, up to
    // coverCacheLimit(), the longest unused let go first.
    void setInitialPositions(int selectedIndex, bool waitForCovers = true);
    // how many games beyond each end of the row are decoded ahead for the scroll that brings them in
    static const int Lookahead = 6;
    // puts the covers the CoverLoader has finished on the GPU, the nearest first - one a frame while the
    // row moves, two at rest; true if it did any. Once a frame from the launcher's loop.
    bool pumpCovers();
    // start the scroll animation towards the next / previous game, `speed` milliseconds long; eased
    // (easeOutCubic) for a tap, linear for a held stick so that one step runs into the next
    void scrollLeft(int speed, bool eased = true);
    void scrollRight(int speed, bool eased = true);
    // the selected cover moves up to make room for the game menu, and back down when it closes
    void moveMainCover(bool toGamesRow);
    // a scroll or a cover's own move (the main cover raised or lowered) is in progress, or the shine crossing
    // the selected cover
    bool animating() const;
    // the same two places, taken at once with no animation - for a screen that comes back with the menu
    // still open, or a reload that must not drop the cover while the menu shows
    void snapMainCover(bool toGamesRow);
    // advances every cover's animation; call once per frame before render()
    void updatePositions();
    void render();

private:
    // when every animation has finished a scroll, settle the covers on their final positions
    void updateVisibility();
    // the item at a position in the row counted from the first game: a game for 0..size-1, an empty box
    // before or after (nullptr when further out than the fills reach)
    PsCarouselGame *itemAt(int index);
    // the composed empty box, shared by every placeholder; made on first use
    void loadPlaceholderTexture();
    // the covers are composed in render targets: when the renderer lost them (Renderer::targetsLost), the
    // shown and wanted ones are composed again before the next frame
    void reloadLostTextures();
    // every item: the games, then the empty boxes
    template <class F> void forEachItem(F f) {
        for (auto &game : games)
            f(game);
        for (auto &box : leftFill)
            f(box);
        for (auto &box : rightFill)
            f(box);
    }

    // the covers kept at most (AB_COVER_CACHE overrides): the shown and the lookahead ones always, the
    // rest up to this many - each a 226x226 RGBA target, 0.2 MB. Sony's own carousel loaded every cover at
    // once, which on a big library ran the console out of video memory.
    static int coverCacheLimit();
    void evictCovers();
    // asks the CoverLoader for the shown and wanted covers still missing, nearest to `middle` first
    void requestMissingCovers(int middle);
    // a 226x226 target an evicted cover left, to compose the next one into (creating one is slow on the
    // console's GPU); an invalid Texture when there is none
    ableem::Texture spareTarget();

    ableem::GuiBase &gui_;
    CoverLoader loader_;
    std::vector<ableem::Texture> targetPool_;
    unsigned long placement_ = 0;    // counts setInitialPositions: PsCarouselGame::lastWanted's clock
    ableem::Texture placeholderTex_; // the empty box of `boxKind`, invalid until a placeholder is shown
    ableem::Rect placeholderContent_;
    float placeholderThickness_ = 0.08f;
    unsigned long texturesDrawnAt_ = 0; // Renderer::targetsLost() the covers were composed at

    // the layer: a row at rest is drawn once into layer_ and shown with one copy a frame after that, instead of
    // a copy per cover strip (over a thousand on the console). What was drawn is told by its signature - every
    // shown cover's texture, place, turn and shade; a moving row is drawn straight to the screen, and baked
    // again the first frame it stands still. AB_LAYERS=0 turns it off.
    void drawCovers(const std::vector<const PsCarouselGame *> &visible);
    ableem::Texture layer_;
    std::vector<std::uintptr_t> layerSignature_, lastSignature_;
    bool layerValid_ = false;

    // the selected cover's light: a soft glow behind it in the theme's selection colour, breathing slowly,
    // and - when it comes to rest - a shine that crosses its front once (Options "Cover shine", config.ini
    // covershine): evoimg/sheen.png's diagonal band at the face's height, clipped to its width. Both follow the
    // face's real width and height (core/model/cover_light.h) and are drawn outside the layer, the glow under
    // it, the shine over it (only on the selected game facing the viewer - never on an empty box). shineTex_ is
    // loaded when a crossing is due (drawShine). A theme's `coverGlow` frame (G5k) is drawn instead of the
    // square glow when it has one: round the face, scaled with the cover, at the glow's alpha.
    void drawGlow();
    void drawShine(long now);
    ableem::Texture glowTex_, shineTex_;
    // the breathing's clock (G5o2): an ambient tween on the program's Tweens (abgui::ambient::clockPhase) keeps
    // glowPhase_ at the platform's ticks in ms, wrapped, started by the first frame the glow is drawn; the owner
    // (declared after the float it writes) stops it with the carousel
    float glowPhase_ = 0.0f;
    bool glowRuns_ = false;
    abgui::TweenOwner glowOwner_;
    int shineFor_ = -1; // the game the shine last crossed (-1: none since the row moved)
    long shineAt_ = 0;  // when it starts crossing
};
