# Going public: free, parallel CI on GitHub-hosted runners

Plan written 2026-09-22. The pipeline is complex and every build today runs on one 2-core / 3.8 GB Linode
(`psc-build`), one job at a time - slow, and the only machine in the loop. Public repositories get
**unlimited, free, standard GitHub-hosted runners** (4 vCPU, x64 *and* native arm64), so the fix is to make
the required repositories public and move the *compiling* onto GitHub's runners, leaving the Linode to do
nothing but publish. This file is the plan; remove it once the pipeline runs green in public (the "finished
plans leave `docs/`" rule) and fold what is still true into CLAUDE.md's Build section and `docs/ci.md`.

This reverses two standing rules - **"repos stay private"** and **"build on psc-build only, workflows stay
gated off"** (the `no-github-ci` / `repos-stay-private` memory notes). It is the owner's decision to reverse
them, made deliberately here; nothing in the phases below flips repository visibility or the `AB_CI_ENABLED`
switch on its own - those two are owner actions, called out where they fall.

**Status 2026-09-22:** the owner has decided **all repositories will go public** - the whole migration is
option A throughout (below), no private-repo checkout token is needed. Timing is open ("at some point"), so
this is a plan of record, **not being implemented yet**: nothing here is coded until the owner is ready to
go public and asks for it.

## Why this, and why it is nearly free to do

- **Unlimited free minutes on public repos.** Standard GitHub-hosted runners are free with no minute cap on
  public repositories (private repos get only 2,000 min/mo). Native arm64 runners (`ubuntu-24.04-arm`,
  `windows-11-arm`, 4 vCPU) are GA and free for public repos too - a bonus we do not need today (see "Later").
- **Parallelism is the real win, not raw cores.** The five cross targets are a matrix; on GitHub's runners
  they build *concurrently* on five separate 4-core machines instead of queuing one behind another on the
  single 2-core Linode. Wall-clock for the whole set drops from ~20-45 min sequential to the slowest single
  target (~5-8 min). Free plan allows 20 concurrent jobs - `native` + 5 cross + `plan` fit with room.
- **The hard parts are already done.** The toolchains live in one Docker image on GHCR
  (`ghcr.io/autobleem/autobleem-build`), the cover databases are baked into that image's `db` stage, and both
  workflows already carry a `self-hosted`/`github` runner switch. Going public is mostly *flipping the
  default placement* and *making three repositories and one package public*, not rewriting the pipeline.

## What this decides (owner)

1. **Make the repositories public** - decided 2026-09-22: **all of them**, at a time of the owner's
   choosing. The three that this pipeline needs are `autobleem/AutoBleem2`, `autobleem/pcsx-ab2`,
   `autobleem/pcsx-abnxt`; all are GPL-3.0(-or-later) already, so this is licence-honest. The emulator repos
   must be public because `ci.yml`'s `cross` job checks them out next to the tree and a hosted runner's
   default `GITHUB_TOKEN` can read only the repo it runs for - with everything public, the bare
   `actions/checkout` works unchanged and no checkout token is involved. The download-repo tooling
   (`tools/repo_*.py`), the plan docs and the infra strings are all fine in the open (see the audit); the
   GitLab legacy mirrors and the server-side `/home/claude/autobleem-repo` tree are *not* git repositories on
   GitHub and stay as they are.
2. **Builds move to GitHub-hosted runners by default; the Linode becomes publish-only.** The self-hosted
   runner keeps exactly three jobs, all of which must touch the server's disk: `image` (bakes the cover
   databases, only there), `site` (bind-mounts the download repo and runs `make_pc_image.sh --mount` as
   root), and `site-refresh`. Everything that only compiles goes to `ubuntu-24.04`.
3. **The `autobleem-build` GHCR package becomes public** so hosted runners can pull it without a token dance.
   The 2019 cover databases it carries were in every public release zip already, so the image is public data
   too (the same reasoning as `docs/ci-plan.md`).

## Pre-flight audit (done 2026-09-22, before any visibility change)

