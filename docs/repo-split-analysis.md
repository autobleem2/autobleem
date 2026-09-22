# Splitting the launcher repo into logical projects — analysis

Analysis written 2026-09-22, ahead of migrating the launcher into the `autobleem2` org. The repo has grown
to hold far more than "the launcher": an emulator-free C++ suite, a Docker build image, the download-site
tooling, Pi/PC OS installers and image builders, theme content, user manuals and a sample-games pipeline.
This report inventories what is really in here, maps the coupling that constrains any split, and proposes a
concrete, phased breakup with the migration mechanics. It is analysis for a decision — nothing here is acted
on until the split is chosen.

## 1. What is actually in the repo today

Tracked files by area (counts are files; C/C++ line counts where relevant):

| Area | Files | Notes |
|---|---|---|
| `lib_ableem/` | 331 | The portable library. **223 files are vendored third-party** (sqlite, nlohmann json, miniz, libchdr + zstd/lzma/zlib, plog); ~106 are our own engine + ui. ~399k C/C++ lines total, but the vast majority is vendored. |
| `src/` | 271 | **The launcher itself** — `~23k` C/C++ lines: `core/` (ab_core), `gui/` (ab_classic/ab_ui), `evoui/` (ab_evoui), `main.cpp`, `autobleem.*`, `tools/` (absplash, abfatflag). |
| `payload/` | 237 | Release USB tree: **Themes (148, five themes)**, Apps (53), Autobleem/rc + bin (22), Docs (7), RetroArch skeleton (5), the exploit dir. |
| `apps/` | 121 | Four separate programs: `pscbios` (53, 2.4k LoC), `abflashkit` (37, 1.4k), `installer` (21, 3.9k), `updateroms` (10, 0.7k). |
| `toolchains/` | 91 | psc/rpi/rpi64/pcusb/mingw cmake files; **85 of the 91 are the rpi SDL2 devkit headers**. |
| `tests/` | 68 | doctest suites against `ab_core`. |
| `manuals/` | 55 | User manual (EN/PL) markdown, CSS, screenshots. |
| `tools/` | 52 | A grab-bag (see §3) — site tooling, image builders, theme/sample/manual generators, dev drivers. |
| `payload_linux/` | 41 | The Pi/PC appliance package: `install.sh`, first-boot UI, systemd, plymouth. |
| `docs/` | 14 | Dev docs + plan files. |
| `docker/` | 10 | The build image (`Dockerfile`, `ab-validate`, `build-image.sh`, `run.sh`), the site server (`repo/`), the runner (`runner/`). |
| `payload_rpi/` | 8 | **Stale** — leftover from the `payload_rpi`→`payload_linux` rename; should be removed regardless of any split. |
| `ci/` | 4 | `ci/build.sh` and helpers. |
| `installer/` | 1 | `installer/windows/autobleem.nsi` (the Windows NSIS installer). |
| root | — | `CMakeLists.txt` (builds **everything**), `make_*.sh`, `CLAUDE.md`, licence/notices. |

## 2. The coupling that constrains the split

**One `CMakeLists.txt` builds the whole C++ suite** as a single dependency chain:

```
lib_ableem (ableem_engine → ableem/ui)
   └─ ab_core ─ ab_classic ─ ab_ui ─ ab_evoui ─ autobleem-gui (the launcher)
                    │             └─ (also) absplash, abfatflag
                    ├─ apps/pscbios      (links ab_classic + pscbios_core)
                    ├─ apps/abflashkit   (links ab_classic + abflashkit_core)
                    ├─ apps/updateroms   (links ab_core)
                    └─ apps/installer    (links ab_core / ableem_engine)
```

Consequence: **every C++ program in the repo — the launcher, both console tools, both PC/Windows installers —
shares `lib_ableem` and `ab_core`, and most share `ab_classic` (the UI).** You cannot move any of them into
its own repo without first making those shared libraries a consumable dependency (a git submodule or a
versioned package). That single fact divides the repo into two very different kinds of content:

- **C++ that shares the core** — can only be split by extracting `lib_ableem`/`ab_core` first (costly).
- **Scripts / content / ops** — Python, shell, JSON, markdown, artwork. **Zero build coupling.** These are
  the "not really the launcher" parts and are cheap to extract today.

The download-site tooling confirms the divide: `tools/repo_*.py` import nothing from the build, and the CMake
build references nothing under `tools/repo_*`, `manuals/`, `tools/samples/`, `payload_linux/`.

## 3. Categorising `tools/` (the grab-bag) by concern

