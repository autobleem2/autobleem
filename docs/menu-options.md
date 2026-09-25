# AutoBleem - controls and menu options

What each button does on each screen. AutoBleem boots straight into the launcher (EvolutionUI); the
classic start screen of earlier versions is gone.

## Launcher (the carousel)

| Control | What it does |
|---|---|
| Left / Right | Previous / next game. Holding scrolls faster after a moment. |
| L1 / R1 | Jump to the previous / next first letter of the title. |
| Down | Opens the options row for the selected game (see below). |
| Cross | Starts the selected item. A PS1 game runs in pcsx-ab unless "Play using RA" (per game) or "Play all PSX games with RA" (Options) says RetroArch; a light-gun game always runs in RetroArch. A RetroArch entry starts RetroArch with its core. An App shows its readme first. |
| Square | Starts the selected PS1 game in RetroArch (when RetroArch is installed). |
| Triangle | Button guide. In resume-slot mode, deletes the selected resume point after a confirmation. |
| Select | Next set: PS1 -> RetroArch -> Lightgun Games -> Apps. The Lightgun set is skipped while no game is flagged. |
| L2 + Select | The PS1 set's sub-set (All Games, Internal Games, a game folder, Favorites, History) or the RetroArch playlist. |
| Start | A random game from the current list. |
| L2 + R2 | The system menu (below). |

### Sets