- **Secret scan: clean.** 591 commits across all refs. No `.pem`/`.key`/`id_rsa`/`id_ed25519`/`.env`/
  credential/keystore file was ever added; no `ghp_`/`github_pat_`/`AKIA`/`xox`/`AIza`/PEM-private-key/
  `ACCESS_TOKEN=`/`RUNNER_TOKEN=` string appears anywhere in history or the working tree. The only
  `sudo -S` / `plink` hits are prose in CLAUDE.md describing the Pi-over-ssh *method*, with no literal
  password. **No rotation needed.** Re-run before flipping visibility if history changes:
  `git log --all -p | grep -aInE '<the pattern set above>'`.
- **Infra strings are not secrets.** `autobleem.retromenele.pl` is the shipped download-site URL, compiled
  into `src/code/core/services/update_service.h`, `src/resources/platform/*.ini`, the PC installer and the
  manuals - it is a *product* URL and already public in every binary. `212.71.244.78:9090` is that same
  server's plain-HTTP mirror. `psc-build` is a local ssh-config alias. Nothing here needs redacting; 81
  occurrences across ~29 files, all of them intentional.
- **The tree is already publish-clean.** GPL-3.0-or-later (`LICENSE`, 2026-09-21); no BIOS file in the repo
  (only hashes/URLs); OFL fonts; generated sounds/images/music; `TRADEMARKS.md` carves the name/logo/theme
  artwork out of the grant. The cover databases are git-ignored (`db/`) - they ride in the image, not the
  repo.
- **The workflows already expect this.** `ci.yml` forces `pull_request` onto `ubuntu-24.04` ("a self-hosted
  runner on a public repository must never execute a fork's code"), gates everything on
  `vars.AB_CI_ENABLED`, and takes a `runner: self-hosted | github` dispatch input. `docs/ci-plan.md` was
  written assuming the repo is public.

## Design - what actually changes

### Runner placement

| Job (`ci.yml` / others) | Today | After | Why |
|---|---|---|---|
| `native` (tests, format, lint) | hosted on PR, else self-hosted | **`ubuntu-24.04` always** | free, no server disk needed |
| `plan` | `ubuntu-24.04` | unchanged | trivial |
| `cross` (psc/rpi/rpi64/pcusb/win) | self-hosted by default | **`ubuntu-24.04` by default**, self-hosted only on dispatch `runner: self-hosted` | free, parallel; nothing but the image + checkout needed |
| `release` (tag → draft release) | `ubuntu-24.04` | unchanged | uses `GITHUB_TOKEN` only |
| `site` (publish + `pc-image --mount`) | self-hosted, `--privileged`, repo bind-mount | **unchanged (self-hosted only)** | writes the server's disk, needs root for mmdebstrap |
| `image.yml` | self-hosted | **unchanged (self-hosted only)** | bakes the cover DBs, only there |
| `site-refresh.yml` | self-hosted | **unchanged (self-hosted only)** | writes the server's disk |

The one-line effect: `cross`'s `runs-on` default flips from `self-hosted` to `ubuntu-24.04`; `native` stops
choosing self-hosted at all. The `site`/`image`/`site-refresh` jobs are untouched - they *must* stay on the
server, and they only ever trigger on `push`/tag/`schedule`/dispatch, never on a fork PR.

### The image and the cover databases (already solved, one setting)

`image.yml` builds `docker/Dockerfile` on the self-hosted runner (the cover DBs are mounted there at
`AB_COVERS_DIR`) and pushes `ghcr.io/autobleem/autobleem-build:latest` + `:<sha>`. For hosted runners to
`container:`-pull it, the GHCR package must be **public** (Org → Packages → autobleem-build → Package
settings → Change visibility → Public), or the org must allow the repo's `GITHUB_TOKEN` to read it. This is
a one-time click, already flagged in `docs/ci.md`. No image or Dockerfile change.

### The emulator-repo checkout (resolved: all public)

`cross` and `site` do `actions/checkout` of `autobleem/pcsx-ab2` and `autobleem/pcsx-abnxt`. With all
repositories public (the 2026-09-22 decision), the bare `actions/checkout` steps work exactly as written -
no token, no secret, no Phase 3 change to these steps. (Had an emulator repo stayed private, the fallback
would have been a fine-grained PAT with read on it, stored as a repo secret and passed via `token:` to the
two checkout steps - safe from fork PRs, since forks never receive secrets. That path is now moot and is
kept only as a note in case the decision ever narrows.)

### sccache → GitHub Actions cache (new work)

Today `ci/build.sh` sets `CMAKE_C/CXX_COMPILER_LAUNCHER=sccache` and `docker/run.sh` mounts the cache from
the host (`~/.cache/autobleem-sccache`) - which only helps the self-hosted runner. On hosted runners that
directory is empty every run, so every build is cold (psc ~263 s, etc.). Cold + parallel is already faster
than warm + sequential on the Linode, but we can keep it warm:

- Add `SCCACHE_GHA_ENABLED=true` for hosted jobs and provide the GitHub Actions cache backend token via
  `mozilla-actions/sccache-action` (a step before the build), and have `ci/build.sh` **not** force
  `SCCACHE_DIR` when `SCCACHE_GHA_ENABLED` is set (a small guard in the script). sccache then reads/writes
  the repo's Actions cache instead of a local dir.
- Caveat: the Actions cache is ~10 GB per repo with LRU eviction; five targets' object caches can crowd it.
  Key the cache per target and accept that the least-used target may cold-build occasionally. The image's
  toolchains and the cover DBs are *in the image*, so only app/emulator compilation is ever cold - the cache
  only has to cover that.

This is the only genuinely new code in the migration; everything else is a placement flip and GitHub
settings.

### Security hardening for a public repo with a self-hosted runner

- **Fork PRs never touch the server.** Already enforced (`pull_request` → `ubuntu-24.04`). Keep it, and turn
  on *Settings → Actions → General → Fork pull request workflows → "Require approval for all outside
  collaborators"* (or "all first-time contributors").
- **Least-privilege token.** Set *Settings → Actions → General → Workflow permissions → "Read repository
  contents and packages permissions"* as the default, and keep the per-job `permissions:` blocks (`release`
  = `contents: write`, `image` = `packages: write`) that already exist.
