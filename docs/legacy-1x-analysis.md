# The 1.x launcher's history: where it is, what is newer than our base, what to port

Written 2026-09-21, after the old GitLab on the build server was recovered and retired. This is the
record of *every* place the 1.x launcher's source survives, what each holds beyond the snapshot
AutoBleem 2 started from, and what of it is worth carrying into 2.x.

## Where the code is now

| copy | what | where |
|---|---|---|
| **`screemerpl/autobleem-1x`** (private) | the GitLab's `root/autobleem`: 1572 commits, 2018-12 -> 2022-04, 9 branches, tags `v0.1b`..`v0.9.0` | GitHub; clone at `E:\Programming\autobleem-1x`; bare mirror `psc-build:~/gitlab-mirror/root-autobleem.git` |
| **`screemerpl/autobleem-ng`** (private) | AutoBleem-NG's `main` (`b7bc39a2`, 2026-06-01) + tag `v1.1.0` - the org is gone from GitHub; this was recovered from the clone the port was made from | GitHub; clone at `E:\Programming\autobleem-ng` |
| 14 more private repos | the GitLab's other projects: `pscbios`, `abflashkit`, `autobleem-payload`, `psc-kernel`, `psc-bluez`, `psc-rootfs`, `libmamecd`, `autobleem-build`, `autobleem-themes-pack`, `autobleem-gameports-pack`, `pcsx-ab-2020`, `amiberry-psc`, `psc-toolchain`, `openbor-psc` | GitHub, under `screemerpl` |
| the Mac's working trees | `G:\MacSSD\Profil-screemer\NetBeansProjects\cbleemsync` (HEAD `feature/chd`, 2020-12; **uncommitted work of 2021-03-29**, below) and `Programowanie\autobleem` (2020-03) | the imaged Mac SSD on `G:` |

