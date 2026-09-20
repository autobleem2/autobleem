# PC targets plan: PC-USB and PC-Windows

The plan approved on 2026-09-20 (from `~/.claude/plans/`), kept here while it is being executed and deleted when every step is done, as the Pi plan was. **Status** at the bottom.


## Context

The console (PSC) and Raspberry Pi packages are at "alpha - does the job". The next focus is the PC, as two
products shown as two tabs under one "PC" platform on the download site:

- **PC-USB** - a bootable USB stick image for **32-bit x86 PCs** (older CPUs included), built from
  **Debian 12 Bookworm i386** (the last Debian with an i386 kernel - Trixie has none; Bookworm LTS runs to
  mid-2028), behaving exactly like the Pi appliance: boots straight into the launcher on tty1 (SDL kmsdrm, no
  X), games on an exFAT data partition on the same stick, the first boot = the framebuffer install UI +
  `install.sh`, and the same online self-update (UpdateService -> pending.json -> `autobleem-update` ->
  `install.sh --update`). "Live boot" = boots from the stick on any PC without touching its disks, persistent
  like the Pi card (not squashfs/overlay). Flashed with Rufus (DD mode) / Etcher / dd - no Pi Imager.
  Debian publishes no raw i386 disk image, so the image is **built from packages** (mmdebstrap + GRUB for
  BIOS, UEFI-ia32 and UEFI-x64).
- **PC-Windows** - an **NSIS installer, per-user, no admin**: app in `%LOCALAPPDATA%\Programs\AutoBleem`, data
  tree default `%USERPROFILE%\Documents\AutoBleem` (changeable). RetroArch, when ticked, is the **official
  `RetroArch-Win64-setup.exe` run silently** (`/S /D=<data>\RetroArch\bin`) + cores/bundles from buildbot.
  Self-update: the launcher downloads the next `AutoBleemSetup-<v>.exe` and runs it `/S`.
- **pcsx-ab on both** (new win64 + i386 dist targets in `E:\Programming\pcsx-rearmed-develop`); RetroArch's
  pcsx_rearmed core stays the fallback `install.sh` already implements. No x86 dynarec - interpreter.

What the exploration pinned as the real gaps (file:line in the phases): `AB_DEBUG_HOST` is a CPU heuristic
(`environment.h:13` - an i386 Linux build would compile as the *console*); no zero-arg root discovery and
`config.ini` lives in the resources dir (`environment_setup.cpp:56`, `config.cpp:16,103`);
`System::runAndWait` is a `-1` stub on Windows and every launch goes through `rc/*.sh` (`system.cpp:158`,
`launch.cpp:31-38`); the image builder assumes a 2-partition Pi MBR + `cmdline.txt`
(`make_rpi_image.sh:314,460`); `install.sh` dies on any arch but armhf/arm64 and `configure_boot` is all
cmdline.txt/config.txt (`install.sh:211,1473`); the site has one-level tabs and `rpi/retroarch/latest.json`
is hard-coded in `update_service.cpp:191`.

This file becomes `docs/pc-targets-plan.md` (phase G) and is deleted when every step is done, as the Pi plan was.

**Order of execution: PC-USB first, then Windows.** The phases below are lettered by subject, not by order;
run them as **A -> D -> E (F2 alongside) -> B -> C (F1 alongside) -> G**. Phase A (the platform model) is the
prerequisite for both and stays first.

## Decisions (made; the "why" in one line)

