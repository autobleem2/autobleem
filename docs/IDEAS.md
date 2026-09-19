# AutoBleem - ideas for future development

A running list of things that might be worth doing, before they're worth planning. Each entry is a quick
"is this possible, and how big is it" writeup - not an implementation plan. When an idea is picked up for
real, it gets its own plan (and, once done, this entry is removed or marked done - see "Finished plans
leave docs/" in CLAUDE.md's spirit, though a *researched-but-not-started* idea can just stay here).

Rough complexity scale used below: **S** (a sitting, one file or a script), **M** (a few files, one
subsystem, a day or two), **L** (touches several subsystems / new infrastructure), **XL** (a new toolchain,
target, or a build pipeline change).

---

## Prebuilt Raspberry Pi image for Raspberry Pi Imager

**Status:** implemented (2026-09-19, `tools/make_rpi_image.sh`, `payload_rpi/system/autobleem-firstboot.*`,
`tools/rpi_imager_repo.json` - see CLAUDE.md's "Flashable image for Raspberry Pi Imager"), **not yet run on
real hardware**.

Ship the Pi port as a flashable `.img.xz` (alongside the existing tarball + `install.sh`) that Raspberry Pi
Imager's "Use custom" can write directly, with Imager doing hostname/user/WiFi/SSH/locale and AutoBleem's
own setup finishing itself on first or second boot.

- **Is it possible:** yes. Imager accepts a plain `.img`/`.img.xz` with a small JSON manifest
  (`init_format`, `extract_sha256`, ...) for "Use custom" with a local JSON, or a hosted repo JSON via
  `--repo`. Since our image starts from an official Raspberry Pi OS Lite image untouched on the
  cloud-init/firstrun side, Imager's normal customisation keeps working - we just inherit whatever
  `init_format` the base image already declares.
- **Build host:** the Pi 400 itself (native ARM, no qemu/chroot needed for an injection-only build -
  loop-mount the base image's boot/root partitions, drop files in, no code runs inside the image at build
  time). `psc-build` (the x86 remote) is not a fit - no sudo there.
  `WSL2` is a possible alternative later (not installed on this dev machine today - needs the optional
  component + a reboot) if a chroot-based (pre-installed packages) build is wanted down the line.
  Docker was not checked.
- **Complexity: L for an injection-only MVP** (stock Lite + the AutoBleem package + a first-boot systemd
  service that runs `install.sh --yes`, disabling itself once done, re-arming on the next boot if it
  couldn't reach the network yet). No pre-installed packages, no pre-built RetroArch - first boot is exactly
  as slow as today's manual install, just triggered automatically instead of typed by hand.
  **XL** if packages/RetroArch get pre-baked into the image (needs a chroot-capable host; armhf-on-aarch64
  needs `qemu-arm-static`/binfmt, arm64 chroot is native on the Pi's own 64-bit kernel).
- **Open questions before building:** exact tool availability on the Pi 400 for image manipulation
  (`losetup`, `kpartx`, `xz -T0` multi-core) - not yet verified live (no sudo password on hand during
  research); how big the recompressed image + xz time turns out to be on a Pi 400.
- Full plan: [`docs/rpi-image-and-update-plan.md`](rpi-image-and-update-plan.md) (Part 2).

---

## Fast in-place update for an already-installed Pi

**Status:** researched, not started (came up alongside the image idea above, but stands on its own).

Two asks: (1) `install.sh`, re-run against a Pi it already set up, should detect that and take a fast path
- skip repartitioning, package installs, the RetroArch source build, and the core/BIOS/thumbnail downloads,
  and only refresh what changed. (2) a way to update without ssh at all: drop a new release tarball into a
  folder on the exFAT data partition from a PC, applied automatically at the next boot.

- **Is it possible:** yes, and cheaply - no new runtime dependencies needed.
- **Mechanism:** a sha256-diff copy (compare each payload file's hash against what's already installed,
  copy only what differs) is the dependency-free equivalent of an rsync delta and works identically whether
  the source is a freshly-extracted tarball (the drop-on-partition path) or the staged tree over ssh (the
  `install.sh` re-run path) - one implementation, two entry points.
- **Where the boot-time apply would hook in:** `payload_rpi/system/autobleem-session.sh` already loops
  around starting `autobleem-gui`; an update check/apply step before that loop starts (writing progress to
  `/dev/tty1`, since the service's stdout goes to the journal, invisible before the launcher owns the
  screen) is a natural fit and needs no new service.
- **Complexity: M.** Contained to `payload_rpi/install.sh` and `payload_rpi/system/autobleem-session.sh`
  (plus a new small shared shell helper for the diff-copy). No new tools, no image work.
- Full plan: [`docs/rpi-image-and-update-plan.md`](rpi-image-and-update-plan.md) (Part 1).

---

## PC Linux build that boots/runs from a USB stick, like the original console

**Status:** researched, not started.

A minimal Linux install on a USB stick that boots straight into AutoBleem full-screen on a PC - the same
appliance idea as the Raspberry Pi port, but x86/x86_64 and with arbitrary PC hardware instead of one known
SoC.

- **Is it possible:** yes, and a good chunk of it already works today for free. `autobleem-gui` already
  runs natively on Linux (`make_sys.sh` - "a plain host build into `build_sys/`", no MinGW shims, no
  cross-toolchain) and the 1-arg command line (`autobleem-gui /path/to/usb-root`) is exactly
  `EnvironmentSetup::fromRoot()` - the "everything under one root" layout the Pi port already uses
  (`src/code/core/services/environment_setup.cpp:16`). So the app side of this is not new work; what's
  missing is entirely the *distro/appliance* side - getting a PC to boot into that binary with nothing else
  running.
- **X11/Wayland turn out not to be needed.** SDL2 talks to the DRM/KMS device directly (`SDL_VIDEODRIVER=kmsdrm`),
  which is exactly what the Pi's `autobleem.service` already sets (`payload_rpi/system/autobleem.service`) to
  draw straight to the screen from a plain systemd service on a virtual terminal, no compositor. The same
  should work on a PC with a KMS-capable driver (Intel/AMD's kernel modesetting drivers, and Nouveau for
  Nvidia - the closed Nvidia driver is the one common case that does *not* do KMS the same way and would be
  the one real per-machine risk). A desktop session (X11 or Wayland) is only needed if we *don't* go the
  direct-KMS route - simplest to treat "minimal Linux + kmsdrm, no X/Wayland at all" as the default plan, the
  same architecture as the Pi, and keep a full desktop as a fallback for the Nvidia case.
- **What actually is new work: the bootable-USB-appliance layer.** Three ways to get there, in rising order
  of effort:
  1. **Install a minimal existing distro straight onto the stick** (Debian/Ubuntu Server minimal, or Alpine
     Linux - Alpine specifically has a documented "diskless"/USB-install mode plus an `apkovl` overlay
     mechanism built exactly for "always boot straight into one app" appliances), then apply a genericized
     version of the Pi's own playbook on top: a systemd unit that owns tty1 the way
     `payload_rpi/system/autobleem.service`/`autobleem-session.sh` do, `SDL_VIDEODRIVER=kmsdrm`, packages
     from the distro's own repo (SDL2, RetroArch) instead of cross-compiled. This reuses the Pi port's
     pattern almost line for line - no new mechanism, just a new base OS and native (not cross-compiled) x86
     packages.
  2. **`debootstrap`-built custom root, from scratch**, written straight to the USB device: partition,
     format, `debootstrap` a minimal Debian root, install a kernel + systemd + SDL2 + GRUB (both BIOS and
     UEFI boot entries, since PCs vary - unlike the Pi's fixed firmware), then the same appliance layer as
     above. More control over what's on the stick and its size, but it's a real "build a Linux system from
     packages" script, closer in spirit to `tools/make_rpi_image.sh` (see the Pi image idea above) than to
     anything in this repo today.
  3. **A proper Buildroot/Yocto image**, the way dedicated emulation distros (Lakka, Batocera) do it - full
     control, smallest/fastest-booting result, but a new build toolchain to adopt and maintain, not
     justified until the simpler approaches are proven out.
- **The one real hard problem a fixed-hardware Pi target doesn't have:** unknown PC hardware. The Pi image
  work targets one SoC family; a PC USB stick has to boot and drive video/audio/input across whatever
  motherboard/GPU/Wi-Fi chipset it's plugged into. That's exactly the problem Lakka/Batocera already solve
  by shipping a broad kernel driver set - worth looking at what driver/firmware breadth they carry as a
  reference point before committing to approach 1 or 2 above.
- **Complexity: L** for approach 1 (existing minimal distro + genericized Pi appliance layer - most of the
  mechanism already exists in this repo, ported rather than invented). **XL** for approach 2 (a real
  from-scratch Linux-on-a-stick builder) or approach 3 (a new build system entirely).
- **Open questions before building:** which approach the owner wants to start with; BIOS vs. UEFI boot
  support (probably both, via GRUB); Nvidia's closed driver as a known gap either way (falls back to a
  desktop session in that case rather than kmsdrm); whether "persistent" (writes/saves survive, like the
  console's USB stick) needs a full installed OS (approach 1/2, trivially persistent - it's a normal
  filesystem) versus a live/squashfs image with an overlay (only relevant if approach 3 is ever pursued).

---

## Port to Atari VCS 800 as a sideloaded app reading a USB stick

**Status:** researched, not started.

The Atari VCS 800 is a real, confirmed device: an AMD "Bobcat"/Zen-based embedded APU (R1606G, x86_64,
Radeon Vega 3 graphics), 8 GB RAM, 32 GB flash, running a Debian-based OS Atari calls **Atari Mode** - a
locked-down launcher UI - with a separate **PC Mode** that boots an entirely different OS from an external
drive (disconnect the drive, it boots back into Atari Mode). [Atari's own support
docs](https://support.atari.com/hc/en-us/articles/17386515521563-Operating-System) and
[technical specs](https://support.atari.com/hc/en-us/articles/17386531014043-Technical-Specifications)
confirm the hardware and OS; community sources describe homebrew/sideloaded content as distributed via
**AppImage** (the standard single-file Linux app format) and note that plugging in a USB drive is what
"unlocks the open Sandbox" for sideloading, with no separate developer registration needed for that.

This idea is specifically "AutoBleem as a tile inside Atari Mode's own launcher, reading games off a USB
stick" - not the same as booting PC Mode from a USB drive, which is really just a generic x86_64 PC in an
Atari-shaped box and would be the [PC Linux USB idea above](#pc-linux-build-that-bootsruns-from-a-usb-stick-like-the-original-console)
with one specific, well-known, already-mainline-supported GPU (amdgpu/Mesa) instead of arbitrary hardware -
notably *easier* than that idea on the driver-compatibility front, if it ever came to that.

- **What already works, unchanged:** `make_sys.sh` already builds a native Linux x86_64 `autobleem-gui`
  (see the PC-Linux-USB idea above), and `EnvironmentSetup::fromRoot()` (1-arg command line) is exactly
  "point me at a USB-stick-shaped root and I'll find Games/, System/Databases/, themes/ under it" -
  precisely what "reads a USB stick" needs, no new environment/path logic. Gamepad input goes through
  `ableem::Input`/`Joystick` over SDL2's normal joystick/game-controller API
  (`lib_ableem/include/ableem/ui/joystick.h`), which should see Atari's Bluetooth Classic/Modern
  controllers as ordinary SDL controllers the same way it already does on Windows/Linux/Pi - Atari's own
  material describes both controllers as "PC compatible."
- **What's genuinely new and unverified (no public SDK access to check these without a devkit or a unit):**
  1. **How a sandboxed AppImage actually gets its window/screen.** Atari Mode's own launcher almost
     certainly owns the display (some compositor, X11 or Wayland - not confirmed which); a sideloaded app
     is unlikely to get raw DRM/KMS master the way the Pi appliance does, so this would run as an ordinary
     windowed/fullscreen-windowed SDL2 app under whatever session Atari Mode provides, not as a boot-owning
     appliance - simpler in one way (no systemd/tty1 plumbing to write) but means the direct-KMS trick from
     the PC-Linux-USB idea doesn't apply here.
  2. **Filesystem access to an arbitrary USB stick from inside the sandbox** - whether a sideloaded AppImage
     can read any mounted USB drive by path, or only a specific app-private storage folder Atari's sandbox
     hands it. This decides whether `fromRoot()` can point straight at the stick or whether the games need
     to live inside the app's own sandboxed data directory instead (still workable, just a different root).
  3. **Whether the sandbox allows spawning a second process** - `LaunchService`/`System::runAndWait` fork
     and exec pcsx-ab as a separate binary next to the launcher; if AppImage sideloads run under any kind of
     per-app process/container isolation that blocks executing a sibling bint binary, PCSX launches would need
     a different mechanism (or the whole thing would need to be one static binary with PCSX linked in,
     unlike every other AutoBleem target today).
  4. **pcsx-ab's own x86_64 buildability** - `pcsx-rearmed-develop` is developed and tested there for ARM
     targets (console, Pi); whether it already has a working plain x86_64 Linux build (interpreter core, no
     ARM dynarec, same situation as any desktop Linux build) needs checking in that repo, not this one.
- **Complexity: L**, *if* items 1-3 above turn out to be permissive (ordinary windowed SDL2 app, USB path
  access, normal process spawning all allowed) - then this is mostly packaging (`make_sys.sh` build +
  resources into an AppImage) plus verifying pcsx-ab builds for x86_64, no new subsystems. **Jumps to XL**
  if the sandbox turns out to restrict any of those (would need e.g. a different launch mechanism for PCSX,
  or a rethink of where "the USB stick" lives from the app's point of view).
- **Open questions before building:** getting hands-on with an actual VCS 800 unit (or at minimum,
  developer documentation beyond the public support site - Atari's own "Developers" support page exists but
  wasn't readable during this research pass) is the real blocker to answering points 1-3 above; everything
  else is groundwork this repo already has.

---

## A visual platform-select screen for the RetroArch roster (Recalbox-style system carousel)

**Status:** researched, not started.

Right now, choosing which RetroArch playlist ("system"/"platform") to browse is a plain vertical text list
(`GuiPlaylists`, `gui/menus/gui_playlists_menu.h`), opened with L2+Select while the carousel is on the
RetroArch set (`GuiLauncher::loop_chooseRAPlaylist()`,
[evoui_launcher_actions.cpp:147](../src/code/evoui/screens/evoui_launcher_actions.cpp#L147)). The idea is to
replace or front that with a visual, horizontally-scrolling system-select screen - a row of console
logos/tiles you scroll through with the pad, the way Recalbox/EmulationStation-style frontends show their
system carousel before you drop into a platform's game list.

- **Is it possible:** yes, and the state that already exists lines up well:
  - `RetroArchService::playlistNames()`/`gameCount()` already give the launcher the exact ordered list this
    screen would show (`src/code/core/services/retroarch.h:57-58`).
  - `GameSetSelection` already carries `raPlaylistIndex`/`raPlaylistName` as first-class selection state
    (`src/code/core/model/game_set.h:36-37`) - a platform screen is choosing among values that already exist,
    not inventing new state.
  - The **carousel machinery itself is exactly the right building block already**: `Carousel`/
    `PsCarouselGame` (`src/code/evoui/carousel.*`) already do "big box" tiles at their own aspect ratio with
    a 9-slice frame (`BoxKind::BigBox`, used for RetroArch games and Apps today), cover-flow turning/scaling
    for off-center items, and - notably - **empty-box placeholders when there's no art**
    (`PsCarouselGame::emptyBox()`), which is precisely the fallback a system with no logo yet would need. No
    new rendering engine is needed; this is "one more thing populates a `Carousel`," the same pattern the
    PS1/RetroArch/Apps rows already use.
  - `LauncherScreenState` (`src/code/evoui/screens/evoui_launcher.h:25`) is a small, deliberately explicit
    enum (`Games, Set, Resume, Info`) that a `Platform` state would join cleanly, or this could be its own
    small screen (closer to how `GuiSystemMenu` is its own file) reached the same way `loop_chooseRAPlaylist()`
    is today - either shape is a contained, in-pattern change.
- **The one real gap: there is no system-logo artwork anywhere in this repo today** (checked - no
  `xmb`/console-logo/system-logo assets under `lib_ableem/`, `payload/`, or `src/resources/`). A Recalbox-style
  wheel lives or dies on that art, and it doesn't exist yet for the ~50+ systems `RA_ROM_SYSTEMS` in
  `payload_rpi/install.sh` lists. Options, in rising effort: start with **text tiles** (system name rendered
  onto a plain tile via `TextRenderer`, the same "no art yet" fallback the empty-box placeholder already
  models) so the screen works day one; then source or commission real per-system logos later (community
  logo packs exist for ES/Recalbox-style themes, but licensing/attribution would need checking per pack,
  the same care already taken for e.g. the bundled `NotoSansSC-Regular.otf` font's OFL license); a theme
  could eventually ship its own system-logo set the way it ships jewel-case/big-box frame art today.
- **Complexity: M.** Once the "no art yet" fallback is accepted for a first pass, this is almost entirely
  UI-layer work reusing existing `Carousel` plumbing and existing `RetroArchService`/`GameSetSelection`
  state - no new engine or service code. The open-ended part is sourcing/creating real logo art per system,
  which is content work, not architecture work, and can land incrementally after the screen itself works.
- **Open questions before building:** whether this *replaces* `GuiPlaylists` outright or sits in front of it
  as a nicer entry point (with the text list kept as a fallback/dense view - useful once a scan produces
  many systems); where in the input flow it's reached (L2+Select as today, or promoted into the main
  Select-cycle as a dedicated step, e.g. "RetroArch systems" between RetroArch and Lightgun in
  `nextGameSet()`); whether Favorites/History (which sit among the RetroArch playlists today) get their own
  tiles in this row or stay reached another way.

---

## Port pcsx-ab changes from AutoBleem-NG's emulator fork

**Status:** researched, not started.

The frontend port ([[autobleem-ng-port]]) worked because our `src/code` snapshot was byte-identical to a
specific commit of `AutoBleem-NG/autobleem`, so the 122-commit delta was a clean `git diff`. **pcsx-ab is a
different shape of problem**, confirmed via the GitHub API (not guessed from a rendered page):
`AutoBleem-NG/pcsx_rearmed_psc` is its own fork of **`notaz/pcsx_rearmed`** (upstream), created 2025-12-20 -
not a fork of `autobleem/PSC-pcsx-rearmed` or of our own `pcsx-ab2` lineage. Our
`E:\Programming\pcsx-rearmed-develop` also traces back to `notaz/pcsx_rearmed` but diverged from it
independently, years earlier, via a file-copy import (its own first commit is literally "Import pcsx-ab with
a CMake build..." - no shared history). **The two forks have no common ancestor to diff against each other
directly** - this has to be read as a feature list and re-implemented against our tree, not cherry-picked or
rebased.

One piece of good news: the NG fork was **archived (read-only) on 2026-09-19** - it stopped moving right as
this was being researched, so whatever is there now is a fixed target, not a moving one.

- **The actual size of their PSC-specific work is small and well-isolated.** Comparing
  `AutoBleem-NG:psc-autobleem` against `notaz/pcsx_rearmed:master` (GitHub's compare API - most of the
  branch's "2,026 commits" are just notaz's own upstream history pulled in by periodic merges): **14 real
  commits, 25 files changed**. Two groups:
  - **Build-system files** (`Dockerfile`, `Makefile.psc`, `config.mak.psc`, `.dockerignore`) - their own
    Docker/GCC9 cross toolchain, not reusable as-is (we build on the Sony crosstool-NG toolchain via
    `make_psc.sh` on the remote build server - see CLAUDE.md's Build section). Skip these outright.
  - **Source changes**, worth assessing one at a time rather than as a block:
    - **`frontend/psc_m3u.c`/`.h`** (new, ~165 lines total) - minimal in-frontend `.m3u` playlist parsing
      that drives an eject->next-disc swap for **separate per-disc image files** (`.cue`/`.bin`/`.chd`). This
      directly closes a gap our own notes already flag: *"pcsx-ab's standalone frontend has no .m3u support
      (only its libretro build)"*. Our tree does already have multi-disc support of a narrower kind
      (`cdrIsoMultidiskSelect`/`swap_cd_multidisk()` in `frontend/menu.c`, confirmed locally) - but that's
      for a single **embedded**-multi-track image (`.pbp`); it's exactly the case their own header comment
      says it can't handle ("the menu's PBP-multidisk path can't handle for separate CHD files") that
      `psc_m3u.c` adds. Self-contained, small, a clear and narrow win.
    - **`frontend/psc_eject.c`/`.h`** (new, ~118 lines) - a 3-function module (`init`/`poll`/`finish`) that
      reads an eject input and drives `psc_m3u_advance()`. Small, depends on the m3u module above.
    - **`libpcsxcore/misc.c`** - a one-line change worth taking on its own merits regardless of the rest:
      their save-state header is a fixed string (`"STv4 PCSXra AutoBleem-NG"`); ours is currently
      `"STv4 PCSX v" PACKAGE_VERSION` (confirmed in our tree, `libpcsxcore/misc.c:1012`) - tied to the build's
      version string, so a save state's header changes on every version bump. Pinning ours the same way is
      a one-line, low-risk fix with an obvious benefit (save states stay loadable across our own rebuilds).
    - **`frontend/psc_launcher.c`/`.h`, `frontend/psc_input.h`** - AutoBleem launcher-argument and BIOS
      handling as their own modules. Our tree already does equivalent work, differently shaped: `-region`/
      `-filter`/`-ratio`/`-enter` are parsed inline in `frontend/main.c` (confirmed locally). This is
      probably "different implementation of something we already have" rather than a clean gap - their two
      specific commit messages here ("map AutoBleem BIOS selection", "guard async SPU menu option") are
      worth reading individually in case either is a bug fix ours is still missing, rather than assuming the
      whole module ports over.
    - **`frontend/plat_sdl.c`** (the biggest single diff, +268/-148), plus smaller changes to `frontend/menu.c`/
      `.h` and `frontend/plugin_lib.c` - **not reviewed in this pass**, and the highest-risk bucket: our own
      tree independently modified its SDL2 video path (`9e50ed4`, "Add a Windows (MinGW) development build
      and run the SDL2 video path on it"), so their changes and ours may already overlap or conflict. Needs
      an actual side-by-side read of both diffs before deciding what, if anything, to take.
- **Complexity: S** for the savestate-header pin alone. **M** for `psc_m3u`/`psc_eject` together (small,
  self-contained, a real and specifically-documented gap closed). **Unknown, possibly M-L** for the
  `psc_launcher`/`plat_sdl`/`menu.c` bucket until someone actually reads those diffs against our current
  code - flagged rather than estimated blind.
- **How to port it, concretely, when picked up:** clone the archived fork (`git clone --branch psc-autobleem
  https://github.com/AutoBleem-NG/pcsx_rearmed_psc` - archived repos stay clonable, just not writable) into
  a scratchpad, treat `psc_m3u.c`/`.h` and `psc_eject.c`/`.h` as near-drop-in additions to
  `pcsx-rearmed-develop/frontend/`, wire the eject/advance calls into our own `main.c`'s launcher-arg path
  and `menu.c`'s CD-swap menu entry, and read the `misc.c`/`plat_sdl.c`/`psc_launcher.c` diffs by hand rather
  than merging them mechanically - there's no shared history for `git cherry-pick`/`git merge` to work with.
- **Open questions before building:** whether m3u playlists are something AutoBleem's own scanner should
  start generating for multi-disc USB games once pcsx-ab can read them (today `GameScanner::mergeMultiDiscFolders`
  hands pcsx-ab only the first disc's `.cue` and generates the `.m3u` for RetroArch only - see CLAUDE.md's
  "DiscSuffix" note); what exactly `psc_launcher.c`'s two bug-fix commits do, unread so far.

---

## Real CRT/scanline shader simulation in pcsx-ab (GLES/GL, RetroArch-style)

**Status:** researched, not started.

- **What "scanlines" means in pcsx-ab today is much cruder than a shader.** The only working implementation
  is a software post-process in `frontend/plugin_lib.c` (`pl_vout_flip()`, `scanlines`/`scanline_level` from
  the on-screen menu): it darkens every other output row by a percentage, on the CPU, via
  `bgr555_to_rgb565_b()` with a blend level - an ARM-NEON-only code path (`#ifdef __ARM_NEON__`), 16bpp
  only. No curvature, no phosphor/aperture-grille mask, no bloom - just alternate-row darkening, the classic
  "cheapest possible" scanline trick.
- **There's a second, more promising rendering path already in the tree, but its "shader" hooks are dead
  code.** `plugins/gpu-gles/` builds as `gpu_gles.so` when the console build turns on `PCSXAB_GLES`
  (`make_psc.sh` does: *"PCSXAB_GLES on: EGL on Weston, gpu_gles.so"* - confirmed in `CMakeLists.txt:45`,
  whose own help text spells out the two output paths side by side: *"OpenGL ES output through EGL on a
  Wayland surface (the PlayStation Classic's Weston). Off means video goes through an SDL2 renderer +
  streaming texture - what a Raspberry Pi on KMSDRM uses"*). So there are genuinely two blit backends today:
  raw GLES/EGL on Weston (console only, `PCSXAB_GLES=ON`), and SDL2's `SDL_CreateTexture`/`SDL_RenderCopy`
  (`frontend/libpicofe/plat_sdl.c` - Pi/KMSDRM and, when GLES is off, the console too). **Both are plain
  texture blits with no shader stage** - `gpu-gles/gpuPlugin.c`'s own `SetScanLines()` and
  `XPRIMdrawTexturedQuad()` are empty function bodies, leftover from when this file was a full 1999-era
  hardware-accelerated OGL1 PS1 GPU plugin (its own source comment: *"OGL1 plugin is a 'shader-free' zone"*)
  - not something this fork ever finished, not something that quietly already works.
- **The GPU emulation itself doesn't need to change.** `gpu_neon`/`gpu_unai` (the plugins actually doing PS1
  polygon rasterization, software/NEON) are unaffected either way - a CRT shader is a *display-stage*
  post-process over the already-rendered frame, exactly how RetroArch itself applies its shader zoo over a
  software-rendered PS1 core's output, not something the rasterizer plugin needs to know about. That keeps
  this from touching the trickiest, most performance-sensitive code in the tree.
- **What's actually missing is shader plumbing, on both backends:**
  - SDL2 path (Pi, and console when GLES is off): `SDL_Renderer`'s high-level API (what `plat_sdl.c` uses
    today) has **no custom-fragment-shader hook** in SDL2 - only SDL3 exposes a GPU/shader API. Getting a
    real shader here means bypassing `SDL_CreateRenderer`/`SDL_RenderCopy` for the final blit and dropping
    to raw GL/GLES calls instead (the window is already created with `SDL_WINDOW_OPENGL`, so a GL context is
    reachable, just not currently used that way) - own framebuffer/texture, own shader compile+link, own
    quad draw. New code, not a config flip.
  - Console GLES/EGL/Weston path: closer to ready in spirit (a real GLES context already exists via
    `gpu_gles.so`), but the concrete hookup point is `plugins/gpu-gles/gpulib_if.c` specifically - most of
    the rest of that directory (`gpuDraw.c`, `gpuPrim.c`, `gpuTexture.c`) is the same vestigial full-GPU-plugin
    code the empty scanline stubs live in and is very likely dead weight for what's actually used (the
    "output" blit only), not a working shader pipeline to extend. This needs a direct read of
    `gpulib_if.c` before estimating further, not an assumption either way.
- **A real starting point exists, just not in this codebase**: RetroArch's own shader zoo has GLSL ports of
  the well-known CRT shaders (`crt-easymode`, `crt-lottes`, `zfast-crt`, ...) - AutoBleem's own Pi installer
  already fetches a `shaders_glsl` bundle from `buildbot.libretro.com/assets/frontend/shaders_glsl.zip` for
  RetroArch itself (`payload_rpi/install.sh`'s `download_retroarch_content()`). That's RetroArch's own
  shader *loader* infrastructure, not reusable directly by pcsx-ab, but the shader *source* (GLSL fragment
  shaders, mostly self-contained math over a source texture + screen coordinates) is a real, permissively-
  licensed (RetroArch's shader repos are public/BSD-ish per-shader) reference to port one or two of the
  simpler ones from, rather than deriving CRT math from scratch.
- **Complexity: M** for a first cut on **one** backend done properly - pick the console's GLES/EGL path (the
  more authentic "CRT on real PSC hardware" case, and the one with an existing GL context to build on) or
  the SDL2 path (reaches the Pi and PC builds too, more testable on a dev host first), add a minimal
  GL/GLES2 shader compile+link+quad-draw utility, wire one ported GLSL CRT shader through it, expose
  on/off + maybe an intensity value the same way `scanlines`/`scanline_level` already do in the menu. **L**
  to do both backends properly and keep them behaviorally consistent, since they're genuinely separate code
  paths today with no shared abstraction between them.
- **Open questions before building:** read `gpu-gles/gpulib_if.c` to confirm exactly what it does today
  (display-only blit, as the CMake help text implies, or something more); whether GLES2 (the console's
  `GLESv1_CM`/`GLES_CM` library names in `CMakeLists.txt:101` look more like GLES1 - worth double-checking
  the console's actual GLES version, since GLES1 has no programmable shaders at all and would need a
  GLES2/3 context instead) is actually available on the PSC's GPU, since a fixed-function GLES1 pipeline
  can't run a fragment shader at all; performance headroom on both the PSC's embedded GPU and a Pi at
  1080p/60 for a full-screen shader pass every frame, on top of what the carousel/launcher already costs
  (see `docs/IDEAS.md`'s sibling entries and CLAUDE.md's carousel-perf notes for how tight that budget
  already is on a Pi).

---

## DuckStation as an alternate PS1 core on powerful machines (PC, Pi 5, ...)

**Status:** researched, not started. **One finding below (licensing) should be resolved/accepted before any
other work on this idea, not discovered partway through.**

- **The gating fact: upstream DuckStation forbids redistributing a modified build.** Its README states it
  plainly: *"As per the terms of CC-BY-NC-ND, redistribution of unmodified releases and code is permitted"*
  but *"You are not permitted to publish modified versions of this repository"* - confirmed directly from
  `stenzek/duckstation`'s own README, not a secondhand summary. This is a real relicense, not a licensing
  quirk to work around: DuckStation was GPL-3.0 until it, and a fork preserving the pre-relicense commit
  exists and is still GPL-3.0 - `Trixarian/duckstation-gpl` (confirmed via the GitHub API: license
  `GPL-3.0`, not archived, 31 stars/5 forks). Its own history makes the cutover explicit - the fork's
  earliest relevant commit is literally titled **"Reverting to before License change"** (2024-09-24), and
  everything since then (last push 2026-02-18) is cosmetic (README URL cleanup) - **it does not backport
  post-relicense upstream fixes or features**. So "**customized** DuckStation" has exactly two honest paths,
  with a real tradeoff between them:
  1. **Don't modify DuckStation at all** - drive the official, unmodified upstream binary entirely through
     its command line and config files, the same arm's-length relationship AutoBleem already has with
     RetroArch (`RetroArchService`, a separate installed/downloaded binary, never patched or redistributed
     by this repo). This sidesteps the redistribution restriction completely (nothing of DuckStation's own
     code is modified or republished) and is current with upstream, but "teach DuckStation to ..." has to
     mean "find the setting/flag that already does it," not "add code."
  2. **Build on `duckstation-gpl` if real source changes are wanted.** Legally clean (GPL-3.0), but frozen
     ~18 months behind upstream and not maintained as a tracking fork - any real customization here means
     either living without that year and a half of upstream fixes, or taking on the job of rebasing it
     forward, which is its own significant undertaking.
- **The specific ask - "make states on exit" - looks like it may already just be a setting, under path 1.**
  DuckStation's documented CLI already has `-resume` (*"Load resume save state. If a boot filename is
  provided, that game's resume state will be loaded, otherwise the most recent resume save state will be
  loaded"*) as a first-class concept distinct from numbered save slots, which strongly suggests an
  auto-save-a-resume-state-on-exit behavior already exists as a togglable setting - this needs confirming
  against DuckStation's actual settings/ini documentation (not found in this research pass), but the
  vocabulary alone ("resume state," separate from slot 1-N) is a good sign it's not new engineering.
- **Where this fits architecturally: a fourth `LaunchService` path, not a pcsx-ab replacement.**
  `LaunchService` already has three shapes (PCSX, RetroArch, an App's own `startup`) picked by game and
  mode; "DuckStation for powerful machines" reads as a new mode alongside RetroArch's "Play using RA" -
  built, invoked with the right CLI flags and config paths for a given game/resume-point, not a wholesale
  swap of the PS1 path the way `RetroArchService` already coexists with pcsx-ab rather than replacing it.
  That also naturally answers "which machines": whichever platform's `.ini`
  (`resources/platform/<platform>.ini`, the existing `PlatformConfig` mechanism used for
  RetroArch's binary/core paths) names a DuckStation binary at all - Pi 5/PC, never the PSC itself (armv8
  QEMU/JIT-recompiler performance on the console's actual A53-class CPU + the CC-BY-NC-ND terms both argue
  against it; DuckStation itself lists x86-64/AArch64/armv7 support, no PSC-specific build exists or would
  make sense to chase).
- **"All the functionality we have now" is the bigger unknown, and mostly a matter of confirming DuckStation
  equivalents exist, not building them:**
  - **Resume-slot picture thumbnails** (what `ResumePointService::pictureForSlot`/the resume-slot menu
    show) - **unconfirmed** whether DuckStation's save states embed a screenshot that can be read without
    loading the state, or expose one as a sidecar file. This is the one item that could actually block
    parity with the current resume-slot UI rather than just needing a config flag, and needs checking against
    DuckStation's save-state format docs directly.
  - **Memory cards** - DuckStation defaults to **per-game** virtual memory cards, a different model from
    AutoBleem's shared, swappable `!MemCards` sets (`MemcardService::swapInForLaunch`/`swapOutAfterLaunch`).
    Whether DuckStation's per-game memcard directory is redirectable to a path AutoBleem controls (so the
    existing card-management UI keeps meaning something for DuckStation-launched games too) is unconfirmed
    in this pass - the search results only established that the per-game default exists and is described as
    the "safest" option, not whether/how it's configurable.
  - **Multi-disc** - DuckStation has native, standard `.m3u` support, which is *better* than pcsx-ab's
    current standalone-frontend situation (see the pcsx-ab/AutoBleem-NG porting idea above) - a genuine net
    gain here, not a gap to close.
  - **BIOS/region selection, aspect/filter** - DuckStation is generally more automatic about these
    (game-database-driven) than pcsx.cfg's explicit per-game values; likely simpler than pcsx-ab here, not
    harder, but the concrete equivalent of `GameSettingsService`'s pcsx.cfg writer (a DuckStation-side
    per-game ini it also supports, per the CLI docs' "per-game state") needs reading, not assuming.
  - **Lightgun support** - not checked in this pass at all; needed for parity with
    `LightgunService`/`GameSet::Lightgun`.
- **Complexity: M** for path 1 (unmodified upstream binary, driven entirely by CLI flags and its own config
  files) - shaped like the existing RetroArch integration (`RetroArchService`, `LaunchService`'s RetroArch
  path, a platform-ini-named binary), most of the work is confirming DuckStation's own settings cover what's
  needed and writing the `LaunchService` glue, not new emulator code. **L-XL** for path 2 (real source
  changes), and gated on the strategic call of building on a stale GPL fork rather than an engineering
  estimate.
- **Open questions before building:** which path the owner wants (unmodified-binary integration is very
  likely the right default given the licensing terms); DuckStation's actual save-state/screenshot format;
  whether per-game memcard paths are redirectable; lightgun support; and reading DuckStation's real settings
  documentation (not just CLI `-help` text) for the exact resume-on-exit/BIOS/region equivalents once this
  is picked up.

---

<!-- Add new ideas below this line, same format: Status / possibility / complexity estimate / open questions -->
