# Render performance plan (RELEASE-21, 2026-09-28)

Status: plan, no code yet (the owner: the whole plan first). Code references are launcher develop 20a6211 and
autobleem-core develop 3c51416.

## Why

An idle launcher takes a whole CPU core. In the PC-USB test VM (Mesa's software renderer, no GPU) a sandbox
launcher uses 100% of a core, the stick's own launcher 170%. On the PSC and the Pis vsync holds it at 60 fps,
but it still redraws everything 60 times a second while nothing changes - work, heat and power for no picture
change.

Causes, from the code:
- **Every screen is a busy loop.** `GuiScreen::loop` (core `lib_ableem/src/ui/gui_screen.cpp:22-179`) and the
  launcher's own loop (`evoui_launcher_input.cpp:22-166`) poll input without waiting (`SDL_PollEvent`) and redraw
  the whole frame on every pass. Nothing sleeps (`Platform::delay()` has no callers; the one exception is
  `GuiUpdateProgress`'s `delay(16)`). The only brake is vsync in `present()` - absent in a VM, headless, or a
  hidden window.
- **A launcher frame is ~74 texture copies and 18 texture switches** (`AB_FRAME_STATS=1`), whether anything moved
  or not.
- **Something always moves**, so "draw only when something changed" alone never idles: the play button breathes
  forever (`PsZoomBtn`) in the Games state, the arrow bounces forever (`PsMoveBtn`) in the Set state.
- **Four screens spin at 100% without drawing at all**: `GuiConfirm` (core `gui_confirm.cpp:52`), `GuiBtnGuide`
  (`evoui_btn_guide.cpp:87`), `GuiEditorRA` (`gui_game_editor_ra_menu.cpp:58`), `GuiSelectMemcard`
  (`gui_select_memcard.cpp:100`); also `GuiScreen::fastForwardUntilAnotherEvent` (`gui_screen.cpp:201`) while a
  direction is held and `Gui::criticalException` (`gui.cpp:149`). Extensions showing a `GuiConfirm` inherit it.
- **The DebugDriver reads every frame back**: its thread turns the frame cache on at start
  (`debug_driver.cpp:192`), so `present()` does a full `SDL_RenderReadPixels` each frame.
- **Per-frame system calls**: the launcher's footer-hint signature stats the RetroArch binaries every frame in the
  Games state (`hintSignature()` -> `Env::retroArchInstalled()`, `evoui_launcher_screen.cpp:910`); Game Manager
  does a `statvfs` every frame (`renderFreeSpace`, `gui_game_manager_menu.cpp:83`); the update download stats
  its `.part` file every frame.

Measured in the VM: the no-driver run is still 100%, so the redraw is the main cost, not the readback. One run
reached 1.2 GB RSS in 14 s - not explained yet (step A2).

## Goals

- An idle launcher (Games state, the play button breathing) at a few percent of a core in the VM, and a
  measurable drop on the PSC and a Pi.
- No visible change: the carousel scrolls exactly as smoothly as today, every animation keeps its speed.
- A screen can never freeze because a change was not noticed (fallbacks below).
- Everything measured before and after with the performance overlay.

## A - Measure first

**A1. The performance overlay** (the owner): Options -> Diagnostics -> "Show performance" (config.ini
`perfoverlay`, false by default; `AB_PERF_OVERLAY=1` for scripts). A small white text on a black box in the
bottom-left corner, on every screen - drawn by `Renderer::present()` itself, so the classic screens, the
extensions and the console tools show it without a change of their own. Updated once a second:
- FPS (frames presented in the last second) and the frame time (average / worst);
- the program's CPU load (% of one core, from the process's CPU time) and the whole system's;
- the CPU's hardware threads (`SDL_GetCPUCount`) and the program's own threads;
- the program's memory (RSS);
- the renderer's name (`opengl`, `opengles2`, `software`...) and, once B/C land, the frame rate the screen
  asked for (active / ambient / idle).
Linux reads `/proc/self/stat`, `/proc/self/status`, `/proc/stat`; Windows `GetProcessTimes`,
`GetSystemTimes`, the process memory counters; anything unavailable shows `-`. Its own built-in bitmap font
(no TTF - it works on every screen, before a theme loads, and costs one small texture); drawn after the frame,
so it never ends up in a cached layer. The option's label and description in all 16 language files.
**A2.** With the overlay and `AB_FRAME_STATS`: the baseline on the VM (the launcher's Games and Set states,
Options, Game Manager, the Quick menu, the Button Guide, a confirm) and the RSS question from the spike.

## B - Core: pacing, waiting, safety (lib_ableem + core's classic screens)

**B1. A frame cap in `Renderer::present()`**: when present() came back sooner than the frame budget (vsync did
not wait), sleep the rest. 60 fps by default, `AB_MAX_FPS` lower (a sandbox 20-30). Real vsync hardware is
unaffected.

**B2. A frame pacer** in `GuiScreen` (and used by the launcher's loop): a screen says each pass what it needs -
**active** (full rate), **ambient** (30 fps default, `AB_AMBIENT_FPS`) or **idle** (draw nothing; wake on
input or at the next timed change, at most 250 ms later so the polls keep running). Between frames it waits
for input instead of spinning: `SDL_WaitEventTimeout`, plus a wake-up from the DebugDriver's injected-event
queue (its events are not SDL events, `input.cpp:221-233`). Input latency stays one frame. The rule that keeps
screens from freezing: **anything not declared idle or ambient runs active** - a forgotten case costs CPU,
never a stale picture.

**B3. The spinning screens wait instead**: `GuiConfirm`, `GuiBtnGuide`, `GuiEditorRA`, `GuiSelectMemcard`,
`fastForwardUntilAnotherEvent` (keeps its repeat timing, sleeps between steps), `Gui::criticalException`.

**B4. Animations on time, not frames** - a cap would slow them: `GuiMcManager`'s card icons step every 6
passes (`evoui_mc_manager.cpp:366-372`), `GuiSplash` fades ±10 alpha a pass (`gui_splash.cpp:83-104`). Both to
milliseconds.

**B5. The DebugDriver's readback on demand**: `shot`/`grab` request the next frame (the existing
`captureNextFrame` path) and wake the pacer so an idle screen presents one; the per-frame frame cache goes.
`frames` and `wait_idle` keep working from the presented-frame counter.

**B6. Render-target safety**, needed before any cached layer:
- `setTarget` gets a stack (`pushTarget`/`popTarget`): today `TextRenderer::cachedRun`
  (`text_renderer.cpp:288/305`) and `PsCarouselGame::loadTex` (`carousel_game.cpp:69-147`) end with
  `setTarget(nullptr)`, which would silently send the rest of a layer to the screen.
- `Event::Type::RenderReset` (`input.cpp:430`, nobody handles it): drop every cached layer and the text cache,
  redraw everything.

**B7. The text cache**: at 512 entries it clears everything at once (`text_renderer.cpp:279-280`) - a burst of
re-rendering; an LRU instead. The scan and download bubbles make a new text texture for every distinct
"12/40" / byte count - their text updates throttled to 4 a second (B/C together).

**B8. Classic screens draw on change**: `GuiMenuBase` screens (Options, Game Manager, Memory Cards,
Playlists, the game-dir menu), the facts/hardware pages, the text page, the action menus, the set picker, the
extensions and processors lists, the update prompt - no looping animations there, so after the input's frame
they go idle; timed ones say when (the facts page's 1 s refresh, `GuiKeepDisplay`'s countdown, the keyboard's
caret at 2 Hz). `GuiAbout`'s starfield and the surprise game stay active (an easter egg, shown by choice).

**B9. Per-frame system calls out**: `Env::retroArchInstalled()` cached (refreshed with the 5 s battery poll
and after a launch); `renderFreeSpace` every 2 s; the `.part` size every 250 ms; `PsMeta`'s font fitting and
the word wrapping in `GuiAppStart`/`GuiAbout`/`GuiBtnGuide` cached per text instead of measured every frame.

## C - The launcher: layers

Today's draw order (`GuiLauncher::render`, `evoui_launcher_screen.cpp:1067-1132`), which the layers must keep:
background, footer, play button, **play text (ambient)**, settings panel, metadata panel, **arrow (ambient)**,
the Set state's labels -> **the carousel** -> the snap -> the icon row -> the footer hints -> the pad batteries
-> **the scan, extension and notification bubbles** -> the resume-slot selector -> the fade.

**C1. The layers**
| layer | what | redrawn when |
|---|---|---|
| **back** (cached) | background, footer, play button, settings panel, metadata panel, the Set labels | state, selection, metadata, theme or language changes; while the settings or metadata panel slides |
| ambient (live) | play text, arrow | every ambient frame (they sit between back and the carousel) |
| **carousel** (cached at rest) | the covers | every frame while it scrolls or the main cover rises/falls; baked at rest |
| **front** (cached) | snap, icon row, footer hints, pad batteries, resume selector | selection, icon-row animation, hints signature, battery poll, state |
| **bubbles** (cached) | scan, extension, notification bubbles | while one slides in/out; on a new text; at a hold's end |
| fade (live) | the black fade-in | the first 300 ms |
A frame at rest is then: back copy + two ambient sprites + carousel copy + front copy + bubbles copy - about 5
copies instead of 74. The overlay (A1) goes on top of all.

**C2. The carousel** (the owner): its own layer. At rest it is baked (one copy a frame). While it scrolls it is
drawn live at the full rate - the smoothness is the point of the cover flow - with back and front staying
cached, so a scroll frame costs the covers alone. The deferred cover loads (`loadOneMissingTexture`, one
off-screen decode per idle frame) run on the pacer's idle ticks, not as a reason to draw. Later, if the scroll
is still heavy on the VM: covers far from the centre (small, dark, barely moving) baked, only the moving ones
redrawn.

**C3. The notification bubbles** (the owner): their state changes move out of `render()` into an
`update(now)` (today a frame must run at `hideAt_` for the fade to start), with `animating()` and
`nextChangeAt()`; a bubble sliding or fading makes the frame active, a shown one is cached, and the pacer
wakes at the hold's end. The scan bubble's text throttled to 4 updates a second.

**C4. What makes the launcher active** (everything else: ambient while play text or arrow is visible, idle
otherwise): input in the last 2 s; `carousel.scrolling` or any cover's `animationStart != 0`;
`menu->animationStarted`; `settingsBack->animEndTime`; `meta->animEndTime`; `fadeAlpha > 0`; a bubble
animating; a busy frame; a held direction (the carousel's run).

**C5. Dirty marking**: every state change in the launcher (`setState`, the selection, `updateMeta`, the hint
signature, the battery poll, `loadAssets`, `reloadGames`, the scan's roster change, a return from any sub-screen)
marks the layers it touches. When unsure: mark all three - a wasted redraw, never a stale one.

**C6. Safety nets**: `AB_LAYERS=0` turns caching off (every frame drawn as today) - to compare and as the
fallback; a full re-bake at most once a second at idle catches anything missed. **A pixel test**: a DebugDriver
walk through the launcher's states with `AB_LAYERS=0` and `=1`, the frames compared - a difference is a
missing dirty mark.

## D - Sandboxes and the driver (the rest of RELEASE-21)

Virtual pads with padsim's vocabulary, batteries, `kbd combo`, clips, `wait_screen`/`wait_idle`,
`AB_INPUT_ISOLATED`, one script language with `abvm run`; the headless window size settable (today 1024x768,
the 16:9 canvas letterboxed); SIGTERM ends the launcher cleanly.

## F - The console: a newer SDL, and GLES only where needed (the owner, 2026-09-28)

The console is where the carousel costs most: its SDL is **2.0.14** (the ceiling today - autobleem-main
`docs/history/launcher-build.md`, "the wl_shell ceiling"), which has no `SDL_RenderGeometry` (2.0.18), so
`copyTrapezoid` takes its fallback of **one copy per output column** (`renderer.cpp:525-557`) - every turned
cover's face and spine drawn as a few hundred thin copies. The VM's 2.26 draws the same cover in one call. To
be confirmed first with the overlay and `AB_FRAME_STATS` on the PSC (A2).

**F1. A newer SDL on the console (preferred route): SDL 2.30.x, with the `wl_shell` window brought back.**
Why 2.0.14 is the ceiling, and what each blocker takes:
- **Sony's Weston 1.11 offers `wl_shell` only (no xdg shell); SDL 2.0.16 removed `wl_shell`.** A patch
  re-adding it as a fallback when the compositor has no `xdg_wm_base`: the shell surface, fullscreen
  (`wl_shell_surface_set_fullscreen`), the configure/ping handling - ported from 2.0.14's own code into 2.30's
  window code. The precedent: retroarch-psc's `wl_shell_fallback.patch` (202 lines) did the same for RetroArch
  1.22 and runs on the console.
- **SDL 2.0.20+ needs libwayland >= 1.18; the console has 1.12.** Two ways: (a) ship a newer
  **libwayland-client** in `libs.tar.gz` next to SDL (`/tmp/lib` comes first) - the wire protocol is stable, a
  1.22 client talks to a 1.11-era compositor, and libwayland keeps its ABI for the older users (the GPU's EGL
  blob among them); or (b) make the 1.18/1.20 calls optional in the patch (`wl_proxy_marshal_flags`,
  the proxy tags). (a) is less code; the risk is the PowerVR EGL blob and its `libwayland-egl` - checked first.
  The protocol code is then generated by the matching wayland-scanner (today a 1.12 one is built for this).
- **Other dependencies to check on the console**: xkbcommon (SDL 2.30 loads it dynamically), the EGL version
  the blob reports, ALSA.
- **What it gives beyond the carousel**: `SDL_RenderGeometry` everywhere (the trapezoid in one call, and
  shaded/tinted quads for later effects), the newer GLES2 renderer's batching, virtual joysticks with real
  vendor/product (2.24 - the DebugDriver's pads would work on the console too), years of game-controller and
  hidapi fixes (third-party pads, mappings), the audio fixes.
- **The cost and the risks**: every program on the console loads SDL from `/tmp/lib` - the launcher, the console
  tools, both PS1 emulators, the Apps - so one change reaches all of them at once. SDL2's ABI is backward
  compatible (no rebuild needed to keep working), but behaviour can change: window/fullscreen, vsync pacing
  (newer SDL paces Wayland with frame callbacks), input. The emulators are frozen as code; their runtime
  library changing means an owner session with the before/after fps check. The launcher and the console tools
  are rebuilt against the new headers to get `SDL_RenderGeometry`.
- **The steps**:
  1. Recon on the console (read-only): libwayland/EGL/libwayland-egl/xkbcommon versions and who links what; the
     compositor's globals.
  2. In `autobleem-build`: SDL 2.30.x with the `wl_shell` patch and the libwayland route, on its own image tag
     (the `:develop` image unchanged until it is proven); `ab-validate psc` taught the new version (it refuses
     anything but 2.0.12/2.0.14 today).
  3. **A/B on the stick**: both SDL builds in the libs pack, a marker file choosing the old one - so the owner
     can switch back without reinstalling, and every test compares the two.
  4. The console checks: the launcher (fullscreen, pads, audio, the overlay's numbers), the console tools, every
     App, both emulators (owner session, fps before/after), standby and power off, the Reset button.
  5. Then the launcher and console tools built against the new headers, and 2.0.14 kept as the fallback for a
     release or two.
- **Not a route**: replacing Sony's Weston with our own newer compositor, or SDL's KMSDRM backend without
  Weston - the GPU's blob (DDK 1.9, tied to kernel 4.4) comes with its EGL/Wayland integration for that Weston;
  swapping the compositor risks the whole display stack.

**F2. GLES only where needed (fallback, and for effects later)**: if F1 does not work out, a narrow GLES2 path
for `copyTrapezoid` alone - one quad with a perspective-correct shader instead of the column copies, behind the
same `Renderer` API, used only when the renderer is `opengles2`/`opengl` (`SDL_RenderFlush` +
`SDL_GL_BindTexture`, both in 2.0.14); everything else stays on SDL. No full rewrite of the renderer: SDL already
draws through GLES on the console and the Pis, and in the VM (no GPU) GLES would change nothing.

Order: F1's recon (step 1) right after A2 - it decides between F1 and F2; the rest of F after C, since at rest
the carousel is cached anyway and F mostly speeds up the scroll on the console.

## E - Checking it

- Unit tests (doctest): the pacer's rate decision (a pure function of the flags and times), the bubble's timed
  states, the text LRU, the target stack.
- The pixel test (C6) in the native suite where a renderer exists, and on the VM.
- The overlay's numbers before/after on the VM for the list in A2; in a console session on the PSC and a Pi.
- The extensions' ABI: `GuiScreen`/`Renderer` change layout - `AB_SDK_ABI` goes up; console-tools and ext_store
  are rebuilt with it (the CI and the appliance enforce it).

## Order and size

1. A1 overlay + A2 baseline (S)
2. B1 cap, B3 spinning screens, B4 time-based animations, B5 readback, B9 syscalls (S each)
3. B2 pacer + B8 classic screens on change (M)
4. B6 target stack + render reset, B7 text LRU (S-M)
5. C launcher layers, carousel, bubbles + C6 pixel test (L)
6. D driver/sandbox work (M-L)
7. E measurements on the console and the Pi (a console session)

Out of scope: the emulators (pcsx-abnxt is frozen - its own driver and pacing only in an owner session), the
Apps, the synchronous cover decode at the scroll's settle (a hitch, not idle CPU - noted for later).
