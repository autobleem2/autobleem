# AutoBleemInstaller - the console's installer for Windows

The way AutoBleem gets onto a PlayStation Classic stick (2026-09-20, the owner's brief: a small Win32
program like UpdateRoms, the look of the Pi's first-boot screen, an updater as much as an installer, the
built tarball shipped next to the exe, the questions as checkboxes). One statically linked exe, no SDL, no
AutoBleem theme; the bundle a release carries is `AutoBleemInstaller-<version>.zip` =
`AutoBleemInstaller.exe` + `README.txt` + `autobleem-psc-<version>.tar.gz` (`tools/make_installer_bundle.sh
<tarball>`; the zip is `PACKAGE_KINDS` "installer" on the site, the console's Install panel).

- **`installer_core`** (`src/core/installer_job.*`, links `ab_core`, tested from
  `tests/apps/test_installer_core.cpp` over a fake site and a package `tests/support/tar_builder.h`
  writes): `InstallOptions` (the stick, the package, the repository URL, the six checkboxes, a scratch
  dir), `InstallerJob::inspect()` (is AutoBleem on the stick - `Autobleem/bin/autobleem/autobleem-gui` -
  and which `VERSION`; `RetroArch/bin/retroarch` + its `VERSION`; which covers; the package's `VERSION`),
  `phasesFor()` (the step titles the options ask for) and `run()`: **the package** (listed, must hold
  the launcher; the shipped theme names are noted), **the stick** (an update keeps `config.ini` aside
  and removes what the package ships - `Autobleem/bin/autobleem`, `bin/emu`, `rc`, `lib/libs.tar.gz`,
  `start.sh`, `Apps/pscbios`, `Apps/abflashkit`, `Docs`, the shipped `Themes/<name>` - and nothing else),
  **the old layout** (`src/core/legacy_layout.*`, a phase only when `LegacyLayout::detect()` finds an
  AutoBleem 1.0 / NG / RetroBoot stick - `retroarch/` at the root with `retroarch.cfg` or `cores/`, or
  `roms/` at the root: `retroarch` -> `RetroArch/bin`, `retroarch/system` -> `RetroArch/bios`, `roms` ->
  `RetroArch/roms`, `themes` -> `Themes`, `retroarch.cfg` and every playlist rewritten to the new paths
  (`system_directory`, `rgui_browser_directory` set outright), RetroBoot's `Applications.lpl`, `Sony -
  PlayStation.lpl` and `AutoBleem.lpl` removed - the launcher's first scan rebuilds the PS1 export and
  every per-system playlist from `RetroArch/roms` (no `roms.fingerprint` on an old stick) - the RetroBoot
  libraries and xpad.ko copied to `Autobleem/lib/`, a RetroBoot-era `Apps/<name>` made self-contained
  from `retroarch/apps/<name>` with its scripts rewritten (`rewritePaths`, `rewriteRunScript`),
  `Apps/retroboot` removed; the same steps as `tools/install_autobleem.py --stage layout`, whose run on the
  owner's stick came first), **unpacking** (`TarArchive::extract` to the root, `config.ini` put back, the Games/System folders
  made), **UpdateRoms** (`releases/unstable.json` and `latest.json`; the release whose `psc-fs` is this very
  package, else the pre-release, else the stable one; its `updateroms` zip unpacked to `<stick>/UpdateRoms/`
  - the console has no network, box art comes from a PC run of it; none on the site = a line, not a
  failure), **the cover databases** (`db/covers{J,U,P}.db`, the `.sha256` sidecar first - a file already
  there with that hash is not fetched), **RetroArch** (`psc/retroarch/latest.json` -> the zip -> the
  binary, `theme/Autobleem2.png` -> `Retroarch themes/`, the font -> `fonts/`, the zip's `VERSION`;
  `retroarch.cfg` written when there is none: `:/`-relative directory keys, `system_directory =
  /media/RetroArch/bios`, `rgui_browser_directory = /media/RetroArch/roms/`, then the build's own
  `theme/retroarch-psc.cfg` keys; an existing cfg is kept, but when the binary was just replaced the
  build's keys are set in it - a RetroBoot-era cfg on 1.22.2 has the XMB theme enum and
  `quit_on_close_content` wrong), **cores** (`psc/cores` -> `RetroArch/bin`), **libraries**
  (`psc/libs` -> `Autobleem/lib`; symlinks skipped, `app_env.sh` makes them), **apps** (`psc/apps` ->
  the root, `apps.json` left out), **assets** (libretro's `assets autoconfig database-rdb
  database-cursors cheats overlays shaders_glsl` bundles from `buildbot.libretro.com/assets/frontend/`,
  each skipped when its folder is already filled, a lost one reported and gone past), **BIOS files**
  (`psc/bios/latest.json` -> `biospack.txt`; per file: kept when size and sha256 match, else fetched to
  `.part`, checked, renamed; a failure is one line and the run goes on; needs RetroArch chosen or on the
  stick), **samples** (`samples/latest.json`; `Games/` + `SAMPLES.md` always, the pack's
  `RetroArch/roms` -> `RetroArch/roms` and `RetroArch/thumbnails` -> `RetroArch/bin/thumbnails` only
  with RetroArch; `System/samples.txt` remembers it, as on the Pi). Downloads land in a scratch dir
  (`<root>/System/Install`, removed at the end; the tests use their own) and every site file with a
  published sha256 goes through `downloadVerified()`. The `Downloader` is an interface (`fetch(url, file,
  progress, error)`), the `InstallListener` gets `onPhase/onProgress/onLine`; `ShouldStop` is polled
  between files and inside a download.
- **`installer`** (`AutoBleemInstaller.exe`): `main.cpp` (the arguments; `--quiet --drive F:
  [--covers JUP] [--retroarch] [--bios] [--samples] [--package F] [--repo U]` prints the lines on the
  console it was started from; the package is looked for next to the exe by `GetModuleFileName`, not
  argv[0]), `win32_platform.*` (`listRemovableDrives()` - `DRIVE_REMOVABLE` only, label, file system,
  sizes, an unformatted one flagged; `formatDrive()` - Windows' `format.com /FS:FAT32|exFAT /Q /V:SONY
  /Y` through a pipe, or `fat32format.exe` next to the exe for FAT32 above 32 GB, which `format.com`
  refuses; `WinInetDownloader` - `InternetOpenUrl`/`InternetReadFile` with the content length for the
  progress, HTTP status checked, a short download rejected) and `win32_window.*`: one 640-wide window,
  the launcher's return splash (`src/resources/splash/autobleem.jpg`, RCDATA 1 in `installer.rc`, drawn
  with GDI+ - rows 90..630 of it) on top, then either the questions (the drive box + Refresh + FAT32/exFAT
  + Format..., a bold status line - fresh install / update from which version / not FAT32 / no package -
  the three cover checkboxes, RetroArch, BIOS (enabled only with RetroArch chosen or present), samples,
  Install/Update) or the progress (Step n of N, a bar for the steps, a bar for the step, the log, Stop /
  Close / Back). A format or an install runs on a `std::thread` into a mutex-guarded `State`, a 100 ms
  timer moves it into the controls; a finished format returns to the questions with the drives re-read.

Built on the dev hosts only (root `CMakeLists.txt`, next to UpdateRoms). Verified 2026-09-20 on the PC:
41/41 tests; a `--quiet` install of the pre-release package into a folder over the real site (the
three covers in 25 s), then RetroArch + BIOS + samples into the same folder; the window with no stick in.
A `--quiet` run of the same options is what the tests' fake site mirrors (a fresh install, an update
over a used stick, an AutoBleem 1.0 stick, RetroArch over a RetroBoot-era cfg, a stop, a site down).
**Ran on the owner's stick and the result booted on the console (2026-09-20).** The install names the
stick SONY (`ensureVolumeLabel`, before the job; the status line announces it when the label differs).
