//
// GuiSetPicker::render() (evoui_set_picker.cpp ~206-283) - UIREV-4, the set picker's row/panel layout, two
// findings from the UI review (!autobleem\out\ui-review\report.md, section 5 "Set picker", P1/P2):
//
//   P1 High - "long titles overlap the count" (report.md:46, `evoui_set_picker.cpp:256-261` pre-fix): the row
//   loop drew `e.title` (FONT_22_MED) at a fixed `x` with no upper bound at all - nothing stopped it running
//   under `e.detail` (the count, e.g. "21 games", FONT_15_BOLD) drawn right-aligned at the row's other end.
//   The fix elides the title to the real gap between `x` and where the count begins, minus the 20 px margin
//   `gui_game_manager_menu.cpp:40` already uses for the same "title elided before a fixed right-hand column"
//   shape (`xoffset_R - 20`).
//
//   P2 Medium - "panel height changes per tab, tab strip jumps" (report.md:47): `panelHeight` was sized from
//   `tabs[tab].entries.size()` alone, so switching to a tab with fewer rows (or the empty "Not installed" Apps
//   tab, or any tab before RetroArch's playlists/Apps' categories are counted) shrank the centred panel and
//   moved the tab strip. The fix sizes the panel from the tallest tab, computed once per frame and reused for
//   every tab; only which rows are drawn inside that constant height changes.
//
// Like `test_menu_base_navigation.cpp`/`test_panel_style_footer.cpp` in autobleem-core, this cannot drive the
// real `GuiSetPicker::render()`: it needs a live Gui/ThemeAssets/TextRenderer with real loaded theme fonts,
// which needs an AppBase over a real theme/resources tree this repository does not build a test host for. So
// this suite runs a small local model of the two formulas (mirrored line for line from the fixed
// evoui_set_picker.cpp) instead.
//
// P2 is pure arithmetic on `entries.size()` per tab - no font metrics involved, and the "before"/"after"
// numbers below are exact, not approximated.
//
// P1 needs real glyph widths to say for certain whether a given title's *rendered* pixels reach the count.
// `charWidth(fontPx)` is the same documented *approximation* `test_panel_style_footer.cpp` uses for this same
// Open Sans family (average px per character at a given point size - not real advance widths), carried over
// verbatim rather than reinvented. Under it, the longest system/playlist name actually shipped in this
// repository's own data - "Nintendo - Super Nintendo Entertainment System" (`src/resources/platform/
// roms_systems.cfg:33`, also the exact string the UI review's launcher-banner finding quotes as visibly cut,
// report.md:21 "Showing: Retroarch Nintendo - Nintend...") - leaves only a *thin, unenforced* margin before
// a realistic count column with the pre-fix (unbounded) draw: every character past it costs ~13 px at
// FONT_22_MED, so a title only a handful of characters longer, a slightly wider real font (Open Sans commonly
// runs wider than this average-character model, the same caveat test_panel_style_footer.cpp notes), or a
// deeper `indent` (a nested USB Games sub-folder row, which shifts the same margin away one 24 px step at a
// time) closes it - which is exactly what report.md P1 (an independently confirmed screenshot finding,
// shots 29-31/46/60/61) already caught. What this suite proves without needing real metrics at all: pre-fix
// there was *no* minimum margin - zero, by construction, since nothing bounded the title's width; post-fix
// there always is, because `elide()`'s own contract ("the text if it fits maxWidth in this font, else as much
// of it as does with \"...\" on the end", `gui/text_renderer.h`) guarantees the rendered text never exceeds
// the `maxWidth` passed to it - so bounding that call at `detailX - x - 20` is, by definition, a hard
// guarantee of at least 20 px of clearance, for a title of any length.
//
#include "doctest/doctest.h"

#include <algorithm>
#include <string>
#include <vector>

