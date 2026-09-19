# A download repository on the build server: `autobleem.retromenele.pl`

Plan written 2026-09-19, before implementation - the same role `docs/ci-plan.md` played. Remove it
(CLAUDE.md's "finished plans leave docs/" rule) once every step is done, moving what stays true into
CLAUDE.md's Build section.

## Why

Three things that are slow, manual, or impossible today, all with one answer - a plain HTTP file tree the
build server serves and that anything with `wget`/`curl` can read:

1. **The Pi installer builds RetroArch from source** (10-40 minutes on a Pi 400, longer on a Pi 3), on
   every install, on every card. A prebuilt RetroArch per architecture, downloaded in seconds, makes a
   first boot about as long as its downloads.
2. **Releases have nowhere to live** but the PC's `build_*` folders and the server's `dist/`. GitHub
   releases are the eventual home for the five packages, but the flashable Pi images (0.9 GB each, two
   architectures) and the Raspberry Pi Imager metadata want a URL of their own -
   `tools/rpi_imager_repo.json` still has `url`/`icon` placeholders, and Imager's "Add repository" needs
   an `os_list.json` at a stable address.
3. **The build inputs that are not in git** - the three cover databases (290 MB, `/opt/autobleem/db` in
   the Docker image, `db/` on the PC) - are copied by hand to wherever a build happens. Served from one
   place, the Docker image build, a GitHub-hosted runner and a Pi installer can all fetch them.

## Facts (checked 2026-09-19)

- The server is `psc-build` (Linode, Ubuntu 18.04, Docker 24.0.2, **22 GB free** of 79 GB, public IP
  212.71.244.78). The `claude` user is in the `docker` group and has no sudo. nginx is already on the
  host's port 80 for other sites (`autobleem.tk`, `q3.retromenele.pl`, a GitLab proxy) - **port 80 is not
  ours** and nothing here touches the host's nginx.
- **The owner maps `autobleem.retromenele.pl` to the server's port 9090** (`retromenele.pl` is on
  Netlify; the mapping and any TLS termination are on that side, not in this repo). Our side is plain
  HTTP on 9090, no certificates, no root.
- The self-hosted runner is a container (`docker/runner/compose.yml`, `autobleem-runner`, up) talking to
  the host's Docker; `ci/build.sh` writes packages to `~/autobleem/dist/<target>/`
  (`autobleem-psc-<v>.zip`, `autobleem-rpi.tar.gz`, `autobleem-rpi-arm64.tar.gz`, `autobleem-win-<v>.zip`,
  `UpdateRoms-<v>.zip`).
