# AutoBleem - controls and menu options

What each button does on each screen. AutoBleem boots straight into the launcher (EvolutionUI); the
classic start screen of earlier versions is gone.

## Launcher (the carousel)

| Control | What it does |
|---|---|
| Left / Right | Previous / next game. Holding scrolls faster after a moment. |
| L1 / R1 | Jump to the previous / next first letter of the title. |
| Up | The Quick menu (below). Also on an empty set. |
| Down | Opens the options row for the selected game (see below). |
| Cross | Starts the selected item. A PS1 game runs in pcsx-ab unless "Play using RA" (per game) or "Play all PSX games with RA" (Options) says RetroArch; a light-gun game always runs in RetroArch. A RetroArch entry starts RetroArch with its core. An App shows its readme first. |
| Square | Starts the selected PS1 game in RetroArch (when RetroArch is installed). |
| Triangle | Button guide. In resume-slot mode, deletes the selected resume point after a confirmation. |
| Select | The set picker: the PlayStation, RetroArch and Apps tabs (L1 / R1) and the groups in each - All Games, Internal Games, a game folder, Favorites, History, Lightgun Games; a RetroArch playlist; the Apps. |
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
| Options (the gear) | everything | Options (also in the System menu). The gear opened the Quick menu from 2026-09-26 to 2026-09-29; the Quick menu is Up. |
| Game | PS1 and RetroArch games | The game editor (below). |
| Memory Card | PS1 games | The two-card memory card editor for the game. |
| Resume | PS1 games with resume points | Pick a resume point to continue from. |

## The keyboard

Every screen driven by the pad works from a keyboard on every platform (a PC without a pad, the console with a USB keyboard, the Pi, the PC stick) (2026-09-26):

| Key | Pad button |
|---|---|
| Arrow keys | d-pad |
| Enter | Cross |
| Esc or Backspace | Circle (on a development build Esc closes the program; Backspace is Circle there) |
| Tab | Triangle |
| Space | Square (on a development build Space is Start - its letter map owns it) |
| F1 / F2 | Select / Start |
| Page Up / Page Down | L1 / R1 |
| Home / End | L2 / R2 |
| F10 | L2 + R2 - the System menu |

In a screen where text is typed (the on-screen keyboard) the keys type instead. The Button Guide (Triangle, or
Tab) lists the keys beside the pad buttons when a keyboard is connected or has been typed on.

## Quick menu (Up)

A short panel over the launcher, for what a player reaches for from the carousel (2026-09-26):

| Item | What it does |
|---|---|
| Re-Scan Games | Starts a scan (a note says so when one is running already). |
| Store | Runs the Store extension (`Extensions/store/`); a notification line when it is not installed. |
| Network & Controllers | Only where an installed extension provides the `network` entry (`Provides=network` in its `extension.ini` - PSC-Bios on the console, a Pi and the PC stick): Wi-Fi, Bluetooth pairing, DualShock 3 pairing, the controller mapping wizard. When that extension is installed but disabled, the item is greyed with "enable it in Extensions" - Cross opens the Extensions list at it. |
| System menu... | The System menu (below). |

Up / Down move (wrapping), Cross picks, Circle goes back. Nothing is only here: every item is in the
System menu too.

## System menu (L2 + R2)

Grouped under headings the cursor skips (2026-09-26):

| Group | Items |
|---|---|
| (top) | Re-Scan Games (a "Scan running" note while one runs), Extensions (the Store is one) |
| Library | Game Manager (refuses while a scan is running), Memory Cards, Scanner processors (refuses while a scan is running) |
| System | Options, Network & Controllers (only where provided - see the Quick menu), Hardware Information (the built-in facts page on every platform, Square saves the logs), Software Update (an "Update available" note when there is one), About |
| Leave | RetroArch (or EmulationStation - exits the launcher into it), Power Off (confirmed) |

The rows are one line (32 px, headings 24 px) and the selected item's description is in a strip above the
footer, so the thirteen items and three headings fit on the screen without scrolling (a longer list would
scroll, with markers). `tools/ab_drive.py`'s `menu "<title>"` / `quick "<title>"` pick an item by its
English title in any language (`menu 3` still counts items, headings not included).

## Scanner processors

