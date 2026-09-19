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
- **`autobleem.retromenele.pl` is a DNS `A` record to the server** (`retromenele.pl`'s DNS is on Netlify;
  the record is the only thing on that side). Ports 443 and 9090 are free and reachable from the Internet
  (checked with a throwaway container on 2026-09-19). Docker's daemon does the port binding, so a
  container of the `claude` user can own 443 without sudo.
- The self-hosted runner is a container (`docker/runner/compose.yml`, `autobleem-runner`, up) talking to
  the host's Docker; `ci/build.sh` writes packages to `~/autobleem/dist/<target>/`
  (`autobleem-psc-<v>.zip`, `autobleem-rpi.tar.gz`, `autobleem-rpi-arm64.tar.gz`, `autobleem-win-<v>.zip`,
  `UpdateRoms-<v>.zip`).
- What must **never** be hosted: BIOS files (the installer's manifests hold hashes and RetroBIOS URLs only),
  copyrighted games, anything from the console's firmware. Cover databases are AutoBleem's own and shipped
  in every release since 2019 - fine to host.

## Decisions

- **Server**: **Caddy** in a container (`caddy:2`), serving a bind-mounted directory
  **`/home/claude/autobleem-repo`** with `file_server browse` on two listeners: **443, HTTPS** for
  `autobleem.retromenele.pl` with a Let's Encrypt certificate Caddy obtains and renews itself through the
  **TLS-ALPN-01** challenge (it runs over 443, so the host's nginx on 80 is not in the way -
  `disable_http_challenge`), and **9090, plain HTTP**, the direct `http://212.71.244.78:9090` fallback.
  Not nginx: Caddy is the one that does certificates without root or port 80. The runner container gets
  the same directory mounted, so a CI job publishes by copying a file. No upload endpoint of any kind -
  publishing is `rsync` over ssh (from the PC or the Pi) or a copy inside the runner. Read-only to the
  world, no listing of dotfiles. What this cannot give is an `http://` -> `https://` redirect for the
  domain (that is port 80): three lines in the host's nginx if the owner ever wants it, not needed.
- **One tree, one naming rule**: every artefact that can have several versions lives in a versioned
  folder with a `.sha256` next to it and a `latest.json` (machine) / `latest` symlink (human) at the
  parent. Consumers pin a version or read `latest.json`; nothing parses directory listings.
- **A generated landing page** (`index.html` at the root, `tools/repo_index.py`, rerun after every
  publish): the latest release's five packages, the two images with the Imager instructions, the RetroArch
  builds - links and sizes, nothing dynamic. Everything else is nginx's autoindex.
- **Retention** (the owner's rule): pre-release builds are not kept - a new pre-release replaces the
  previous one, in `releases/` and in `rpi-imager/images/` alike (`repo_index.py` deletes the older ones
  on every publish). The page shows the latest stable release and, in its own panel marked as a
  development build, that one pre-release (`releases/unstable.json` for machines), and the newest image
  set even when it is a pre-release (the owner asked for both when they vanished). Of the RetroArch builds only the newest
  version is kept. Stable releases and their images stay (1.8 GB of images a release; a stable release is
  rare).
- **The repo's base URL is data, not code**: `AB_REPO_URL` (default `https://autobleem.retromenele.pl`)
  in `ci/`, `--repo URL` in `install.sh`, `repo=` in `autobleem.txt`, so a mirror or a LAN copy for testing
  is one setting. `http://212.71.244.78:9090` is the same tree without TLS.

## Layout

```
autobleem-repo/
  index.html, rpi-install.html      generated (tools/repo_index.py); assets/ for them (hero, font, icon)
  releases/
    latest.json                      the newest stable release: {"version": "v2.0.0", "date": ..., "files": {"psc": {"url","sha256","size"}, "rpi", "rpi64", "win", "updateroms"}}
    unstable.json                    the one pre-release kept, same shape
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
    cores/
      latest.json                    {"armhf": {"url","sha256","date"}, "arm64": {...}}
      armhf/cores-armhf-<YYYYMMDD>.tar.gz   every core + info/assets/autoconfig/database/cheats/overlays/shaders,
      arm64/cores-arm64-<YYYYMMDD>.tar.gz   laid out as the RetroArch tree, plus cores.manifest; newest kept
  db/
    coversU.db coversP.db coversJ.db + .sha256   the cover databases, unchanged since 2020 - what a Pi
                                     install fetches (the Pi package leaves them out since step 7)
```

## Steps

Each step is its own feature branch and commit (gitflow), each proven before the next. **Done steps are
removed from here** (the owner's rule); what they built is described in CLAUDE.md's "The download
repository" paragraphs and in the tools' own headers:

- 1-2: the Caddy server (`docker/repo/`), `tools/repo_publish.sh` + `tools/repo_index.py` + `repo_assets.py`,
  the ab2 page and the Pi manual - live at `https://autobleem.retromenele.pl/` since 2026-09-19.
- 3-4: `ci/build_retroarch.sh` (RetroArch cross-built in the image) and `install.sh --retroarch prebuilt`
  as the default - verified on the Pi 400.
- 6: Imager's repository URL (`rpi-imager/os_list.json`; `make_rpi_image.sh --repo` fills `url`/`icon`),
  in the README - 2026-09-19.
- 7: the Pi package without the cover databases (`make_rpi_package.sh` leaves them out, `--with-covers`
  puts them back; `install.sh`'s `install_cover_databases` fetches them from `db/`, sha256-checked, not
  behind `--no-downloads`) - 2026-09-19, the owner's call.
- 8: `ci/build_cores.sh` (the cores and bundles downloaded from buildbot, packed as
  `rpi/cores/<arch>/cores-<arch>-<date>.tar.gz`, newest kept) and `install.sh`'s `download_cores_tarball`
  (one download into a `RetroArch/` with no cores yet; the buildbot loop is the fallback and the re-run
  path) - 2026-09-19.

### Step 5 - CI publishes (S)

`ci.yml`'s tag job (a `v*` tag → the five packages) also runs `tools/repo_publish.sh --local release
<tag> dist/*/…` inside the runner (the bind mount from step 1), so a tag lands on the repo and as a GitHub
draft release in one run. `latest.json` is only rewritten by a non-prerelease tag. The Docker image build
(`docker/build-image.sh`, `image.yml`) takes the cover databases from `$AB_REPO_URL/db/` when
`AB_COVERS_DIR` is unset - with that, `image.yml` can run on a GitHub-hosted runner too. Two more jobs on a
schedule and `workflow_dispatch`: `retroarch.yml` (`ci/build_retroarch.sh` + publish) and `cores.yml`
(`ci/build_cores.sh` + publish), monthly. **The owner does not want a pre-release published from the server
yet** (2026-09-19) - the job is written so a tag does it, and the first run is the owner's call.

### Step 9 - images built on the server, rootless (M) - agreed 2026-09-19

`tools/make_rpi_image.sh` loop-mounts the base image, which needs root - the reason images have been built
on the Pi 400. Everything it does inside the image is file placement, and the standard tools do that on an
*unmounted* image: `debugfs -w` (e2fsprogs) for the ext4 root - `write` the tarball and
`autobleem-firstboot.sh` into `/opt/autobleem-image/`, `write` the `.service` into
`/etc/systemd/system/`, `symlink` it under `multi-user.target.wants/`, `mkdir` as needed - and `mtools`
(`mcopy -i <img>@@<offset>`) for the FAT boot partition: `cmdline.txt` out, `resize` removed, back in;
`autobleem.txt` in. Partition offsets from `sfdisk -J` on the image file. xz, sha256 and the Imager JSON
are plain userland. So `ci/build_image.sh <armhf|arm64> --package ...` runs inside the existing
`autobleem-build` image through `docker/run.sh` (the Dockerfile's last stage gains `mtools`; `e2fsprogs`,
`xz-utils` and `util-linux` are there), no `--privileged`, no qemu - still injection-only. Verification is
mount-free too: `debugfs -R "ls -l ..."` and `mcopy` out to compare, plus the re-decompressed image's
partition table. The Pi 400 then only ever *tests* an image, on the owner's request from the PC - never as
part of CI (there is no Pi in the cloud, the owner's rule). The mount-based path stays in
`make_rpi_image.sh` for a machine with root; `ci/build_image.sh` is what the server and a tag run. One CI
run then yields the five packages *and* the two images, published together.

## Out of scope

- DNS: the owner's, on the Netlify side (one `A` record).
- Mirroring libretro's thumbnails or RetroBIOS: they stay at their sources (the cores and bundles are
  mirrored as one tarball, step 8 - done); the launcher's `OnlineAssets` keeps its URLs.
- An "update available" check in the launcher, and the fast in-place Pi update (IDEAS.md): both would read
  `releases/latest.json` - this repository is their prerequisite, not their implementation.
- Uploads through HTTP, accounts, statistics.

