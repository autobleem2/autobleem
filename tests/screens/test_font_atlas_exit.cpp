//
// BUG-11: the launcher aborted at quit about one run in three with "free(): invalid pointer". The glyph atlas
// (lib_ableem font.cpp, atlas()) is a function-local static made on the first drawn text; the Gui static is made
// before it, so the atlas was destroyed first and ~Renderer, run later from ~Gui, released its textures into a
// destroyed vector. This suite builds the same order: a static that owns a headless GuiBase is made first, a text
// is drawn (the atlas is made), and the process ends with the static's destructor running after the atlas's turn.
// The check is the process exit: before the fix it aborted or failed under ASan, after it the status is 0.
//
#include "doctest/doctest.h"

#include <ableem/ui/font.h>
#include <ableem/ui/gui_base.h>

#include <cstdio>
#include <cstdlib>
#include <exception>
#include <memory>
#include <string>

using namespace std;

namespace {

struct LateRelease {
    unique_ptr<ableem::GuiBase> gui; // destroyed at exit, like the launcher's Gui static
};

LateRelease &holder() {
    static LateRelease h; // made before the atlas, so destroyed after it - the failing order
    return h;
}

} // namespace

TEST_CASE("a text drawn on a renderer that a static destroys at exit: the atlas outlives it") {
    setenv("AB_HEADLESS", "1", 1);
    LateRelease &h = holder();
    try {
        h.gui = make_unique<ableem::GuiBase>("ab_test_font_atlas_exit", 320, 240);
    } catch (const exception &e) {
        const string why = e.what();
        // ctest counts exit 77 as skipped (SKIP_RETURN_CODE in tests/CMakeLists.txt), not as a pass
        fprintf(stderr, "SKIPPED test_font_atlas_exit: no usable renderer here (%s)\n", why.c_str());
        fflush(stderr);
        exit(77);
    }
    ableem::Renderer &renderer = h.gui->renderer();
    const string ttf = string(AB_LAUNCHER_RESOURCES) + "/fonts/RedHatText-Medium.ttf";
    ableem::Font font = ableem::Font::load(renderer, ttf, 18);
    REQUIRE(font.valid());
    renderer.setDrawColor(ableem::Color(0, 0, 0, 255));
    renderer.clear();
    font.draw(renderer, 10, 10, "atlas outlives the renderer");
    renderer.present();
}