| Concern | Files | Coupling |
|---|---|---|
| **Download site / ops** | `repo_publish.sh`, `repo_index.py`, `repo_index_merge.py`, `repo_assets.py`, `repo_icon.png`, `rpi_imager_repo.json`, `rpi_imager_local_manifest.py` | None to the build. `repo_assets.py` reads `payload/Themes/ab2` (data only). |
| **Appliance / images / installers (Linux)** | `make_rpi_image.sh`, `make_pc_image.sh`, `make_rpi_package.sh`, `pc_image/10_autobleem`, `install_autobleem.py`, `biospack.py`, `clean_autobleem_usb.ps1` | Consume a built launcher tarball; otherwise standalone. |
| **Theme content generators** | `make_theme_images.py`, `make_theme_sounds.py`, `make_theme_music.py`, `make_ab2_icons.py`, `make_bigbox_frame.py`, `make_evoimg_icons.py`, `make_icon.py` | Produce `payload/Themes/*` assets. Standalone (Pillow etc.). |
| **Sample games** | `samples/*` (manifest, covers, shots), `build_samples.py` | Standalone (stdlib + Pillow). |
| **Manuals** | `build_manuals.py`, `manual_shots.py` | `manual_shots.py` drives a **built** launcher (DebugDriver) for screenshots — runtime coupling only. |
| **Windows packaging** | `make_installer_bundle.sh`, `make_updateroms_bundle.sh`, `make_win_package.sh` | Package the C++ installer/tools' outputs. |
| **Build / packaging (all targets)** | `make_psc_package.sh`, `make_third_party_notices.py` | Part of building the C++ suite. |
| **Dev drivers / debug** | `ab_drive.py`, `ui_tour.py`, `win_drive.ps1`, `make_usb.py`, `manual_shots.py`, `psc_mount_debug.sh`, `psc_sleep_test.sh`, `lang_tools.py`, `format.sh`, `lint.sh`, `defender_exclusions.ps1` | Belong with the C++ (they build/drive/test it). |

## 4. Installers, specifically (the question that surfaced this)

Installers do **not** form one project — they split by nature along the C++ line:

- **Linux appliance installer (Pi + PC-USB)** — `payload_linux/` (`install.sh`, `autobleem-session.sh`,
  `autobleem-firstboot.sh`, `autobleem-install-ui.py`, systemd units, plymouth, the shrink/grow initramfs
  hooks) plus the image builders in `tools/` (`make_rpi_image.sh`, `make_pc_image.sh`,
  `make_rpi_package.sh`, `biospack.py`, `install_autobleem.py`, `rpi_imager_*`). **Pure scripts/Python that
  consume a built launcher tarball.** Clean to extract → the *appliance* repo.
- **Windows / console-stick installers** — `apps/installer` (`AutoBleemInstaller.exe` for console USB
  sticks, `AutoBleemWinSetup.exe` for the Windows product), `apps/updateroms` (`UpdateRoms.exe`), and
  `installer/windows/autobleem.nsi`. **The C++ ones link `ab_core`/`ableem_engine`** — so they are chained
  to the launcher's C++ and cannot move without extracting the core. The `.nsi` and the `make_*_bundle.sh`
  are packaging that wraps those binaries.

So: the Pi/USB (OS) installer is a clean, script-only extraction; the console-stick and Windows installers
are C++ programs that stay wherever the shared core lives.

## 5. The pivot (decided): a shared-foundation submodule

**Decision (owner, 2026-09-22): extract the shared C++ foundation as a git submodule**, joined into each
program the way `pcsx-abnxt` consumes `autobleem/libpicofe`. This lets each program become its own repo.

The key correction from the link analysis: **`lib_ableem` alone is not the shared foundation.** Every tool
links `ab_core`, and the console tools also link `ab_classic`:

| Program | links |
|---|---|
| launcher (`autobleem-gui`) | lib_ableem, ab_core, ab_classic, ab_ui, ab_evoui |
| `pscbios`, `abflashkit` | lib_ableem, ab_core, **ab_classic** |
| `updateroms`, `installer` | lib_ableem, ab_core |

So the submodule is the whole reusable stack — **`lib_ableem` + `ab_core` + `ab_classic`** — i.e. the
"AutoBleem SDK": the portable engine/UI wrapper (namespace `ableem`), the SDL-free model+services
(`ab_core`), and the game-agnostic classic-UI framework (`ab_classic`) that the console tools already draw
with by design. `ab_ui` and `ab_evoui` are launcher-only and stay in the launcher.

**One-time refactor cost:** `ab_classic` and `ab_ui` currently share the `src/code/gui/` directory, split
only by the CMake source lists (not by folders). Extracting `ab_classic` means physically moving those files
(known from the CMake lists) into the submodule, away from the launcher-only `ab_ui` files. `ab_core`
(`src/code/core/`) and `lib_ableem/` move whole. After that, each consumer adds the submodule and links what
it needs (PC tools link `ab_core`; console tools also link `ab_classic`).

