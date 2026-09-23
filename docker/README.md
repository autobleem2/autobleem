# docker/ - the build image

`autobleem-build` is one Docker image with every toolchain AutoBleem is built with: the native Linux build
(tests, clang-format/clang-tidy), the Raspberry Pi 32-bit and 64-bit cross compilers, mingw-w64 for
Windows, and the PlayStation Classic toolchain (a Debian Stretch sysroot, gcc-6 and the SDL2 family built
from source - AutoBleem-NG's recipe). The cover databases are baked in, so a console package is complete
wherever the image runs. `https://github.com/autobleem2/autobleem-main/blob/develop/docs/archive/ci-plan.md` has the reasoning; the `Dockerfile` header lists the stages.

Nothing here is built on the Windows PC: the image is built and used on a Docker host (the build server,
a GitHub Actions runner).

## Building the image

```bash
docker/build-image.sh                # -> autobleem-build:latest, ~1 h the first time on two cores
docker/build-image.sh --target pi    # just up to one stage (base, native, pi, mingw, db, psc, all)
docker/build-image.sh --no-cache
```

The cover databases (`coversJ.db`, `coversP.db`, `coversU.db`, 280 MB) are not in git. `build-image.sh`
stages them into `docker/db/` (git-ignored) from `--covers DIR` / `$AB_COVERS_DIR`, else the checkout's
`db/` when the files there are real, else the build server's `/AutoBleem/BUILD/data_that_gets_copied/
cover_databases/`.

Versions (SDL2, mingw SDL2, LLVM, the Debian release) are `ARG`s at the top of each stage in the
`Dockerfile`; pass `--build-arg NAME=value` to override one.

## Using it

```bash
docker/run.sh ci/build.sh native     # one target: native, psc, rpi, rpi64, win, all
docker/run.sh ci/build.sh all
docker/run.sh                        # a shell, the checkout mounted at its own path
```

`run.sh` mounts the checkout at its own path (and the pcsx-ab checkout next to it, if there is one) and runs
as the calling user, so build directories (`build_sys/`, `build_psc/`, `build_rpi/`, `build_rpi64/`,
`build_mingw/`) persist between runs, stay yours, and are the same ones `make_*.sh` would use.

Inside the image:

| | |
|---|---|
| `/opt/psc/` | the console toolchain: `bin/armv8-sony-linux-gnueabihf-*`, `sysroot/`, `gcc-6/`, `sdl2/` (`AB_PSC_TOOLCHAIN=/opt/psc`) |
| `/opt/mingw-sdl2/` | SDL2 + image/mixer/ttf for x86_64-w64-mingw32: headers, import libs, `.pc` files, DLLs (`AB_MINGW_SDL2`) |
| `/opt/autobleem/db/` | the cover databases (`AB_COVERS_DB_DIR`) |
| `arm-linux-gnueabihf-*`, `aarch64-linux-gnu-*` | Debian's Pi cross compilers, SDL2 dev packages under `/usr/lib/<triplet>` |
| `x86_64-w64-mingw32-*-posix` | mingw-w64 |
| `clang-format`, `clang-tidy` | LLVM 22, the major MSYS2 has |
| `ab-validate <stage>` | the checks each layer ended with; rerun any time |
