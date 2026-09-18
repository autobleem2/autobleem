# Launcher animation and renderer performance plan

Written 2026-09-18 after the cover-flow carousel (14 covers a side, drawn as perspective strips) and the 1.5x
output on a 1080p Pi went in. The complaint: scrolling the carousel looks "low fps" on the Pi 400, and 4x
MSAA took the launcher's idle CPU from ~17% to ~60% of a core there. This is a code review of every
animation path and of what the renderer does per frame, and the order in which to fix things. Nothing here
has been measured yet - step 0 is the measuring.

## 1. What the frame does today

`GuiLauncher::loop()` ([evoui_launcher_input.cpp:19](../src/code/evoui/screens/evoui_launcher_input.cpp))
runs once per vsync (`SDL_RENDERER_PRESENTVSYNC`): rebuild the four translated header strings, `update(time)`
on every `PsObj`, `menu->update`, `carousel.updatePositions()`, `scans().poll()`, `render()`, then the
auto-repeat check and the event poll. `render()`
([evoui_launcher_screen.cpp:538](../src/code/evoui/screens/evoui_launcher_screen.cpp)) draws, in order:
the static elements (background and panels - one full-screen copy each), the carousel, the snap, the menu,
three hint texts, the notification lines, the scan status line, the front elements, the fade.

What that costs, counted in `SDL_RenderCopy` calls, because on the GL backends that is the unit of CPU work
(each call is a batched quad; a texture or colour-mod change ends a batch and is a draw call):

| What | Calls per frame | Where |
|---|---|---|
| 28 turned covers, one strip per **output** column (1.5x: ~90-250 each) + the spine | **~4000** | `Renderer::copyTrapezoid`, [renderer.cpp](../lib_ableem/src/ui/renderer.cpp) |
| Text: every run is drawn **10 times** when the halo is on - 9 offsets in the shadow colour, then the text - and SDL_FontCache is one copy per glyph | **~2000-3000** (a dozen runs of ~20 glyphs) | `TextRenderer::drawRun` [text_renderer.cpp:250](../src/code/gui/text_renderer.cpp), `Font::drawColor` |
| Colour-mod changes: two per turned cover (spine, face), one per text pass | ~150 batch breaks | same |
| Everything else (background, panels, buttons, icons, menu) | ~40 | `PsObj::render`, `PsMenu::render`, `PsMeta::render` |

So an idle frame is ~6-7 thousand copies and ~200 draw calls, all of it CPU-side work in SDL's batcher on
a Cortex-A72 - that is the 17%. MSAA adds a full-screen resolve on the GPU per frame plus whatever the
driver does to wait for it, and at 1920x1080 that is the jump to 60%. None of this is the *animation*
code; the animation code is cheap. What makes a scroll look bad is a different set of things:

## 2. Why a scroll stutters

A scroll is `nextCarouselGame()` ([evoui_launcher_screen.cpp:592](../src/code/evoui/screens/evoui_launcher_screen.cpp)):
`carousel.scrollLeft(110)` starts a **110 ms** animation (6-7 frames at 60 Hz), and then, **in the same
frame, synchronously, on the main thread**:

1. `updateMeta()` -> `PsMeta::updateTexts()` (cheap: strings) -> `showOptions()` -> **`loadSnap()`**: a
   PNG decode from the SD card (`Texture::loadFile`) when the theme has a snap panel.
2. `menu->setResumePic(app.resumePoints().lastPicture(game))`: `lastPicture()`
   ([resume_point.cpp:104](../src/code/core/services/resume_point.cpp)) does up to 11 `DirEntry::exists`
   stats plus a file read in the game's `!SaveStates` folder, and `setResumePic()`
   ([evoui_menu.cpp:231](../src/code/evoui/controls/evoui_menu.cpp)) then **decodes the resume screenshot
   PNG** - every scroll, whether the menu is open or not.
3. At the *end* of the animation `updateVisibility()` -> `setInitialPositions()` -> `loadTex()` on the one
   game that just became visible at the far end: **a Named_Boxarts PNG decode** (they are 400-700 px) plus
   the composite into a render target, and `freeTex()` on the one that left. For a RetroArch set the
   cover path is a `ThumbnailLookup::findBoxArt` over the whole directory listing first.

Each of those is 10-40 ms on the Pi's SD card and CPU, i.e. one to three dropped frames out of the
seven the animation has - a hitch at the start of every scroll and another at its end. That, not the
draw-call count, is most of what reads as low fps.

Then the motion itself:

4. **Linear interpolation** everywhere: `Carousel::updatePositions()`
   ([carousel.cpp:238](../src/code/evoui/carousel.cpp)), `PsMenu::update()`, `PsMoveBtn::update()`,
   `moveMainCover()`. A cover that moves at constant speed and stops dead looks mechanical; the eye reads
   the stop as a stutter even when no frame was dropped.
