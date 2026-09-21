# Licensing plan: going open source without giving the product away

The analysis of 2026-09-21 and the steps that follow from it. Kept here while it is being executed, deleted
when every step is done (as the other plans were - the git log keeps it). **Status** at the bottom.

## Where the licences stand today

| Repo | Visibility | LICENSE file | GitHub detects |
|---|---|---|---|
| `autobleem/AutoBleem2` (this) | private | **CC BY-NC-ND 3.0** | NOASSERTION |
| `screemerpl/cbleemsync` (0.x-0.9.1, public since 2018) | public | **GPL-3.0** - only ever GPL, one LICENSE commit | GPL-3.0 |
| `autobleem/AutoBleem` (public mirror, last push 2020-12) | public | GPL-3.0 | GPL-3.0 |
| `screemerpl/autobleem-1x` (the GitLab line) | private | GPL-3.0 until **2020-02-28** (`8efe338`, `e23aead`), then CC BY-NC-ND | - |
| AutoBleem-NG (org gone; `E:\Programming\autobleem-ng`) | was public | CC BY-NC-ND inherited from 1x, **README says "GPL-3.0"** | - |
| `autobleem/pcsx-ab2` | **private** | GPL-2.0-or-later | GPL-2.0 |
| `autobleem/pcsx-abnxt` | public | GPL-2.0-or-later | GPL-2.0 |
| `autobleem/retroarch-psc` | **private** | GPL-3.0 (RetroArch's) | GPL-3.0 |

Two facts shape everything else:

1. **The 0.9.1 code was published under GPLv3 for two years** (cbleemsync, and the `autobleem/AutoBleem`
   mirror is still up). A GPL grant cannot be revoked: anyone may fork that code today under GPLv3 whatever
   our `LICENSE` says. The 2020 switch to CC BY-NC-ND could only cover screemer's own code - the other
   copyright holders never agreed to it (see "Copyright holders").
2. **The CC BY-NC-ND we carry is a violation, not merely a bad choice.** Creative Commons says its licences
   are not for software; concretely, NC and ND are "further restrictions" the GPL forbids, and the launcher
   binary contains GPL code (below). The site has been distributing that binary under the CC notice since
   2026-09-19.

## Third-party components

### Launcher (`AutoBleem2`: `autobleem-gui`, the tools, the installers)

| Component | Where | Licence | Effect |
|---|---|---|---|
| **unecm.c (Neill Corlett, 2002)** | `lib_ableem/src/engine/unecm.c` -> `ableem_engine` -> every binary | **GPL-2.0-or-later** | the whole launcher binary is a GPL derivative; its licence must be GPL-compatible |
| **rmsgpack decoder** | `lib_ableem/src/engine/rdb_reader.cpp` - "AutoBleem-NG's, kept as it was", author cornelk | GPL-3.0 by the author's stated intent (NG's README) | same |
| SDL_FontCache (Jonathan Dearborn) | `lib_ableem/src/ui/SDL_FontCache.*` | MIT | keep notice |
| SQLite | `lib_ableem/third_party/sqlite` | public domain | - |
| nlohmann json + fifo_map | `lib_ableem/third_party/nlohmann` | MIT | keep notice |
| miniz | `lib_ableem/third_party/miniz` | MIT | keep notice |
| plog | `lib_ableem/third_party/plog` | MIT | keep notice |
| libchdr (Romain Tisserand) | `lib_ableem/third_party/libchdr` | BSD-3 | keep notice |
| zlib / LZMA SDK 24.05 / zstd 1.5.6 | `libchdr/deps` | zlib / public domain / BSD-3 (dual GPLv2) | keep notices |
| LZMA SDK 7z reader | `lib_ableem/third_party/lzma-7z` | public domain | - |
| doctest 2.4.11 | `tests/third_party/doctest` | MIT | tests only |
| SDL2, SDL2_image, SDL2_mixer, SDL2_ttf | linked; `payload/Autobleem/lib/libs.tar.gz` | zlib | - |
| libiconv (in `libs.tar.gz`) | shipped as a shared `.so` | **LGPL** | fine as a replaceable shared library; keep it dynamic |
| libogg / libvorbis (in `libs.tar.gz`) | shipped | BSD-3 | keep notice |
| Selawik Light, Noto Sans SC | `payload/Themes/ab2`, `src/resources/fonts` | SIL OFL 1.1 (`OFL.txt` next to each) | - |
| Kenney "Space Shooter Redux", SketchyLogic track | `src/resources/surprise_game` | CC0 (`license.txt` there) | - |
| Zrnic (Typodermic) | `Themes/ab2`, `aergb`, `default`, `evolution` | Typodermic's free-font EULA | **verify that the font file may be redistributed** |
| `default/4.wav`, `aergb/mel.ogg`, `ab2/ab.ogg` | themes | unknown | **provenance needed** |
| "Absolute Terror" (Nihilore) | `Themes/evolution` (`credit.txt`) | royalty-free per the author's site | confirm the terms allow redistribution in a package |
| UPX, NSIS, the toolchains | build only | GPL w/ exception, zlib | no effect on the output |

