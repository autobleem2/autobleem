# CI: building every target, by hand and on GitHub Actions

One Docker image, `autobleem-build` (`docker/`), holds every toolchain; one script, `ci/build.sh`, builds,
checks and packages a target inside it; two workflows run that on a self-hosted runner (the build server)
or on GitHub's runners. `docs/ci-plan.md` was the plan; this is the operator's page.

## The targets

| `ci/build.sh` target | build dir | toolchain | what lands in `dist/<target>/` |
|---|---|---|---|
| `native` | `build_sys/` | the image's gcc + Debian SDL2 | nothing to ship - runs ctest, `lang_tools.py validate`, `format.sh --check`, `lint.sh` |
| `psc` | `build_psc/` | `toolchains/psc/PSCtoolchainV8.cmake` over `/opt/psc` (Stretch sysroot, gcc-6, SDL2 2.0.14 built in the image) | `autobleem-psc-<v>.zip` - the USB stick's root (`tools/make_psc_package.sh`) |
| `rpi` | `build_rpi/` | `toolchains/rpi/RPitoolchain.cmake` (Debian `arm-linux-gnueabihf`) | `autobleem-rpi.tar.gz` (`tools/make_rpi_package.sh`) |
| `rpi64` | `build_rpi64/` | `toolchains/rpi64/RPi64toolchain.cmake` (Debian `aarch64-linux-gnu`) | `autobleem-rpi-arm64.tar.gz` |
| `pcusb` | `build_pcusb/` | `toolchains/pcusb/PcUsbToolchain.cmake` (Debian `i686-linux-gnu`, `-march=i686`, the image's `pcusb` stage) | `autobleem-pcusb-i386.tar.gz` - the 32-bit PC stick (`tools/make_rpi_package.sh --arch i386`); its unit tests run in the image, i386 being native there |
| `win` | `build_mingw/` | `toolchains/mingw/MinGWtoolchain.cmake` (mingw-w64 posix + `/opt/mingw-sdl2`) | `autobleem-win-<v>.zip`, `UpdateRoms-<v>.zip` (`tools/make_win_package.sh`) |

`<v>` is `git describe --tags --always --dirty`. The build directories are the ones `make_*.sh` use, so a
tree built one way is picked up incrementally by the other. `AB_JOBS`, `AB_NO_LINT=1`, `AB_NO_UPX=1`,
`AB_CLEAN=1`, `AB_NO_SCCACHE=1` are the knobs (see the script's header).

**sccache** (2026-09-20) sits in front of every compiler the image has - `ci/build.sh` (and pcsx-ab's)
configure with `CMAKE_C/CXX_COMPILER_LAUNCHER=sccache`, one cache keyed by each compiler's own binary, so
the native, Pi, MinGW and console builds all draw on it; `docker/run.sh` mounts it from the host
(`~/.cache/autobleem-sccache`, `AB_SCCACHE_DIR`; 10 GB, `AB_SCCACHE_SIZE`) so it outlives the container. A
run ends with the hit/miss stats. A clean checkout (the CI's) then compiles only what changed since the
last run on that host.

**pcsx-ab first.** For `psc`, `rpi`, `rpi64` and `pcusb` the script begins with the emulator: pcsx-ab's own
`ci/build.sh <target>` in its checkout (`AB_PCSX_DIR`, else `../pcsx-ab`, `../pcsx-ab2` or
`../pcsx-rearmed-develop` next to this tree; the CI checks out `autobleem/pcsx-ab2` there), and the stripped
`pcsx-ab` + `plugins/*.so` replace `payload/Autobleem/bin/emu/` or `payload_linux/Autobleem/bin/emu{,-arm64,-i386}/`
before the package is made - so a package always ships an emulator built by the same image, from the same
run. pcsx-abnxt (`AB_PCSXNXT_DIR`, else `../pcsx-abnxt`) is built the same way right after it, into
`Autobleem/bin/emunxt{,-arm64,-i386}/`. `AB_NO_PCSX=1` ships the checked-in binaries instead (a developer
without that checkout); a checkout without the target yet (`pcusb`) is reported and the package ships
without that emulator. The console
emulator is built with `gles=ON` (EGL on Weston - `gpu_gles.so`), the Pis with SDL2's renderer; the 32-bit
targets get the NEON GPU/GTE and Ari64's dynarec, the 64-bit Pi the C interpreter (no aarch64 dynarec in
this fork). `docker/run.sh` mounts that checkout at its own path next to this one.

The Pi and Windows toolchain files work on the Windows PC as before (SysGCC, MSYS2) and pick the Debian
cross compilers when the SysGCC directory is not there; the console's `PSCtoolchainV8.cmake` takes
`AB_PSC_TOOLCHAIN` (`/opt/psc` in the image; `make_psc.sh` still points it at the server's old
`/opt/toolchain`, which is no longer what releases are built with).

## By hand, on the build server

```bash
ssh psc-build
cd autobleem                        # a checkout (or make_psc.sh's rsync of the tree); ../pcsx-ab next to it
docker/build-image.sh               # once, and after a change under docker/ - ~1 h the first time
docker/run.sh ci/build.sh psc       # or native, rpi, rpi64, win, all
ls dist/psc
```

Measured on the server (2 cores), 2026-09-19: `native` 180 s (tests 37/37, format, tidy), `psc` ~3 min
(with pcsx-ab), `rpi` and `rpi64` ~3.5 min each, `win` ~5 min; a full `all` about a quarter of an hour once
the build directories exist. By hand, what the workflow's `site` job does after a build (2026-09-21, the
whole pre-release refreshed this way): `tools/repo_publish.sh --local release v2.0.0-pre0-<sha> dist/*/...`
for the five packages, then each emulator's `tools/make_packages.sh` in its tree and `repo_publish.sh --local
pcsx-ab|pcsx <version> ../<emu>/dist/packages/*`. The server's trees are rsync copies without `.git`:
`AB_GIT_VERSION=v2.0.0-pre0 AB_GIT_HASH=<sha>` for the launcher and `AB_GIT_DESCRIBE=<git describe>` for
pcsx-abnxt go in the environment, through `docker/run.sh`'s `AB_*` pass-through.

`docker/run.sh` mounts the checkout at its own path and runs as you. A build's output stays in `build_*/` and
`dist/`; `docker/run.sh` alone gives a shell in the image. `docker/README.md` has the image's layout.

## On GitHub Actions

Both workflows are behind one switch: the repository variable **`AB_CI_ENABLED`** (*Settings -> Secrets and
variables -> Actions -> Variables*). Until it is `true`, every run is skipped - so the workflows can be
merged before the runner exists, and the pipeline can be paused without touching the files.

- **The image** is built by **`autobleem2/autobleem-build`**'s `image.yml` (moved there 2026-09-23; that
  repo owns the Dockerfile - this tree's `docker/` is a stale copy): on the self-hosted runner for the host's
  layer cache, the cover databases fetched from the site's `db/` against their `.sha256`, pushed as
  `ghcr.io/autobleem2/autobleem-build:latest` + `:<sha>` on master or by hand. The package grants the
  autobleem-build repo write access under the organisation's Packages -> autobleem-build -> Package settings
  -> Manage Actions access.
- **`.github/workflows/ci.yml`**: `native` on every push and pull request; `psc`/`rpi`/`rpi64`/`pcusb`/`win`
  on pushes to develop/master, on `v*` tags and by hand, each with both emulators checked out next to the
  tree (`autobleem/pcsx-ab2`, `autobleem/pcsx-abnxt` with its submodules and tags - its `REV` is `git
  describe`) and their `dist/` uploaded as `emu-<target>` artifacts. **A push to develop publishes** (the
  `site` job, self-hosted only, since 2026-09-21): the five launcher packages to `releases/v2.0.0-pre0-<sha>/`
  - the pre-release, replacing the previous one, which is the launcher's "latest" update channel - and the
  two emulators' packages under `emu/` (each repository's `tools/make_packages.sh` over the artifacts'
  `dist/` folders; the artifacts drop file modes, the job puts the x bit back). The version is `plan`'s:
  the tag on a tag, else `AB_VERSION_FALLBACK` from CMakeLists.txt + the short sha, passed to `ci/build.sh`
  as `AB_GIT_VERSION`/`AB_GIT_HASH` (a bare `git describe --always` in a tagless checkout would name the
  build by its hash alone, which the site would take for a stable release). The console's stick installer
  bundle is made there too (`ci/build.sh win` leaves `AutoBleemInstaller.exe` in `dist/win/`,
  `tools/make_installer_bundle.sh --exe` zips it with the psc tarball), and then the three images - the two
  Pi images (rootless) and the PC stick's (`make_pc_image.sh --mount`, the job's container is privileged
  for it) - are built from the packages and published, replacing the previous pre-release's; the dispatch
  input `images` skips them. On a `v*` tag the same publish goes under the tag's name (a plain tag = the
  latest stable release) and `release` collects the artifacts into a **draft** GitHub release (`gh release
  create --notes-from-tag`) - publish it from the Releases page once the notes are right. "Run workflow"
  takes the runner (`self-hosted` / `github`), a subset of the cross targets, `publish` for a by-hand
  pre-release publish and `images`.
- Pull requests always run on GitHub's runners: a self-hosted runner on a public repository must never run a
  fork's code. Keep *Settings -> Actions -> General -> "Require approval for all outside collaborators"* on.
- Artifacts: `native` 7 days, the packages 30 days, the release forever.

## The self-hosted runner

A container on the build server (`docker/runner/compose.yml`, image `myoung34/github-runner`), talking to
the host's Docker through the socket. It is **org-scoped** to `autobleem2`, so one runner serves every
repo's `self-hosted` jobs (the emulators' `publish`, the launcher's `site`/`image`/`site-refresh`) instead
of one runner per repo:

```bash
ssh psc-build
cd autobleem/docker/runner
cp .env.example .env               # ACCESS_TOKEN = a PAT that can register org runners for autobleem2:
                                   # a classic PAT with the `admin:org` scope, or a fine-grained PAT scoped
                                   # to the autobleem2 org with "Self-hosted runners: read and write".
                                   # RUNNER_WORK = the work directory (default under the claude user's home)
mkdir -p $(sed -n 's/^RUNNER_WORK=//p' .env)
docker compose up -d
docker compose logs -f             # "Listening for Jobs" - and it shows under the org's
                                   # Settings -> Actions -> Runners
```

What the compose file fixes and why: `RUNNER_SCOPE: org` + `ORG_NAME: autobleem2` register one runner for
the whole org; `RUNNER_WORK` is the same path inside and outside the container (a job's `container:` mounts
the workspace by *host* path through the socket); the cover databases are mounted read-only at
`/srv/autobleem-covers` and named in `AB_COVERS_DIR` for `image.yml`; labels `linux,x64,psc-build` (a job's
`runs-on: self-hosted` matches the runner's automatic `self-hosted` label). The server has two cores and
3.8 GB, so the runner takes one job at a time; `runner: github` on a dispatch moves a build to GitHub's
4-core machines. **Org runners must be allowed for the repos that use them**: the org's Settings -> Actions
-> Runner groups -> Default group -> allow `pcsx-ab`, `pcsx-abnxt` (and the launcher), or all repositories.

Housekeeping: `docker system prune -f` now and then (build cache grows with every image rebuild), and
`docker compose pull && docker compose up -d` in `docker/runner/` to update the runner.

## Changing a toolchain version

Every version is a build-arg at the top of its stage in `docker/Dockerfile` (SDL2 for the console, the
mingw SDL2 packages, LLVM, UPX, the Debian release). Change the default there, build the image, and let
`ab-validate` (`docker/ab-validate.sh`, run at the end of each stage) prove the stage still links a C++ +
SDL program that fits the target - for the console: nothing above GLIBC_2.24 / GLIBCXX_3.4.22, ARMv8, no
RPATH, a Wayland SDL2.
