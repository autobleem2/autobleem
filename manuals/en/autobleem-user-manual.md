# AutoBleem 2 User Manual

AutoBleem 2 is a game launcher for the **PlayStation Classic** - and, since version 2, for a **Raspberry Pi**,
a **PC booted from a USB stick** and **Windows**. It shows your PS1 games as a shelf of covers with their
box art and details, starts them in the bundled PCSX emulator, and, with RetroArch installed next to it,
plays the games of other systems too. This manual covers installing it on each platform, everyday use, and
the tools that come with it.

> The downloads for every platform are at **https://autobleem.retromenele.pl/**. The page is organised by
> platform: the *Install* panel of each one is what you download; the *Build inputs* below it are what the
> installers fetch by themselves.

## 1. What you get

- **The launcher** - the carousel of covers, the sets (PlayStation, RetroArch, Apps), the game details, the
  system menu, the options, the memory-card and save-state tools. The same program on every platform.
- **Two PS1 emulators** - `pcsx-abnxt`, the current one (the default), and `pcsx-ab`, the classic one
  AutoBleem has always shipped. You pick one in the options; both use the same settings and memory cards.
- **RetroArch** (optional on every platform) for the other systems: NES, SNES, Mega Drive, Game Boy, arcade
  and many more. AutoBleem builds its RetroArch lists from the ROMs you copy in and starts each game with
  the right core.
- **The console tools** (PlayStation Classic only): *PSC-Bios* for WiFi, the clock and gamepad mapping, and
  *ABFlashKit* for installing the AutoBleem kernel.
- **UpdateRoms** for Windows: refreshes the RetroArch lists and box art of a console stick on a PC, because
  the console itself has no network.

![The launcher: the shelf of covers, the selected game's details, the button hints](../images/en/launcher.jpg)

<!-- pagebreak -->

## 2. Installation

### 2.1 PlayStation Classic

You need a Windows PC, a USB stick (USB 2.0, 8 GB or more; the installer formats it if you ask) and the
stock console. AutoBleem runs from the stick without any change to the console. The stick must be
**FAT32** for a stock console - its kernel cannot read exFAT. Only a console with the AutoBleem kernel
installed (ABFlashKit, chapter 6) also boots from an exFAT stick, which lifts FAT32's 4 GB file limit.

1. Download **AutoBleemInstaller-<version>.zip** from the PlayStation Classic panel of the site and unpack
   it anywhere. It holds `AutoBleemInstaller.exe` and the AutoBleem package it installs.
2. Plug the stick in and start `AutoBleemInstaller.exe`. Pick the drive at the top. Tick what you want:
   - **Format the stick** - only for a fresh stick (everything on it is erased). Pick FAT32 unless the console
     has the AutoBleem kernel.
   - **Cover databases** - the box art and details of the PS1 library (ticked by default; about 300 MB).
   - **RetroArch** - RetroArch with its cores, the extra applications (Doom, Quake, Amiga, ...) and the
     libretro assets, for the other systems' games. Off by default; it can be added later by running the
     installer again.
   - **BIOS files** - the BIOS files the RetroArch cores need (needs RetroArch).
   - **Sample games** - a few free homebrew games so the shelf is not empty.
3. Press **Install** and wait. The progress bars and the log show every step; the stick is named `SONY`
   at the end, and `UpdateRoms` is put on it (see chapter 5).
4. Take the stick out safely, plug it into the console's **second USB port** (the right one, player 2) and
   switch the console on. AutoBleem starts instead of the stock menu.

To **update** a stick, run a newer installer over it: your games, saves, settings and RetroArch content
stay; only AutoBleem's own files are replaced. A stick made with AutoBleem 1.0 or AutoBleem-NG is brought
to the new layout automatically.

> The stock console has no clock and no network: dates are only shown after the AutoBleem kernel is
> installed (chapter 6), and box art for RetroArch games comes from UpdateRoms on the PC (chapter 5).

Games go into the `Games` folder of the stick, one folder per game - see section 3.9 for the layout.

### 2.2 Raspberry Pi

AutoBleem turns a Pi into a small console: it boots straight into the launcher, with no desktop. Two ready
images are on the site - 32-bit and 64-bit - plus a tarball for an existing Raspberry Pi OS Lite.

| Model | 32-bit image | 64-bit image | Notes |
|---|---|---|---|
| Raspberry Pi 5 | yes | yes | |
| Raspberry Pi 4 Model B, Pi 400 | yes | yes | |
| Raspberry Pi 3 Model B / B+ / A+ | yes | yes | fine for the launcher and PS1 |
| Raspberry Pi Zero 2 W | yes | yes | 512 MB of RAM: PS1 runs, the heavier RetroArch cores do not |
| Raspberry Pi 2 Model B | yes | v1.2 only | slow for anything 3D |
| Raspberry Pi 1, Zero, Zero W | no | no | ARMv6 - neither image runs |

The **32-bit image is the recommended one** for PS1 games: `pcsx-ab`'s fast ARM recompiler is 32-bit only,
so the 64-bit build runs PS1 games slower. The 64-bit image has the larger set of RetroArch cores.

**With Raspberry Pi Imager:**

1. Install Raspberry Pi Imager (raspberrypi.com/software). In *Choose OS* pick *Use custom* and the
   `autobleem-<version>-rpi-armhf.img.xz` (32-bit) or `-arm64.img.xz` (64-bit) you downloaded - or add
   the repository URL `https://autobleem.retromenele.pl/rpi-imager/os_list.json` in the app's settings and
   pick AutoBleem from the list.
2. Use Imager's customisation screen (the gear, or the question after *Next*) to set the **user name and
   password, the WiFi network and country, and enable SSH**. AutoBleem needs a network on its first boot.
3. Write the card, put it in the Pi with a screen and a keyboard or pad connected, and power it on.

**The first boot** takes 5 to 25 minutes and shows what it does on the screen. Without a network it asks
for one (a WiFi list, the password), then asks whether to install RetroArch (a minute without an answer
means yes), grows the system partition, makes the `AUTOBLEEM` data partition from the rest of the card,
installs RetroArch and its cores, the BIOS packs and the sample games, and reboots into the launcher.

The answers can be given in advance in **`autobleem.txt`** on the card's boot partition (editable on any
PC before the first boot):