- What must **never** be hosted: BIOS files (the installer's manifests hold hashes and RetroBIOS URLs only),
  copyrighted games, anything from the console's firmware. Cover databases are AutoBleem's own and shipped
  in every release since 2019 - fine to host.

## Decisions

- **Server**: `nginx:alpine` in a container, port 9090, `autoindex on`, serving a bind-mounted directory
  **`/home/claude/autobleem-repo`** (the runner container gets the same directory mounted, so a CI job
  publishes by copying a file). No upload endpoint of any kind - publishing is `rsync` over ssh (from the
  PC or the Pi) or a copy inside the runner. Read-only to the world, no PHP, no listing of dotfiles.
- **One tree, one naming rule**: every artefact that can have several versions lives in a versioned
  folder with a `.sha256` next to it and a `latest.json` (machine) / `latest` symlink (human) at the
  parent. Consumers pin a version or read `latest.json`; nothing parses directory listings.
- **A generated landing page** (`index.html` at the root, `tools/repo_index.py`, rerun after every
  publish): the latest release's five packages, the two images with the Imager instructions, the RetroArch
  builds - links and sizes, nothing dynamic. Everything else is nginx's autoindex.
- **Retention**: images are the only big thing (1.8 GB a release). Keep the last three release image
  sets; `tools/repo_publish.sh --prune` deletes older ones (and never anything under `db/` or `releases/`).
- **The repo's base URL is data, not code**: `AB_REPO_URL` (default `https://autobleem.retromenele.pl`)
  in `ci/`, `--repo URL` in `install.sh`, `repo=` in `autobleem.txt`, so a mirror or a LAN copy for testing
  is one setting. The direct `http://212.71.244.78:9090` is the fallback while the mapping is being set up.

## Layout

```
autobleem-repo/
  index.html                         generated (tools/repo_index.py)
  releases/
    latest.json                      {"version": "v2.0.0", "date": ..., "files": {"psc": {"url","sha256","size"}, "rpi", "rpi64", "win", "updateroms"}}
    v2.0.0/                          the five packages + SHA256SUMS + release.json (same shape as latest.json)
  rpi-imager/
    os_list.json                     what Raspberry Pi Imager's "Add repository" takes (the filled-in tools/rpi_imager_repo.json)
    icon.png                         AutoBleem's icon for Imager's list
    images/<version>/                autobleem-<v>-rpi-armhf.img.xz, -arm64.img.xz, .sha256 each (built on the Pi 400)
  rpi/
    retroarch/
      latest.json                    {"version": "v1.22.2", "armhf": {"url","sha256"}, "arm64": {...}}
      v1.22.2/
        retroarch-v1.22.2-armhf.tar.gz   a `make DESTDIR=... install` of /usr/local + DEPENDS (runtime packages)
        retroarch-v1.22.2-arm64.tar.gz
  db/
    coversU.db coversP.db coversJ.db + .sha256   the cover databases, unchanged since 2020
```

## Steps

Each step is its own feature branch and commit (gitflow), each proven before the next.

### Step 1 - the server (S)

`docker/repo/compose.yml` (`nginx:alpine`, `9090:80`, `./nginx.conf` and `/home/claude/autobleem-repo`
read-only mounted) and `docker/repo/nginx.conf`: `autoindex on; autoindex_exact_size off;
autoindex_localtime on;`, `sendfile on`, `types` so `.json` is `application/json`, `.img.xz`/`.tar.gz`/
`.zip` are `application/octet-stream`, `location ~ /\.` denied, `add_header Cache-Control` short on
`*.json` and `index.html`, long on the versioned folders. `docker compose up -d` on the server as `claude`.
Prove: `curl -sI http://212.71.244.78:9090/` from the PC, then the owner's mapping, then
`https://autobleem.retromenele.pl/`. The runner compose gets the same bind mount (`/home/claude/
autobleem-repo` at the same path inside).

**Check with the mapping in place**: a 900 MB image downloads whole through the domain (a proxying front
can have body or time limits a plain DNS record would not). If it does not, the images' links on the
landing page use the direct `http://212.71.244.78:9090` address and everything else stays on the domain.

### Step 2 - publish + index tools (S)

`tools/repo_publish.sh <kind> <files...>` - `kind` is `release <version>`, `image <version>`, `retroarch
<version>`, `db` - rsyncs over `ssh psc-build` into the right folder, writes the `.sha256` sidecars, then
runs `tools/repo_index.py` on the server (python3 is there) to regenerate `latest.json`s and
`index.html`. Run from the PC (MSYS2), from the Pi (the images), or on the server itself (`--local`, what
the CI job does). `--prune` for the image retention. Prove: publish the current `db/` (the real cover DBs
from the Docker image's `/opt/autobleem/db`) and the two `933bd2f` images from the Pi.

### Step 3 - prebuilt RetroArch (M)

`ci/build_retroarch.sh <armhf|arm64> [tag]` **inside the Docker image**, cross-compiled - the image already
has `crossbuild-essential-armhf/-arm64` and the multiarch SDL2 dev packages; it gains the rest of
`install.sh`'s build-dep list as `:armhf`/`:arm64` (`libasound2-dev libudev-dev libusb-1.0-0-dev libgbm-dev
libdrm-dev libegl-dev libgles-dev libfreetype-dev zlib1g-dev libxml2-dev libflac-dev`). Same `./configure`
flags as `install.sh` (KMS/EGL/GLES, udev, ALSA, SDL2, networking; no X11/Wayland/Qt/ffmpeg) with
`CROSS_COMPILE=arm-linux-gnueabihf-`, `PKG_CONFIG_LIBDIR` pointed at the multiarch dir, `make
DESTDIR=<stage> install`, plus a `DEPENDS` file (the runtime packages, from `ldd` + `dpkg -S` over the
staged binary) and `VERSION`. Built against the image's Bookworm libraries, so it runs on Bookworm and
Trixie Pi OS alike (a binary built on the older glibc runs on the newer). Output
`retroarch-<tag>-<arch>.tar.gz`; `--latest` asks GitHub for the newest `v*` tag as `install.sh` does.

If RetroArch's `qb` configure fights the cross pkg-config (it is not the buildbot's preferred path), the
fallback is the same script under `docker run --platform linux/arm/v7 debian:bookworm` with
`qemu-user-static` - slow (an hour or two on the server's 2 cores) but RetroArch releases monthly, and it
is exactly what the Pi does natively. Decide by trying the cross build first.

Prove on the Pi 400: untar over `/`, `apt-get install` the `DEPENDS`, start the launcher, play a Genesis
game and a PS1 game through RetroArch. Then `tools/repo_publish.sh retroarch v1.22.2 ...`. A `retroarch.yml`
workflow (`workflow_dispatch` + a monthly schedule, self-hosted) builds and publishes the newest tag.

### Step 4 - the installer uses it (M)

`install.sh --retroarch prebuilt|source|apt|none`, **`prebuilt` the new default**: read
`$REPO/rpi/retroarch/latest.json`, download the matching architecture's tarball (`.part`, sha256 checked,
skipped when `/usr/local/share/autobleem/retroarch.version` already says that version), untar into `/`,
`apt-get install` the `DEPENDS` list (no build-essential, no `-dev` packages), write the version stamp.
`--repo URL` and `autobleem.txt`'s `repo=` override the base URL. **Falls back to `source`** when the repo
is unreachable or has no build for this architecture - the installer must keep working with the server
down. The first-boot script's RetroArch question text changes from "10-40 minute build" to "a download".
`payload_rpi/README.md` and CLAUDE.md's Pi section follow.

Prove: a fresh install on the Pi 400 with `--retroarch prebuilt`, timed against today's build.

### Step 5 - CI publishes (S)

`ci.yml`'s tag job (a `v*` tag → the five packages) also runs `tools/repo_publish.sh --local release
<tag> dist/*/…` inside the runner (the bind mount from step 1), so a tag lands on the repo and as a GitHub
draft release in one run. `latest.json` is only rewritten by a non-prerelease tag. The Docker image build
(`docker/build-image.sh`, `image.yml`) takes the cover databases from `$AB_REPO_URL/db/` when
`AB_COVERS_DIR` is unset - with that, `image.yml` can run on a GitHub-hosted runner too.

### Step 6 - Raspberry Pi Imager (S)

`tools/rpi_imager_repo.json`'s placeholders become real: `url` = `$AB_REPO_URL/rpi-imager/images/<v>/…`,
`icon` = `$AB_REPO_URL/rpi-imager/icon.png` (a 128x128 PNG of the logo, added to `tools/`), `devices`
per architecture (`pi3-32bit`, `pi4-32bit`, `pi400-32bit`, `pi5-32bit`, `pizero2-32bit` and the `-64bit`
set). `make_rpi_image.sh` writes the JSON with the URL it is given (`--repo`), `tools/repo_publish.sh image`
uploads the images and installs the JSON as `rpi-imager/os_list.json`. `payload_rpi/README.md`'s
"Flashing with Raspberry Pi Imager" gets the one-liner: *Imager → App Options → Content Repository →
`https://autobleem.retromenele.pl/rpi-imager/os_list.json`* (or `rpi-imager --repo <that URL>`) - no
local manifest needed any more; `tools/rpi_imager_local_manifest.py` stays for offline builds.

Prove: Imager on the PC lists both images from the URL and flashes the armhf one with presets.

### Step 7 (optional, the owner's call) - the Pi package without the cover databases (S)

The Pi tarball is 306 MB, 290 MB of it the cover databases - the very thing that overflowed the image's
root on 2026-09-19. With `db/` on the repo, `make_rpi_package.sh --no-covers` ships without them and
`install.sh`'s `install_payload` fetches `coversU/P/J.db` by sha256 from `$REPO/db/` when
`Autobleem/bin/db/` is empty (the launcher already tolerates their absence - it shows `default.png`). The
image's staged package drops to ~16 MB and the first boot needs no extra root growth for it. The console
zip keeps its databases (a console has no network). Not done until the owner says so - it makes a Pi
install depend on the repo for its covers.

## Out of scope

- TLS, DNS, the domain mapping: the owner's, on the Netlify side.
- Mirroring libretro's cores, assets, thumbnails or RetroBIOS: they stay at their sources; the installer
  and the launcher's `OnlineAssets` keep their URLs.
- An "update available" check in the launcher, and the fast in-place Pi update (IDEAS.md): both would read
  `releases/latest.json` - this repository is their prerequisite, not their implementation.
- Uploads through HTTP, accounts, statistics.

## Open questions for the owner

1. The Netlify mapping: a proxy rewrite, or a DNS record straight to the server? (A DNS record needs TLS
   on the server, which 9090 does not have; a proxy carries every download through Netlify's bandwidth.
   Either way the plan is the same; only step 1's check differs.)
2. Step 7 - a Pi package that needs the repo for its covers, or the self-contained 306 MB one?
3. Retention: three release image sets (5.4 GB) on 22 GB free - or fewer?
