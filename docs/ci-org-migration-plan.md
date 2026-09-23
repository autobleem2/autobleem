# Migrating to the `autobleem2` org, public + full GitHub-Actions CI

Plan written 2026-09-22. Supersedes and absorbs `docs/ci-public-migration-plan.md` (the "go public in
`autobleem`" plan): the owner has created a **new GitHub organisation, `autobleem2`**, and the decision now
is to gather every repository needed to build AutoBleem 2 into that one org, all **public GPL-3.0-or-later**,
and run the whole pipeline on **GitHub-hosted runners** with a small set of shared, reusable workflows that
cover develop nightlies, releases, pre-releases and manual triggers. The build server (`psc-build`) keeps
only the jobs that must touch its disk (baking the cover databases, and publishing to the download site).

This plan is the record; nothing here flips visibility, transfers a repo, or sets `AB_CI_ENABLED` on its own
— those are owner actions, called out where they fall. The still-valid audit, runner-placement and sccache
detail from the old public-migration plan are folded in below; delete `docs/ci-public-migration-plan.md` once
this doc is the one people read (kept for now so its audit is not lost mid-migration).

## 1. Component inventory — everything needed to build AutoBleem 2

Five repositories build the shipping product; two toolchain **images** underpin them; a handful of
second-tier repos supply optional Apps/ports. Current homes and remotes below are what a `git remote -v`
shows today.

### Tier 1 — required to build a release (move to `autobleem2`)

| # | Component | Today's repo | Proposed `autobleem2` name | Builds | Toolchain image | Ships |
|---|---|---|---|---|---|---|
| 1 | **Launcher** (the app, console tools, all tooling, the download-site scripts, the Docker image, `ci/build.sh`) | `autobleem/AutoBleem2` | `autobleem2/autobleem` | `native` + 5 cross targets (`psc rpi rpi64 pcusb win`) | **owns `autobleem-build`** | the 5 launcher packages, the 3 disk images, the PC/console installers, the download site |
| 2 | **pcsx-ab** (classic PS1 emulator, the 2017 snapshot) | `autobleem/pcsx-ab2` | `autobleem2/pcsx-ab` | `psc rpi rpi64 pcusb` | `autobleem-build` (toolchain cmake only) | `emu/pcsx-ab/` packages; folded into launcher payloads |
| 3 | **pcsx-abnxt** (next PS1 emulator, r26 fork) | `autobleem/pcsx-abnxt` | `autobleem2/pcsx-abnxt` | `psc rpi rpi64 pcusb` | `autobleem-build` | `emu/pcsx-abnxt/` packages; folded into launcher payloads |
| 3a | **libpicofe** (pcsx-abnxt's front-end submodule) | `autobleem/libpicofe` | `autobleem2/libpicofe` | (submodule of #3) | — | consumed via `submodules: recursive` |
| 4 | **RetroArch for the console** (frontend + libretro cores, xz-core patch) | `autobleem/retroarch-psc` | `autobleem2/retroarch-psc` | RetroArch + cores | **own image** (crosstool-ng + `/opt/psc` stage in its own `Dockerfile`) | `psc/retroarch/`, `psc/cores/`, `psc/libs/`, `psc/apps/` |
| 5 | **PSC kernel/flasher payload** (Buildroot; boot.img + abrootfs.tgz for abflashkit) | `autobleem/psc-kernel-payload` *(private)* | `autobleem2/psc-kernel-payload` | boot.img + abrootfs.tgz (2 variants) | **own image** `autobleem-kernel-build` (extends `autobleem-build` + Buildroot host deps) | kernel-payload preview build input; abflashkit `kernel/` artefacts |

Notes that drive the workflow design:

- **One shared image for #1–#3.** The launcher and both emulators compile inside the same
  `autobleem-build` image; the emulators carry only `toolchains/{psc,rpi,rpi64,pcusb}/` cmake files, no
  Dockerfile. The launcher's `ci/build.sh` already checks the emulators out next to itself and builds them
  first, so a launcher package always ships an emulator built by the same image and run. That coupling means
  the **launcher repo is the natural home of the release pipeline**; the emulator repos need only a light
  "build my `dist/` and publish my packages" workflow.
- **#4 and #5 own their images.** `retroarch-psc` has a separate toolchain (crosstool-ng, a different
  `/opt/psc` recipe); `psc-kernel-payload` builds a kernel with the console gcc-6 plus a Buildroot userland,
  in `autobleem-kernel-build` which *extends* `autobleem-build`. These two are independent pipelines that
  publish their own artefacts to the site on their own cadence (RetroArch and cores change rarely; the kernel
  payload rarer still). They are **not** rebuilt on every launcher push.
- **The cover databases** ride inside `autobleem-build` (its `db` stage), not in any repo (`db/` is
  git-ignored). Baking them is the one step that must run on the server (they live only there), so the
  **image build stays self-hosted**. Everything downstream just pulls the image.

### Tier 2 — optional Apps / ports (migrate later, not on the release critical path)

These produce the third-party Apps bundled via the site's `psc/apps` pack; the launcher builds and releases
fine without them (the packs are published independently and installed on demand). They live under the
owner's personal `screemerpl` account today (GitLab-mirror migration, 2026-09-21).

| Component | Today | Note |
|---|---|---|
| Amiberry PSC port | `screemerpl/amiberry-psc` | Apps pack |
| OpenBOR PSC port | `screemerpl/openbor-psc` | Apps pack (history scrubbed of a 100 MiB win-sdk blob) |
| Themes pack | `screemerpl/autobleem-themes-pack` | optional themes |
| Gameports pack | `screemerpl/autobleem-gameports-pack` | Apps pack sources |

### Not migrated (reference/history only)

- **GitLab kernel mirror chain** (`screemerpl/psc-kernel`, `psc-bluez`, `psc-rootfs`, `abflashkit`,
  `psc-toolchain`, `autobleem-1x`, `libmamecd`, …) — superseded by `psc-kernel-payload` (#5) for the kernel,
  and by the in-tree `apps/` ports for the tools. Keep as archived provenance under `screemerpl`; they are
  **not** build inputs and do not move to `autobleem2`.
- The server-side download tree `/home/claude/autobleem-repo` and the retired GitLab instance are not git
  repos on GitHub; unaffected.

## 2. Standardising the repos during migration

While transferring, fix names, descriptions and metadata to one house style. **Recommended** (owner to
confirm — see §7):

- **Repo names**: drop the version suffixes that were disambiguators inside the old org.
  - `AutoBleem2` → **`autobleem`** (the org already carries the "2"). Product name stays "AutoBleem 2".
  - `pcsx-ab2` → **`pcsx-ab`** (matches the shipped binary name and every `AB_PCSX_DIR` fallback).
  - `pcsx-abnxt`, `retroarch-psc`, `psc-kernel-payload` — already clean, keep.
- **Descriptions** (one line each, GPL-3.0, links to the site):
  - `autobleem` — "AutoBleem 2 — game launcher / front-end for the PlayStation Classic, Raspberry Pi and PC."
  - `pcsx-ab` — "PCSX-ReARMed fork (pcsx-ab) — the classic PS1 emulator AutoBleem 2 ships."
  - `pcsx-abnxt` — "PCSX-ReARMed r26 fork (pcsx-abnxt) — AutoBleem 2's next PS1 emulator."
  - `retroarch-psc` — "RetroArch + libretro cores cross-built for the PlayStation Classic."
  - `psc-kernel-payload` — "Buildroot tree rebuilding the PlayStation Classic kernel-flasher payload."
- **Default branch**: `develop` for the launcher and emulators (matches current flow); `main` for
  retroarch-psc (already). Releases are cut from tags, so the default branch is the nightly line.
- **Topics/labels**: `playstation-classic`, `emulator`, `retrogaming`, `gpl-3-0`, `raspberry-pi` as fits.
- **Licence + community files**: every repo already GPL-3.0; add a short `SECURITY.md` and ensure `LICENSE`
  is at the root (launcher has it). `TRADEMARKS.md` (name/logo/theme carve-out) travels with the launcher.

**Transfer, not re-create.** GitHub's *Transfer ownership* moves a repo with its history, issues, PRs, stars
and — crucially — **sets up permanent redirects** from `autobleem/*` to `autobleem2/*`, so existing clones,
the `actions/checkout repository:` references and the site URLs keep resolving during the cutover. A rename
also redirects. Re-creating + pushing would lose all of that and break every cross-checkout mid-migration.
The only follow-up after a transfer is updating each local remote (`git remote set-url`) and the hard-coded
`repository:` strings in the workflows (§4) once everything has landed.

## 3. Workflow architecture — reusable, four trigger classes

Design goal: **one reusable build workflow per toolchain image**, called by thin per-repo trigger workflows,
so a new component is "add a caller that says which targets." GitHub reusable workflows (`workflow_call`)
plus a composite action for the common container+checkout+sccache preamble.

### 3a. Shared building blocks (live in the launcher repo, `autobleem2/autobleem`)

- **`.github/workflows/_build.yml`** (`on: workflow_call`) — the current `ci.yml` `cross`/`native` bodies
  generalised: inputs `target`, `runner` (`github`/`self-hosted`), `image`, `version`, and the emulator
  checkout toggle. Emits the `dist/<target>/` and `emu-<target>` artifacts. Other repos call it, or a copy,
  via `uses: autobleem2/autobleem/.github/workflows/_build.yml@develop`.
- **`.github/actions/setup-build`** (composite) — pull/login the container, `actions/checkout` with
  `fetch-depth: 0`, and set up **sccache on the GitHub Actions cache** (`mozilla-actions/sccache-action` +
  `SCCACHE_GHA_ENABLED=true`), keyed per target. This replaces the host cache mount that only helped the
  Linode. `ci/build.sh` gets a one-line guard: when `SCCACHE_GHA_ENABLED` is set, do **not** force
  `SCCACHE_DIR`, so it uses the GHA backend.
- The **image workflow** (`image.yml`) stays self-hosted and owns publishing `ghcr.io/autobleem2/autobleem-build`
  (`:latest` + `:<sha>`). The GHCR package must be **public** so hosted runners pull it tokenlessly.

### 3b. Trigger classes (the four the owner asked for)

| Class | Trigger | Runs | Version string | Publishes |
|---|---|---|---|---|
| **Nightly / develop** | `push` to `develop` (+ optional nightly `schedule` if quiet) | `native` + 5 cross on hosted runners, parallel | `v2.0.0-pre0-<sha>` (pre-release) | site pre-release (replaces previous), emulator `emu/` packages, the 3 images — the current `site` job |
| **Pre-release** | `push` of a `v*-pre*` / `v*-alpha*` / `v*-beta*` tag, **or** manual dispatch `publish: true` | same 5 cross + `release` | the tag | draft GitHub **pre-release** (`gh release create --prerelease`) + site under the tag |
| **Release (stable)** | `push` of a plain `v*` tag (no `-pre`/suffix) | same 5 cross + `release` | the tag | draft GitHub **release** + site as the "latest" stable channel |
| **Manual** | `workflow_dispatch` | inputs: `runner` (github/self-hosted), `targets` (subset), `publish`, `images` | dispatch-derived | as chosen |

Pull requests: `native` only, **always on hosted runners** (a self-hosted runner on a public repo must never
run a fork's code). This is already how `ci.yml` behaves; keep it.

Pre-release vs release is a one-line predicate on the tag: `contains(tag, '-')` (semver pre-release syntax) →
`--prerelease`, else stable. The site already distinguishes them (`unstable.json` vs `latest.json`); the
GitHub release flag just mirrors that.

### 3c. Per-repo callers

- **`autobleem2/autobleem`** — the full `ci.yml` (native + cross matrix + release + self-hosted `site`),
  essentially today's file with the runner default flipped to hosted and the org strings updated.
- **`autobleem2/pcsx-ab`, `autobleem2/pcsx-abnxt`** — a small workflow: on `push`/`workflow_dispatch`, build
  the four Linux targets in `autobleem-build`, run `tools/make_packages.sh`, upload artifacts, and on a
  tag/dispatch publish `emu/<name>/` to the site (self-hosted publish step, or hand off to the launcher's
  `site` job). These mostly exist as `ci/build.sh` + `make_packages.sh`; they just need the workflow file.
- **`autobleem2/retroarch-psc`** — its own image + `make release`; on tag/dispatch publish
  `psc/retroarch`, `psc/cores`, `psc/libs`, `psc/apps`. Low cadence.
- **`autobleem2/psc-kernel-payload`** — Buildroot build in `autobleem-kernel-build`; on dispatch/tag,
  publish the payload preview build input. Rarest cadence; incremental caching matters most here.

### 3d. The site publish stays server-side

`site`, `image.yml` and `site-refresh.yml` remain **self-hosted** (they write the server's disk / bake the
cover DBs / build the PC image as root with `mmdebstrap`). Everything that only *compiles* moves to hosted
runners. Net effect vs today: `native` and `cross` default to `ubuntu-24.04`; the server stops compiling and
just publishes.

**To do - page updates through CI (the owner's ask, 2026-09-23).** `autobleem2/autobleem-repo` gets a
workflow of its own, so changing the download page is a commit and nothing more: on a push to `master` that
touches `tools/repo_index.py`, `tools/repo_assets.py` or `tools/repo_publish.sh` (and on
`workflow_dispatch`), a self-hosted job runs `tools/repo_publish.sh --local index` (plus `assets` when
`repo_assets.py` changed) in a sibling `autobleem-build` container with `/home/claude/autobleem-repo`
bind-mounted - the same pattern the emulators' `publish` jobs use. A job running from a git checkout keeps
the generator's three-way merge base moving, and the root-owned-files problem is already handled
(`--local` as root hands the tree back to its owner). No pull-request trigger, since the job runs on the
self-hosted runner. Until it exists, a page change is published by hand from a checkout (autobleem-repo's
`CLAUDE.md`, which also holds the rules for the pages' look).

## 4. The concrete string/settings changes the migration needs

Once repos are transferred/renamed:

- **Workflow `repository:` refs** — `ci.yml` (and any caller) hard-codes `autobleem/pcsx-ab2` and
  `autobleem/pcsx-abnxt` in `actions/checkout`. Update to `autobleem2/pcsx-ab` and `autobleem2/pcsx-abnxt`.
- **Image refs** — `IMAGE: ghcr.io/autobleem/autobleem-build:latest` → `ghcr.io/autobleem2/autobleem-build:latest`
  in `ci.yml`, `image.yml`, `docker/run.sh` default, `docs/ci.md`.
- **Product/site links in code** — the download URL `autobleem.retromenele.pl` is a product URL, unaffected
  by the org move. The GitHub links in READMEs (`github.com/autobleem/AutoBleem2` → `.../autobleem2/autobleem`)
  and `retroarch-psc`'s README reference should be updated (redirects cover the gap meanwhile).
- **The runner registration PAT** (`docker/runner/.env`, git-ignored) is re-issued for the `autobleem2`
  org/repo with *Administration: read/write*; the runner re-registers against the new repo.
- **`AB_CI_ENABLED`** repo variable set on `autobleem2/autobleem` (and each caller repo) — the master switch;
  stays `false`/unset until §6 Phase 5.
- **Local remotes** — `git remote set-url origin` in each of the five working checkouts on the PC and on
  `psc-build`.

## 5. Security / correctness for a public repo + self-hosted runner

Carried from the audit (re-verify at the tip that gets published):

- **Secret scan clean** across all history (591 commits): no key/token/credential ever committed; infra
  strings (`autobleem.retromenele.pl`, `212.71.244.78`, `psc-build`) are product/local aliases, not secrets.
  Re-run `git log --all -p | grep -aInE '<pattern set>'` before flipping visibility on each repo, especially
  `psc-kernel-payload` which has not been through the audit yet.
- **Fork PRs never reach the server**: `pull_request` pinned to hosted runners; turn on *Settings → Actions →
  Require approval for all outside collaborators*.
- **Least-privilege `GITHUB_TOKEN`**: default read; per-job `permissions:` (`release: contents: write`,
  `image: packages: write`) kept.
- **GHCR package public** so hosted runners pull the image tokenlessly.
- Optional later: SHA-pin `actions/*` and `sccache-action`; Dependabot for workflow actions.

## 6. Phased migration (one reviewable step each)

Everything through Phase 4 is inert (gated by `AB_CI_ENABLED`), so it can land while the current server
pipeline keeps running.

1. **Plan + decisions.** This doc; owner confirms org scope, the rename set (§2/§7), and transfer-vs-recreate.
   *Verify:* doc merges; secret scan re-run clean on each repo to be published (incl. `psc-kernel-payload`).
2. **Reusable-workflow refactor (launcher).** Extract `_build.yml` + `setup-build` composite; add
   `SCCACHE_GHA_ENABLED` guard to `ci/build.sh`; flip `native`/`cross` defaults to `ubuntu-24.04`; add the
   pre-release-vs-release tag predicate to `release`. Still gated. *Verify:* `actionlint`; a throwaway
   `runner: github` dispatch shows sccache hits on a second run.
3. **Emulator + RetroArch + kernel caller workflows.** Add the thin per-repo workflows (§3c), still gated /
   dispatch-only. *Verify:* each parses; a manual dispatch builds and uploads artifacts on hosted runners.
4. **Transfer + standardise (owner).** Transfer the five repos into `autobleem2`; rename per §2; set
   descriptions/topics/branches; make each **public**; make the GHCR image public; update all `repository:`/
   `IMAGE:` strings and local remotes (§4); re-issue the runner PAT and re-register the self-hosted runner
   against `autobleem2/autobleem`. *Verify:* anonymous `docker pull ghcr.io/autobleem2/autobleem-build:latest`
   and anonymous clone of each emulator repo succeed; redirects from old URLs resolve.
5. **Bring the pipeline up.** Set `AB_CI_ENABLED=true`. Push a no-op to `develop`. *Verify:* `native` + 5
   `cross` run **in parallel on hosted runners** and go green; `site` publishes the pre-release on the
   self-hosted runner; measure wall-clock + sccache hit rate. Cut one `v2.0.0-pre1` tag to confirm the draft
   pre-release collects all artifacts, and (later) one stable `v*` for the release path.
6. **Retire the Linode from the build path.** Server compiles nothing — only `image`, `site`, `site-refresh`.
   Fold the still-true parts of `docs/ci.md`/`ci-plan.md` into CLAUDE.md's Build/CI section, delete this file
   and the old plan docs, and update the `no-github-ci` / `repos-stay-private` memory notes to record the
   decision. *Verify:* a full develop push touches the server only for `image`/`site`/`site-refresh`.

## 7. Pilot: pcsx-ab, done first and on its own (owner's scope, 2026-09-22)

The owner chose to **start with the old emulator alone**, because it is small and self-contained, and move
the rest after it is green. `pcsx-ab` already has `ci/build.sh` and `tools/make_packages.sh`, four Linux
targets, and no image of its own (uses `autobleem-build`), so it exercises the whole shared-image +
hosted-runner + sccache + site-publish path with the least new code — a true template.

**Written and inert (Phase 3), 2026-09-22:** `pcsx-ab`'s
`.github/workflows/build.yml` (in the emulator repo, `E:\Programming\pcsx-rearmed-develop`). Jobs:
`plan` (targets + version) → `build` (matrix `psc rpi rpi64 pcusb`, parallel, `autobleem-build` container,
sccache-on-GHA via `mozilla-actions/sccache-action`) + `windows` → `package` (reassemble, `make_packages.sh`)
→ `publish` (self-hosted, tag/dispatch only, `emu/pcsx-ab/` via the launcher's `repo_publish.sh`). Triggers:
push develop/master, `v*` tags, PRs (build only, hosted), and `workflow_dispatch` (runner / targets /
publish). All gated by `AB_CI_ENABLED`; the image ref is `ghcr.io/autobleem2/autobleem-build:latest`.

**The Windows build now runs in CI.** Until now the emulator's `.exe` was only ever built by hand on the
owner's PC (`make_win.sh`, MSYS2) — the Linux `autobleem-build` image cannot cross it usefully. The pilot's
`windows` job builds it **natively on a free `windows-latest` hosted runner** with `msys2/setup-msys2`
(UCRT64: gcc, cmake, ninja, SDL2, libpng), a Release build into `build_win_rel/`, and stages exe + runtime
DLLs + plugin DLLs; `package` picks them up so `make_packages.sh` emits `pcsx-ab-<v>-win64.zip` alongside the
four Linux tarballs. This is a capability the by-hand and server pipelines never had, and the same pattern
gives the **launcher** a CI Windows build later (the launcher's `win` target is likewise MSYS2-only today).

Remaining pilot steps (owner + follow-up):

1. Transfer `autobleem/pcsx-ab2` → `autobleem2`, rename to `pcsx-ab`, set description/topics, make public;
   make the `autobleem-build` GHCR package public so the container pulls tokenlessly.
2. Re-issue the runner PAT for `autobleem2` and re-register the self-hosted runner (needed only for the
   `publish` job; build/package/windows are hosted).
3. Set `AB_CI_ENABLED=true` on `autobleem2/pcsx-ab`; dispatch once (`runner: github`) → confirm four green
   parallel Linux builds + the Windows job + a `packages` artifact carrying all five files.
4. Point the launcher's `cross`/`site` checkouts at `autobleem2/pcsx-ab` once the launcher itself migrates
   (later round) — the pilot proves the standalone emulator pipeline now; the launcher's consumption of it
   is verified when the launcher moves.

Once `pcsx-ab` is green, `pcsx-abnxt` is a near-copy (add `submodules: recursive`, its version is `git
describe`), then the launcher, then `retroarch-psc` / `psc-kernel-payload` with their own images on their
own low cadence.

## 9. Progress log

- **2026-09-22 — pilot repos transferred + made public.** `autobleem/pcsx-ab2` → **`autobleem2/pcsx-ab`**
  (transferred, renamed, now **public**), `autobleem/pcsx-abnxt` → `autobleem2/pcsx-abnxt`,
  `autobleem/libpicofe` → `autobleem2/libpicofe` (both already public). Descriptions/topics set; local remotes
  repointed. Old URLs redirect. `pcsx-ab` secret-scanned clean (30 commits + tree); GPL-3.0 `LICENSE` added
  (core sources are GPLv2-or-later, so GPL-3 is valid by the "or later" option; `COPYING` kept) and README
  refreshed. Both emulator `build.yml` pilots committed and pushed (inert, `AB_CI_ENABLED`-gated).
- **2026-09-22 — gitflow on `pcsx-ab`.** `master` fast-forwarded to the develop tip (no releases yet) and
  set as the **default branch** (matching `pcsx-abnxt`/`libpicofe` and the AutoBleem project convention:
  master = production/default, develop = integration, `feature/*` off develop, `release/*` → master + tag).
  The two merged `feature/*` branches were pruned. The `build.yml` triggers already fit: develop push =
  pre-release/nightly, `v*` tag on master = release.
- **BLOCKER — the toolchain image is not on GHCR.** `ghcr.io/autobleem2/autobleem-build` (and the old
  `autobleem/…`) return 404: the image has only ever existed in the build server's local Docker (the
  workflows have never run). The pilot's hosted jobs pull that image, so **it must be built and pushed to
  `ghcr.io/autobleem2/autobleem-build` and made public** before any green run. One server command
  (`docker/build-image.sh --tag ghcr.io/autobleem2/autobleem-build` + `docker push --all-tags`), then Org
  → Packages → Change visibility → Public. `image.yml` keys off `repository_owner`, so once the launcher
  itself migrates it will push there automatically.

- **2026-09-22 — pilot is GREEN.** `autobleem2/pcsx-ab` `build.yml` ran end to end on hosted runners:
  `plan` + the four Linux targets (`psc/rpi/rpi64/pcusb`, parallel) + the native `windows` job + `package`,
  producing the full package set (`pcsx-ab-<v>-{psc,rpi-armhf,rpi-arma64,pcusb}.tar.gz`, `…-win64.zip`,
  `…​.json`) — including the **first-ever CI Windows build**. Two real bugs fixed on the way, each a genuine
  repo problem, not just CI: (1) the shell scripts were committed from Windows as `100644`, so `ci/build.sh`
  hit "Permission denied" on Linux (fixed to `100755` for all scripts — also fixes fresh clones); (2)
  sccache's built-in GitHub-Actions-cache backend calls the **retired legacy artifact-cache API (v1)** and
  fails every build with "services aren't available" — replaced with a local `SCCACHE_DIR` persisted by
  `actions/cache@v4` (the current cache service), keyed per target, so caching still speeds builds and a
  cache miss is a cold compile, never a failure. `publish` stays self-hosted/tag-or-dispatch only.
  **These same two fixes must be applied to every other repo's workflow** (pcsx-abnxt next).

- **2026-09-22 — releases + node24 + cache proven.** Added a `release` job to both emulators: a `v*` tag
  attaches all five packages (four Linux tarballs + `win64.zip`) plus the json manifest to a **public GitHub
  Release** on the repo, hyphenated tags marked pre-release, on a hosted runner (`GITHUB_TOKEN`, no
  self-hosted runner). Pinned the actions to their node24 majors (`checkout@v7`, `cache@v6`,
  `upload-artifact@v7`, `download-artifact@v8`) - clean logs, no Node-20 warnings. First public release cut:
  **`autobleem2/pcsx-ab` `v2.0.0-alpha1`** (pre-release, 6 assets). The `actions/cache`-backed sccache is
  warming (Linux targets now build in well under a minute). Both emulators' `develop` and `master` carry all
  of this; `master` is the release line. **The download home for an emulator is its repo's Releases page**;
  the site `emu/` publish stays the separate product path (needs the self-hosted runner, not yet registered).

- **2026-09-22 — pcsx-abnxt enabled, with a fork-aware gitflow.** Unlike pcsx-ab (a linear snapshot), abnxt's
  `master` mirrored upstream notaz/pcsx_rearmed (75 commits past `release r26`) while `develop` held all
  AutoBleem work + `build.yml` - genuinely diverged, no fast-forward. Canonical gitflow set up (owner's
  choice): the upstream-tracking `master` was preserved as a new **`upstream`** branch (for pulling notaz
  fixes), and `master` was repointed to `develop` as our production line. So all three repos now read the
  same: **master = default/production, develop = integration, feature/* off develop, release/* + `v*` tags
  on master**; abnxt additionally keeps `upstream` = the notaz mirror. The fork's five upstream CI workflows
  (`ci-libretro`, `ci-linux{,-armhf,-arm64}`, `ci-libretro-emscripten`) were removed from develop/master and
  **disabled repo-wide** (`gh workflow disable`) so the `upstream` mirror branch can't spawn them. abnxt
  pulls the shared image fine (the org-member `GITHUB_TOKEN` has access even though the package is not
  anonymously public), and its `build.yml` (submodules recursive, `git describe` versions) runs the same
  plan + the tag `release` job.

- **2026-09-23 — Tier 1 transfer complete.** `retroarch-psc` and `psc-kernel-payload` (from the old
  `autobleem` org) moved into `autobleem2` and made **public** after the audit (history secret-scanned clean;
  no proprietary blobs - `psc-kernel-payload` is 36 text files, `retroarch-psc`'s font carries its OFL).
  The plan's "not migrated" list was wrong about one repo: **`psc-kernel` is a real build input** -
  `psc-kernel-payload` builds the kernel (Buildroot's `linux` package is off) from `sources/psc-kernel`,
  the MT8167 4.4.22 vendor fork no upstream has, and the `boot.img` we publish owes GPL-2 source. So
  `screemerpl/psc-kernel` → **`autobleem2/psc-kernel`** (public, default branch `master`), with the three
  gcc>=10 build fixes that had lived only in the build server's checkout pushed to `master`, and
  `psc-kernel-payload` now carries it as a real submodule. `psc-bluez` is *not* an input (the build uses
  upstream BlueZ; the old sixaxis patch is an archived reference) - moved back, private under `screemerpl`.
  The split repos also gained CI on 2026-09-23: `autobleem-build` owns the image workflow (the launcher's
  stale copy is gone; the cover databases come from the site's `db/`), the tools' test suites moved from
  `autobleem-core` into the tool repos on a shared harness, `autobleem-pc-tools` packages its three Win32
  programs, `autobleem-appliance` assembles the console's package, and `psc-kernel-payload` builds on a
  hosted runner.

- **2026-09-23 - the download page redesigned; development builds.** The page (autobleem-repo
  `tools/repo_index.py`) is a slim bar + short banner, per-platform tabs with an Install table and folded
  build inputs, one table style with release / pre-release / dev pills; the kernel flasher payload is a row.
  The owner approved the look - the rules are in autobleem-repo's `CLAUDE.md`. **Development builds**: each
  component's develop CI keeps a rolling `nightly` pre-release (autobleem-build's
  `.github/actions/nightly-release`: the tag moved to the commit, the assets replaced; the launcher's
  `publish-launcher.yml` now builds on develop pushes too, named by `git describe`), and
  autobleem-appliance's `assemble.yml` has a `nightly` channel (schedule 03:17 UTC, or a dispatch with
  channel=nightly) that assembles from those (`AB_SOURCE_TAG=nightly`) and publishes to the site's
  `nightly/<version>/` - the page's "dev" pill, three kept, never an update channel. A scheduled run
  skips a night on which no component's nightly changed (the folder's `sources.json` fingerprint).

### Minimal path to a first green pilot run (DONE)
1. Owner (build server): build + push the image to `ghcr.io/autobleem2/autobleem-build`; make the package public.
2. Owner: set repo variable `AB_CI_ENABLED=true` on `autobleem2/pcsx-ab`.
3. Dispatch `build.yml` with `runner: github`, `publish: false` — hosted-only, no self-hosted runner needed.
   The self-hosted runner + PAT are needed only later for the `publish` job (tag/dispatch-publish).

## 8. Open decisions for the owner (§7 question)

1. **Rename set** — accept `AutoBleem2→autobleem`, `pcsx-ab2→pcsx-ab`? (Redirects make it safe; strings get
   updated in Phase 4.) Or keep current names to minimise churn?
2. **Org scope** — move all five Tier-1 repos now, or start with the launcher + `pcsx-ab` pilot and move the
   rest after it is green?
3. **Tier-2 ports** (amiberry/openbor/packs under `screemerpl`) — into `autobleem2` too eventually, or leave
   as personal-account Apps sources?
4. **Nightly cadence** — is `push`-to-develop publishing enough (current behaviour), or also a timed
   `schedule` nightly for quiet days?

## 10. Update channels, the PSC installer and the PC-stick flasher (2026-09-23)

The pipeline now publishes three channels - **release** (`releases/latest.json`, a stable `v*`), **testing**
(`releases/unstable.json`, the one pre-release) and **nightly** (`nightly/latest.json`, the newest development
build) - and Raspberry Pi Imager gets one list per channel (`rpi-imager/os_list{,-testing,-nightly}.json`).
Everything that updates itself or installs from the site still spoke the pre-split language. The owner's
decisions: the PC-stick flasher may require administrator rights (writing a raw disk cannot be done per
user - the one exception to the per-user rule); the PSC installer needs **no offline fallback** (it always
downloads from the chosen channel).

Found broken, not only outdated:

- **The launcher re-offers its own version** on the old "latest" channel: it calls itself
  `Version::VERSION-GIT_HASH` (`v2.0.0-alpha2-5d7629b`) while the site's folders are now the bare tag
  (`v2.0.0-alpha2`) - the old `<VERSION>-<hash>` folder rule the split pipeline no longer follows.
- **A carried-over package made a release offer another version's file**: the pre-release carry-over moved
  alpha1's Windows files into `releases/v2.0.0-alpha2/`, so a Windows launcher would download alpha1's
  `AutoBleemSetup` as "v2.0.0-alpha2" and loop. Carry-over existed because packages were published one
  kind at a time; the appliance now publishes every kind of a release together.

Steps (one commit each, the rule of this plan):

1. **autobleem-repo: no carry-over; a file of another version is not the release's.** `index_releases` stops
   moving packages between pre-releases, and a package whose name carries a version other than its folder's
   goes to `other_files` (kept on disk, never offered as the release's kind).
2. **Launcher: `Version::DESCRIBE`** (`git describe --tags --exclude nightly`: the tag at a
   tag, `v2.0.0-alpha2-6-gba7365c` between tags - exactly the site's release and nightly folder names), in
   the launcher's and autobleem-core's `generate_version.cmake`; the installed version the update check
   compares is that.
3. **Launcher: the channels.** `UpdateService` takes `release | testing | nightly | off` (`release` ->
   `releases/latest.json`, `testing` -> `unstable.json` falling back to `latest.json`, `nightly` ->
   `nightly/latest.json`); config.ini `updates`' default follows the build (a stable tag -> release, a
   `-alpha/-beta/-rc/-pre` tag -> testing, a build between tags -> nightly), the old values migrate
   (`stable` -> `release`, `latest` -> `testing`); Options -> "Updates" offers the four and is shown on every
   platform with an update path (Pi, PC stick, Windows), in all 16 languages; `test_update_service.cpp`
   covers the channels and the describe comparison.
4a. **The stick install/update logic moves into core.** What AutoBleemInstaller does to a stick with a
   package - unpack `psc-fs` over it keeping everything of the user's (config.ini, games, saves, cards,
   ROMs), the legacy-layout conversion, the ROM folders, UpdateRoms - is `installer_core` in
   autobleem-pc-tools today; it moves into autobleem-core (the engine side: `TarArchive`, the catalogs) as a
   `StickInstaller` both the PC installer and the console's own updater (step 8) call. The launcher still
   keeps its own copy of core (`src/code/core`, `lib_ableem`) while the tool repos use autobleem-core as a
   submodule; this step lands the logic in both, and making the launcher consume autobleem-core as a
   submodule too (one copy) is the follow-up worth doing before more shared code accumulates.
4. **PSC installer: a channel dropdown.** AutoBleemInstaller (autobleem-pc-tools; the launcher's
   `apps/installer` copy kept identical) downloads the stick package (`psc-fs`) and UpdateRoms from the chosen
   channel's catalog and shows the stick's installed version (its `VERSION`) against the channel's; the
   package is no longer bundled - `assemble-psc.sh` ships `AutoBleemInstaller-<v>.zip` as the exe and its
   README only. The other downloads (RetroArch, cores, libs, apps, BIOS, covers) are channel-less catalogs.
5. **PC-stick flasher for Windows** (autobleem-pc-tools, the installer's look): a removable disk, a channel,
   the `.img.xz` (`pc/images/` for release/testing, the nightly's `pc-i386` image) downloaded and sha256
   checked, decompressed on the fly (the LZMA SDK's xz decoder vendored next to its 7z reader), written to
   `\\.\PhysicalDriveN` after locking and dismounting its volumes, read back and compared, ejected; fixed and
   system disks refused, size and model shown, a second confirmation. Manifest `requireAdministrator`.
   Shipped in the pc-tools zip and published on the page's PC-stick panel next to the image; `dd` from Linux
   stays documented.
6. **Proof on hardware/VM:** one real online update of a Pi (or the PC-stick VM) from an assembled package
   through `autobleem-update` -> `install.sh --update` (never run with an appliance-built package yet); one
   Windows self-update (C3) against the testing channel; a PC stick written by the flasher booted in
   VirtualBox.
7. **Docs:** the manuals' update and install sections, CLAUDE.md's "The online update", the
   `update-version-match` memory note (the rule it records is gone).
8. **The console updates itself when it is online** (the owner's ask, 2026-09-23: the AutoBleem kernel gives
   it WiFi - pscbios - and the RNDIS gadget). Built into the psc launcher, but **active only when the launcher
   senses a working connection**: a probe of the download site at start and before each daily check (no
   route / no answer = the feature stays silent; a stock-kernel console never sees it). The channel,
   Options -> "Updates", the prompt and the download are the Pi's `UpdateService`, keyed `psc-fs` - the
   stick package is already in every release's and nightly's catalog, so the CI side is only making sure
   `psc-fs` stays published with every channel (it is). The apply cannot run inside the launcher (it
   replaces the launcher's own files): the launcher exits with `MENU_OPTION_UPDATE`, `rc/selection.sh`
   (already a copy on tmpfs, the stick not busy - the standby's design) runs a small updater built from
   step 4a's `StickInstaller` over the downloaded package, then loops back into the new launcher; a failure
   leaves the old files and logs to `System/Logs/update.log`. To settle first: **what fetches on the
   console** - busybox wget (HTTP only - the site's plain `:9090` mirror, with the sha256 from the catalog
   still checked), a curl added to the kernel payload (adding a tool is within the overlay's rules), or a
   fetch in-process; and what an update does to a stick RetroArch/cores/Apps already on it (untouched, as
   the PC installer's update keeps them). Needs `AB_ONLINE_UPDATE` for psc behind the probe, and the console
   pass on real hardware before it is on by default.

Later, noted: a nightly whose launcher did not change keeps the launcher's describe as its folder name, so
an installed nightly sees no update when only an emulator changed (name the folder by date + hash, or
compare `sources.json`); a switch to an older channel is offered as an "update" (kept - switching channels
is the point).