- **The runner registration PAT and the publish path live on the server, not in the repo** (`docker/runner/
  .env`, git-ignored; the `site` job bind-mounts a host directory rather than reading a secret). Nothing new
  is exposed by going public.
- **Optional supply-chain hardening**: pin `actions/*` and `mozilla-actions/sccache-action` to commit SHAs.
  Not required to ship; note it for later.

## Phases (one commit each, verify each)

The whole thing is inert until Phase 5 flips `AB_CI_ENABLED` - so Phases 1-3 can land on `develop` with the
pipeline still paused, exactly as the original CI plan intended.

1. **Pre-flight and decisions.** This document; the audit above; owner confirms option A vs B for the
   emulator repos and that the three repos will go public. *Verify:* the doc merges; the secret scan re-run
   is clean at the tip that will be published.
2. **sccache on the Actions cache.** Add the `mozilla-actions/sccache-action` step and `SCCACHE_GHA_ENABLED`
   env to `native` and `cross` in `ci.yml`; add the `SCCACHE_GHA_ENABLED` guard to `ci/build.sh` so a set
   `SCCACHE_DIR` is skipped in GHA mode. *Verify:* a `runner: github` dispatch (still gated, so run it once
   `AB_CI_ENABLED` is briefly on, or on a throwaway branch with the var set) shows sccache reporting cache
   hits on the second run; the self-hosted path is unchanged (its host mount still used when
   `SCCACHE_GHA_ENABLED` is unset).
3. **Runner-placement flip.** In `ci.yml`: `native` `runs-on: ubuntu-24.04` unconditionally; `cross`
   `runs-on: ${{ inputs.runner == 'self-hosted' && 'self-hosted' || 'ubuntu-24.04' }}` (default hosted). The
   emulator `checkout` steps are left as written (all repos public - no token). Leave `site`/`image`/
   `site-refresh` self-hosted. *Verify:* `git grep` shows no build job defaulting to self-hosted except
   `site`/`image`/`site-refresh`; the workflows still parse (`actionlint` if available).