Trade-off accepted: submodule friction (bump the pointer in each consumer when the foundation changes) in
exchange for genuinely separable programs — the same trade the emulators already run with `libpicofe`.

## 6. Proposed repositories

| Repo | Contents | Why separate | Coupling out |
|---|---|---|---|
| **`autobleem-core`** (submodule) | `lib_ableem/` + `ab_core` (`src/code/core/`) + `ab_classic` (the ab_classic files from `src/code/gui/`) + `tests/`. The AutoBleem C++ SDK. | The shared foundation every program links; a submodule joined into each consumer (like `libpicofe`). | Consumes `autobleem-build`. |
| **`autobleem`** (the launcher) | `ab_ui` + `ab_evoui` + `main.cpp` + `autobleem.*` (the launcher-only C++), the dev `make_*.sh`, the console `payload/Autobleem/rc` + `payload/RetroArch` skeleton, the **default theme (ab2)**, dev `docs/`, `CLAUDE.md`, licence/notices. Submodules `autobleem-core`. | The console product; thin on top of the SDK. | `autobleem-core`, `autobleem-build`. |
| **`autobleem-console-tools`** | `apps/pscbios`, `apps/abflashkit` (+ their `payload/Apps/<tool>` resources). Submodules `autobleem-core` (links `ab_classic`). | Separate on-console products with their own lifecycle; drawn with the shared classic UI. | `autobleem-core`, `autobleem-build`. |
| **`autobleem-pc-tools`** | `apps/updateroms`, `apps/installer`, `installer/windows/autobleem.nsi`, `make_installer_bundle.sh`, `make_updateroms_bundle.sh`, `make_win_package.sh`. Submodules `autobleem-core` (links `ab_core`). | The Windows/console-stick installers and UpdateRoms — distinct PC products. | `autobleem-core`, `autobleem-build`. |
| **`autobleem-build`** | `docker/Dockerfile`, `ab-validate.sh`, `build-image.sh`, `run.sh`, `ci/`, `toolchains/`, `make_third_party_notices.py`. | Already a standalone GHCR artifact consumed by **every** C++ repo (launcher, core, tools, both emulators). Centralises the toolchain instead of each repo carrying a copy. | Publishes the image; everything consumes it. |
| **`autobleem-appliance`** | `payload_linux/`, `payload_rpi/` (after cleanup), the image/installer scripts (`make_rpi_image.sh`, `make_pc_image.sh`, `make_rpi_package.sh`, `install_autobleem.py`, `biospack.py`, `pc_image/`, `rpi_imager_*`, `clean_autobleem_usb.ps1`). | OS provisioning for the Pi/PC appliances — a distinct domain from the game UI; scripts only. | Consumes the launcher's release tarball + `autobleem-repo` (fetches cores/BIOS). |
| **`autobleem-repo`** (site/ops) | `tools/repo_*.py`, `repo_index_merge.py`, `repo_assets.py`, `repo_icon.png`, `docker/repo/` (Caddy), `rpi_imager_repo.json`. | Pure ops for the download server; no build coupling; has its own merge machinery; a candidate to stay **private**. | Reads other repos' release artifacts + ab2 assets. |
| **`autobleem-themes`** | The four non-default themes (`aergb`, `autobleem`, `default`, `evolution`) + the theme generators (`make_theme_*.py`, `make_ab2_icons.py`, `make_bigbox_frame.py`, `make_evoimg_icons.py`). | Content with its own authors/lifecycle (artwork, fonts, generated assets). | Data-only; the launcher ships the default, the rest are a pack. |
| **`autobleem-manuals`** | `manuals/`, `build_manuals.py`, `manual_shots.py`. | User documentation; translators; its own release cadence. | `manual_shots.py` needs a built launcher for screenshots (runtime only). |
| **`autobleem-samples`** | `tools/samples/`, `build_samples.py`. | Licence-driven game content, published to the site independently. | Standalone. |

Consolidation option: `autobleem-themes` + `autobleem-manuals` + `autobleem-samples` are all small "content"
repos and could be one `autobleem-content` repo if the repo count feels high. The C++ repos
(`autobleem-core`, `autobleem`, `autobleem-console-tools`, `autobleem-pc-tools`) and the infra repos
(`autobleem-build`, `autobleem-appliance`, `autobleem-repo`) are the ones that matter and stay distinct.
Note `autobleem-console-tools` and `autobleem-pc-tools` could also be a single `autobleem-tools` repo (both
submodule the core; two binaries each) if four C++ repos is one too many — the console vs PC split is by
target, not by dependency.