The GitLab instance itself (12.8.1, Feb 2020, root's compose stack in `/srv/gitlab`) is to be shut
down: `code.retromenele.pl` never pointed at that server, and six years of unpatched CVEs make it
unsafe to expose. Nothing in it is needed any more - `~/gitlab-mirror` on the server is a second copy
of every repository.

## The lineage, in one line

```
2018-12 ... 924a02cb (develop, 2021-03-14) == AutoBleem 2's root commit
                 \
                  feature/lightgun_support (Axanar, 29 commits, 2022-03/04)   <- the GitLab's newest
                           \
                            AutoBleem-NG main: +93 more (Axanar to 2022-08, cornelk 2025-12 -> 2026-06), v1.1.0
```

**Our base is the GitLab's `develop` tip exactly.** The GitLab has nothing on `develop` we lack. Its
only branch ahead of `develop` with features, `feature/lightgun_support`, is the *start* of the NG line
(same commit hashes in NG's `main`), and the NG line was ported on 2026-09-18 (CLAUDE.md, "AutoBleem-NG
port"). Everything below is the branch-by-branch proof.

## Branch by branch (`autobleem-1x`, against `develop`)

| branch | ahead / behind | what it is | verdict |
|---|---|---|---|
| `master` | 0 / 103 | the v0.9.0 release line; `v0.9.0..develop` is the 103 commits 0.9.1 was made of | contained |
| `feature/chd`, `feature/KernelRelease`, `feature/abnet` | 0 / n | merged long ago | contained |
| **`feature/lightgun_support`** | 29 / 0 | Axanar, 2022-03-24 -> 04-10: `LightgunGames` (paths in `lightguns.txt`), the Lightgun set, the RA game editor (`gui_gameEditorMenu_RA`), lightgun icons on the meta panel, "lightgun implies play in RA", skip the set when empty, purge vanished paths, `PsGame::isPS1/isRA/isApp`, `Opscreenh` 585 -> 615 in the default theme, cout silencing, a libs.tar.gz with libmamecd | **ported** - see the checklist below |
| `feature/Eris_merge` | 48 / 212 | Swingflip, 2020-02/03: AutoBleem as a Project Eris `.mod` - `TARGET_PSC_ERIS` paths (`/media/project_eris/opt/retroarch`), a `CONSOLIDATE` layout with everything under `/media/Autobleem/`, "Exit to Boot Menu", Travis CI, a `make_packages.sh` | **do not port** - Project Eris is dead (2020); our layout decision is made (`Themes/`, `RetroArch/`, `Games/` at the root) |
| `feature/refactoring` | 17 / 390 | screemer, 2019-12: `libs/autobleemui` (`guigfx/application`, `gfx`, `gfxtheme`) - a first attempt at a UI library | **superseded** by `lib_ableem` / `ab_classic` |
| `ci` | 3 / 200 | `.gitlab-ci.yml` on `docker.io/screemer/psc-toolchain5`, `DockerToolchain.cmake`, `make_docker.sh` | **superseded** by `docker/` + `ci/` |

Tags: all 17 are on `develop`'s history - no release was ever cut from an unmerged branch.

### The lightgun checklist (branch behaviour -> where it lives in 2.x)

| 1.x | 2.x |
|---|---|
| `LightgunGames` over `lightguns.txt`, `PathForLightgunFile` (folder for PS1, image path for RA) | `LightgunService` (`core/services/lightgun.*`) - PS1 games carry the flag in Game.ini / internal.db, RA games by image path in `System/lightguns.txt` |
| `SET_LIGHTGUN` between RetroArch and Apps, "Showing: Lightgun Games" | `GameSet::Lightgun`, `GameQueryService::lightgunGames()` |
| skip the set when no lightgun game exists | `evoui_launcher_input.cpp:416` |
| `PurgeGamesNotFound` | a reload of the set drops what is gone |
| "Lightgun Game" row in the PS1 editor; on switches play-in-RA on and locks it | `gui_game_editor_menu.cpp` `OPT_LIGHTGUN`, `GameSettingsService::setLightgun` |
| the RA game editor (`gui_gameEditorMenu_RA`) | `gui_game_editor_ra_menu.*` |
| lightgun / lightgun2 icons on the meta panel (by player count) | `evoui_meta.cpp:234` |
| leaving the Lightgun/Favorites set when its last game is edited out | `reloadLightgunSetAfterEdit()`, `reloadFavoritesAfterRemoval()` (NG's `3cc3ff87` fix included) |
| `Opscreenh` 615 in the default theme | `payload/Themes/default/theme.json` `classic.menuPanel.h` = 615 |
| `PsGame::isPS1/isRA/isApp` | not added (`foreign`/`app` are tested directly) - cosmetic |
| the libmamecd `libs.tar.gz` | moot - libchdr is vendored |

**One thing to verify on screen** (not a port, a check): `ab2` and `aergb` keep `menuPanel.h` = 560
while the game editor is 18 rows since the Boot logo row; `default` and `evolution` are taller. If a
row falls below the panel on ab2, raise its `menuPanel.h` the way the lightgun branch raised the
default's.

## The NG line beyond the GitLab (`autobleem-ng`, `924a02cb..main`, 122 commits)

Already assessed in the port of 2026-09-18 (CLAUDE.md lists what came over and what was left out on
purpose: the Docker/CI pipeline, gtest, the RetroBoot-1.2.1 Apps payload). Re-checked on 2026-09-21
against the recovered clone: the Axanar commits after the lightgun merge (2022-05 -> 2022-08: history
and last_played kept across a rescan - `test_scan_service.cpp:98`; locked games keeping serial/region;
`play_using_ra` not reset by a scan; CHD not getting `.cue` in the lpl; the year on the meta panel; the
per-size bold font cache; binary-mode cover writes; the classic UI removed) and cornelk's 2025-12 ->
2026-06 work (libchdr, plog, clang-format/tidy, translations + Chinese, rdb metadata, libretro
thumbnails, the multi-disc merge, adaptive text, the thumbnail cache in Game.ini, no more `.lic`) are
all in 2.x in one form or another. **No open item from NG.**

## Your own post-0.9.1 work (the "fixes + games folder structure" you remember)

Searched on 2026-09-21: every local drive (C/D/E/G), the NAS's six shares, the VirtualBox disks, the
imaged Mac SSD. No `screemer` commit exists after `924a02cb` in any repository (NG's 122 are all
Axanar's and cornelk's), and no checkout newer than 2021-03 was found anywhere. What *was* found:

- **`NetBeansProjects/cbleemsync` on the Mac SSD - the whole working tree diffed against `924a02cb`**
  (every file on disk, tracked or not, line endings and macOS junk ignored; `payload/` included): the
  tree is `924a02cb` minus that commit's own one-line `util.cpp` stream fix (it sits one commit behind,
  at `647b2b7`), plus exactly this, staged on 2021-03-29 and never committed:
  - `libs/abtools/` - `ab_env::init(platform_ini)` / `get_path(name)` / `kernel_found()` (a `TODO`
    returning true) over a vendored `mINI` (`ini.h`, 757 lines of upstream header);
  - `src/resources/platform_mac.ini` - every path as a key: `p_root`, `p_autobleem`, `p_resources`,
    `p_apps`, `p_rc`, `p_games`, `p_memcards`, `p_saves`, `p_system`, `p_ra`, `p_raplaylists`,
    `p_racores`, `p_raroms`, `p_dbregional`, `p_dbinternal`, `p_themes`, `p_covers`, `p_pscore`;
  - `CMakeLists.txt` (the `abtools` library, the `starter` target removed), `main.cpp`
    (`ab_env::init("platform_mac.ini")`), `main.h` (the include).
  Nothing else: no change to the scanner, the launcher, the services or the rc scripts; `payload/` is
  byte-identical to `924a02cb`; `db/coversU.db` is the only ignored file. About 60 lines of our own
  code. **This is the idea 2.x implemented as `PlatformConfig` + `resources/platform/<platform>.ini`**
  (`retroarch_dir`, `retroarch_roms_dir`, `retroarch_core`, ...), so it is done. Its `p_saves=Games/!SaveStates`
  is not a change either: **save states have been central since 1.x** - `GameScanner` (`game_scanner.cpp:366`)
  puts every game's states in `Games/!SaveStates/<game folder name>/` (internal games: `/<id>/`), next to
  `Games/!MemCards`; the key only spelled out the layout that existed. The *reason* behind it (the owner,
  2026-09-21): games should be movable into sub-folders of `Games/` and keep their memory cards and
  saves. What that needs today is in "The plan" below.
- The Mac's other checkouts: `Programowanie/autobleem` (`develop` of 2020-03, one CMake line
  uncommitted); `CLionProjects/pscbios` (`feature/gamecontroller`, 2020-05-08 - the GitLab has it) with
  44 uncommitted lines of 2020-05-11: a Bluetooth pairing experiment in the gamepad menu
  (`bluetool` made discoverable/pairable/scanning on `init()`, an `info <hard-coded MAC>` call, and
  a "Bluetooth dongle not connected" message without the AutoBleem kernel) - an experiment, not
  a feature; `CLionProjects/ABFlashKit` (IDE files only); `payload` (2020-03, symlink-mode noise);
  `libmamecd` and `PSC-pcsx-rearmed` clean. `Desktop/AutoBleem.zip` (6.5 GB, made on the backup day)
  holds a `cbleemsync` of **January 2019**, a `Release` tree, PBPs, a CoverDB and junk - nothing new.
- Eight 2019 branches the GitLab never got (`feature/memory-card-manager`, `feature/mc-manager`,
  `feature/rename-memory-card`, `feature/repair-corrupted-cue`, `feature/fix-savestates` (ogg
  support), `feature/gui-updates-and-memcard-rename`, `feature/test-coverdb`, `hotfix/trello-issues`)
  and ten stashes from 2019 - all pre-0.7 work whose results reached `develop` in other commits.
- `Documents\AB build` (2021-07): a stick image with the Dec-2020 CHD build of the launcher and an
  **`autobleem-rpi`** binary of 2020-12-05 - a 1.x Raspberry Pi build (the `PI_DEBUG` ifdef in
  `environment.cpp`), the ancestor of today's Pi port. No source with it.
- A video on the NAS, `Public\backup\download\"Quick and dirty pcsx autobleem version running on Linux
  vm x86.mp4"` (2021-06-11): AutoBleem + pcsx on an x86 Linux VM. No VM disk from that time survives
  (`kubrick.vdi` is empty, `OSX.vdi` is gone, `Elementary.vdi`'s filesystem was created 2026-02-23).

If the work you remember was on a machine not searched (a laptop, another PC), the signature to look
for is a `cbleemsync` or `autobleem` folder with `src/code/launcher/gui_launcher.cpp` dated after
2021-03-14, or a `libs/abtools` folder.

## The plan

Nothing in the recovered history changes 2.x's code. What is left is small, and mostly not code:

1. **Retire the GitLab** (root on the build server): `docker compose down` in its directory, remove
   `/etc/nginx/sites-enabled/gitlab.conf` and reload nginx, then `/srv/gitlab` and the
   `gitlab/gitlab-ce` image can go. Keep `~/gitlab-mirror` until the GitHub copies have been cloned
   once from somewhere else.
2. **Graft the old history onto this repository** so `git blame`/`git log` reach back to 2018 without
   rewriting anything: `git remote add legacy https://github.com/screemerpl/autobleem-1x.git`,
   `git fetch legacy`, `git replace --graft <our root commit> 924a02cb`. Per clone, not pushed - a
   `replace` ref is local unless pushed (`git push origin 'refs/replace/*'` would share it). Optional.
3. **Check the game editor on `ab2`** (18 rows in a 560-high panel) and raise `menuPanel.h` if a row
   is cut off. Ten minutes.
4. **Done 2026-09-21** (`041ab7e`, and the ROM-scan speed-up the same evening, `0c75eb4`): moving a game
   into a sub-folder keeps everything (the owner's original reason for the
   2021 `platform_mac.ini`). What survives a move today: the save states, resume pictures and the game's
   own "SONY" card (`Games/!SaveStates/<folder name>/`, keyed by the folder's *name*, so its place in the
   tree is irrelevant), the shared cards (`Games/!MemCards/`) and the card choice, favorite, lightgun,
   play-using-RA, highres, the metadata and the cover cache (all `Game.ini` keys, and the ini moves with
   the folder; the sub-dir set is recursive already). What is lost: **`HISTORY` and `LAST_PLAYED`**,
   which live only in `regional.db`, because a rescan keys the known games by *path* (`ScanService::poll`
   deletes every row whose path is not in the worker's `currentPaths` at `ScanStarted`, and the moved
   folder then arrives as a new game with a new id - the launcher also loses its selection). The fix:
   - `ScanStarted` keeps the vanished rows aside (id, path, serial) instead of deleting them;
   - `GameVerified` for a path the database does not know first looks for a vanished row with the same
     folder name **and** the same serial (the verified game carries it) - a match is a *move*:
     `GameDatabase::updateGamePath(id, newPath)` (new; `UPDATE GAME SET PATH=? WHERE GAME_ID=?`, the
     DISC rows are rewritten by `replaceDiscs` as for any updated game), the id, history and last_played
     stay, and the game is reported in `updatedGames`, not `addedGames`, so the carousel keeps it selected;
   - at the end of the scan the vanished rows nobody claimed are deleted as now;
   - `writeSubDirRows`/`writeAutobleemList` take the id-by-path map from the database after the moves
     are applied (they already do - they run after the rows are written).
   Tests in `tests/core/test_scan_service.cpp` (the real-scan integration test is the right harness):
   scan, mark history/last_played, move the folder under `Games/RPG/`, scan again - same id, history and
   last_played kept, one `updatedGames` entry and no `removedGameIds`; and the negative case, a *different*
   game (other serial) appearing under the vanished name is a new game. No script, no service and no
   layout changes; every existing stick keeps working. Estimate: half a day with the tests.
   - A known quirk to leave alone unless it bites: two *different* games whose folders have the same
     name in two sub-folders share one `!SaveStates/<name>/` (`GameCatalogService::deleteUsbGame`
     already refuses to delete a shared one). Keying the folder by serial instead would fix it but move
     every existing stick's states; not worth it for a corner case.
5. **Nothing to port from `Eris_merge`, `refactoring` or `ci`.**
6. A "Move to folder..." action in the Game Manager (pick a sub-folder or make one, `rename()` the
   game's folder, `updateGamePath`) - deferred: the owner wants to rework the Game Manager first.

## How this was established (for the next time)

- GitLab 12.x hashed storage: `@hashed/xx/yy/<sha256 of project id>.git`; the project path is in each
  repo's `config` under `[gitlab] fullpath`. Reading them needs no GitLab - a container with
  `/srv/gitlab` mounted read-only and `git -c safe.directory=*`.
- `git rev-list --left-right --count develop...<branch>` per branch, `git merge-base --is-ancestor`
  per tag, and `git cat-file -e <hash>` in the mirror to tell a local branch the server never saw.
- The Mac trees show every file as modified on NTFS/exFAT (modes, CRLF): `-c core.fileMode=false`
  and `diff --stat --ignore-cr-at-eol`, and `diff --cached` for what was staged.