namespace {

using std::string;
using std::vector;

// same approximation as test_panel_style_footer.cpp (~0.6 px per point, Open Sans Bold/Medium's rough
// average advance) - not real glyph metrics, just enough to reason about the formulas' arithmetic
int charWidth(int fontPx) {
    return (fontPx * 3 + 2) / 5;
}
int textWidth(int fontPx, const string &s) {
    return static_cast<int>(s.size()) * charWidth(fontPx);
}

const int Font15 = 15, Font22 = 22;
const int PanelWidth = 800; // evoui_set_picker.cpp's own constant

//*******************************
// P1: the row layout, mirrored from evoui_set_picker.cpp's fixed render()
//*******************************

// mirrors the fixed code: RowInset (24) + 8 from the panel's edge, plus indent
int rowTitleX(int panelX, int indent) {
    return panelX + 24 + 8 + indent * 24;
}
// mirrors the fixed code: where the right-aligned count starts
int rowDetailX(int panelX, int panelW, const string &detail) {
    return panelX + panelW - 24 - textWidth(Font15, detail);
}
// mirrors the fix's elide-width formula: gui_game_manager_menu.cpp's own "- 20" margin convention
int titleMaxWidth(int panelX, int panelW, int indent, const string &detail) {
    return rowDetailX(panelX, panelW, detail) - rowTitleX(panelX, indent) - 20;
}

//*******************************
// P2: the panel-height formula, mirrored from evoui_set_picker.cpp's fixed render()
//*******************************

const int TabsHeight = 112, FooterHeight = 54, RowHeight = 44; // evoui_set_picker.cpp's own constants

int visibleRows(int screenH, int panelMargin) {
    return std::max(1, (screenH - 2 * panelMargin - TabsHeight - FooterHeight) / RowHeight);
}

// mirrors the pre-fix formula: sized from the current tab alone
int panelHeightPreFix(int rows, size_t currentTabEntries) {
    const int shown = std::max(1, std::min(rows, static_cast<int>(currentTabEntries)));
    return TabsHeight + shown * RowHeight + FooterHeight;
}

// mirrors the fix: sized from the tallest tab, the same for every tab
int panelHeightFixed(int rows, const vector<size_t> &tabEntries) {
    int maxShown = 1;
    for (size_t n : tabEntries)
        maxShown = std::max(maxShown, std::min(rows, static_cast<int>(n)));
    return TabsHeight + maxShown * RowHeight + FooterHeight;
}

} // namespace

//*******************************
// P1
//*******************************

TEST_CASE("P1 pre-fix: the row loop drew the title with no width bound at all - the gap it happened to leave "
          "for the longest real title/count in this screen was a thin, unenforced margin, not a guarantee") {
    // the longest real system/playlist name in the repo's own data (roms_systems.cfg:33), also the exact
    // string the same UI review quotes as visibly cut elsewhere on the launcher (report.md:21) - and the
    // set picker's RetroArch tab draws every playlist name as its row title exactly this way (indent 0)
    const string title = "Nintendo - Super Nintendo Entertainment System";
    const string detail = "128 games";
    const int panelX = 0; // only the width math matters below, not an absolute screen position
    const int x = rowTitleX(panelX, 0);
    const int detailX = rowDetailX(panelX, PanelWidth, detail);
    const int rawTitleWidth = textWidth(Font22, title);
    const int gapNoMargin = detailX - x; // what pre-fix code effectively had to work with, unenforced
    CHECK(x == 32);
    CHECK(detailX == 695);
    CHECK(rawTitleWidth == 598);
    // the margin pre-fix happened to leave here - real, but nothing in the code guaranteed it. It is thin:
    // a handful more characters (a longer playlist name, a slightly wider real font than this average-char
    // model, a translated string - see report.md's own P4/S6 findings on translated strings running longer)
    // erases it, which is exactly the overlap report.md P1 caught on a real build
    const int preFixMargin = gapNoMargin - rawTitleWidth;
    CHECK(preFixMargin == 65);
    CHECK(preFixMargin < 100); // "thin" - under 100 px in an ~660 px row, with zero enforced minimum
}