## 7. Migration mechanics

- **Preserve history per subtree.** Use `git filter-repo --path <dir> --path <dir2>` (or `git subtree split`)
  to carve each area into a new repo with its history intact, rather than a fresh copy. Do this from a clone
  so the source is untouched until the cutover.
- **The build image is the linchpin.** `autobleem-build` must publish `ghcr.io/autobleem2/autobleem-build`
  (already done manually); the launcher and emulators then consume it via the `container:` image and fetch
  the toolchain cmake files — either vendored per repo (today's emulator pattern) or as a submodule of
  `autobleem-build`. Decide one and apply it to all three C++ repos.
- **Each repo gets its own gated `build.yml`**, mirroring the emulator pilot (native/cross matrix where it
  builds, or a content/publish job where it doesn't). Releases via `v*` tags → GitHub Releases + the site.
- **Cross-repo release wiring** already has a precedent: the emulators' `publish` checks out the site tooling
  (`repo_publish.sh`). After the split that lives in `autobleem-repo`; the appliance and content repos call
  it the same way. The site's self-hosted publish stays on the org runner.
- **Watch the data edges:** `repo_assets.py` reads `payload/Themes/ab2` (ab2 stays in `autobleem`, so the
  site repo fetches it as a released asset or via a pinned path); `manual_shots.py` needs a launcher build
  (the manuals CI pulls a launcher release); the appliance consumes the launcher tarball + `autobleem-repo`
  cores/BIOS. None are code coupling, but each needs an artifact hand-off defined.

## 8. Recommended sequencing

0. **Transfer the launcher into `autobleem2` first (owner, 2026-09-22).** Transfer `autobleem/AutoBleem2` →
   `autobleem2`, rename to **`autobleem`** (matches the `autobleem2/autobleem` reference the emulator publish
   jobs already use). It stays **private** through the transfer (like pcsx-ab did); making it public needs
   the secret-scan pass first. Redirects keep the old URL working. Then the cleanup/extractions below happen
   from within the org.
1. **Cleanup (in place):** delete the stale `payload_rpi/`, and the stray `pcsx-ab-fastboot` file.
2. **`autobleem-build`** — extract the image + toolchains + `ci/`. Point the two emulators (and everything
   below) at it. Also removes the emulators' duplicated toolchain copies. Highest leverage: every C++ repo
   benefits.
3. **`autobleem-repo`** — extract the site/ops tooling (clean, no build coupling; unblocks publishing from
   every repo, including the emulators already waiting on it).
4. **`autobleem-core`** — carve out `lib_ableem` + `ab_core` + `ab_classic` (the one-time `ab_classic`/`ab_ui`
   file separation, §5) with history, as the submodule. Prove it builds standalone with its `tests/`.
5. **Migrate `autobleem` (the slimmed launcher)** into `autobleem2`, now submoduling `autobleem-core` and
   consuming `autobleem-build` + `autobleem-repo` — this is the launcher migration the runner is waiting on.
6. **`autobleem-console-tools`, `autobleem-pc-tools`** — split the apps out, each submoduling `autobleem-core`.
7. **`autobleem-appliance`** — extract the Pi/PC installers + image builders.
8. **Content repos** (`-themes`, `-manuals`, `-samples`) — lowest urgency, do as convenient.

Steps 4–6 are the deeper refactor; if time-boxed, steps 2–3 + migrating the launcher *whole* (apps included)
still unblocks the runner, and the core/tools split can follow. But since the submodule model is decided,
doing step 4 before the migration means the launcher lands in `autobleem2` already slim.

## 9. Risks / notes

- **Submodule coordination is the accepted cost.** With the foundation split out, a change touching both the
  SDK and a consumer is two commits + a pointer bump (as with `libpicofe`). Mitigate by keeping
  `autobleem-core`'s API stable and bumping consumers in a batch; if the friction outweighs the benefit for
  the tools, fold `console-tools` + `pc-tools` back into one `autobleem-tools` (still submoduling the core).
- **Toolchain ownership** is the one genuinely shared build input across the three C++ repos — settle
  vendored-copy vs submodule once, consistently.
- **`CLAUDE.md` is one giant file** describing all of this; after the split each repo needs its own trimmed
  `CLAUDE.md`, and the cross-repo picture wants a short top-level map (in `autobleem` or a meta repo).
- **History size:** `lib_ableem/third_party` dominates line count but is vendored; a `filter-repo` of the
  launcher keeps it. That is fine — it is the launcher's dependency set — but it means the "slim" launcher
  repo is still large on disk because of vendored code, not because of mixed concerns.
