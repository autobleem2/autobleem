# Atari VCS 800 port plan - AutoBleem as a sideload / PC-Mode appliance

**Status (2026-09-22): researched against the real OS, not started. A single on-hardware test gates the
whole shape of the project** (see "The one test that decides everything"). A tester with a VCS 800 unit is
available; no devkit is needed - the SDK and OS are public.

This supersedes the "Port to Atari VCS 800 as a sideloaded app reading a USB stick" entry in
[`docs/IDEAS.md`](IDEAS.md), which is now trimmed to a pointer here.

## What the Atari VCS 800 is

The 2021 Atari console: an AMD embedded APU (Ryzen "Bobcat"/Zen R1606G, **x86-64**, Radeon Vega 3), 8 GB RAM,
32 GB eMMC. Two modes:

- **Atari Mode** - the stock, locked-down launcher (a Unity dashboard). Third-party software is sideloaded
  here as a **`.bundle`** run by a `homebrew-daemon`, inside a **sandbox**.
- **PC Mode** - boots a *different* OS from an external drive; unplug the drive and it boots back to Atari
  Mode. This is just "the VCS as a normal x86-64 PC."

## Ground truth from the OS image (not guessed)

The owner downloaded Atari's own recovery image (`atari-flasher-ab-upgrade.img`, a GPT flasher: EFI +
`flasher-storage` holding `atari.img.gz`). Unpacking `atari.img` (GPT: A/B EFI, A/B **dm-verity** rootfs,
`var`, 10.9 GB `storage`) and reading `rootfs-A` gave the following, all first-hand:

| Thing | Fact |
|---|---|
| OS | **Apertis v2021** (`ID_LIKE=debian`, = **Debian Buster amd64**), built by **Collabora**; image `20250512.072359.production`; A/B + dm-verity read-only root |
| Compositor | **Weston (Wayland)**, `--shell=kiosk-shell.so --xwayland`, output HDMI-A-1 1920x1080 - *the same Wayland/Weston shape as the PSC console AutoBleem already runs on* |
| SDL2 | preinstalled `libSDL2-2.0.so.0.16.0` (**2.0.16**), and it is Atari's **patched SDL2** ([github.com/atari-vcs/libsdl2](https://github.com/atari-vcs/libsdl2)) with **built-in SDL_GameController mappings** for both pads. SDL2_ttf 2.0.15 present; **SDL2_image / mixer / net are NOT** - bundle those. |
| GPU | OpenGL + Vulkan preinstalled with driver support (amdgpu/Mesa) |
| Controllers | Atari Classic `USB 0x3250:0x1001` (name "Atari Classic Controller"), Modern `0x3250:0x1002` ("Atari Game Controller", generic layout); rumble via SDL force-feedback; `PreferXBoxMode` flag if middleware needs 360 IDs. The Classic's twist axis needs the SDL Joystick API (it maps as hat 0). |
| Network | permitted from a bundle (no Store access) |
| Sideload runtime | `homebrew-daemon` (a **user** systemd service, `WantedBy=atari-session.target`); installs/logs/coredumps via a Homebrew web UI |

### The sandbox - the decisive constraint

Straight from the OS's own `homebrew-daemon` docs (`/usr/share/homebrew-daemon/doc/`), a sideloaded bundle
runs in **a lightweight container** and may touch only:

- **read-only**: its own unpacked bundle (its working directory),
- **read/write**: a private, per-user, per-bundle **`$HOME`** (path unpredictable, isolated from every other
  bundle and every other user),
- **read/write**: `/tmp` (RAM-backed, volatile),
- in `/dev`: **HID devices only**.

It states plainly: do not assume access to any other directories. **A USB stick auto-mounted by udisks2 at
`/media/...` is outside the container**, so an Atari-Mode bundle **cannot be assumed to read it**. That
kills the original premise ("AutoBleem in Atari Mode reading your PS1 library off a USB stick") for the
sandboxed path. Process spawning within the bundle is fine (the docs' own library-bundling pattern `exec`s a
sibling binary; the community guide backgrounds a helper), so launching a **bundled** `pcsx-ab` works - it
just saves into `$HOME`, not a stick.

## The SDK (public, no gatekeeping)