4. **Go public (owner).** Make `AutoBleem2`, `pcsx-ab2`, `pcsx-abnxt` public. Make the `autobleem-build`
   GHCR package public. Turn on fork-PR approval and set default workflow permissions to read (above).
   *Verify:* the GHCR image pulls anonymously (`docker pull ghcr.io/autobleem/autobleem-build:latest` from a
   logged-out shell); the two emulator repos are cloneable anonymously (option A).
5. **Bring the pipeline up.** Set repo variable `AB_CI_ENABLED=true`. Push a no-op to `develop`. *Verify:*
   `native` + the five `cross` jobs run *on GitHub's runners in parallel* and go green; `site` runs on the
   self-hosted runner and publishes the pre-release; measure wall-clock and sccache hit rate; if a target is
   consistently cold, tune its cache key. Cut a `v2.0.0-pre1`-style tag once to confirm the `release` draft
   still collects all five artifacts.
6. **Retire the Linode from the build path.** Once Phase 5 is stable, the server compiles nothing - it only
   runs `image`, `site`, `site-refresh`. Rewrite CLAUDE.md's Build/CI section around "builds on GitHub-hosted
   runners, publish on the server," fold `docs/ci-plan.md`'s still-true parts in, delete this file and
   `docs/ci-plan.md`, and update the `no-github-ci` / `repos-stay-private` memory notes to record the
   decision. *Verify:* a full develop push touches the server only for `image`/`site`/`site-refresh`.

## Rollback

Every step is reversible without data loss:

- Set `AB_CI_ENABLED=false` to pause the whole pipeline instantly (no file change).
- Dispatch `runner: self-hosted` to force any run back onto the Linode, or revert the Phase 3 commit to make
  it the default again.
- Repository visibility can be set back to private (the emulator checkouts then need option B's token, and
  the free-minutes benefit is lost - but nothing breaks). Note: making a repo private again does **not**
  un-publish what was already cloned or the GHCR image; treat "go public" as the one genuinely
  forward-only step and the reason Phases 1-3 land first, fully reviewed, before Phase 4.

## Later (not in this round)

- **Native arm64 jobs.** `ubuntu-24.04-arm` is free on public repos; the Pi targets *could* build natively
  there. Not worth it now - the x86 cross image already produces armhf/arm64, and the console target must be
  x86-cross regardless (it needs the Stretch gcc-6 + glibc-2.24 sysroot). Revisit only if a native ARM build
  or ARM-hosted tests earn their keep.
- **Third-party accelerated runners** (Depot / Blacksmith / Namespace). Only relevant if free hosted runners
  prove too slow *and* the repo must go private again; on a public repo, free unlimited hosted runners are
  the cheaper, simpler answer.
- **Supply-chain SHA-pinning** of the actions used, and Dependabot for the workflow actions.
- **wine64 tests for `win`** and `make_rpi_image.sh` in CI - already deferred by `docs/ci-plan.md`,
  unaffected by this change.

## Open questions / owner decisions

1. ~~Emulator repos public or token-fetched?~~ **Resolved 2026-09-22: all repositories go public**, so the
   bare checkouts stand and no token is needed.
2. **When.** The only remaining decision is timing - "at some point." Phases 1-3 (the plan and the inert,
   `AB_CI_ENABLED`-gated code) can land whenever; Phases 4-6 (visibility, the switch, retiring the server)
   wait for the owner's go-ahead. Nothing is implemented until then.
3. Keep `site`/`site-refresh` publishing from the self-hosted runner (recommended - the site *is* the
   server's disk), or eventually move publishing to a hosted runner that rsyncs to the server over ssh with a
   deploy key? The latter would let the Linode drop the Docker runner entirely; out of scope here, noted for
   the "retire the server" endgame.