| Question | Decision |
|---|---|
| Build-flavour model | One CMake cache string **`AB_TARGET` = `psc \| rpi \| pcusb \| win \| dev`** -> exactly one `AB_PLATFORM_<X>` define; derived macros in `environment.h`: `AB_DEBUG_HOST` (= `dev` only), `AB_APPLIANCE` (rpi \|\| pcusb), `AB_ROOT_RELATIVE_LAYOUT` (!psc), `AB_HAS_INTERNAL_GAMES` (psc \|\| dev). Today's `AB_PLATFORM_RPI` checks are re-keyed by what they *mean*. |
| Windows product vs dev build | Separate configuration `-DAB_TARGET=win` (`ci/build.sh win`, `make_win.sh --product` -> `build_win_product/`). `make_win.sh` default stays `dev` = today's semantics, so `tools/win_drive.ps1`/keyboard-as-pad/splash runner are untouched. |
| `config.ini` on Windows | New `Environment::setStateDir()/getPathToStateDir()` (default = working path -> psc/rpi/pcusb unchanged); `win` sets it to `<data>\System`. The 9 *writers* move (`config.cpp:16,103`, `gui_options_menu.cpp:282`, `online_assets.cpp:141,232`, `scan_service.cpp:81,88`, `games_hierarchy.cpp:197,212`, `game_scanner.cpp:82,344`); `getWorkingPath()` stays "the resources dir" for its readers. |
| pcsx-ab run dir on Windows | **`-dotdir/-biosdir/-pluginsdir` options in pcsx-ab** (`frontend/main.c:112 make_path()` is the one seam), not junctions (junctions need NTFS - a data root on an exFAT stick would fail). Windows emulator built with plugins off (peops GPU/SPU built in on non-ARM). |
| RetroArch `.dll` cores | Platform-ini key `core_extension=.dll` -> `Env`, used by `retroarch_cores.cpp:89` and the `retroarch_core` default. Data, not `#ifdef`. |
| Launch on Windows | `LaunchPlan {exe, args, cwd}` built by `LaunchService::planPcsx/planRetroArch()`; ini key `launch_mode=script\|direct` picks script argv (psc/rpi/pcusb/dev) or direct exec (win). The plan is data -> testable on any host. |
| cmd-window flashes | `System::runShellCommand()` (POSIX `system()`, Windows `CreateProcessW cmd.exe /c` + `CREATE_NO_WINDOW`) becomes the default `CommandRunner` of `OnlineAssets` and `UpdateService`. curl.exe ships with Windows 10+. WinINet stays in `apps/installer`. |
| Launcher window during a game | `ProcessRunner::minimisesLauncherWindow()` (true for `WinProcessRunner`) -> `Gui::minimize()/restore()` (`SDL_MinimizeWindow`, `SDL_RestoreWindow`+`SDL_RaiseWindow` in lib_ableem). `needsExclusiveDisplay()` false. |
| Fullscreen | **Always, on every real target - launcher, pcsx-ab and RetroArch alike** (the owner's rule: a seamless, console-like experience; no option, no toggle). PC-USB: kmsdrm *is* the whole display; pcsx-ab and RetroArch there run on kmsdrm the same way. Windows: the launcher opens `SDL_WINDOW_FULLSCREEN_DESKTOP` (`Gui::setFullscreen(true)` static before the first `getInstance()`, like `setWindowTitle`; `outputScale = min(w/1280, h/720)`), pcsx-ab is started fullscreen-desktop (F1 adds `-fullscreen` to its frontend, and the Windows build defaults to it), RetroArch gets `--fullscreen` plus `video_fullscreen=true` in the cfg we write. Only the `dev` build keeps a 1280x720 window (`win_drive.ps1` needs it). |
| Order of work | **PC-USB first, Windows second** (the owner's call): A -> D -> E (+F2 in parallel) -> B -> C (+F1) -> G. |
| Windows setup helper | **Extend `apps/installer/`**: a second job `windows_install_job.*` and a second exe `AutoBleemWinSetup.exe` from the same tree (shared `installer_core`, `win32_window`, `WinInetDownloader`, `InstallListener`, `TarArchive/ZipArchive/Sha256/PackCatalog`). Not a new app. |
| RetroArch on Windows | `RetroArch-Win64-setup.exe /S /D=<data>\RetroArch\bin` (`/D=` last, unquoted, no trailing `\`). **Phase C step 1 verifies its `RequestExecutionLevel`**; if `admin`, fall back to `RetroArch.7z` + bundled `7zr.exe`. Version pinned in the site's `win/retroarch/latest.json` (published by hand like `psc/retroarch`). |
| Kernels on PC-USB | Both `linux-image-686-pae` and `linux-image-686`; GRUB picks with `cpuid -p` in our own `/etc/grub.d/10_autobleem` (`10_linux` disabled), hidden menu, 2 s. Verify `cpuid -p` in Bookworm's GRUB 2.06 at the first image build; fallback = PAE only + a note. |
| i386 CPU flags | `-march=i686 -mtune=generic` (no SSE2) - Debian's own i386 baseline; Pentium M / Athlon XP class boots. `-D_FILE_OFFSET_BITS=64`. |
| Rootless image build | Default `--rootless` (mmdebstrap `--mode=unshare` -> ext4 without root -> `sfdisk` on the file, `mformat/mcopy` ESP, `grub-mkimage` + `grub-bios-setup` on the file), plus `--mount` for root / `docker run --privileged`. **Phase E starts with a half-day spike** that decides which one the server supports (`docker/run.sh` runs `-u uid:gid` with no subuid entry; the user is in the docker group, so `--privileged` is available without sudo). |
| RetroArch update catalog | Ini key `retroarch_catalog=rpi/retroarch/latest.json` (pcusb: `pc/retroarch/latest.json`, win: empty = no RA check) -> `UpdateService::Config::retroarchCatalog`, replacing the literal at `update_service.cpp:191`. Site dirs: `pc/retroarch/`, `pc/cores/`, `pc/images/`, `win/bios/`, `win/retroarch/`. |
| Payload layout | `git mv payload_rpi payload_linux`; **one** `install.sh` with `PLATFORM=rpi\|pcusb` (detected, `--platform` overrides) and per-platform functions (`preflight_<p>`, `configure_boot_<p>`, `set_wifi_country_<p>`, boot dir). `Autobleem/bin/emu-i386/` next to `emu/` and `emu-arm64/`. |

## Phase A - platform model refactor (no behaviour change; every target still builds)

**Files**
- `CMakeLists.txt`: `set(AB_TARGET "" CACHE STRING ...)`; empty -> `^arm|aarch64` = `psc` else `dev`;
  `AB_TARGET_RPI=ON` kept one release as an alias (deprecation message). `add_compile_definitions(AB_PLATFORM_<UPPER>)`.
  `ABLEEM_EMBEDDED_TARGET` forced ON for `psc|rpi|pcusb` (not by processor string, L106). `AB_ONLINE_UPDATE`
  OFF only for `psc`. `apps/` gates (L376, L383): pscbios/abflashkit for `psc|dev`; updateroms/installer for `dev|win`.
- `toolchains/rpi/common.cmake:55` -> `set(AB_TARGET rpi CACHE STRING "" FORCE)`; `toolchains/psc/PSCtoolchainV8.cmake` -> `psc`.
- `src/code/core/services/environment.h:13-15,31-33` -> the derived macros above; `#error` unless exactly one
  `AB_PLATFORM_*`. Drop `PI_DEBUG` (a Pi as a dev host is `-DAB_TARGET=dev` on the Pi).
- `environment.cpp:19` `platformName()`: `psc/rpi/pcusb/win/pc`.
- Re-key: `autobleem.cpp:58` backup_internal.sh -> `AB_PLATFORM_PSC`; `game_query.cpp:22`, `gui_options_menu.cpp:90`
  -> `AB_HAS_INTERNAL_GAMES`; `gui_options_menu.cpp:107` Updates row -> `AB_ONLINE_UPDATE && (AB_APPLIANCE || AB_PLATFORM_WIN)`;
  `gui.cpp:36` 1.5x scale -> `AB_APPLIANCE || AB_PLATFORM_WIN`; `gui.cpp:56` MSAA 4 on `dev|win`, 0 elsewhere;
  `system.cpp:43` powerOff shuts down on `AB_APPLIANCE`, else exit; `app.cpp:53` update keys -> a compile-time
  table (`rpi`/`rpi64` by `__aarch64__`, `pcusb`+`i386`, `win-setup`, dev env vars as today);
  `evoui_launcher_actions.cpp:749` -> `AB_APPLIANCE`; `autobleem.cpp:43` runner: dev splash / else fork (win in B).
- `src/code/core/services/platform_config.{h,cpp}` + `Env`: keys `launch_mode`, `core_extension`,
  `retroarch_catalog`, `pcsx_dir` (relative to the resources dir; win only).
- Inis: `src/resources/platform/pcusb.ini` (= rpi.ini + `retroarch_catalog=pc/retroarch/latest.json`),
  `rpi.ini` + `retroarch_catalog=rpi/retroarch/latest.json`; all get `launch_mode=script`, `core_extension=.so`;
  `pcusb.cores.cfg` = `rpi.cores.cfg`. (`win.ini` in B.)
- `make_win.sh --product` flag (used in B).

**Tests**: `tests/core/test_platform_config.cpp` - the new keys, defaults, and a loop loading every
`src/resources/platform/*.ini`.

**Verify**: `./make_win.sh` green (ctest, format, lint); on the server `docker/run.sh ci/build.sh native psc rpi
rpi64 win` green; the Pi tarball's `install.sh --update` on the Pi 400 once - nothing visible changes.

## Phase B - Windows product runtime (testable from the zip + a hand-built pcsx-ab.exe)

**B1 - state dir + zero-arg root**
- `lib_ableem/include/ableem/engine/environment.h` + `.cpp`: `setStateDir/getPathToStateDir`; the 9 writers switch.
- `core/services/environment_setup.{h,cpp}`: `struct HostFacts {programDir, registryDataRoot, pointerFileDataRoot,
  documentsDir}`, `fromWindowsInstall(const HostFacts&)`: precedence HKCU `Software\AutoBleem\DataRoot` ->
  `dataroot.txt` next to the exe (portable zip) -> `<Documents>\AutoBleem`; working path = programDir (resources
  next to the exe), state = `<data>/System`, covers db = `<data>/System/Databases`, Games/Themes/System/RetroArch
  under `<data>`; creates the tree, copies `<program>/Themes/*` into `<data>/Themes` when missing.
  `fromArguments()` gains the zero-arg case under `AB_PLATFORM_WIN` only (1/2 args keep working).
  New `core/services/windows_host.{h,cpp}` (`_WIN32`): `RegGetValueW`, `GetModuleFileNameW`, `SHGetKnownFolderPath(FOLDERID_Documents)`.
- Tests: `test_environment_setup.cpp` - the three precedences with injected `HostFacts` (every host);
  `test_config.cpp` - config.ini lands in the state dir.

**B2 - LaunchPlan + WinProcessRunner + direct launch**
- `core/services/launch.{h,cpp}`: `LaunchPlan`, `planPcsx(game, resume)` / `planRetroArch(game)`; direct mode:
  pcsx exe `<resources>/<pcsx_dir>/pcsx-ab.exe`, cwd that dir, args `-dotdir <ssFolder> -biosdir <System/Bios>
  -filter F -ratio A -lang L -region 4 -enter 1 [-load N] -cdfile <img>`; the `gameFolder/pcsx.cfg ->
  ssFolder/pcsx.cfg` copy (`launch.sh:41-43`) done in C++ in direct mode; RetroArch: first existing
  `Env::retroArchBinaries()`, `--config <ra>/retroarch.cfg -L <core> <file>`, NEON/PEOPS ->
  `<ra>/cores/pcsx_rearmed_libretro<ext>`. `writeSelectionScript()` no-op in direct mode.
- `process_runner.{h,cpp}`: `run(const LaunchPlan&)`, `minimisesLauncherWindow()`, `WinProcessRunner`
  (`CreateProcessW`, MS quoting rules, `CREATE_NO_WINDOW`, `lpCurrentDirectory`, `WaitForSingleObject`, exit
  code logged). `System::runAndWait` Windows stub (`system.cpp:158`) becomes that.
- `autobleem.cpp:43`: `win` -> `WinProcessRunner`; `launchGame()` minimize/restore branch.
- `retroarch_cores.cpp:89`, engine `environment.cpp:134`: core extension from Env.
- `src/resources/platform/win.ini`: `retroarch_dir=RetroArch/bin`, `retroarch_binary=retroarch.exe`,
  `retroarch_core=cores/pcsx_rearmed_libretro.dll`, `core_extension=.dll`, `retroarch_roms_dir=RetroArch/roms`,
  `retroarch_bios_dir=RetroArch/bin/system`, `launch_mode=direct`, `pcsx_dir=emu`, `download_command=curl -sfL
  -m 20 -o "%o" "%u"`, `repo_url=...`, `update_download_command=curl -sfL -o "%o" "%u"`, `retroarch_catalog=`.
- Tests: `tests/core/test_launch.cpp` - the recording fake records `LaunchPlan`; script argv unchanged (pins
  today's 9 args), direct pcsx (resume/no resume, `.cue`/`.chd`, spaces in paths), direct RetroArch (NEON,
  PEOPS, foreign `.dll` core), pcsx.cfg copy, selection script skipped. `test_retroarch.cpp` - `.dll` discovery.

**B3 - no cmd flashes, real free space**
- `system.{h,cpp}`: `runShellCommand()`, `diskSpace(path, free, total)` shared with `system_info.cpp:487`;
  `getAvailableSpace()` real on Windows and `dev`. `online_assets.cpp:48`, `update_service.cpp:48` default to it.
- Tests: a `test_system.cpp` for `diskSpace` on the temp dir.

**B4 - GUI exe, icon, fullscreen**
- `CMakeLists.txt` (`win`): `-mwindows`, `src/resources/win/autobleem.rc` (icon + version info), `tools/make_icon.py`
  (PNG-in-ICO around `tools/repo_icon.png`, stdlib). `Gui::setFullscreen(true)` on `win` (and every real
  target - `dev` alone keeps the window), `Platform::createWindow` `SDL_WINDOW_FULLSCREEN_DESKTOP` + the scale
  from the desktop mode. No config key, no toggle, no Options row. The direct launch plans (B2) start pcsx-ab
  with `-fullscreen` and RetroArch with `--fullscreen`.
- `tools/make_win_package.sh`: the product build's files + `Themes/` + `emu/` (pcsx-ab win dist when present;
  `AB_NO_PCSX` ships nothing, the RetroArch fallback stands) + `dataroot.txt.example`.

**Verify by hand** (PC): `./make_win.sh --product`; build pcsx-ab with its `make_win.sh` (needs F1's `-dotdir` -
until then test RetroArch launches only), copy `pcsx-ab.exe` + DLLs into `build_win_product/emu/`; run
`autobleem-gui.exe` with no args -> tree in `Documents\AutoBleem`; drop a game in `Games/`; launch -> pcsx-ab
full screen, launcher minimised, comes back raised; `System/config.ini` there, none in the program dir; no
console window ever; Power Off exits; `autobleem.log` shows `WinProcessRunner` lines.

## Phase C - Windows setup helper + NSIS + CI `win` + self-update apply

**C1 - `WindowsInstallJob`** (`apps/installer/src/core/windows_install_job.{h,cpp}`)
- `WindowsInstallOptions {programDir, dataRoot, covers[3], retroarch, bios, samples, update}`; phases: data tree,
  shipped themes, cover DBs (reuse the PSC job's phase), RetroArch (fetch `stable/<v>/windows/x86_64/
  RetroArch-Win64-setup.exe`, `<v>` from the site's `win/retroarch/latest.json`; run `/S /D=<data>\RetroArch\bin`
  via CreateProcess + wait behind a `SetupRunner` seam; cores from `nightly/windows/x86_64/latest/.index-extended`
  (`*_libretro.dll.zip` -> `RetroArch/bin/cores/`); the info/assets/autoconfig/database/cheats/overlays bundles as
  `download_retroarch_content` does; a `retroarch.cfg` with every dir key under `<data>\RetroArch\bin`,
  `video_fullscreen=true`), BIOS pack from `win/bios/biospack.txt`, samples (RetroArch part only with RetroArch),
  `install_ps1_bios` copy. `--update`: replace shipped files, keep the user's files + `System/config.ini`, delete
  `games.fingerprint`/`roms.fingerprint`.
- `tools/biospack.py --arch win64`: `BUILDBOT_INDEX` gets `{os}` (`linux`/`windows`), suffix `_libretro.dll.zip`,
  manifest -> `repo_publish.sh win-bios` -> `win/bios/biospack.txt`.
- `apps/installer/CMakeLists.txt`: second exe `AutoBleemWinSetup.exe` (`src/main_winsetup.cpp` + the shared
  window; `--quiet`, `--update`); `apps/installer/CLAUDE.md` updated.
- Tests: `tests/apps/test_installer_core.cpp` - the job with the fake downloader/listener into a temp root
  (tree; update keeps config.ini + deletes fingerprints; the RetroArch step through a fake `SetupRunner`).

**C2 - NSIS** (`installer/windows/autobleem.nsi`)
- `RequestExecutionLevel user`, `SetShellVarContext current`, `InstallDir $LOCALAPPDATA\Programs\AutoBleem`; MUI2
  pages: welcome, data folder (`$DOCUMENTS\AutoBleem` - show the resolved path; OneDrive Known-Folder-Move would
  sync every disc image, so offer `$PROFILE\AutoBleem` on that page), components (RetroArch, cover DBs, BIOS,
  samples), instfiles, finish (run the launcher). Writes HKCU `Software\AutoBleem` `DataRoot/InstallDir/Version/
  Options`, the HKCU Uninstall key, Start Menu + Desktop shortcuts; `ExecWait`s `AutoBleemWinSetup.exe --root
  "<data>" [options]` (`--quiet` under `/S`). `/S`: options from the registry, `--update` mode, `.onInit` waits up
  to 30 s for the launcher's named mutex `Global\AutoBleemLauncher` (held by `main.cpp` on `win`); `/RESTART`
  starts the launcher at the end. Uninstaller keeps the data tree unless ticked. Output `AutoBleemSetup-<v>.exe`.
- `docker/Dockerfile` mingw stage: `nsis`; `docker/ab-validate.sh mingw` checks `makensis -VERSION`.
- `ci/build.sh win` (L188): `-DAB_TARGET=win`, `build_pcsx win` -> `build_mingw/emu` (F1), `make_win_package.sh`
  (stage + portable zip), then `makensis -DVERSION=$VERSION -DSTAGE=... installer/windows/autobleem.nsi` ->
  `dist/win/AutoBleemSetup-<v>.exe`.
- `tools/repo_index.py`: `PACKAGE_KINDS` + `("win-setup", ^AutoBleemSetup-.*\.exe$, "Windows installer")`; `INDEX_VERSION` 23.

**C3 - self-update apply on Windows**
- `app.cpp` win branch: `platformKey="win-setup"`, `arch=""`; `evoui_launcher_actions.cpp:749` on `AB_PLATFORM_WIN`:
  `System::startDetached("<System/Updates>/AutoBleemSetup-<v>.exe", {"/S","/RESTART"})`, then
  `menuOption = MENU_OPTION_UPDATE; menuVisible = false` (launcher exits, mutex drops, installer proceeds, restarts it).
- Tests: `test_update_service.cpp` - a fake `release.json` with a `win-setup` file; `compare()` picks it; no
  RetroArch check when `arch` is empty.

**Verify**: `docker/run.sh ci/build.sh win` -> `dist/win/AutoBleemSetup-<v>.exe`; on the PC run it (SmartScreen:
More info -> Run anyway), tick RetroArch+covers+samples, watch the helper, Start Menu entry, launcher fullscreen,
a NES sample runs in RetroArch; run the same exe `/S` -> silent update, config kept; publish to the site's
pre-release, set Options -> Updates latest on a build one hash older -> prompt -> download -> the launcher
restarts on the new version; uninstall keeps `Documents\AutoBleem`.

## Phase D - i386 toolchain, `ci/build.sh pcusb`, package, install.sh/firstboot generalisation

**D1 - toolchain + Docker stage**
- `docker/Dockerfile`: stage `pcusb` after `pi`: `dpkg --add-architecture i386`, `crossbuild-essential-i386`,
  `libsdl2{,-image,-mixer,-ttf}-dev:i386 libpng-dev:i386 zlib1g-dev:i386 libjack-jackd2-dev:i386` (the jackd2 pin
  at L73-75 must include i386). `ab-validate.sh pcusb`: link with `i686-linux-gnu-g++`, `file` = `ELF 32-bit LSB
  ... Intel 80386`, and *execute it* (`SDL_VIDEODRIVER=dummy` - i386 runs natively on the amd64 host).
- `toolchains/pcusb/PcUsbToolchain.cmake`: `CMAKE_SYSTEM_PROCESSOR i686`, `i686-linux-gnu-gcc/g++`,
  `CMAKE_LIBRARY_ARCHITECTURE i386-linux-gnu`, `SDL2_DIR /usr/lib/i386-linux-gnu/cmake/SDL2`, `-march=i686
  -mtune=generic -Os -s -D_FILE_OFFSET_BITS=64`, `AB_TARGET pcusb`, **`AB_BUILD_TESTS ON`** (the suites run on
  the host as i386 - a free gate). Root `CMakeLists.txt`: a `pcusb` branch like the Pi's.
- `ci/build.sh pcusb` -> `build_pcusb/`, ctest, validate, UPX, `make_rpi_package.sh --platform pcusb --arch i386`.

**D2 - payload rename + one install.sh** (two commits: the `git mv`, then the switch)
- `git mv payload_rpi payload_linux`; fix the path in `tools/make_rpi_package.sh`, `tools/make_rpi_image.sh`,
  `ci/`, `.github/`, `CLAUDE.md`, `docs/`.
- `payload_linux/install.sh`: `detect_platform()` (`/proc/device-tree/model` vs `dpkg --print-architecture` =
  i386; `--platform`); `preflight`: arch gate `armhf|arm64|i386` (`i386 -> RA_ARCH=x86`), `BOOT_DIR`
  `/boot/firmware` (rpi) / `/boot/efi` (pcusb; in fstab by label so it is mounted on BIOS boots too), `DISK` from
  `findmnt /` on pcusb; `PLATFORM_DIR=rpi|pc` for `retroarch/latest.json` and `cores/latest.json`;
  `download_bios_pack` picks `biospack-i386.txt`; `configure_boot_pcusb`: `/etc/default/grub`
  `GRUB_CMDLINE_LINUX_DEFAULT="quiet loglevel=3 logo.nologo vt.global_cursor_default=0 consoleblank=0 splash
  plymouth.ignore-serial-consoles"`, `GRUB_TIMEOUT=2 GRUB_TIMEOUT_STYLE=hidden GRUB_GFXMODE=auto
  GRUB_GFXPAYLOAD_LINUX=keep`, `update-grub`; `hdmi_mode` logged as ignored on pcusb (KMS takes the native mode);
  `install_retroarch_source` gains `--enable-opengl` on x86; `summary` without raspi-config. Everything generic
  (`install_boot_splash`, `grow_root`, `ensure_data_partition`, `mount_data`, `install_service`,
  `install_update_helper`, `--update`) untouched.
- `system/autobleem-session.sh`: `hdmi_audio()` -> `pi_hdmi_audio()` when `/proc/asound/cards` has `vc4hdmi`,
  else `pc_hdmi_audio()`: the HDMI/DP pcm whose `/proc/asound/card*/eld#*` says `monitor_present 1` ->
  `/etc/asound.conf`, else ALSA's default.
- `system/autobleem-firstboot.sh`: `$BOOT_DIR/autobleem.txt` via `detect_platform`, `set_wifi_country()`
  (raspi-config on rpi / `iw reg set` + `/etc/modprobe.d/cfg80211.conf` on pcusb), probe host `deb.debian.org`,
  tarball/dir `autobleem-<platform>*`, `ssh-keygen -A` when host keys are missing (the image deletes them).
- `Autobleem/rc/launch.sh:86`, `launch_rb.sh:50`: `/usr/lib/*/libretro` glob.
- `tools/make_rpi_package.sh` (name kept): arch row `i386) BUILD_SUBDIR=build_pcusb; TARBALL=autobleem-pcusb-i386.tar.gz;
  EMU_SRC_SUBDIR=emu-i386; TOP=autobleem-pcusb`; `payload_linux/Autobleem/bin/emu-i386/` checked in once F2 built
  it (until then `AB_NO_PCSX` + the RetroArch core fallback `launch.sh:74-95`).
- `repo_index.py`: `("pcusb", ^autobleem-pcusb-i386.*\.tar\.gz$, ...)`.
- Tests: `test_platform_config.cpp` loads `pcusb.ini`; `shellcheck payload_linux/install.sh` in `build_native` if
  the image has it (add to the Dockerfile native stage).

**Verify**: `docker/run.sh ci/build.sh pcusb` (ctest as i386), `dist/pcusb/autobleem-pcusb-i386.tar.gz`; a Debian
12 i386 VM (netinst, no desktop) or the target PC: `sudo bash install.sh --yes` -> exFAT partition, RetroArch
(prebuilt from E, else source), cores, BIOS, splash, reboot into the launcher on tty1 over kmsdrm;
`autobleem-gui --sysinfo` over ssh; then `install.sh --update` from a second tarball.

## Phase E - the image builder, i386 RetroArch/cores/BIOS, site image kind, manual

**E0 - spike (half a day max)**: on the server, inside the image, `mmdebstrap --mode=unshare --variant=apt
--architectures=i386 --format=tar bookworm /tmp/t.tar` via `docker/run.sh --userns` (`--security-opt
seccomp=unconfined --security-opt apparmor=unconfined`, a `builder` user with `/etc/subuid` in the image,
`uidmap`). Works -> rootless default. Fails -> `docker/run.sh --privileged` (root in the container, `losetup -P`,
`mount`, `grub-install`) as the default, noted in `docs/ci.md`. The script keeps both modes either way.

**E1 - `tools/make_pc_image.sh`** (`--package --work --out --version --root-size 4G --esp-size 256M --rootless|--mount --dry-run`)
1. `mmdebstrap --architectures=i386 --variant=minbase --components=main,non-free-firmware --include=<list>
   --format=tar bookworm root.tar`, customize hooks: `/etc/fstab` (`LABEL=AUTOBLEEM_ROOT /`, `LABEL=ABBOOT
   /boot/efi vfat`), hostname `autobleem`, user `autobleem:autobleem` in `sudo`, `/etc/default/grub` (as D2),
   `/etc/grub.d/10_autobleem` (two `menuentry`s `root=LABEL=AUTOBLEEM_ROOT`, `insmod cpuid; if cpuid -p; then
   set default=pae; else set default=nonpae; fi`, `10_linux` chmod -x), `/boot/grub/grub.cfg` from the same
   template (no `grub-mkconfig` in the chroot; `update-grub` on the first boot regenerates it), plymouth theme +
   `plymouth-set-default-theme autobleem` + `update-initramfs -u -k all` **in the chroot** (i386 runs natively,
   unlike the Pi's foreign-arch injection), ssh enable symlink, `rm /etc/ssh/ssh_host_*`, empty
   `/etc/machine-id`, `/opt/autobleem-image/{autobleem-pcusb-i386.tar.gz, autobleem-firstboot.sh,
   autobleem-install-ui.py, splash.png}` + the unit and its wants symlink (the same five writes as `inject_payload`).
   Packages: `linux-image-686-pae linux-image-686 initramfs-tools systemd systemd-sysv systemd-timesyncd udev
   dbus kmod sudo locales console-setup kbd python3 network-manager wpasupplicant iw wireless-regdb rfkill
   iproute2 ca-certificates curl wget unzip parted exfatprogs e2fsprogs dosfstools alsa-utils plymouth
   libsdl2-2.0-0 libsdl2-image-2.0-0 libsdl2-mixer-2.0-0 libsdl2-ttf-2.0-0 libpng16-16 zlib1g libgl1
   libgl1-mesa-dri libegl1 libgles2 libgbm1 grub2-common grub-pc-bin grub-efi-ia32-bin grub-efi-amd64-bin
   openssh-server firmware-linux-free firmware-misc-nonfree firmware-amd-graphics firmware-iwlwifi
   firmware-atheros firmware-realtek firmware-brcm80211 firmware-intel-sound firmware-sof-signed pciutils
   usbutils` (the `-bin` GRUB packages, never `grub-pc`/`grub-efi-ia32` - their postinst runs `grub-install`).
2. Root fs without root: `mke2fs -d root.tar` needs e2fsprogs >= 1.47.1 (Bookworm: 1.47.0) - build 1.47.2 into
   the Docker `all` stage (recommended), or `genext2fs -a root.tar` + `tune2fs` to ext4 as the fallback;
   `resize2fs` to `--root-size`.
3. Raw image: `truncate`, `sfdisk` (MBR: p1 `ef` FAT32 at 1 MiB, p2 `83` root, no p3 - `ensure_data_partition`
   makes it on the first boot; the 1 MiB gap holds core.img), `mformat -i img@@off -F -v ABBOOT` + `mcopy` of
   `EFI/BOOT/BOOTIA32.EFI`, `BOOTX64.EFI`, `autobleem.txt`; `dd conv=notrunc seek=<p2>` the ext4.
4. GRUB from the host's `grub-pc-bin/grub-efi-ia32-bin/grub-efi-amd64-bin` (added to the Docker `all` stage):
   `grub-mkimage -O i386-pc -p '(hd0,msdos2)/boot/grub' -o core.img biosdisk part_msdos ext2 fat normal linux
   search search_label cpuid gfxterm all_video gzio echo test configfile`; `grub-bios-setup -d . -b boot.img -c
   core.img -m device.map img` (fallback: `dd` boot.img bytes 0..445 + core.img at sector 1). EFI: `grub-mkimage
   -O i386-efi|x86_64-efi -p /boot/grub -c early.cfg` (`search --label AUTOBLEEM_ROOT --set=root; configfile
   /boot/grub/grub.cfg`) with `part_msdos part_gpt ext2 fat normal linux search search_label cpuid efi_gop efi_uga
   all_video gfxterm gzio echo test configfile loadenv`.
5. `xz -T0 -4` -> `autobleem-<v>-pcusb-i386.img.xz` + `.sha256`.
- Docker `all` stage: `grub-pc-bin grub-efi-ia32-bin grub-efi-amd64-bin dosfstools uidmap` (+ e2fsprogs 1.47.2;
  optional `qemu-system-x86 ovmf ovmf-ia32` for an `AB_IMAGE_SMOKE=1`-gated headless boot - TCG, no KVM there).

**E2 - RetroArch/cores/BIOS for i386**: `ci/build_retroarch.sh i386` (`i686-linux-gnu`, `PKG_CONFIG_LIBDIR=
/usr/lib/i386-linux-gnu/pkgconfig`, `--enable-opengl` added to the KMS/EGL/GLES set; Docker `retroarch` stage gets
the `:i386` dev packages); `ci/build_cores.sh i386` (`ra_arch=x86`); `tools/biospack.py --arch i386`
(`buildbot_arch=x86`, `payload_linux/system/biospack-i386.txt`); `repo_publish.sh` kinds `pc-retroarch`,
`pc-cores`, `pc-image` -> `pc/retroarch/<tag>/`, `pc/cores/`, `pc/images/<v>/`; `repo_index.py`: `RETROARCH_RE`/
`CORES_RE` arch `armhf|arm64|i386`, `index_retroarch(dir)`/`index_cores(dir)`/`index_images(dir, regex)`
parametrised over `rpi/` and `pc/`, `PC_IMAGE_RE`, the same one-pre-release retention.
- `update_service.cpp:191` -> `config_.retroarchCatalog`; `App::applyUpdateSetting` fills it from
  `Env::retroArchCatalog()`. Test: fake site with `pc/retroarch/latest.json` keyed `i386`; the Pi case still reads
  `rpi/`; empty = no check.

**E3 - `render_pc_install`** (`tools/repo_index.py` -> `pc-install.html`): what you need (i686+ CPU, 1 GB RAM,
8 GB+ stick, keyboard for the first boot), flashing (Rufus **DD mode**, Etcher, `dd bs=4M`), booting (boot menu
key; BIOS or UEFI; **Secure Boot off** - unsigned GRUB, no signed i386 kernel; a 64-bit UEFI machine loads
`BOOTX64.EFI` into the 32-bit kernel), what the first boot does (screen, WiFi/Ethernet, RetroArch question, root
grown to `root_gib`, the rest becomes the exFAT `AUTOBLEEM` partition), where games go, options (`autobleem.txt`
on the first partition, `ssh autobleem@...` password `autobleem` - change it), caveats (Nvidia: nouveau + the
non-free firmware for Maxwell+, the proprietary driver is out; non-PAE CPUs get the second kernel automatically;
no i386 dynarec - PS1 on the interpreter, fine at 2 GHz+).
- `ci.yml` (`plan` targets, release list, `site` job builds+publishes the PC image and `AutoBleemSetup`) and
  `site-refresh.yml` (i386 RetroArch/cores) - written, gated as today (the owner builds on the server).

**Verify**: `docker/run.sh [--userns|--privileged] tools/make_pc_image.sh --package dist/pcusb/... --work
build_pc_image --out build_pc_image/out`; QEMU on the PC: `qemu-system-x86_64 -m 1024 -drive format=raw,file=img`
(BIOS), `-bios OVMF.fd` (UEFI x64), `qemu-system-i386 -bios OVMF32` (UEFI ia32) - GRUB (Shift) shows both
kernels, the first-boot screen appears, Ethernet, install completes into the launcher; then a real stick on the
owner's target PC (a BIOS *and* a UEFI machine); `tools/repo_publish.sh --local pc-image ...`; the manual renders.

## Phase F - pcsx-ab (sibling repo `E:\Programming\pcsx-rearmed-develop`; parallel - B needs F1, D needs F2)

- **F1**: `frontend/main.c:112` `-dotdir/-biosdir/-pluginsdir` (the `make_path` seam, `-h` text) and
  `-fullscreen` (SDL fullscreen-desktop in `plat_sdl`, the default for the Windows build); `ci/build.sh win`
  (`toolchains/mingw/`, Release, `-DPCSXAB_PLUGINS=OFF`; `dist/` = `pcsx-ab.exe` + `SDL2.dll` from
  `/opt/mingw-sdl2/bin` + zlib/libpng DLLs - Debian has `libz-mingw-w64-dev` but no mingw libpng: build it from
  source into `/opt/mingw-sdl2` in AutoBleem's Dockerfile mingw stage, or a `PCSXAB_PNG=OFF` option). AutoBleem's
  `build_pcsx win` copies `dist/` into `build_mingw/emu/`.
- **F2**: `ci/build.sh pcusb` there (`toolchains/pcusb/`, i686-linux-gnu, `-march=i686`, SDL2:i386 + libpng:i386,
  `.so` plugins, `file` gate `Intel 80386`); AutoBleem's `build_pcsx pcusb` -> `payload_linux/Autobleem/bin/
  emu-i386/`, committed once like the Pi's.
- Its `make_win.sh` stays Debug for the dev loop.

## Phase G - site tabs + docs

- `tools/repo_index.py` `tabbed()` (L801): a second level from `<h3 class="subtab" id="pc-usb">` / `id="pc-windows">`
  markers inside the `pc` section - per section split the body on that regex, emit `<nav class="subtabs">` +
  `<section class="subtab">`s; id regex `[a-z-]+`; `show(id)` activates a subtab's parent (`data-parent`) and
  itself, a plain section shows its first subtab; ~25 lines of JS, `.subtabs a` pills in `PAGE_CSS`. PC section:
  PC-USB = image + tarball (Install), `pc/retroarch` + `pc/cores` (Build inputs); PC-Windows = `win-setup` +
  `win` (portable zip) + `updateroms`; link to `pc-install.html`. `INDEX_VERSION` 24.
- `CLAUDE.md`: "PC-USB" and "PC-Windows" sections + the platform table (`AB_TARGET` -> macros -> ini);
  `docs/pc-targets-plan.md` (this file); `payload_linux/README.md` (PC section); `apps/installer/CLAUDE.md`;
  `docs/ci.md` (`pcusb`, `--userns`/`--privileged`, `win` -> setup exe); `TODO.md`.

## Risks to carry into the commits

- **Bookworm i386 EOL** mid-2028 (LTS); no successor with an i386 kernel - note in the manual and CLAUDE.md.
- **`mmdebstrap --mode=unshare` in Docker** needs seccomp/apparmor unconfined and a subuid range - spike E0.
- **Shared ssh host keys / machine-id** in a prebuilt image: deleted at build, regenerated on the first boot;
  the `autobleem/autobleem` account goes in the manual and `install.sh`'s summary.
- **Secure Boot** off (unsigned GRUB + i386 kernel). **Non-PAE**: verify GRUB's `cpuid -p` once.
- **Nvidia/KMS**: nouveau needs `firmware-misc-nonfree`; the proprietary driver is out. SDL kmsdrm falls back to
  the software renderer on GPUs with no GBM/EGL (SDL 2.26 in Bookworm; `ableem::Renderer` needs render targets,
  which it has). pcusb MSAA 0.
- **exFAT + tar as root**: the update stage stays on the root fs with `--no-same-owner` (already so).
- **`/D=`** last and unquoted; if RetroArch's installer is `RequestExecutionLevel admin` -> the 7z fallback.
- **SmartScreen** on the unsigned `AutoBleemSetup-<v>.exe` and the downloaded RetroArch setup: "More info ->
  Run anyway" in the docs; no code signing planned.
- **OneDrive** redirecting Documents (the installer page shows the resolved path, offers `%USERPROFILE%\AutoBleem`).
- **cmd flashes**: grep for `popen(`/`system(` before shipping C; `execUnixCommand` is PSC-only after A.
- **Tests as i386** on the server: `-D_FILE_OFFSET_BITS=64`; watch `time_t`/`off_t` assumptions.
- **Two kernels** double `update-initramfs -u -k all` on the first boot - acceptable.

## Verification (end to end, in execution order)

1. A: all five existing targets build and test green on the PC and the server; the Pi still updates.
2. D: the i386 tarball installs on a Debian 12 i386 VM/PC with `sudo bash install.sh`, boots into the launcher on
   tty1 fullscreen over kmsdrm, a game runs fullscreen and returns, updates with `install.sh --update`.
3. E: the image boots under QEMU in BIOS, UEFI-x64 and UEFI-ia32, then on real hardware; the first boot ends in
   the launcher with the data partition made; the site lists the image and the manual page.
4. B: `autobleem-gui.exe` (product build) with no args makes and uses `Documents\AutoBleem`, opens fullscreen,
   launches pcsx-ab and RetroArch directly and fullscreen, no console windows, settings in `<data>\System\config.ini`.
5. C: `AutoBleemSetup-<v>.exe` from the Docker `win` target installs per-user with RetroArch quietly installed
   into `<data>\RetroArch\bin`, cores present, a sample runs; `/S` updates in place; the launcher's Software
   Update downloads the next setup exe from the site and comes back on the new version; uninstall keeps the data.
6. G: the landing page shows PC with the PC-USB / PC-Windows sub-tabs; `CLAUDE.md` documents both targets.

## Status (2026-09-20)



- **A** done (`AB_TARGET`, the derived macros, the new ini keys) - all five existing targets green on the server.

- **D** done: the `pcusb` Docker stage and toolchain, `ci/build.sh pcusb` (37/37 suites as i386), `payload_linux/` with one `install.sh` (`PLATFORM=rpi|pcusb`), the tarball.

- **E** done apart from real hardware: `tools/make_pc_image.sh` builds under `docker/run.sh --privileged` (E0's answer: the server's kernel refuses user namespaces in a container); the image boots in VirtualBox (BIOS, PAE kernel) through GRUB, plymouth and the first-boot screen; `pc/retroarch/` published, `pc/cores/` in progress; the manual page is `pc-install.html`. UEFI (32- and 64-bit) and real hardware untested.

- **F2** not started: pcsx-ab has no i386 target; the stick plays PS1 through RetroArch's pcsx_rearmed core meanwhile.

- **B, C, F1, G** not started (Windows).