5. **Auto-repeat is stop-and-go**: holding the stick runs `nextCarouselGame(60)` every 100 ms after 300 ms
   ([evoui_launcher_input.cpp:40-58](../src/code/evoui/screens/evoui_launcher_input.cpp)) - a 60 ms move,
   then 40 ms standing still, repeat. That is jerky by construction.
6. **Input is dropped while scrolling**: `if (!carousel.scrolling)` guards both the tap and the repeat, so
   a second tap inside the 110 ms is lost and the carousel feels unresponsive rather than smooth.
7. The turn `angle` and `scale` interpolate linearly with x too, so the cover swinging into the middle
   grows and turns at constant rate - fine, but it would follow the same easing.

## 3. The plan

In order. Each step is one commit; steps 1-3 are the ones that change what the user sees, 4-6 are the
renderer's cost, 7 is what MSAA needs to be affordable.

### Step 0 - measure first (`FrameStats`)

A tiny counter in `GuiBase` (or `Renderer`): frame time min/avg/max over the last second, copies and
batch breaks per frame (count them in `Renderer::copy`/`copyTrapezoid`/`setColorMod` paths), logged with
`PLOG_INFO` once every 5 s when `AB_FRAME_STATS` is set (env on a dev host and the Pi, never on the
console). Also a one-line `PLOG_INFO` from `PsCarouselGame::loadTex` and `Texture::loadFile` with the
milliseconds taken when they exceed 5 ms. Then the numbers below get measured on the Pi at 1x/1.5x,
MSAA 0/2/4, before anything else changes, so each later step can show its gain.

### Step 1 - take the disk off the scroll path

- `loadSnap()` and `setResumePic(lastPicture())`: run them when the carousel **settles**, not when the
  scroll starts - `GuiLauncher` keeps a `pendingMetaGameId` and does the loads in the first frame after
  `carousel.scrolling` goes false (or after a short 80 ms idle, so a held stick never loads them at all).
  Also skip `setResumePic` altogether unless the menu is showing (`state == Set`) - it is only drawn there.
- Covers: keep a **lookahead** of 2 games beyond each end loaded, so the game that becomes visible at the
  end of a scroll already has its texture, and load the lookahead one game per idle frame (`Carousel::
  loadOneMissingTexture()` called from `loop()` when nothing is animating). The texture budget goes from
  29 to 33 composited 226x226 (339x339 at 1.5x) RGBA textures - ~15 MB at 1.5x, fine on a Pi; on the
  console SideCovers itself may need to come down (see "console" below).