The programs in `System/Processors/` that every scan runs before it reads the games
(autobleem-main's `docs/archive/scanner-processors-plan.md`), in two sequences - one per tab:

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
| Emulator screen scaling (under Display) | How the PS1 emulator scales the picture for every game (`scaler` in config.ini, `$AB_SCALER` for pcsx-abnxt): 1x1 (plain pixels), 2x (integer), 4:3, 4:3 (integer) or Full screen - pcsx-abnxt's own scaler modes. The classic pcsx-ab and RetroArch know only full screen or not. Was the on/off Widescreen until 2026-09-29 (on = Full screen, off = 4:3). (The picture filter is per game since 2026-09-24 - the game editor's Filter row.) |
| PS1 Emulator | Which emulator plays PS1 games: `pcsx-ab`, the one AutoBleem has always shipped, or `pcsx-abnxt`, the next one (current upstream PCSX-ReARMed with AutoBleem's additions). Both use the same settings, memory cards and resume points (pcsx-ab's save-state layout, which pcsx-abnxt writes and reads too since 2026-09-24): a game left in one continues in the other. A resume point saved on the HLE BIOS (no BIOS file) is the exception - the other emulator starts the game from its beginning. |
| Swap Player 1 / Player 2 (PS1 emulators) | Swaps the first two SDL pads' PS1 ports (`padswap` in config.ini). PS1 only - RetroArch is unaffected. |
| Update RA Config | Whether AutoBleem writes its settings into RetroArch's config when it starts a game there. |
| Play all PSX games with RA | Every PS1 game starts in RetroArch. |
| Fetch box art online | Whether the scan fetches missing covers (and RetroArch's databases, when there are none) from libretro's servers. Only on a platform that can (a Pi, a PC); one probe per scan decides whether there is a network, and a cover the server does not have is not asked for again. |
| Splash timeout | How long the "Showing: <set>" splash stays after a set change: Skip (not shown), 1s ... 20s (`showingtimeout`; 0 kept it up for ever until 2026-09-29). |
| Language | Applied at once. |
| Use Default Font / Font (under Fonts) | On: every screen draws in Open Sans, the launcher's own font, on every theme (a theme's `classic.font` is not read since 2026-09-29). Off: in the font chosen on the next row - any non-empty `.ttf`/`.otf` in `retroarch/fonts` or `resources/fonts` - the classic screens, the menus, the panels' titles and footers, the extensions (PSC-Bios, the Store) and the tools alike. A few parts keep their fixed look whatever this says (`ThemeAssets::fixedFonts()`): About and its hidden game, and the launcher's game details, game menu (its title, description and the resume-slot picker), hints and pad batteries. With the row on, changing the Font row only stores the choice. A font file that cannot be opened falls back to the default. `themefont`/`font` in config.ini. Applied at once. |

The rows are grouped: Interface (Display first, then Emulator screen scaling, Theme, Cover Style, Cover shine, Language, Splash timeout), Fonts, Sound,
Emulation, Library, Updates, Diagnostics. An on/off row shows its value as text (ON/OFF, translated) like any other row.
The rows spread over the panel; more than fit at the font's size page (Up/Down move through them).
Left/Right change a value one step a press; held, the value scrolls on, faster the longer it is held. A row
that reloads (Theme, Music, Language, the fonts) shows the values while held and loads the one it stops on
a moment (0.45 s) after the release - quick presses in a row load only the last value; leaving with Circle
loads a change still waiting. L1/R1 jump several values, one jump a press. The PS1 game editor's rows (Up/Down and
Left/Right) step and scroll the same way.
Cross saves and leaves, Circle leaves without saving, Start picks a random theme, music track or font.

## PS1 game editor

Four headings: **Game**, **Display**, **Rendering**, **Emulator**. Display is pcsx-abnxt's in-game menu's
Picture section - its rows, names and values - except its Scaling and Display (output mode) rows, which are
global Options (Emulator screen scaling, Display mode). Every value below is the game's pcsx.cfg (the game
folder's, and the copy in `!SaveStates`); the per-platform lists are `GameSettingsService::filtersFor` /
`smoothingsFor` / `neonGpuFor`.