### Emulators and RetroArch

- **pcsx-ab2 / pcsx-abnxt**: GPLv2-or-later throughout - the PCSX core, Ari64's dynarec, every plugin
  (gpu_neon, gpu_unai, gpulib, gpu-gles, dfsound, dfxvideo, cdrcimg), Sony's PSC patches (released by
  Sony under the GPL because they had to). libpicofe is GPL/LGPL/MAME tri-licensed, lightrec LGPL-2.1,
  GNU lightning LGPL, libretro-common MIT, libchdr BSD-3. **Nothing there is ours to relicense**; every
  patch of ours is GPLv2-or-later.
- **retroarch-psc**: GPLv3 (RetroArch). Same.

## Copyright holders in our own code

| Holder | Where | Under what |
|---|---|---|
| screemer | everything | can relicense own work |
| **AKA-Axanar** (629 commits on 1x vs screemer's 330) | most of the 0.9.x UI/launcher logic the refactor kept | contributed 2019-2020 under **GPLv3**; never agreed to CC BY-NC-ND unless something exists in writing |
| cornelk (78) + Axanar (44), AutoBleem-NG 2026 | the NG port: rdb reader, metadata/thumbnail lookup, multi-disc merge, lightgun, fitted text, ... | GPL-3.0 (NG's README) |
| mGGk-fr, Quenty31, clach04 | translations, trivia | GPLv3 |
| the 2026 refactor | the rest | ours |

A non-GPL licence would need Axanar's and cornelk's written consent *or* a rewrite of their
contributions, plus a clean-room replacement of unecm. Axanar's share alone makes that impractical.

## Assets that block a public repo regardless of licence

**Sony Interactive Entertainment's PSC firmware files**, tracked in git and shipped in every package - a
takedown risk on a public repository:

- `SST-Medium.ttf` / `SST-Bold.ttf` - six copies: `src/resources/sony/font`, `payload/Themes/default/font`,
  `payload/Themes/evolution/font`.
- the five UI sounds `cursor/cancel/home_up/home_down/resume_new.wav` in `default` and `evolution`.
- the launcher images lifted from `/usr/sony/share/data` (`GR/Footer.png`, `CB/PlayerOne.png`,
  `GR/Acid_C_Btn.png`, `CB/Function_BG.png`, `GR/*_Btn_ICN.png`, `MC/Dot_Matrix.png`, `GR/arrow.png` - see
  `docs/theme-format.md`), now `launcher_footer.png`, `meta_panel.png`, `play_button.png`,
  `settings_panel.png`, `hint_*.png`, `memcard_grid.png`, `arrow.png` in `default`; byte-identical in
  `aergb`; **partly in `ab2`** (footer, meta panel, play button, settings panel, the three hints, arrow,
  memcard grid); `meta_panel.png`/`play_button.png` in `evolution`; `arrow`, `memcard_pencil`,
  `menu_resume`, `meta_panel` in `autobleem`.

Clean: `internal.db` (9 KB of facts, no blobs), the BIOS manifests (hashes and URLs only, no BIOS file),
the cover databases and `vendor/retroboot-bundle` (both untracked - the site distributing the cover
databases is a separate, pre-existing risk unrelated to the code licence).

## The decision

**GPL-3.0-or-later for AutoBleem2.** GPL-2.0-or-later stays on the two pcsx repos, GPL-3.0 on
retroarch-psc.

- It is the only licence that is at once compatible with unecm and the NG code, what the code was
  already released under, what every co-author agreed to, and compatible with the emulator repos.
- No open-source licence stops forks - that is the definition. What GPLv3 stops is what "stealing" means
  in practice: **taking the code into a closed product or a closed fork**. Every redistributed
  modification must ship its source under the same terms, with our copyright intact.
- Source-available licences (PolyForm Noncommercial, BUSL 1.1, CC BY-NC-SA) would also block commercial
  forks, but they are not open source, they are incompatible with the GPL code inside the binary, and the
  0.9.1 GPL grant means a competitor can fork that anyway.
- The protection copyright cannot give - the **name** - comes from trademark. GPLv3 §7(e) explicitly lets
  us decline trademark rights: "AutoBleem", the logo and the theme artwork are not licensed; a fork must
  rename. That is what would have stopped the RetroBoot/NG-style confusion.
- AGPL buys nothing (no network service). LGPL/MIT for `lib_ableem` alone would need unecm moved out of
  it - only worth it if we want third parties to embed the engine.

## Steps

1. **Close the compliance gaps that exist today**, independent of the open-sourcing date: the site and
   the packages distribute `pcsx-ab`, `pcsx-abnxt` and our RetroArch build - GPL binaries whose sources
   sit in **private** repos, i.e. no source offer. Make `pcsx-ab2` and `retroarch-psc` public (their READMEs
   already say GPL), or publish a source tarball next to each binary under `emu/` and `psc/retroarch/`
   on the site and link it from `repo_index.py`.
2. **Replace `LICENSE`** with the GPL-3.0-or-later text; copyright line "Copyright (C) 2018-2026 screemer
   and the AutoBleem contributors"; a "License" section in `README.md`; the About screen shows the licence
   line (GPLv3 §5(d) "Appropriate Legal Notices" for an interactive program). SPDX headers
   (`// SPDX-License-Identifier: GPL-3.0-or-later`) on our own sources are optional; clang-format leaves
   headers alone. Remove any remaining mention of CC in docs.
3. **`THIRD_PARTY_NOTICES.md`** at the root: the tables above, with the verbatim licence texts the MIT/BSD
   ones require, and the `libs.tar.gz` contents. `make_*_package.sh` copies it into every package next to
   the binary; the site's release panel links it.
4. **`TRADEMARKS.md`**: name, logo, the ab2 artwork; forks rename. Referenced from `LICENSE`'s additional
   terms note and the README.
5. **Strip Sony's assets** before the repo goes public:
   - Selawik Light replaces SST everywhere (`ab2` already does): `ThemeAssets`/`Fonts` fall back to it,
     `src/resources/sony/font` goes, `EnvironmentSetup`'s Sony data path is used only on the console.
   - CC0 replacements for the five sounds.
   - Redraw the footer, meta panel, play button, settings panel, hints, arrow and memcard grid
     (`tools/make_ab2_icons.py` already draws the icons this way - extend it).
   - `default` becomes a clean theme (or an alias of `ab2`); `evolution`, `aergb`, `autobleem` move to a
     theme pack on the site (`samples/`-style, `repo_publish.sh themes`) with their own credits.
   - A test that fails when a file with a known Sony hash is under `payload/` or `src/resources/`.
6. **Settle the remaining assets**: Zrnic (replace with an OFL face if Typodermic's EULA forbids
   redistribution), the three music files (replace or document), Nihilore's terms.
7. **Confirm the contributors** (optional, removes the last ambiguity): one-line emails to Axanar and
   cornelk - "your contributions are GPLv3".
8. **Go public**: `autobleem/AutoBleem2` visibility, the site's release panels linking the tag's source,
   `CLAUDE.md`/README updated, this file deleted.

## Status

- 2026-09-21: analysis done, plan written. Nothing executed yet.
