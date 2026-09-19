# CI: one Docker image that builds every target, and a pipeline on the owner's server

Plan written 2026-09-19, before implementation - the same role `docs/retroarch-scanner-plan.md` and
`docs/rpi-image-and-update-plan.md` played for their features. Remove it (CLAUDE.md's "finished plans leave
docs/" rule) once the pipeline runs, moving what is still true into CLAUDE.md's Build section.

**Status 2026-09-19 (end of day):** steps 1-5 and 7 below are done and verified on the server - the image
builds, every target builds/checks/packages inside it (with pcsx-ab built first), CLAUDE.md is updated.
Step 6 is written (`.github/workflows/image.yml`, `ci.yml`, `docker/runner/compose.yml`, `docs/ci.md`) but
**not yet exercised**: the runner is not registered (needs the owner's PAT), the image is not on GHCR, no
workflow has run. Delete this file once a push has gone green on the self-hosted runner.

## Context

Four build targets, four different hosts today:

| Target | How it is built today | Toolchain | Tests |
|---|---|---|---|
| PlayStation Classic | `make_psc.sh`: rsync to the Linode over ssh, build there | Sony's crosstool-NG GCC 8.2 at `/opt/toolchain` (543 MB; the public copy is [autobleem/PSC-CrossCompile-Toolchain](https://github.com/autobleem/PSC-CrossCompile-Toolchain), 177 MB packed), console sysroot with SDL2 2.0.4 dev files | none (`AB_BUILD_TESTS` forced off) |
| Raspberry Pi 32-bit | `make_rpi.sh` on the Windows PC | SysGCC for Raspberry Pi, `C:\sysGCC\raspberry` (Windows-only binaries; sysroot rsynced from a Pi, no SDL headers - `toolchains/rpi/sdl2-devkit` borrowed from MSYS2) | none |
| Raspberry Pi 64-bit | `make_rpi64.sh` on the Windows PC | SysGCC 64-bit, `E:\sysGCC\raspberry64` (same shape) | none |
| Windows | `make_win.sh` in MSYS2 UCRT64 | MSYS2 gcc + SDL2 packages | **the only place the doctest suites, `lang_tools.py validate` and `format.sh --check` run** |

Plus `make_sys.sh`, a plain native Linux build nobody runs. Packaging is scattered: `tools/make_rpi_package.sh`
(both Pi tarballs), `tools/make_updateroms_bundle.sh` (the Windows `UpdateRoms/` folder), `make_psc.sh` copying
the two console tools into `payload/Apps/`, and **no script at all for the console release zip** - `make_psc.sh`
leaves `build_psc/dist/autobleem-gui` "for the release script to pick up", and that script has never been
written. `payload/` is the USB tree minus `Autobleem/bin/autobleem/` (binary + resources) and `Autobleem/bin/db/`.

The owner's server (`psc-build` in `~/.ssh/config`): a 2-core / 3.8 GB Linode, Ubuntu **18.04**, kernel 6.14,
41 GB free, **Docker 24.0.2 already installed**. The `claude` user there is not in the `docker` group and has
no passwordless sudo - someone with root has to `usermod -aG docker <ci user>` once (see "Server
prerequisites"). 18.04 as a Docker *host* is fine: the containers bring their own userland and the kernel is
current.

### What AutoBleem-NG does, and what we take from it

The fork's `Dockerfile` (read at `b7bc39a`, 2026-06-01; `.github/workflows/ci.yml`, `Makefile`) does not use
Sony's toolchain at all. It bootstraps a **Debian Stretch armhf sysroot** with `mmdebstrap` from
`archive.debian.org`, extracts Stretch's **gcc-6** cross compiler next to it (the newest GCC whose libstdc++
matches the console's glibc 2.24 / libstdc++ 6.0.22), rewrites the sysroot's absolute symlinks, builds its own
`wayland-scanner`, and then **builds SDL2 2.0.12 + SDL2_image/mixer/ttf from source** against that sysroot -
shipping them in a rebuilt `libs.tar.gz`. Wrapper scripts named `armv8-sony-linux-gnueabihf-gcc/g++` make the
unchanged `PSCtoolchainV8.cmake` work. Then `make arm` in the image, `docker-validate glibc-symbols` (no
`GLIBC_` above 2.24) and `docker-validate no-rpath`, an ARM `readelf` built for the console, optional UPX, and
`docker cp` to get `build_arm/` out. CI is GitHub Actions on `ubuntu-24.04`: format check (clang-format in a
container), language validation, a native x86 build + ctest, the Docker ARM build with the checks above, and
on a `v*` tag a release job that assembles `payload/` + the build into a zip and `gh release create`s it.

What carries over:

- The **shape**: one image with everything in it, the build runs *inside* it against a mounted or copied
  tree, the result is validated in the image before it leaves, CI is a workflow that calls the same scripts a
  developer can run by hand. `docker-validate`'s three gates are already ours (`tools/check_psc_binary.sh`).
- The release job: `payload/` + the built parts -> one zip, made by a script, attached to a tag.

- **The Stretch sysroot + gcc-6 + SDL-from-source path itself** (the owner's decision, 2026-09-19: the
  2019 Sony toolchain in `autobleem/PSC-CrossCompile-Toolchain` is outdated and stays out; `/opt/toolchain`
  on the server is no longer what the console build uses). The console's own glibc 2.24 / libstdc++
  6.0.22 *is* Stretch's, so a Stretch sysroot is the most honest one there is, and building the SDL2 family
  in the image is what makes `libs.tar.gz` reproducible: the PSC package regenerates it from the image's
  SDL2 2.0.12 + image/mixer/ttf builds the way NG does (other libraries in the checked-in archive kept).
  Every piece of that recipe is reproduced from public sources (archive.debian.org, libsdl-org releases,
  wayland.freedesktop.org) with pinned versions - no binary from anyone's disk goes into the image except
  the cover databases.

What does not:

- NG's host-side hijack of `/usr/arm-linux-gnueabihf` (symlinks pointing the host's cross-libc directory
  into the Stretch sysroot): that directory is where Debian's *own* `crossbuild-essential-armhf` puts the
  Bookworm cross libc our Pi build needs, so the two toolchains would fight over it. The Stretch cross
  compiler's linker scripts are rewritten to plain library names instead (the crosstool-NG form), and its
  host binaries get a RUNPATH (`patchelf`) to their Stretch-era `libisl`/`libmpc`/`libmpfr`/`libgmp`
  rather than a global `LD_LIBRARY_PATH`.
- gtest, `-O3 -mtune=cortex-a35`, the ARM `readelf` - already decided against in the NG port.

## Design

### One image, five toolchains: `docker/Dockerfile`

`debian:bookworm-slim` base, one image `autobleem-build`, multi-stage so each toolchain layer caches on its
own and `--target` can produce a smaller single-purpose image if ever wanted. Estimated ~3 GB.

1. **`base`** - `cmake` (Bookworm's 3.25; the root CMakeLists wants >= 3.12, `make_psc.sh` wanted >= 3.16
   for the console build), `ninja-build`, `git`, `python3`, `rsync`, `zip`, `xz-utils`, `file`, `upx-ucl`,
   `ca-certificates`, `wget`, `patchelf` is *not* needed. `git config --system safe.directory '*'` so
   `cmake/generate_version.cmake` can read a bind-mounted `.git` owned by another uid.
2. **`psc`** - NG's recipe under `/opt/psc/`: `mmdebstrap --variant=extract` of a Debian **Stretch armhf**
   sysroot from archive.debian.org into `/opt/psc/sysroot` (libc6-dev, libgcc/libstdc++-6-dev, ALSA, udev,
   EGL/GLES, Wayland + protocols, xkbcommon, ogg/vorbis, zlib, libpng - the last for pcsx-ab), absolute
   symlinks rewritten relative; Stretch's amd64 `gcc-6-arm-linux-gnueabihf`/`g++-6-...` extracted the same
   way into `/opt/psc/gcc-6` (with Stretch's binutils 2.28 as their dependency), `patchelf`ed to find their
   own `libisl15`/`libmpc3`/`libmpfr4`/`libgmp10`, their cross tree symlink-farmed into the sysroot's
   `usr/arm-linux-gnueabihf/` (where the relocated g++ looks for its C++ headers), their libc linker
   scripts rewritten to bare names; a host `wayland-scanner` 1.12 (the sysroot's libwayland version);
   then **SDL2 2.0.12** (autotools, the backend set of the console's `libSDL2-2.0.so.0.12.0`: Wayland
   shared + dummy video, GLES 1/2 via EGL, ALSA shared, udev, no X11/KMSDRM/pulse/dbus), **SDL2_image
   2.6.3**, **SDL2_mixer 2.6.3** (OGG via vorbisfile, native MIDI, nothing else) and **SDL2_ttf 2.20.2**
   (vendored FreeType, no HarfBuzz) built with `-march=armv8-a -mfpu=neon-vfpv4 -mfloat-abi=hard -O2` and
   installed into the sysroot at `usr/lib` + `usr/include/SDL2` (`CMAKE_INSTALL_LIBDIR=lib`, or a Bookworm
   host would put them under the multiarch dir). `/opt/psc/bin/armv8-sony-linux-gnueabihf-{gcc,g++}` are
   wrappers adding `--sysroot`, the binutils names symlinks - so `toolchains/psc/PSCtoolchainV8.cmake`
   works with `-DAB_PSC_TOOLCHAIN=/opt/psc` unchanged (its `<root>/sysroot` branch), and so does
   `toolchains/psc/cmake/FindSDL2.cmake` (`usr/lib/libSDL2.so`, `usr/include/SDL2/SDL.h`). The layer ends
   with `ab-validate psc`: the sysroot's libc reports 2.24, a C++ + SDL test program links, `readelf` shows
   no RPATH and nothing above GLIBC_2.24 / GLIBCXX_3.4.22. One repo change follows in step 2: the root
   CMakeLists' console branch spells the CPU flags `-march=armv8-a -mfpu=neon-vfpv4` for a GCC older than 8
   (its `<= 8` branch was `-march=armv7ve` with no NEON).
3. **`rpi`** - the Debian way instead of SysGCC: `dpkg --add-architecture armhf` + `arm64`,
   `crossbuild-essential-armhf`, `crossbuild-essential-arm64`, and the SDL2 family **dev packages of both
   foreign architectures** from Debian's own archive (`libsdl2-dev:armhf`, `libsdl2-image-dev:armhf`,
   `libsdl2-mixer-dev:armhf`, `libsdl2-ttf-dev:armhf`, the same four `:arm64`; `Multi-Arch: same`, so they
   co-install), plus `libpng-dev`/`zlib1g-dev` for each so **pcsx-ab builds in the same image** (its
   `CMakeLists.txt` does `find_package(PNG)`/`ZLIB`). This gives real headers, unversioned `.so` links and
   `sdl2-config.cmake` - the whole `sdl2-devkit`/borrowed-headers trick in `toolchains/rpi/cmake/FindSDL2.cmake`
   is unnecessary on Linux. Raspberry Pi OS *is* Debian (32-bit: Raspbian's armhf rebuild - same EABI hard-float
   ABI, same glibc; we compile `-march=armv7-a` as before, so Pi 2+), so a Debian multiarch sysroot is the
   most faithful one we can have without a Pi in the loop.
4. **`mingw`** - `g++-mingw-w64-x86-64-posix` (the *posix* threads variant - `ScanService` is a
   `std::thread`), `binutils-mingw-w64-x86-64`, and the four official **SDL2 mingw devel tarballs** from
   GitHub releases (`SDL2-devel-<v>-mingw.tar.gz`, `SDL2_image-devel-...`, `SDL2_mixer-devel-...`,
   `SDL2_ttf-devel-...`; each has an `install-package` make target that installs `x86_64-w64-mingw32/
   {bin,include,lib,lib/pkgconfig}` under a prefix and rewrites the `.pc` prefix), into
   `/opt/mingw-sdl2`. Versions pinned by build-arg, starting at what MSYS2 gives us today (SDL2 2.32.x,
   image 2.8.x, mixer 2.8.x, ttf 2.24.x). Their DLLs are self-contained (image/ttf carry their png/freetype
   statically), which makes the Windows zip four DLLs instead of MSYS2's DLL closure. `wine64` optional (see
   "Later").
5. **`native`** - `build-essential`, the host `libsdl2*-dev`, and **`clang-format-22` + `clang-tidy-22`
   from apt.llvm.org**, major-pinned to the 22 MSYS2 has (`clang-format --version` there: 22.1.8) - a
   different major would fail `format.sh --check` on files it formats differently. If apt.llvm.org's 22
   drifts from MSYS2's on some construct, the tree gets reformatted once and the pin moves; the check is
   only meaningful with one version in both places.
6. **`db`** - `docker/db/covers{J,P,U}.db` (280 MB, the 2019 cover databases - on the owner's stick, in this
   checkout's git-ignored `db/`, and on the server at `/AutoBleem/BUILD/data_that_gets_copied/
   cover_databases/`; `docker/build-image.sh` copies them into the build context from whichever it is
   pointed at) `COPY`'d to `/opt/autobleem/db/`. The image is the one place the console package can always
   get them, wherever it runs.
7. **`all`** (the default target) - the union. Stage order in the file is native -> pi -> mingw -> db ->
   psc, the slowest and most experimental layer last so iterating on it leaves the others cached.

Not in the image: any AutoBleem source. The tree is bind-mounted at `/src` (`docker/run.sh`), so the image is
rebuilt only when `docker/**` changes and the build is incremental across runs (build dirs live in the
mounted tree, like `make_rpi.sh`'s). The build runs as the calling uid (`-u $(id -u):$(id -g)`), so nothing
in the tree ends up root-owned.

### Toolchain files: make the Pi and Windows ones host-independent

- `toolchains/rpi/RPitoolchain.cmake` and `toolchains/rpi64/RPi64toolchain.cmake` gain an `AB_RPI_TOOLCHAIN`
  cache path the way the PSC file has `AB_PSC_TOOLCHAIN` (defaults `C:/sysGCC/raspberry` and
  `E:/sysGCC/raspberry64` - unchanged behaviour on the Windows PC), listed in
  `CMAKE_TRY_COMPILE_PLATFORM_VARIABLES` like its sibling. When that directory does not exist and the host
  is Linux, they switch to the **Debian multiarch layout**: `arm-linux-gnueabihf-gcc`/`aarch64-linux-gnu-gcc`
  from `PATH`, no `CMAKE_SYSROOT`, `CMAKE_LIBRARY_ARCHITECTURE` set to the triplet, no `CMAKE_MODULE_PATH`
  entry (the stock `find_package(SDL2)` finds Debian's `sdl2-config.cmake`; `lib_ableem`'s bare `SDL2
  SDL2_image SDL2_mixer SDL2_ttf` link names resolve through the dev packages' unversioned links exactly as
  they do in the native build). The CPU flags, `AB_RPI_DEBUG`, `AB_BUILD_TESTS OFF` and `AB_TARGET_RPI ON`
  stay shared - hoisted into one `toolchains/rpi/common.cmake` included by both files rather than
  duplicated a third time. `make_rpi.sh`/`make_rpi64.sh` do not change.
- New `toolchains/mingw/MinGWtoolchain.cmake`: `x86_64-w64-mingw32-{gcc,g++,windres}` (the `-posix`
  variants), `CMAKE_SYSTEM_NAME Windows`, `CMAKE_FIND_ROOT_PATH /opt/mingw-sdl2/x86_64-w64-mingw32`, and
  `PKG_CONFIG_LIBDIR` pointed at its `lib/pkgconfig` so `lib_ableem/CMakeLists.txt`'s existing `if (MINGW)`
  pkg-config branch works untouched (CMake sets `MINGW` for a MinGW cross compiler too). `AB_BUILD_TESTS`
  stays on - the test exes are built, and run only when an emulator is available
  (`CMAKE_CROSSCOMPILING_EMULATOR=wine64`, see "Later"). Static linking of `UpdateRoms.exe` works as on
  MSYS2 (`libwinpthread.a` ships with Debian's mingw-w64); the difference to note is the CRT - Debian's
  mingw-w64 targets msvcrt, MSYS2's UCRT64 targets ucrt; neither matters to us and the SDL devel DLLs are
  msvcrt-built anyway.

### The build entry point: `ci/build.sh <target>`

One script, run inside the container (or on any Linux box with the same packages), targets `native`, `psc`,
`rpi`, `rpi64`, `win`, `all`; each configures with the right toolchain file into `build_<target>/` (the
names `make_*.sh` already use, so a build dir made by one is picked up by the other), builds with `-j$(nproc)`,
validates, packages into **`dist/<target>/`**, and prints what it made. Per target:

| Target | Build dir | Validate | `dist/` output |
|---|---|---|---|
| `native` | `build_sys/` (Debug, `-Wall -Wextra`, `AB_ENABLE_CHD=ON`, `CMAKE_EXPORT_COMPILE_COMMANDS`) | `ctest`, `lang_tools.py validate` x3 (as `make_win.sh`), `tools/format.sh --check`, `tools/lint.sh` (clang-tidy over the compile db - today Windows-only in practice; `lint.sh` reads `build_win/compile_commands.json`, gains a `--build-dir`) | nothing to ship; the job is the gate |
| `psc` | `build_psc/` | `tools/check_psc_binary.sh` on the launcher and both tools (as `make_psc.sh`) | `autobleem-psc-<version>.zip` from the new `tools/make_psc_package.sh` (below); UPX on the three binaries unless `AB_NO_UPX` |
| `rpi` | `build_rpi/` | `file` says ARM EABI5 hard-float; `readelf` highest `GLIBC_` <= Bookworm's 2.36 (the reason for the Bookworm base) | `autobleem-rpi.tar.gz` via `tools/make_rpi_package.sh` (already host-independent: it needs only `build_rpi/autobleem-gui`) |
| `rpi64` | `build_rpi64/` | same, AArch64 | `autobleem-rpi-arm64.tar.gz` via `... --arch arm64` |
| `win` | `build_mingw/` (Release) + `build_updateroms/` | builds; tests if wine | `UpdateRoms/` folder zipped (`tools/make_updateroms_bundle.sh`, generalised: strip/upx by triplet, no MSYS2 paths) and `autobleem-win-<version>.zip` = exe + resources + the four SDL DLLs, the smoke-test build for anyone without MSYS2 |

`tools/make_psc_package.sh` is the missing console release script: `payload/` as checked in (rc scripts,
themes, Apps with the tools `make_psc.sh` copies in, the exploit folder, release notes) + `Autobleem/bin/
autobleem/` (`build_psc/autobleem-gui` + `src/resources`, `internal.db` included - the console needs it) +
`Autobleem/bin/db/` (see "Open questions" - the cover DBs) -> `dist/psc/autobleem-psc-<version>.zip`, laid out
so the zip's root is the stick's root, the way `tools/install_autobleem.py` and the old releases expect.
`<version>` is `git describe --tags` (the same `Version::FULL_VERSION` the splash shows).

`make_psc.sh` **stays** for now - it is the owner's ssh workflow from the Windows PC and does nothing wrong.
A one-line follow-up once the image exists on the server: it can `ssh psc-build docker run ...` instead of
needing `~/opt/cmake` and `/opt/toolchain` on the host, but that is a convenience, not part of this plan.

### The pipeline: GitHub Actions with a self-hosted runner on the Linode

The repo is `github.com/autobleem/AutoBleem2`, public, so Actions is free and already where the fork's
contributors expect CI to be. Two workflows:

- **`.github/workflows/image.yml`** - builds `docker/Dockerfile` **on the self-hosted runner** (the only
  place the cover databases are) and pushes it to `ghcr.io/autobleem/autobleem-build:<git sha>` + `:latest`;
  runs on changes under `docker/**` and on manual dispatch. GHCR is free for a public repo, and the cover
  databases were in every public release zip, so an image carrying them is public data too. It is what lets
  a GitHub-hosted runner run the very same jobs. The first build on the 2-core server is ~40-60 min
  (mmdebstrap and four SDL builds); after that only the changed layer.
- **`.github/workflows/ci.yml`** - `runs-on: [self-hosted, linux, x64]`, every job `container:
  ghcr.io/autobleem/autobleem-build:latest` (Actions runs the job's steps inside it, mounting the checkout;
  no `docker run` plumbing in the workflow), each job one `ci/build.sh <target>` + `upload-artifact` of
  `dist/<target>/`. Jobs: `native` on every push and PR; `psc`, `rpi`, `rpi64`, `win` on pushes to
  `develop`/`master`, on `v*` tags and on manual dispatch (a PR gets the test gate in ~5 min, the full set
  is ~45 min on two cores at `-j2` and would queue behind itself on every push otherwise). **Both runner
  kinds** (the owner's decision): each job's `runs-on` comes from a workflow-level choice - the self-hosted
  Linode by default, `ubuntu-24.04` when the `runner` dispatch input says so or when the server is down;
  `pull_request` jobs are **always GitHub-hosted** - a self-hosted runner on a public repository must never
  execute a fork's PR code, and the repo's Actions settings should require approval for outside
  collaborators' workflows as well. `release` on a
  `v*` tag: `needs` all five, downloads the artifacts, `gh release create --notes-from-tag` with the console
  zip, the two Pi tarballs, the UpdateRoms zip and the Windows zip. Concurrency group per ref with
  `cancel-in-progress` so a second push supersedes a running set.
- The runner itself runs **in a container** on the server (the official `actions/runner` image, or
  `myoung34/github-runner`, with `/var/run/docker.sock` mounted and a persistent `_work` volume), registered
  to the repo with a token the owner generates under Settings -> Actions -> Runners. Ubuntu 18.04 is out of
  the runner's supported list and its glibc 2.27 is at the edge; a containerised runner sidesteps both and
  is one `docker compose up -d` (`docker/runner/compose.yml`, with the token in an env file that is *not*
  committed). Container jobs (`container:` above) work with a socket-mounted runner because the runner
  starts them on the host's Docker.
- Sizing: one runner, one job at a time (2 cores); `-j` = `nproc` inside the container = 2. If it turns out
  too slow, the same workflows run unchanged on GitHub-hosted `ubuntu-24.04` runners (4 cores, the image
  pulled from GHCR) by flipping `runs-on` - that is the reason for GHCR rather than a local-only image.

### Server prerequisites (owner, once)

1. Add the CI user to the `docker` group (`sudo usermod -aG docker claude`, or a dedicated `ci` user).
2. Generate the runner registration token in the repo settings; put it in `docker/runner/.env` on the server.
3. `docker compose -f docker/runner/compose.yml up -d`; the runner shows as online under the repo's Runners.
4. Disk: the image (~3 GB) + the runner's `_work` checkout with five build dirs (~2 GB) + docker's build
   cache; 41 GB free is plenty, `docker system prune` in a weekly cron keeps it so.

### Repo changes, in order (one commit each)

1. `docker/Dockerfile` + `docker/build-image.sh` + `docker/run.sh` + `docker/README.md`; verified on the
   server by building the image there and running a compile smoke test for each of the five toolchains
   (the `psc` layer's own test, `arm-linux-gnueabihf-g++ -o /dev/null` with SDL headers, `x86_64-w64-
   mingw32-g++` against the mingw SDL, `clang-format-22 --version`).
2. Toolchain files: `toolchains/rpi/common.cmake`, the `AB_RPI_TOOLCHAIN` fallback in both Pi files,
   `toolchains/mingw/MinGWtoolchain.cmake`. Verified: `make_rpi.sh`/`make_rpi64.sh` on the Windows PC still
   build byte-identically (the `-Os -s` binaries are deterministic enough to diff), and the container
   builds all three.
3. `ci/build.sh` with the `native`, `rpi`, `rpi64` and `psc` targets; `tools/lint.sh --build-dir`; the
   Pi package script called from it. Verified on the server: `docker/run.sh ci/build.sh all` minus `win`,
   `check_psc_binary.sh` passes on the console binaries, the Pi tarballs install on the Pi 400 (the arm64
   one only as far as the package mechanics - no 64-bit card yet, as today).
4. `tools/make_psc_package.sh` + the `psc` target's packaging; verified by `tools/install_autobleem.py
   --dry-run` against the zip's tree. **Not run on a console** - same caveat as every console binary so far.
5. The `win` target: `ci/build.sh win`, `make_updateroms_bundle.sh` generalised, the Windows zip. Verified
   by copying `dist/win/` to the Windows PC and running the launcher against `usb/` with
   `tools/win_drive.ps1`.
6. `.github/workflows/image.yml` + `ci.yml`, `docker/runner/compose.yml`, `docs/ci.md` (the operator's page:
   how to run a build by hand, how to re-register the runner, how to bump a toolchain version). Verified by
   a push to a branch showing green on the self-hosted runner, and a `v2.0.0-pre1`-style tag producing a
   draft release with all five artifacts.
7. CLAUDE.md's Build section rewritten around `ci/build.sh`; this file deleted.

pcsx-ab (`E:\Programming\pcsx-rearmed-develop`, its own repo) is deliberately outside these steps, but the
image is built so it fits: the same three cross toolchains plus `libpng-dev`/`zlib1g-dev` per architecture.
Giving it a `ci/build.sh` and refreshing `payload/Autobleem/bin/emu*/` from CI output instead of by hand is the
natural next plan.

## Decisions (owner, 2026-09-19)

1. **PSC toolchain: the NG route** - Stretch sysroot + gcc-6 + SDL from source, all in the image; the 2019
   Sony toolchain is not used. `libs.tar.gz` is regenerated from the image's SDL build.
2. **The Windows launcher ships** in the next release, so the mingw cross build and `autobleem-win-<v>.zip`
   are release artifacts, next to `UpdateRoms`.
3. **Cover databases are baked into the image** at build time.
4. **Both self-hosted and GitHub-hosted runners** run the same jobs; the image is built on the self-hosted
   one and published to GHCR.
5. Pi sysroot: Bookworm (the recommendation, not objected to) - glibc 2.36, so the binaries load on
   Bookworm and Trixie alike; `RPI_DEBIAN` build-arg for Trixie.
6. Nothing is built on the Windows PC for this work; every image and target build happens on the server.

## Later (not in this round)

- `wine64` in the image and `CMAKE_CROSSCOMPILING_EMULATOR` so the `win` job runs the doctest suites under
  Wine as well - a second run of the same tests, useful only for Windows-specific paths (`SystemInfoService`'s
  registry/`GetAdaptersAddresses` branches, `System::runAndWait`'s stub).
- `tools/make_rpi_image.sh` in CI: it loop-mounts, so it needs `--privileged` and `losetup` in the container;
  keep it a manual step on the Pi 400 until the flashed image has booted for real.
- `make_psc.sh` switching to `docker run` on the server; pcsx-ab's `ci/build.sh`.