- [`atari-vcs/bundle-gen`](https://github.com/atari-vcs/bundle-gen) - Docker builder, `make-bundle.sh spec.yaml`
  -> `<name>_<version>.bundle`. Builds inside the Atari VCS OS variant regardless of host. YAML: bundle
  metadata (`Name`, `Type`, `HomebrewID`, `Exec`, `Launcher`, `PreferXBoxMode`, ...) + a build section
  (`RequiredPackages`, `RequiredModules`, `BuildCommand`, `Executables`, `Libraries` (system deps
  auto-detected), `Resources`).
- [`atari-vcs/vcs-build-container`](https://github.com/atari-vcs/vcs-build-container) -
  `ghcr.io/atari-vcs/vcs-build-container:base`, the AtariOS/Apertis userland as a container for build + ABI
  checks (the `bundle-gen` base).
- [`atari-vcs/native-example-bundle`](https://github.com/atari-vcs/native-example-bundle) - an SDL2 game
  handling both pad types; the literal template.
- A bundle is a **ZIP with `bundle.ini` at the top level** (<=5 GiB), `Exec` relative, CWD = bundle root.
- Manual/library-bundle path (no `bundle-gen`): a `launcher.sh` that sets `LD_LIBRARY_PATH=$P/lib` and
  `exec`s the binary - identical to how the Windows/PC packages already stage their libs.

## The one test that decides everything

The docs say "don't assume" USB access - not "it is blocked". Before committing to either path, the tester
installs a **~2-line probe bundle** (native `Exec` that lists `/media`, `/run/media`, `/proc/mounts` and
prints `$HOME`, results read from the Homebrew web UI / a file in `$HOME`) with a USB stick plugged in.

- **If a mounted stick is visible from inside the sandbox** -> Path A becomes the full USB product in Atari
  Mode.
- **If it is not** (the likely outcome) -> the USB experience is Path B; Path A is only ever a self-contained
  bundle.

The same probe bundle should confirm the launcher renders under Weston and sees the pads (link SDL2, open a
window, log controllers) - de-risking the whole app side in one hardware round-trip.

## Path A - Atari Mode `.bundle` (self-contained)

AutoBleem as a homebrew tile, data in the private `$HOME`, not a USB stick. Works today in principle; the
open problem is **getting a game library into the sandbox** (no arbitrary USB, Store-blocked; only network or
the web UI). Good for a demo or a small built-in/downloadable set, **not** a USB console-replacement unless
the test above passes.

Work:

1. **New build target.** Add `AB_TARGET=atarivcs` to the platform model
   (`src/code/core/services/environment.h` - it is `AB_APPLIANCE` + `AB_ROOT_RELATIVE_LAYOUT`, no internal
   games, x86-64 native). Root `CMakeLists.txt` needs a native-x86-64 branch that does **not** overwrite
   flags the way the PSC `^arm` branch does (the same pitfall `pcusb` already dodges). `Env::platformName()`
   -> `atarivcs`, `resources/platform/atarivcs.ini` (mirror `pcusb.ini`; `launch_mode=script` with
   `rc/launch.sh`, or `direct`; `download_command`/`repo_url` optional).
2. **A real native appliance build.** `make_sys.sh` today builds a **dev** build (`AB_TARGET` empty ->
   `AB_DEBUG_HOST`: splash runner, keyboard-as-pad, windowed). Add a native appliance build (extend
   `make_sys.sh` or a new script) configured with `AB_TARGET=atarivcs`. Fullscreen via the Windows product's
   existing `SDL_WINDOW_FULLSCREEN_DESKTOP` path; no KMS (Weston owns the display), so `SDL_VIDEODRIVER`
   left to wayland/x11.
3. **Root = `$HOME`.** `EnvironmentSetup::fromRoot()` already is "one root holds Games/, System/, themes/".
   The bundle wrapper passes `$HOME` (or a subdir) as that root. No new path logic; the games/covers/config
   live there.
4. **pcsx-ab native x86-64 Linux.** `E:\Programming\pcsx-rearmed-develop` has `make_win/psc/rpi/rpi64` but
   **no native-Linux build** - add one (it is `make_win.sh` minus the MinGW toolchain: interpreter core, no
   ARM dynarec, same as the Windows x86-64 build). Same for pcsx-abnxt if both are wanted. The bundle ships
   the binary; it saves into `$HOME`.
5. **Package.** A `bundle-gen` YAML: `Type: Game`, `Exec: launcher.sh`, `PreferXBoxMode: true`,
   `Resources: src/resources`, bundled SDL2_image/mixer/ttf (SDL2 itself is preinstalled - do not bundle it).
   `launcher.sh` is the `LD_LIBRARY_PATH` + `exec autobleem-gui $HOME` wrapper. Built in
   `vcs-build-container` for the right Buster/Apertis ABI.
6. **Tile.** `bundle.ini` metadata; install via Atari Homebrew.

## Path B - PC Mode USB appliance (the actual USB experience)

Boot AutoBleem as a full OS from an external drive in PC Mode: full hardware, full filesystem, real USB
access, persistent. This is **the existing** [*"PC Linux build that boots from a USB stick"*](IDEAS.md) idea,
and the VCS is its **easy** case - one known, mainline GPU (amdgpu/Mesa), so the arbitrary-hardware risk that
idea flags is gone. Reuses the Pi/PC-USB appliance pattern almost line for line (a systemd unit owning the
session, SDL kmsdrm or Weston, native x86-64 packages). This is where "read the stick" genuinely works; it
is just not an Atari-Mode tile. If the owner wants the USB console-replacement on a VCS, this is the path -
and it is mostly the PC-Linux-USB work, retargeted to known hardware.

## What is already free either way

- `make_sys.sh` already builds a native x86-64 Linux `autobleem-gui`.
- `EnvironmentSetup::fromRoot()` is exactly the one-root layout both paths need.
- Weston/Wayland + a patched SDL2 with pad mappings = the app and input layers work unchanged (the PSC
  proves the Weston path).
- Fullscreen rendering exists (the Windows product).

## Open questions

1. **USB visibility inside the sandbox** - the gating test above. Decides A-with-USB vs B.
2. Which path the owner wants first (self-contained Atari-Mode demo, or the PC-Mode USB appliance).
3. For A: how a library gets into `$HOME` if the USB test fails (network fetch? a built-in sample set? the
   web UI is debug-only and impractical for large libraries).
4. pcsx-ab / pcsx-abnxt native x86-64 Linux build (new script in the emulator repos) - unverified but
   low-risk (the Windows x86-64 interpreter build already exists).
5. Whether the sandbox's per-user, wiped-per-new-user `$HOME` model is acceptable for A (each console user
   gets a fresh, empty AutoBleem).

## Notes

- The unpacked OS is proprietary; it was read purely for interop facts (versions, paths, config). **Nothing
  from it is redistributed or bundled** into AutoBleem.
- Build/ABI work belongs in `vcs-build-container` (Debian Buster amd64); functional testing is the tester's
  unit only (GPU, pads, USB, the tile).