- Beyond that, the decode itself off the main thread: `Texture::loadFile` splits into `Image::load(path)`
  (SDL_image -> `SDL_Surface`, any thread) and `Texture::fromImage(renderer, image)` (main thread, cheap
  upload). A one-thread decode queue in `Carousel` (like `ScanService`'s worker, lowest priority) decodes
  the lookahead; the main thread only uploads. This is the only item that touches the library's API and
  can wait until step 0 shows the lookahead is not enough.

### Step 2 - easing and continuous motion

- One easing function in `core/model/timing.h` (`easeOutCubic(t)`, or a small enum so a screen can pick)
  applied in `Carousel::updatePositions()`, `moveMainCover()`, `PsMenu::update()`, `PsMoveBtn::update()`,
  `PsZoomBtn`. Same durations; only the curve changes.
- Held stick: when a scroll finishes and the direction is still held, start the next one **immediately**
  from the current position, with a shorter duration (80 ms) and a linear curve while held so the row
  moves at constant speed, and an ease-out only on the final step when the stick is released.
- A tap during a scroll is **queued** (one deep) instead of dropped: `Carousel` gets `pendingSteps`, and
  `updateVisibility()` starts the next scroll straight after `setInitialPositions()` when there is one.
- The scroll interpolates from `current` to `destination` - both slot positions - so a queued scroll that
  starts mid-animation must first snap `current = actual` (it does: `game.current = game.actual` in
  `scrollLeft/Right`), which is why chaining works without a visible jump.

### Step 3 - stop redrawing text ten times

The halo in `TextRenderer::drawRun` draws each run 10 times, per glyph, per frame - ~2500 copies for a
handful of labels that do not change between frames.

- Cache each rendered run as a texture: `TextRenderer` gets a small LRU (`{font, text, colour, shadow} ->
  Texture`, ~64 entries, built by drawing the halo + text once into a render target sized to the run).
  A frame then draws one copy per run. The meta panel, hints, set name, notification lines and the menu
  labels all go through `renderText*`, so they all benefit without changing.
- Cheaper alternative if the cache is not wanted: draw the halo with **4** offsets (the diagonals) at 2 px
  instead of 9 - it looks the same at 1.5x - and skip it entirely for text in the shadow colour's own
  luminance range. Halves the text cost; the cache removes it.
- `PsMeta::render` calls `fittingFont()` (which measures the title repeatedly) every frame; measure once
  in `updateTexts()` and keep the chosen `FontEnum`.

### Step 4 - one draw call per cover (`Renderer::copyTrapezoid` on `SDL_RenderGeometry`)

With SDL >= 2.0.18 (the Pi's 2.32, the PC's) a turned cover is **one** `SDL_RenderGeometryRaw` call: two
vertices per output column (the same per-column geometry and perspective-correct `u` as the strips - the
interpolation between neighbouring columns is over one pixel, so it is exact enough), a triangle strip
of ~200 vertices, the texture bound once, and the **colour per vertex** - so the spine's shade and the
face's Lambert darkening go into the vertex colour and the two `setColorMod` calls per cover go away
(and the face could be shaded as a gradient across its width for free). ~4000 copies and ~60 batch breaks
become ~28 draw calls. The strips stay as the `#else` for SDL < 2.0.18 (the console's 2.0.12), untouched.

Wider strips for the console's path: when the slope of a cover's top edge is under half a pixel per
column, 2-pixel strips are indistinguishable and halve the calls there.

### Step 5 - draw only what changed (optional, after 0-4)

When nothing animates - no scroll, no menu transition, no notification timing out, no fade, no scan
line change - the frame is identical to the last one. `GuiLauncher::loop()` can `present()` the previous
frame without redrawing... except that SDL's back buffer is not guaranteed to survive `present()`
(KMSDRM double-buffers), so the honest version is: render the static part (background, panels, carousel
at rest, texts) into a full-screen render target once per change and copy that one texture per frame,
drawing only the animated elements on top. Worth it only if step 0 shows the idle CPU still matters
after 3 and 4 (it will be the text and the covers, which 3 and 4 remove). Not for the console yet - it
needs the render target at output size.

### Step 6 - the per-frame small stuff

- `loop()` rebuilds `headers`/`texts` (8 translations, string copies) every frame; do it once in
  `loadAssets()` and on a language change.
- `PsMeta::render` loads its 11 icon textures lazily and keeps them - fine - but `Texture::size()` is an
  `SDL_QueryTexture` per call; `PsObj` caches `w/h`, the carousel passes `src` explicitly; audit the
  remaining `size()` calls in per-frame paths.
- `Carousel::render` sorts 28 pointers per frame (`stable_sort`): trivial, keep.
- `scans().poll()`: a mutex and an empty vector swap per frame: trivial, keep.

### Step 7 - MSAA, once the frame is cheap

With steps 3 and 4 the CPU side of the frame is small, so what MSAA costs is the GPU resolve, which is a
fixed price per frame at the output size. Then - agreed with the owner, 2026-09-18 - **the test on the Pi
is 2x** (`AB_MSAA=2` in the unit's environment needs no rebuild), measured against 4x and off with step 0's
numbers;
2x is likely enough at 1.5x output (the slopes are shallow, the strips are already sub-pixel placed) and
is half the resolve. Keep 0 on the console. If even 2x is too much at 1080p, the fallback is the
transparent-margin trick (2 px of transparent border in the composed cover texture, so the linear filter
fades the edge in) - it costs nothing per frame and was the plan before MSAA turned out to be two lines.

## 4. Targets (to be filled in from step 0)

| | Pi 400, 1.5x, MSAA off | after step 3+4 | with MSAA 2x |
|---|---|---|---|
| idle frame, copies | ~6500 | < 200 | same |
| idle CPU | ~17% (measured, `top`) | < 5% | ? |
| scroll: dropped frames | 1-3 per scroll (not measured) | 0 | 0 |
| worst frame during scroll | ? | < 16 ms | < 16 ms |

## 5. The console

None of this has run on a PlayStation Classic yet; its SDL is 2.0.12 (strips, no `RenderGeometry`, no
`RenderCopyF`) and its GPU is a Mali-450 on a 720p mode. The safe defaults there: SideCovers may need to
drop to 10 (fewer textures and strips), MSAA stays 0, the text cache from step 3 is the one item that
helps it most because the halo cost is the same on any GPU. Steps 1 and 2 are pure logic and apply
everywhere.

## 6. Not doing

- A GL/GLES renderer of our own. Everything above stays inside `ableem::Renderer`'s API, which is the
  seam for that if it is ever wanted; the reasons to want it (MSAA control, real quads, shaders) are
  covered by steps 4 and 7 on SDL >= 2.0.18. Revisit after the console has run once and its GL situation
  is known.
- Threads for rendering. SDL's renderer is single-threaded by contract; only decoding (step 1's last
  item) may leave the main thread.