| Key | Default | Meaning |
|---|---|---|
| `root_gib` | 8 | The system partition's size in GiB; the rest becomes the games partition. |
| `hdmi_mode` | 1920x1080@60 | The screen mode for the whole boot (`1280x720@60` for an older TV). |
| `retroarch` | (asked) | `yes` / `no` - RetroArch and the other systems, or PS1 only. |
| `thumbnails` | none | `boxarts` mirrors the whole box-art set for offline covers (~9000 files). |
| `bios`, `downloads`, `samples` | yes | Set to `no` to skip the BIOS packs, every download, or the sample games. |

**On an existing Raspberry Pi OS Lite** (Bookworm or Trixie): copy `autobleem-rpi.tar.gz` (or the arm64 one)
to the Pi, unpack it and run `sudo bash install.sh`. It asks the same questions, makes the data partition
by shrinking the root at the next boot (`--shrink-root <GiB>`), and puts the launcher on the first console.

After the install the card's **`AUTOBLEEM` partition** (exFAT) is what you fill: pull the card and open it on
any PC, or copy over the network (SSH is on). `Games/` for PS1 games, `RetroArch/roms/<system>/` for the
other systems, `System/Bios/` for the PS1 BIOS (section 3.10), `Themes/` for themes.

### 2.3 PC USB stick

The same appliance for any PC that boots from USB - a 32-bit system, so old machines work too:

1. Download `autobleem-<version>-pcusb-i386.img.xz` from the PC panel and write it to a stick of 8 GB or
   more with Raspberry Pi Imager (*Use custom*), balenaEtcher or Rufus (DD mode).
2. Boot the PC from the stick (the boot menu key of your PC - F12, F8, Esc...). Both BIOS and UEFI boot
   work; **Secure Boot must be off**.