| Set | What it shows |
|---|---|
| PS1 | All Games (USB and, when enabled, the console's internal games), Internal Games, a folder under `Games/`, Favorite Games, Game History. |
| RetroArch | One RetroArch playlist at a time; Favorites and History after the platforms. |
| Lightgun Games | Every PS1 and RetroArch game flagged as a light-gun game (see the editors). |
| Apps | The launchable apps under `Apps/`. |

### The options row (Down)

| Option | Available for | What it does |
|---|---|---|
| Settings | everything | AutoBleem's Options. |
| Game | PS1 and RetroArch games | The game editor (below). |
| Memory Card | PS1 games | The two-card memory card editor for the game. |
| Resume | PS1 games with resume points | Pick a resume point to continue from. |

## System menu (L2 + R2)

Re-Scan Games, Extensions (second since 2026-09-25 - the Store is one), RetroArch (or EmulationStation -
exits the launcher into it), Memory Cards, Game Manager (refuses while a scan is running), Hardware
Information, Options, Scanner processors (refuses while a scan is running), Software Update (where the
platform has one), About, Power Off (confirmed). The rows are 46 px (a 19 px title over a 13 px
description), so all eleven fit on the screen without scrolling.

## Scanner processors

The programs in `System/Processors/` that every scan runs before it reads the games
(`docs/scanner-processors-plan.md`), in two sequences - one per tab:

| Button | Does |
|---|---|
| L1 / R1 | the PlayStation tab / the ROMs tab |
| Up / Down | the processor above / below; L2 / R2 a page |
| Cross | switches the selected processor off or on (a switched-off one keeps its place, greyed) |
| Square | picks the processor up; Up / Down then move it through the sequence; Square (or Cross) puts it down |
| Triangle | Run again: forgets what it already did, so the next scan offers it every game again |
| Circle | back - `sequence.ini` saved and a scan requested when anything changed |

A processor with no program for this machine is listed greyed ("Not available for this system") and keeps
its place, since the stick may go to another machine. A processor's progress shows in the scan's bubble at the
top right; a warning or a failure on the notification line under it (the details are in `processors.log`).

## Options

| Setting | What it does |
|---|---|
| AutoBleem Theme | The theme, applied at once. |
| Show Internal Games | Whether the console's built-in games appear in the PS1 lists (not on a Raspberry Pi). |
| Cover Style | The jewel-case frame around covers. |
| Music / Background Music | Which track plays, and whether one plays at all. Stepping through themes no longer restarts the track. |
| Widescreen | The PS1 emulator's aspect ratio for every game. (The picture filter is per game since 2026-09-24 - the game editor's Filter row.) |
| PS1 Emulator | Which emulator plays PS1 games: `pcsx-ab`, the one AutoBleem has always shipped, or `pcsx-abnxt`, the next one (current upstream PCSX-ReARMed with AutoBleem's additions). Both use the same settings, memory cards and resume points (pcsx-ab's save-state layout, which pcsx-abnxt writes and reads too since 2026-09-24): a game left in one continues in the other. A resume point saved on the HLE BIOS (no BIOS file) is the exception - the other emulator starts the game from its beginning. |
| Update RA Config | Whether AutoBleem writes its settings into RetroArch's config when it starts a game there. |
| Play all PSX games with RA | Every PS1 game starts in RetroArch. |
| Fetch box art online | Whether the scan fetches missing covers (and RetroArch's databases, when there are none) from libretro's servers. Only on a platform that can (a Pi, a PC); one probe per scan decides whether there is a network, and a cover the server does not have is not asked for again. |
| Showing Timeout | How long the notification lines stay (seconds; 0 = for ever). |
| Language | Applied at once. |
| Use Font from Theme / Font | Off, and the classic screens (this menu, the editors, the Game Manager) draw in the font chosen on the next row instead of the theme's: any `.ttf`/`.otf` in `retroarch/fonts`, `resources/fonts` or the theme's own folder. The launcher's fonts are the theme's regardless. Applied at once. |

The rows spread over the panel; more than fit at the font's size page (Up/Down move through them).
Cross saves and leaves, Circle leaves without saving, Start picks a random theme, music track or font.

## PS1 game editor

| Row | What it does |
|---|---|
| Favorite | In or out of the Favorite Games set. |
| Lightgun Game | The game is a light-gun game: it joins the Lightgun set and always runs in RetroArch (its pcsx_rearmed core has the guncon); switching it on switches Play using RA on, and keeps it on. |
| Play using RA | This PS1 game runs in RetroArch. |
| Lock data | The scanner leaves this game's Game.ini alone (its title, serial, region and disc list stay as you set them). |
| Filter | How the picture is scaled to the screen: Off (plain pixels), Linear (smoothed) or Sharp (crisp pixels without shimmer). The game's pcsx.cfg `plat_target.hwfilter` (0/1/2) - the key pcsx-abnxt saves from its own menu - passed as `-filter`; the classic pcsx-ab has no Sharp and plays it as Off, and RetroArch's `video_smooth` is on for Linear only. |
| High res, SpeedHack, Scanlines, Scanline Level, Clock, Frameskip, Plugin, Spu Interpolation, Boot logo | The game's pcsx.cfg. Boot logo off (`SlowBoot = 0`) skips the BIOS shell - for a homebrew disc whose custom logo breaks the boot; RetroArch's `pcsx_rearmed_show_bios_bootlogo` follows it. |

Triangle renames the game, Square changes its memory card, Start shares a new card, Circle leaves.

**A game's own config** (2026-09-24). A PS1 game's PCSX settings have one source at a time. Normally that is
the launcher's pcsx.cfg: the game folder's, or `!SaveStates/<id>/` for an internal game. Once an
emulator's menu has used *Save settings for this game*, it is the game's own `.pcsx/pcsx.custom.cfg` in
its `!SaveStates` folder. Both pcsx-ab and pcsx-abnxt load pcsx.cfg and then the custom file over it, and
they only ever save to the custom file.

While the custom file exists, the editor adds a *Saved in the emulator* heading and an **Unlock the
settings** row, and shows the Video and Emulator rows greyed out with the values the emulator will use.
Those rows can be selected but not changed. Cross on Unlock asks for confirmation, then deletes the custom
file (`GameSettingsService::unlock` / `PcsxConfig::unlock`), and the rows are pcsx.cfg's again.

A saved screen shape (`g_scaler3`) and filter (`plat_target.hwfilter`) beat the global Widescreen option
and the editor's Filter row. An old `autobleem.cfg` or `cfg/<label>-<id>.cfg`, from the retired "Save
AutoBleem config" entries, becomes the custom file the first time the game is opened or launched. If there
are several, the newest one wins.

## RetroArch game editor

One row: Lightgun Game. Circle leaves.

## Game Manager

The USB games with their folders, the selected one's cover and screenshot on the left. Cross opens the
editor, Square deletes the game (confirmed; a rescan follows), Triangle deletes every cover PNG next to
the games (confirmed), L1/R1 page, Circle closes.

## Where the data comes from

A game's title, publisher, year and player count come from RetroArch's `Sony - PlayStation.rdb`
(`retroarch/database/rdb/`) when it is there, else from the `covers*.db` databases. Its cover is the PNG
next to the game if you put one there, else the box art in `retroarch/thumbnails/Sony - PlayStation/`
(libretro-thumbnails, matched by the game's name in the rdb), else the covers database's picture. Folders
named `Game (Disc 1)`, `Game (Disc 2)` ... are merged into one `Game` folder by the scan.
