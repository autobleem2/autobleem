# CI: building every target, by hand and on GitHub Actions

One Docker image, `autobleem-build` (`docker/`), holds every toolchain; one script, `ci/build.sh`, builds,
checks and packages a target inside it; two workflows run that on a self-hosted runner (the build server)
or on GitHub's runners. `docs/ci-plan.md` was the plan; this is the operator's page.

## The targets

| `ci/build.sh` target | build dir | toolchain | what lands in `dist/<target>/` |
|---|---|---|---|
| `native` | `build_sys/` | the image's gcc + Debian SDL2 | nothing to ship - runs ctest, `lang_tools.py validate`, `format.sh --check`, `lint.sh` |
| `psc` | `build_psc/` | `toolchains/psc/PSCtoolchainV8.cmake` over `/opt/psc` (Stretch sysroot, gcc-6, SDL2 2.0.12 built in the image) | `autobleem-psc-<v>.zip` - the USB stick's root (`tools/make_psc_package.sh`) |
| `rpi` | `build_rpi/` | `toolchains/rpi/RPitoolchain.cmake` (Debian `arm-linux-gnueabihf`) | `autobleem-rpi.tar.gz` (`tools/make_rpi_package.sh`) |
| `rpi64` | `build_rpi64/` | `toolchains/rpi64/RPi64toolchain.cmake` (Debian `aarch64-linux-gnu`) | `autobleem-rpi-arm64.tar.gz` |
| `win` | `build_mingw/` | `toolchains/mingw/MinGWtoolchain.cmake` (mingw-w64 posix + `/opt/mingw-sdl2`) | `autobleem-win-<v>.zip`, `UpdateRoms-<v>.zip` (`tools/make_win_package.sh`) |

`<v>` is `git describe --tags --always --dirty`. The build directories are the ones `make_*.sh` use, so a
tree built one way is picked up incrementally by the other. `AB_JOBS`, `AB_NO_LINT=1`, `AB_NO_UPX=1`,
`AB_CLEAN=1` are the knobs (see the script's header).

**pcsx-ab first.** For `psc`, `rpi` and `rpi64` the script begins with the emulator: pcsx-ab's own
`ci/build.sh <target>` in its checkout (`AB_PCSX_DIR`, else `../pcsx-ab`, `../pcsx-ab2` or
`../pcsx-rearmed-develop` next to this tree; the CI checks out `autobleem/pcsx-ab2` there), and the stripped
`pcsx-ab` + `plugins/*.so` replace `payload/Autobleem/bin/emu/` or `payload_rpi/Autobleem/bin/emu{,-arm64}/`
before the package is made - so a package always ships an emulator built by the same image, from the same
run. `AB_NO_PCSX=1` ships the checked-in binaries instead (a developer without that checkout). The console
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
the build directories exist.

`docker/run.sh` mounts the checkout at its own path and runs as you. A build's output stays in `build_*/` and
`dist/`; `docker/run.sh` alone gives a shell in the image. `docker/README.md` has the image's layout.

## On GitHub Actions

- **`.github/workflows/image.yml`** builds the image on the self-hosted runner (the cover databases it bakes
  in are only there) and pushes `ghcr.io/autobleem/autobleem-build:latest` + `:<sha>`. Runs when `docker/**`
  changes on develop/master, or by hand ("Run workflow", optionally with every layer rebuilt). The package
  has to be public (or the repo's `GITHUB_TOKEN` allowed to read it) for GitHub-hosted jobs to pull it: once,
  under the organisation's Packages -> autobleem-build -> Package settings.
- **`.github/workflows/ci.yml`**: `native` on every push and pull request; `psc`/`rpi`/`rpi64`/`win` on
  pushes to develop/master, on `v*` tags and by hand; on a tag, `release` collects the five artifacts into a
  **draft** GitHub release named after the tag (`gh release create --notes-from-tag`) - publish it from the
  Releases page once the notes are right. "Run workflow" takes the runner (`self-hosted` / `github`) and a
  subset of the cross targets.
- Pull requests always run on GitHub's runners: a self-hosted runner on a public repository must never run a
  fork's code. Keep *Settings -> Actions -> General -> "Require approval for all outside collaborators"* on.
- Artifacts: `native` 7 days, the packages 30 days, the release forever.

## The self-hosted runner

A container on the build server (`docker/runner/compose.yml`, image `myoung34/github-runner`), talking to
the host's Docker through the socket:

```bash
ssh psc-build
cd autobleem/docker/runner
cp .env.example .env               # ACCESS_TOKEN = a fine-grained PAT for autobleem/AutoBleem2 with
                                   # "Administration: read and write" (the runner registers itself with it)
sudo mkdir -p /srv/actions-runner/work
docker compose up -d
docker compose logs -f             # "Listening for Jobs" - and it shows under Settings -> Actions -> Runners
```

What the compose file fixes and why: `RUNNER_WORKDIR` is the same path inside and outside the container
(a job's `container:` mounts the workspace by *host* path through the socket); the cover databases are
mounted read-only at `/srv/autobleem-covers` and named in `AB_COVERS_DIR` for `image.yml`; labels
`linux,x64,psc-build`. The server has two cores and 3.8 GB, so the runner takes one job at a time - the
whole set is ~20 min with warm build directories, more from a clean checkout; `runner: github` on a dispatch moves it to GitHub's 4-core machines.

Housekeeping: `docker system prune -f` now and then (build cache grows with every image rebuild), and
`docker compose pull && docker compose up -d` in `docker/runner/` to update the runner.

## Changing a toolchain version

Every version is a build-arg at the top of its stage in `docker/Dockerfile` (SDL2 for the console, the
mingw SDL2 packages, LLVM, UPX, the Debian release). Change the default there, build the image, and let
`ab-validate` (`docker/ab-validate.sh`, run at the end of each stage) prove the stage still links a C++ +
SDL program that fits the target - for the console: nothing above GLIBC_2.24 / GLIBCXX_3.4.22, ARMv8, no
RPATH, a Wayland SDL2.