3. The first boot is the Pi's: a network question if there is no cable, the RetroArch question, then the
   install - about eight minutes with a wired network - and a reboot into the launcher.

The stick then has an `AUTOBLEEM` partition for your games, visible on Windows 10 (1903 and newer) as a
second drive when you plug the stick into a running PC. `autobleem.txt` is on the first partition, with the
same keys as on the Pi (no `hdmi_mode` - the PC uses the screen's native mode).

### 2.4 Windows

AutoBleem as a Windows program: full screen, the emulators and RetroArch started as programs.

1. Download **AutoBleemSetup-<version>.exe** and run it. It installs per user, without administrator
   rights: the program under `%LOCALAPPDATA%\Programs\AutoBleem`, the data (games, settings, themes,
   RetroArch) in a folder you choose - `Documents\AutoBleem` by default.
2. Tick the components - the cover databases, RetroArch (the official Windows build and its cores), the
   BIOS files, the sample games - and let the setup helper download them.
3. Start AutoBleem from the Start Menu or the Desktop. On a PC the keyboard works as a pad (section 3.2).

Running a newer setup over it updates the program and keeps the data folder. The launcher also checks the
site once a day and offers an update when there is one (section 3.11).

<!-- pagebreak -->

## 3. Using AutoBleem

### 3.1 The launcher

The launcher opens on the shelf: the covers of the current set, the selected one in the middle, its
details next to it - publisher, year, serial, region, players, when it was last played - and a play
button. The bar at the bottom lists what the buttons do. A scan of the games folder runs in the background
on every start; while it runs, a bubble at the top right shows its progress, and new games appear on the
shelf as they are found.

![The set picker: three tabs, and the groups of the current one with their game counts](../images/en/set-picker.jpg)

### 3.2 Controls

| Button | On the shelf |
|---|---|
| Left / Right | Previous / next game. Holding scrolls on. |
| L1 / R1 | Jump to the previous / next first letter of the titles. |
| Cross | Start the selected game (a PS1 game in the PS1 emulator; a RetroArch game in its core; an App after its read-me). |
| Square | Start the selected PS1 game in RetroArch instead. |
| Triangle | The button guide. |
| Start | A random game from the current set. |
| Select | The set picker: PlayStation / RetroArch / Apps tabs (L1 / R1), the groups of the tab (Up / Down, L2 / R2 a page), Cross picks. |
| Down | Open the icon row under the game (Settings, Game, Memory Card, Resume). Up closes it. |
| L2 + R2 | The system menu (section 3.4). |

On a PC without a pad the keyboard stands in: **X O S T** are Cross, Circle, Square, Triangle; **I J K L**
the directions; **Space** Start, **B** Select; **Q E 1 2** are L1, R1, L2, R2; **Esc** leaves the program.

In every list and menu: Up / Down move, **L2 / R2 turn pages**, L1 / R1 jump to the first / last row,
**Cross selects, Circle goes back**. A screen with settings saves them when you leave it with Circle.

![The icon row under the selected game](../images/en/launcher-icons.jpg)

### 3.3 The sets

**Select** opens the set picker. The PlayStation tab lists *All Games*, *Internal Games* (the console's
built-in twenty, on a PlayStation Classic), every folder you made under `Games/` (a game in a sub-folder
belongs to that group), *Favorite Games*, *Game History* and, when any game is flagged as one, *Lightgun
Games*. The RetroArch tab lists one group per system that has games, plus RetroArch's own Favorites and
History. The Apps tab is the one group of applications. Each row shows how many games it holds; a group
with none opens on an empty shelf with the icon row showing Settings only.

### 3.4 The system menu

**L2 + R2** (together, in either order) opens the system menu over the shelf:

| Item | What it does |
|---|---|
| Re-Scan Games | Looks for new, changed or removed games now (the scan also watches the folder by itself). |
| RetroArch | Leaves the launcher for RetroArch's own menu. Closing RetroArch comes back. |
| Memory Cards | Your memory card sets (section 3.7). |
| Game Manager | The PS1 games as a list with their folders: delete a game, flush the covers. |
| Hardware Information | The machine: system, CPU, storage, network, display, the pads. On a console with the AutoBleem kernel this opens PSC-Bios (chapter 6). |
| Options | AutoBleem's settings (section 3.5). |
| Software Update | (Raspberry Pi and PC) Check the site for a newer AutoBleem or RetroArch now. |
| About | Credits and licence. |
| Power Off | Shuts the machine down, after a confirmation. |

![The system menu](../images/en/system-menu.jpg)

### 3.5 Options

The settings are in groups; Up / Down move between them, Left / Right change a value, Circle leaves and
saves. Every change is applied at once.

| Group / setting | What it does |
|---|---|
| **Interface**: AutoBleem Theme | The look. Themes live in `Themes/`; a theme zip dropped there is unpacked on the next visit. |
| Cover Style | The jewel-case frame drawn around PS1 covers. |
| Language | The launcher's language, applied at once (17 languages). |
| Use Font from Theme / Font | The classic screens' font: the theme's, or any `.ttf`/`.otf` from `resources/fonts`, `RetroArch/fonts` or the theme's folder. |
| Showing Timeout | How long the "Showing: ..." notification stays, in seconds (0 = for ever). |
| **Sound**: Music, Background Music | Which track plays under the launcher (the theme's, or a file from `resources/music`), and whether one plays at all. |
| **Emulation**: PS1 Emulator | `pcsx-abnxt` (the default: current PCSX-ReARMed with AutoBleem's additions) or `pcsx-ab` (the classic). A save state made by one does not load in the other. |
| Widescreen, GFX Filter | The PS1 emulator's display settings for every game. |
| Play all PSX games with RA | Every PS1 game starts in RetroArch's PS1 core. |
| Update RA Config | AutoBleem writes its settings into RetroArch's config when it starts a game there. |
| **Library**: Show Internal Games | The console's built-in games in the PlayStation lists (PlayStation Classic only). |
| Fetch box art online | The scan fetches missing covers from libretro's servers (Raspberry Pi, PC, Windows). |
| **Updates** | (Raspberry Pi, PC, Windows) `stable`, `latest` (the pre-releases too) or `off`. |

![The options, in groups](../images/en/options.jpg)

### 3.6 A game's settings

With a game selected, **Down** opens its icon row: **Settings** (the options above), **Game** (the game's
own settings), **Memory Card** (its memory card) and **Resume** (its save states). Cross opens the one under
the cursor.

The **game editor** shows the game's details on the right and its settings on the left, in three groups:

- **Game**: *Favorite* (in the Favorite Games group), *Lightgun Game* (a light-gun game - it joins the
  Lightgun group and always runs in RetroArch, whose PS1 core has the GunCon), *Play using RA* (this game
  runs in RetroArch), *Lock data* (the scanner leaves the game's title, serial and disc list as you set them).
- **Video**: high resolution, scanlines and their level, frame skip, the GPU plugin.
- **Emulator**: SpeedHack, the CPU clock, SPU interpolation, the boot logo (off skips the BIOS shell - for a
  homebrew disc whose custom logo breaks the boot), and with `pcsx-abnxt` the *Smoothing* filter and the
  *Sony hacks* toggle.

Triangle renames the game, Square changes its memory card, Start shares a new card. Circle saves and leaves.

![The game editor](../images/en/game-editor.jpg)

### 3.7 Memory cards and save states

Every PS1 game has its own memory card by default (kept with its save states in `Games/!SaveStates/<game
folder>/`). **Memory Cards** in the system menu manages **shared sets** - a card several games use, kept in
`Games/!MemCards/`: create one (Square, with the on-screen keyboard), rename (Cross), delete (Triangle).
A game is put on a set with *Change memory card* in its editor, or from its Memory Card icon.

The **memory card editor** (the Memory Card icon) shows the game's card and a second card side by side, with
every save's icon and title: copy a save between the two (Square), delete one (Triangle), defragment a
card (Select). Start swaps the card on the right for another set.

![The memory card editor](../images/en/memory-card-editor.jpg)

**Resume points**: when you leave a PS1 game with the console's Reset button (or the emulator's menu on a
Pi or PC), AutoBleem keeps a save state of where you were and offers it under the **Resume** icon - four
slots, each with a picture of the moment. Cross continues from the slot, Triangle deletes it. A game with
a resume point shows a small picture on its Resume icon.

### 3.8 Starting games, RetroArch and Apps

**Cross** starts the selected game. A PS1 game runs in the chosen PS1 emulator (section 3.5), full screen,
until you leave it - on the console with the front **Reset** button (back to the launcher with a resume
point) or **Power** (the console turns off); on a Pi or PC through the emulator's in-game menu (Select + Start on the pad, or
Esc on a keyboard). **Square** starts a PS1 game in RetroArch instead.

A **RetroArch** game starts in RetroArch with the core the launcher chose for its system; *Close Content*
or *Quit RetroArch* in its menu comes back to the launcher. The RetroArch item in the system menu opens
RetroArch's own menu (XMB) with nothing loaded, for its settings and its own content lists.

An **App** (the Apps set: the console tools, and on a console the extra applications RetroArch's package
brings - Doom, Quake, Amiga, ...) shows its read-me first; Cross starts it, Circle goes back.

![An App's read-me before it starts](../images/en/app-start.jpg)

### 3.9 Adding games

**PS1 games** go into the `Games` folder, **one folder per game**, named after the game:

```
Games/
  Crash Bandicoot/            Crash Bandicoot.cue + Crash Bandicoot.bin
  Final Fantasy VII/          Final Fantasy VII (Disc 1).chd, (Disc 2).chd, (Disc 3).chd
  Platformers/                a folder of games: a group of its own in the set picker
    Klonoa/                   Klonoa.pbp
```

- Formats: `.cue` + `.bin` (or `.img`), `.pbp`, `.chd` (zstd too), `.ecm` (decoded by the scan), `.iso`.
- A multi-disc game is one folder with every disc in it; folders named `Game (Disc 1)`, `Game (Disc 2)`
  ... are merged into one `Game` folder by the scan.
- Games dropped straight into `Games/` (loose files) are sorted into folders by the scan.
- A **cover** is a PNG next to the game's image, named like it. Without one, the box art comes from the
  cover databases, or - with RetroArch installed - from libretro's thumbnail set; on a Pi, a PC or Windows
  a missing one is fetched online (Options → *Fetch box art online*).
- The scan reads each disc's serial number and takes the title, publisher, year, players and region from
  RetroArch's PlayStation database or the cover databases. Change anything in the game editor and tick
  *Lock data* to keep it.

**Other systems** go under `RetroArch/roms/`, **one folder per system, named as RetroArch's databases
are** (the folder is made for you): `Nintendo - Nintendo Entertainment System`, `Nintendo - Super Nintendo
Entertainment System`, `Sega - Mega Drive - Genesis`, `Nintendo - Game Boy Advance`, `FBNeo - Arcade Games`
(or `Arcade`), ... ROMs may stay zipped. On a Pi, a PC or Windows the scan reads them by itself and writes
RetroArch's play lists; on a console stick, run **UpdateRoms** on the PC (chapter 5).

**Apps** go under `Apps/<name>/` with an `app.ini` (the name, the icon, what to run) and a `run.sh`.

**Themes** go under `Themes/<name>/` (`theme.json` and the images) - or drop the theme's zip into `Themes/`.

### 3.10 The PS1 BIOS

On a **PlayStation Classic** the emulator uses the console's own BIOS. On a **Raspberry Pi, a PC and
Windows** put your own PS1 BIOS into `System/Bios/`: `romw.bin` (the US/European SCPH-5501/5502) and
`romJP.bin` (the Japanese SCPH-5500). The installers fill them from the RetroArch BIOS packs unless your
own files are already there. Without them the emulator runs on its built-in HLE BIOS, which many games
tolerate and some do not.

### 3.11 Updates

- **Raspberry Pi, PC stick, Windows**: the launcher checks the site at start and once a day (Options →
  *Updates* is the channel; *Software Update* in the system menu checks now). When there is a newer
  AutoBleem or RetroArch it asks: *Update now* downloads everything and re-runs the installer with the
  first-boot progress screen; *Remind me tomorrow* and *Skip this version* are the other answers. Your
  games and settings stay; the launcher rescans once after an update.
- **PlayStation Classic**: run a newer `AutoBleemInstaller.exe` over the stick (section 2.1).

<!-- pagebreak -->

## 4. Screens

### 4.1 Game Manager

The PS1 games as a list with their folders and the selected one's cover and details. Cross opens the game
editor, **Square deletes the game** (its folder and, after a second question, its save states), Triangle
deletes every cover PNG next to the games (the scan takes them from the databases again), L2 / R2 page.
The free space of the drive is at the top right. The Game Manager waits while a scan runs.

![The Game Manager](../images/en/game-manager.jpg)

### 4.2 Hardware Information

The machine's facts - system, hardware, storage with its free space, network addresses, the display and
audio drivers, the connected pads - re-read every second. On a PlayStation Classic with the AutoBleem
kernel this item opens **PSC-Bios** instead (chapter 6).

![Hardware Information](../images/en/hardware-info.jpg)

### 4.3 The button guide

Triangle on the shelf: every button of every screen on one page.

![The button guide](../images/en/button-guide.jpg)

### 4.4 The on-screen keyboard

Wherever a name is typed - a new memory card set, a game's title, a WiFi password - the same keyboard:
the directions move, Cross types, Triangle deletes, Square is a space, L1 shifts, L2 moves the cursor,
Start confirms, Circle cancels. A USB keyboard types straight in.

![The on-screen keyboard](../images/en/keyboard.jpg)

<!-- pagebreak -->

## 5. UpdateRoms - refreshing a console stick on the PC

The PlayStation Classic has no network, so the RetroArch lists and box art of its stick are made on the PC:
**UpdateRoms** does on the PC what the launcher's scan does on a Pi, with the PC's network and the console's
paths, so the console boots and finds everything in place.

1. Copy your ROMs onto the stick under `RetroArch/roms/<system>/` (section 3.9). The folder names must be
   RetroArch's database names; the installer makes the common ones.
2. Start **`UpdateRoms\UpdateRoms.exe` from the stick** (the installer put it there). It finds the stick from
   where it sits, shows a stage line, a progress bar and a log, and:
   - downloads RetroArch's database bundle when the stick has none, and identifies every ROM by it - a
     game the database knows gets its proper name;
   - writes one play list per system into `RetroArch/bin/playlists/` with the console's paths, keeping
     whatever RetroArch itself added there;
   - fetches the box art of every ROM that has none from libretro's thumbnail servers into
     `RetroArch/bin/thumbnails/`.
3. Take the stick out safely and put it back into the console. The RetroArch tab of the set picker lists
   every system that has games.

Run it again after every change to the ROM folders; a folder nothing changed in is skipped, so a re-run is
quick. The log is `System/Logs/updateroms.log`. A Raspberry Pi card in a card reader can be refreshed the
same way (`UpdateRoms.exe <drive> --target rpi`), though a Pi does it by itself when it has a network.

<!-- pagebreak -->

## 6. The console tools (PlayStation Classic)

Two applications in the Apps set of a PlayStation Classic stick. Both draw in the launcher's theme and
language, and both are driven by the pad - and, in the gamepad wizard, by the console's front buttons.

### 6.1 PSC-Bios

The hardware page: the time and timezone, the WiFi, Ethernet and Bluetooth dongles with their addresses,
and every connected controller with whether it has a button mapping. The network parts need the AutoBleem
kernel (section 6.2); the controller parts work on a stock console too.

![PSC-Bios: the hardware page](../images/en/pscbios-main.jpg)

- **Select - WiFi Settings** (kernel only): the network name (typed, or picked from a scan with Triangle),
  the password, the driver mode, *Write Configuration/Restart Network* to apply, and the timezone. The
  console's IP address is shown once it is connected.
- **Square - Setup Gamepads**: the mapping wizard, and two pages on pairing a DualShock 3 or another
  Bluetooth pad.
- **Triangle - About**, **Circle - back** to the launcher.

**The gamepad wizard** shows the connected pad raw - every axis, button and hat as numbers, and a
DualShock picture that lights up as you press. Because the pad under test cannot be trusted, the wizard
is driven by the console's **front buttons**: **RESET** switches to the next pad, **OPEN** starts the mapping
(then answers each question - press the button lit on the picture, or OPEN when the pad has no such
button), **POWER** cancels or leaves. At the end the new mapping is added for a test and OPEN saves it under
a name of your choice; the launcher loads it from then on.

![The gamepad wizard](../images/en/pscbios-wizard.jpg)

### 6.2 ABFlashKit - the AutoBleem kernel

The AutoBleem kernel is an optional replacement of the console's Linux kernel: it brings a working clock,
USB WiFi and Bluetooth dongles (for PSC-Bios and Bluetooth pads) and the front-button support the emulator
uses for resume points. ABFlashKit installs it, backs the console up first, and can put the console back
to stock through Sony's own recovery.

> **This tool writes to the console's flash memory.** A flash that is interrupted - the power cut, the
> stick pulled - can leave the console unable to start, and installing a custom kernel voids its warranty.
> Keep the console powered and the stick in until it reboots by itself. ABFlashKit opens on this warning;
> *I understand* goes on, *Quit* leaves.

![ABFlashKit's menu](../images/en/abflashkit-menu.jpg)

- **Flash Kernel**: makes a recovery backup of the console's partitions on the stick (`LBOOT.EPB`) if there
  is none yet, checks it and the kernel image, writes the kernel and AutoBleem's system files, and reboots.
  *All done - when the screen goes black replace power cord*: pull the console's power and plug it back in.
- **Full backup**: all four partitions to `LBOOT.EPB`, for a restore later (the previous backup is
  overwritten after a question).
- **Restore Mode**: checks the backup is a stock one, sets the recovery flag and reboots into Sony's
  recovery, which restores the console from `LBOOT.EPB` on the stick - the way back to the stock firmware.

A progress bar under every step shows how far the action is. The tool refuses to flash a console that
runs another custom firmware (BleemSync, Project Eris): restore it to stock first.

<!-- pagebreak -->

## 7. If something goes wrong

- **Logs**: `System/Logs/` on the stick, card or data folder - `autobleem.log` (the launcher),
  `launch.log` and `pcsx.log` (a PS1 game's start and the emulator's output), `retroarch_crash.log`
  (RetroArch left with an error), `update.log` (an online update), `updateroms.log` (UpdateRoms).
- **A game is not on the shelf**: check the folder layout (one folder per game, the image formats of
  section 3.9); `System/Logs/gamesThatFailedVerifyCheck.txt` lists what the scan refused and why. Re-Scan
  in the system menu runs the scan again.
- **No covers**: the cover databases were not installed (run the installer again with them ticked), or,
  for RetroArch games on a console, UpdateRoms has not been run on the PC.
- **A pad does nothing or has its buttons mixed up**: PSC-Bios's gamepad wizard (a console) maps it; on a
  Pi or PC the Hardware Information page lists what SDL sees.
- **The console shows a black screen after a game**: AutoBleem rebuilds its window by itself (up to three
  times); if it stays black, hold the power button and switch the console on again.
- **Raspberry Pi**: `Alt+F2` gives a login prompt on the second console; SSH is enabled from the first boot.
  `sudo journalctl -u autobleem` shows the launcher's service; `sudo systemctl restart autobleem` restarts it.
  A first boot that could not finish (no network) retries on the next boot.
- **Windows**: `Esc` leaves the launcher; the data folder is the one chosen in the setup
  (`Documents\AutoBleem` by default), the logs are in its `System\Logs`.

AutoBleem is free software (GNU GPL v3 or later), with no warranty. Support and news: the Discord server
linked on the About screen, and https://autobleem.retromenele.pl/.