TEST_CASE("P1 post-fix: elide()'s own contract guarantees the fixed 20 px margin for this real title, "
          "unconditionally - the formula matches gui_game_manager_menu.cpp's own -20 convention") {
    const string title = "Nintendo - Super Nintendo Entertainment System";
    const string detail = "128 games";
    const int panelX = 0;
    const int maxWidth = titleMaxWidth(panelX, PanelWidth, 0, detail);
    CHECK(maxWidth == 643); // detailX(695) - x(32) - 20
    // elide()'s documented postcondition ("the text if it fits maxWidth ... else as much of it as does with
    // '...' on the end") means the rendered width is never more than maxWidth - so the count's own x
    // (detailX) is never closer than 20 px away, for a title of any length. Modelled here as the postcondition
    // itself (not a real elide() call - see the file header on why this suite cannot reach one) applied to
    // both a title that already fits (this real one) and one long enough that it would not:
    const int fitsWithinMax = std::min(textWidth(Font22, title), maxWidth);
    CHECK(fitsWithinMax <= maxWidth);
    CHECK(rowDetailX(panelX, PanelWidth, detail) - (rowTitleX(panelX, 0) + fitsWithinMax) >= 20);

    // a title long enough to actually need truncating (a stress case, not a claim about a real title in the
    // repo - it exercises the same guarantee where P1's pre-fix bug would have been worst: no bound growing
    // without limit as the title grows)
    const string longTitle(120, 'x');
    const int longFits = std::min(textWidth(Font22, longTitle), maxWidth);
    CHECK(longFits == maxWidth); // truncated exactly to the guaranteed bound
    CHECK(rowDetailX(panelX, PanelWidth, detail) - (rowTitleX(panelX, 0) + longFits) == 20);
}

TEST_CASE("P1: a deeper indent (a nested USB Games sub-folder row) eats the same margin one 24 px step at a "
          "time - pre-fix that shrinks an already-thin unenforced margin further; post-fix the guarantee "
          "(the elide width formula) accounts for it directly, since x includes indent * 24") {
    const string detail = "128 games";
    const int panelX = 0;
    const int detailX = rowDetailX(panelX, PanelWidth, detail);
    CHECK(titleMaxWidth(panelX, PanelWidth, 1, detail) == titleMaxWidth(panelX, PanelWidth, 0, detail) - 24);
    CHECK(titleMaxWidth(panelX, PanelWidth, 3, detail) == titleMaxWidth(panelX, PanelWidth, 0, detail) - 72);
    CHECK(detailX - rowTitleX(panelX, 3) - 20 == titleMaxWidth(panelX, PanelWidth, 3, detail)); // still exact
}

//*******************************
// P2
//*******************************

TEST_CASE("P2 pre-fix: switching tabs changes panelHeight - the tab strip jumps (report.md:47)") {
    const int rows = visibleRows(720, 40); // SCREEN_HEIGHT 720, PanelStyle::Margin 40 - this screen's real geometry
    // PlayStation tab: 3 entries (a stick with no internal games showing, just All/USB/Favorites say);
    // RetroArch tab: 15 playlists - both plausible real tab contents
    const int psHeight = panelHeightPreFix(rows, 3);
    const int raHeight = panelHeightPreFix(rows, 15);
    CHECK(psHeight != raHeight); // reproduced: the panel's own height depends on which tab is showing
    const int appsHeight = panelHeightPreFix(rows, 0); // the empty "Not installed" Apps tab
    CHECK(appsHeight != raHeight);
}

TEST_CASE("P2 post-fix: panelHeight is the same constant across every tab - sized from the tallest one") {
    const int rows = visibleRows(720, 40);
    const vector<size_t> tabs = {3, 15, 0}; // PlayStation, RetroArch, Apps - same content as the case above
    const int height = panelHeightFixed(rows, tabs);
    // every tab now reports the same height, whichever is "current" - panelHeightFixed does not take a
    // current-tab index at all, which is the point: it cannot vary per tab any more
    CHECK(height == panelHeightFixed(rows, tabs));
    CHECK(height == TabsHeight + std::min(rows, 15) * RowHeight + FooterHeight); // the tallest tab (RetroArch, 15) wins
}

TEST_CASE("P2 post-fix: a tab taller than the visible rows still only sizes the panel to `rows`, same as "
          "today - it scrolls inside a constant-height panel rather than growing the panel") {
    const int rows = visibleRows(720, 40);
    const vector<size_t> tabs = {3, 200}; // a RetroArch tab with 200 games in one playlist
    const int height = panelHeightFixed(rows, tabs);
    CHECK(height == TabsHeight + rows * RowHeight + FooterHeight); // capped at rows, not 200
}
