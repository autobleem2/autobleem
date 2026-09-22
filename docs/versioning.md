# AutoBleem versioning standard

One version scheme for every component of the suite, so a release is trivial to track and the
compile-once/assemble-many pipeline (`docs/repo-split-analysis.md` §6b) can pull "version X" of each piece.
Decided 2026-09-22 (owner): **unified semver + build metadata**, and the version must be **visible in every
component's UI**.

## 1. The version string

Every repo — launcher, `pcsx-ab`, `pcsx-abnxt`, `retroarch-psc`, the console/PC tools, themes, samples —
uses the **same grammar**, driven by its git tag:

| Kind | Tag / version | Example |
|---|---|---|
| **Stable release** | `vX.Y.Z` | `v2.0.0` |
| **Pre-release** | `vX.Y.Z-alpha.N` / `-beta.N` / `-rc.N` | `v2.0.0-rc.1` |
| **Nightly (develop)** | `vX.Y.Z-dev.<shortsha>` | `v2.1.0-dev.a09927c` |
| **Upstream lineage** (forks) | append semver build metadata `+…` | `v2.0.0+r26.g0f4727f1`, `v2.0.0+ra1.22.2` |

- **Unified**: for a coordinated release, every component is tagged the **same** `vX.Y.Z`. "AutoBleem 2.0.0"
  means every component at `v2.0.0`. The assembly pulls that one version of each piece.
- **Pre-release suffixes are dot-numbered** (`-alpha.1`, not `-alpha1`) so they sort by semver.
- **Ordering** is plain semver: `2.0.0-alpha.1 < 2.0.0-beta.1 < 2.0.0-rc.1 < 2.0.0`; a `-dev.<sha>` build of
  the next version sorts below its release. `repo_index.py` uses **one** semver sort — the per-scheme
  `pcsx_version_key` / `psc_version_key` hacks go away.
- **Upstream base never lost**: a fork's own tag is the AutoBleem version; its upstream base (notaz r26,
  RetroArch 1.22.2) rides in the `+build` metadata and in `manifest.json` (see §3).

## 2. How each repo derives it

A single shared helper (lives in `autobleem-build`, copied into each repo's build, à la the launcher's
`cmake/generate_version.cmake`) turns the tag into the version fields at build time:

- `git describe --tags --always --dirty` → the tag when on one, else `<next>-dev.<sha>`.
- Emits a generated `version.h` (C/C++) or a `VERSION`/`manifest.json` (scripts) with the fields in §3.
- `BUILD_TIMESTAMP` is kept stable while tag/hash/branch/dirty are unchanged (the launcher already does this
  — avoids needless rebuilds).
- Nightlies get `<next>` from a repo constant (the launcher's `AB_VERSION_FALLBACK`, renamed consistently);
  a tagged build takes the tag verbatim.

## 3. The recorded fields (version.h / manifest.json)

Every component records, and its packaged `manifest.json` carries:

- `version` — the semver string (`v2.0.0` / `v2.0.0-rc.1` / `v2.0.0-dev.<sha>`).
- `commit`, `branch`, `dirty`, `build_date`.
- `upstream` — for forks, the upstream base (`r26 / g0f4727f1`, `RetroArch 1.22.2`); empty otherwise.
- `full` — a human string combining the above (what the UI shows), e.g.
  `v2.0.0 (master@a09927c)` or `v2.0.0-dev.a09927c+r26`.

## 4. It must be visible — every component shows its version

A user (and a bug report) must be able to read the version off the screen, on every piece:

| Component | Where the version shows |
|---|---|
| **Launcher** | Splash screen + About screen (already: `Version::FULL_VERSION`). Keep. |
| **Console tools** (pscbios, abflashkit) | Their About/opening screen footer — the same `version.h` the launcher uses while co-built; their own after the split. |
| **PC tools** (updateroms, installer) | The window title bar and/or the first-boot/setup screen header. |
| **Emulators** (pcsx-ab, pcsx-abnxt) | The emulator's own menu — a "Version" line (their menu/about screen), and the first line of their log. |
| **RetroArch (console build)** | Already shows RetroArch's version; our build stamp goes in `retroarch.version` and the log. |

Rule for new UI: the version is drawn in a consistent, unobtrusive spot (a footer or an About/System line),
in `full` form, and is always written as the **first line of the component's log**.

## 5. Assembly & the site

- A release's manifest (`releases/<vX.Y.Z>/release.json`) lists each component artifact **at the same
  version** — the assembly reads it to fetch and compose.
- `latest.json` / `unstable.json` carry the one semver; "latest stable" and "the pre-release" are a semver
  comparison, not per-scheme rules.
- The launcher's online update compares its installed `version` to the channel's — unchanged, just uniform.

## 6. Migration (applies as each repo is touched)

1. Add/point the shared `generate_version` helper; emit the §3 fields.
2. Normalise existing tags: the emulators' `v2.0.0-alpha1` → `v2.0.0-alpha.1`; retroarch's `v1.22.2-1` →
   `v2.0.0+ra1.22.2` on its next AutoBleem-coordinated tag (its own `retroarch.version` keeps `1.22.2`).
3. Add the UI version line wherever §4 says it is missing.
4. Collapse `repo_index.py` to one semver sort once every publisher emits the standard string.