| Row | What it does |
|---|---|
| **Game** | |
| Favorite | In or out of the Favorite Games set. |
| Lightgun Game | The game is a light-gun game: it joins the Lightgun set and always runs in RetroArch (its pcsx_rearmed core has the guncon); switching it on switches Play using RA on, and keeps it on. |
| Play using RA | This PS1 game runs in RetroArch. |
| Lock data | The scanner leaves this game's Game.ini alone (its title, serial, region and disc list stay as you set them). |
| **Display** | |
| Resolution | 1x / 2x: the built-in NEON GPU draws 3D at double resolution (`gpu_neon.enhancement_enable`, also Game.ini `Highres`). Only on the console and the Pis, and only while the Plugin is the built-in GPU. RetroArch: `pcsx_rearmed_neon_enhancement_enable`. |
| Remove seams | On/off, `gpu_neon.enhancement_no_seams` (no line = on): no 1-pixel gaps between the parts of a picture at 2x. Shown with Resolution, greyed at 1x as in the emulator's menu. |
| Dithering | Off / On / Always, pcsx-abnxt's `dithering2` (0 none, 1 where the game asks - no line, the default - 2 on everything). RetroArch: `pcsx_rearmed_dithering`, enabled for On and Always. |
| Smoothing | `soft_filter`: None / Scale2x / Eagle2x on the console; None / Scale2x / Eagle2x / HQ2x / HQ3x on the Pis, the PC stick, Windows and a dev host. Greyed with a CRT filter on the console (the emulator turns it off there). |
| Filter | How the picture is scaled to the screen, `plat_target.hwfilter` (no line = Nearest): Nearest, Linear, Sharp, Sharp (simple), Quilez, CRT (fast), CRT-Pi - pcsx-abnxt's list, the same on every platform today. Passed as `-filter`; the classic pcsx-ab knows nearest and bilinear only (Linear is bilinear, the rest nearest), and RetroArch's `video_smooth` is on for Linear only. |
| Scanlines | Off / 1 / 2 / 3, `scanlines` (how thick the dark lines are; an older 1 is the thinnest). The classic pcsx-ab has them on or off. RetroArch: its scanline overlay for anything but Off. Greyed with a CRT filter, which draws its own. |
| Scanline brightness | 0-100, `scanline_level` (hex in the file): how much of the picture shows through the lines. RetroArch: the overlay's opacity. Greyed with a CRT filter. |
| **Rendering** | |
| Plugin | `Gpu3`: the built-in GPU or `gpu_peops.so` (USB games only). |
| Frameskip | `frameskip3`, the emulators' own setting and names: Auto (0 - what the shipped pcsx.cfg gives every game), Off (1, no line), 1 / 2 / 3 frames skipped (2-4). RetroArch gets `pcsx_rearmed_frameskip_type` auto / disabled / fixed_interval with `pcsx_rearmed_frameskip_interval`. |
| **Emulator** | |
| SpeedHack, Clock, Spu Interpolation, Boot logo | The game's pcsx.cfg. Boot logo off (`SlowBoot = 0`) skips the BIOS shell - for a homebrew disc whose custom logo breaks the boot; RetroArch's `pcsx_rearmed_show_bios_bootlogo` follows it. |
| Sony hacks | pcsx-abnxt only: Sony's per-title overrides (`sonyhacks`). |

With the classic pcsx-ab selected (Options -> PS1 Emulator) the rows it does not read - Remove seams,
Dithering, Smoothing - are shown greyed, and can be selected but not changed.

Triangle renames the game, Square changes its memory card, Start shares a new card, Circle leaves.

**A game's own config** (2026-09-24). A PS1 game's PCSX settings have one source at a time. Normally that is
the launcher's pcsx.cfg: the game folder's, or `!SaveStates/<id>/` for an internal game. Once an
emulator's menu has used *Save settings for this game*, it is the game's own `.pcsx/pcsx.custom.cfg` in
its `!SaveStates` folder. Both pcsx-ab and pcsx-abnxt load pcsx.cfg and then the custom file over it, and
they only ever save to the custom file.

While the custom file exists, the editor adds a *Saved in the emulator* heading and an **Unlock the
settings** row, and shows the Display, Rendering and Emulator rows greyed out with the values the emulator will use.
Those rows can be selected but not changed. Cross on Unlock asks for confirmation, then deletes the custom
file (`GameSettingsService::unlock` / `PcsxConfig::unlock`), and the rows are pcsx.cfg's again.

A saved screen shape (`g_scaler3`) and filter (`plat_target.hwfilter`) beat the global Emulator screen scaling option
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
