# RetroArch manager dialogs - UI text spec (AUTOBLEEM-39 part 2 / PLATFORM-24)

For the launcher developer who implements `src/code/evoui/screens/evoui_ra_manager.cpp` and cannot see the PNGs.
Sources: `ui/*.png` (every one was looked at), `make_ra_manager_ui.py` (read, NOT run), `kasia-notes.md`,
`psc-session-2026-10-15.md`. Mockups are 1280x720 PNGs; all numbers below are pixels of that 1280x720 frame.

Facts I could not get from the files (the generator was not run, the theme and the launcher code are not in the snapshot):
- The theme hex values: the generator reads them from `theme.json` (`launcher.colors.*`) of the ab2.0.0 theme. Use the
  theme TOKENS below, never hard-coded hex.
- Which existing launcher widgets match the generator helpers (the launcher source is not in this snapshot): the
  generator says its frames are "9-sliced the way the launcher draws them", so `panel`, `selection`, `chip`,
  `progressTrack`/`progressFill` should be the launcher's own frames.

## 0. Requirements from Kasia's notes and the six decisions

### 0.1 Kasia's notes (verbatim from `kasia-notes.md`)

> Source: Kasia's design check of ra-1a..4c + crt-2a against `psc-session-2026-10-15.md` (22:50Z, 15.10.2026): all screens match owner
> decisions 1-6; no screen counts files or walks the stick when the panel opens. Verbatim notes for the developer:
>
> - a. 2c Remove "N of 7 400 files": the total must not come from a walk at Remove start (that is the same cost as defect 1). Take it
>   from a file list written at install time (per package), or count while deleting and show "N files" plus the package bar by packages
>   done. Either way: never on the UI thread.
> - b. 2a download-assets in MB (defect 3): the 6 libretro zips need their sizes up front: a HEAD/Content-Length per zip before the
>   step, or sizes in the catalog. 2b/2d totals: zip = central-directory entry count (cheap), tar.gz = compressed bytes read (MB).
> - c. Numbers in the mockups are placeholders: 1a Update shows "about 640 MB", 1c "about 400 MB". The code shows real sizes from the
>   catalog/manifest; the 1c text keeps "about 400 MB / 10-15 min" (the measured P4 values).
>
> Her decision mapping, also for the brief:
> - decision 2: no RA size: 1a shows version + free space via statvfs + bar; Remove has no size.
> - decision 3: two bars on every 2* screen: the whole job "Step i of 9 - phase" + the current package in MB or files with its name.
> - decision 4: Stop only on Install screens 2a/2b/2d; 2c Remove has no Stop.
> - decision 5: 1c pre-install notice 10-15 min, ~400 MB, console on, coffee; one layout for every phase; one "why" line per phase;
>   time left + speed on downloads; 3a/3b deleted/stays; 4a-c one OK each.
> - decision 6: cheats always, no skip; 2b counts cheats in files.

### 0.2 The six owner decisions (verbatim from `psc-session-2026-10-15.md`)

> 1. Done = PSC + Pi PASS; the PC stick is fixed at the end.
> 2. No RetroArch size in the panel: the version + the stick's free space.
> 3. Two bars: the whole job (step i of n) and the current package (MB or files, with its name).
> 4. Stop only during Install (keeps the downloaded parts); Remove always runs to the end; a half-done Remove is finished by the next Install/Remove.
> 5. UI reworked from Kasia's mockups (`ui/`, generator autobleem-design feature/platform24-ra-manager-ui f14f2f8): pre-install notice (10-15 min, keep the console on, coffee), one layout for every phase with movement always visible, sizes in MB, one line per phase on why it exists, time left + speed for downloads, Remove confirm with "will be deleted / stays on the stick", result boxes with one OK.
> 6. Cheats stay always (only the progress counts files).

The defects behind them (same file, verbatim): 1. panel computes RetroArch's size on the UI thread (`RaJobService::sizeOnDisk()`,
~90 s frozen screen); 2. `unpack` of a .zip reports `1|1`, bar stands still for minutes; 3. `download-assets` reports `0|0`,
pulsing bar without numbers; 4. the progress panel prints raw byte counts, not MB.

## 1. Shared look (all screens)

Every screen is the launcher background dimmed (Gaussian blur 1.2 + overlay RGBA (8,10,14,150)) with C3 logo and hint bar
behind it, and ONE panel on top. The backdrop is the launcher's own; only the panel is new.

### 1.1 Panel
- Frame `panel` (9-slice). x = 240, width PW = 800 (today's EvoUI column), always.
- Height h per screen (table below). y0 = (720 - 92 - h) // 2 + 20 (centred over the shelf, clear of the hint bar).
- Text inset PAD = 32: body text x = 272 (PX+PAD), right edge x = 1008 (PX+PW-PAD); content width 736.
- Title: bold 28, TEXT, at x = 264 (PX+PAD-8), y = y0+22 (top-left anchor). Below it a rule at y0+66.
- Rule: 1 px line from x = 264 to x = 1016 (PX+PAD-8 .. PX+PW-PAD+8), colour HEAD at alpha 110.
- Footer: a rule at y0+h-64, the hint row centred vertically on y0+h-44.
- Hint row: glyph (default theme `hint_cross` / `hint_circle`, about 28 px round) at x = 272, then 8 px, then the label
  (medium 22, HINT), then 34 px gap to the next pair. Cross = confirm, circle = back/cancel/stop.

| screen | h | y0 | screen | h | y0 |
|---|---|---|---|---|---|
| 1a, 1b | 516 | 76 | 3a | 414 | 127 |
| 1c, 1c-pl | 358 | 155 | 3b | 482 | 93 |
| 2a, 2a-pl, 2b, 2c, 2d | 372 | 148 | 4a, 4b, 4c | 240 | 214 |

(y0 computed from the generator formula; matches the PNGs within 1 px.) Heights are fixed formulas of the row counts, so a
screen with another row count (e.g. Remove with BIOS) changes h and y0 by the same formula.

### 1.2 Theme tokens (`launcher.colors` of the ab2.0.0 theme.json)
| name in generator | token | used for |
|---|---|---|
| TEXT | `text` | titles, main lines, enabled labels |
| SEC | `secondary` | sub-lines, "why" line, percent, "Package i of n", right column in 3a/3b, disabled labels |
| HINT | `hint` | hint labels, time-left/speed line, 3b note, 2c wait line |
| HEAD | `heading` (cyan in the PNGs) | rules, 3a/3b column headers and ticks, 1c bullets and coffee line |
| SELC | `selection` (magenta in the PNGs) | focused row label (1a) |
| dis | `colors.disabled {color, alpha}` | veil over a disabled row |
Frames (`launcher.frames`): `panel`, `selection`, `chip`, `progressTrack`, `progressFill`, `hintBar`(backdrop only). Icons:
`icons/switch_on.png`, `switch_off.png`. Font: Red Hat Text, `fonts.medium` and `fonts.bold`. Red Hat Text has no U+2713
and no narrow no-break space: the tick in 3a/3b is DRAWN, thousands separators are a plain space.

### 1.3 Progress bar (helper `bar`)
`progressTrack` frame over the full width, `progressFill` over `int(w*frac)` px, fill drawn only when that width > 6 px.
Heights: panel-1 free-space bar 10, top (whole job) bar 10, bottom (package) bar 18. Width always 736 at x = 272.

### 1.4 Chip (helper `chip`)
`chip` frame, height 24, width = text width + 16, label bold 13 TEXT at x+8, vertically centred.

### 1.5 Scaling
The generator has no scaling: it draws at 1280x720 with fixed pixels. The spec therefore assumes the launcher's own
logical 1280x720 UI space; keep panel x/width/insets in that space and let the launcher scale it. Nothing in the files says
more; the one scaled check is crt-2a (section 4). Text never wraps in any mockup (single lines only).

## 2. Screens

### 2.1 ra-1a  Panel, installed (h 516)
- Title "RetroArch".
- y0+84: "Installed: v1.22.2" bold 22 TEXT at x 272; right after it (+14 px, y+1) chip "UPDATE: v1.23.0" (only when an update exists).
- y0+124: left "Free on the stick" medium 21 SEC; right-aligned at x 1008 "18.4 GB free of 58.2 GB" medium 21 TEXT (`gib()` = one
  decimal + " GB"; from statvfs). Bar at y0+158, h 10, filled fraction = USED/total = 1 - free/total (68% in the mockup).
- Rule at y0+180. Four rows, pitch 64, first row top at y0+188. Row: label bold 23 at (272, ry+8), sub-line medium 16 at (272, ry+38).
  - "Install" / "Already installed" - disabled.
  - "Update" / "Get v1.23.0 - about 640 MB to download" - enabled, FOCUSED by default: `selection` frame at (242, ry, 796x60), label in SELC, sub in SEC.
  - "Remove" / "Delete RetroArch - games, saves and settings stay" - enabled, label TEXT, sub SEC.
  - "Also delete the BIOS files" / "Only with Remove" - toggle row, indented 28 px (x 300), switch image right-aligned to x 1008 and centred in the row; mockup shows it OFF (`switch_off`), enabled.
  - Disabled row: label SEC, sub SEC at alpha 190, plus a veil rect (244, ry+1, 792x58) in `colors.disabled`.
- Footer hints: [cross] "Select", [circle] "Back". Default action: the focused row (Update).
- Placeholders: v1.22.2, v1.23.0, 640 MB, 18.4/58.2 GB. Code: installed version file, catalog version + download size, statvfs.
- No RetroArch size anywhere (decision 2). Nothing walks the stick when the panel opens (defect 1).

### 2.2 ra-1b  Panel, not installed, no network (h 516)
Same layout. Status line "Not installed" (bold 22, no chip). Free bar example 21.0 of 58.2 GB. All four rows disabled, NO row focused:
- "Install" / "No network - connect in Network & Controllers"
- "Update" / "Not installed"
- "Remove" / "Not installed"
- "Also delete the BIOS files" / "Only with Remove" (toggle off, veiled)
Hints as 1a. (The "not installed, network OK" state - Install enabled - is not mocked: see section 6.)

### 2.3 ra-1c  Before Install (h 358); ra-1c-pl is the Polish copy
Shown after choosing Install, before it starts.
- Title (bold 28): EN "Before you start" / PL "Zanim zaczniemy".
- Intro, 2 lines, medium 21 TEXT, x 272, first line y0+88, pitch 30:
  - EN "Installing RetroArch takes about 10-15 minutes on the console:" / "about 400 MB to download and tens of thousands of small files to write."
  - PL "Instalacja RetroArch trwa na konsoli około 10-15 minut:" / "około 400 MB do pobrania i dziesiątki tysięcy małych plików do zapisania."
- Then +78 px: 2 bullets, pitch 30. Bullet = 7x7 square in HEAD (cyan, deliberately not the focus magenta) at x 272, y+11; text medium 20 SEC at x 290.
  - EN "Keep the console on and the stick in." / "Stop keeps everything downloaded so far - Install carries on later."
  - PL "Nie wyłączaj konsoli i nie wyjmuj pendrive'a." / "Zatrzymanie zachowuje to, co już pobrane - instalacja dokończy później."
- Then +86 px: coffee line bold 21 HEAD: EN "A good moment to go and make a coffee." / PL "Najlepiej iść w tym czasie na kawę."
- Hints: EN [cross] "Start" [circle] "Cancel"; PL [cross] "Start" [circle] "Anuluj". Default: Start (cross).
- Placeholder: "about 400 MB" and "10-15 minutes" are the measured P4 values and STAY literal (Kasia c); everything else is static text.
- Widest PL line ends at x 981 of the 1008 limit (709 of 736 px): no slack for longer text.

### 2.4 Progress screens, common layout (h 372)
One layout for every phase (decision 5). From y0+84 (y):
1. Whole-job line: medium 18 TEXT, x 272: `Step {i} of {n}  -  {phase}` (two spaces each side of the hyphen). Right-aligned at x 1008, same y, medium 18 SEC: `{int(whole*100)}%` (truncated). Bar at y+28, h 10. `whole = (i-1 + stepFraction)/n`.
2. y0+146: package line: bold 22 TEXT at x 272: `{package name} - {done} of {total} {unit}` (hyphen with single spaces; unit "MB" or "files"; numbers grouped with a plain space: "12 400"). Right-aligned at x 1008, y+3, medium 18 SEC: `Package {pi} of {pn}`. Bar at y+38, h 18, fraction done/total.
3. "Why" line: medium 17 SEC at y+70, one sentence per phase.
4. Downloads only: foot line medium 17 HINT at y+98: `About {t} left  ·  {v} MB/s` (middle dot U+00B7, two spaces each side). Absent on unpack/remove.
5. Footer at y0+328: [circle] "Stop" on Install screens (2a, 2b, 2d); on 2c no Stop (see 2.8).
Numbers are always MB (never raw bytes, defect 4) or files, and both bars always move (never a pulsing bar without numbers, defects 2/3).
Titles: "Installing RetroArch" (PL "Instalowanie RetroArch"), Remove: "Removing RetroArch".
The 9 install phases (STEPS, in order): Checking space, Downloading RetroArch, Downloading cores, Downloading libraries,
Downloading apps, Downloading assets, Unpacking, Merging settings, Installing. Remove: n = 1, phase "Removing files".

### 2.5 ra-2a  Downloading cores (also ra-2a-pl, ra-crt-2a)
EN: title "Installing RetroArch"; "Step 3 of 9  -  Downloading cores" 27%; (whole = 2.44/9); package "Downloading emulator cores - 128 of 289 MB",
"Package 3 of 9"; bottom bar 128/289 = 44%; why "Cores are the emulators - one for each console RetroArch can play."; foot "About 1 min left  ·  2.4 MB/s"; Stop shown.
PL: title "Instalowanie RetroArch"; "Krok 3 z 9  -  Pobieranie rdzeni"; "Pobieranie rdzeni emulatorów - 128 z 289 MB"; "Paczka 3 z 9";
why "Rdzenie to emulatory - po jednym dla każdej konsoli, którą obsługuje RetroArch."; foot "Zostało około 1 min  ·  2,4 MB/s" (decimal COMMA);
hint "Zatrzymaj". PL units: "MB", "plików"; "of" = "z".
Placeholders: 128, 289, 27%, 1 min, 2.4 MB/s, 3 of 9. Code: MB downloaded/total from the sizes known up front (Kasia b), speed, ETA.

### 2.6 ra-2b  Unpacking cheats
Title "Installing RetroArch"; "Step 7 of 9  -  Unpacking" 72% (0.55 of step 7); package "Unpacking cheats - 12 400 of 41 000 files", "Package 5 of 9";
bottom bar 12400/41000 = 30%; why "Cheat codes for thousands of games - many small files, so this part is slow."; no foot line; Stop shown.
Counted in FILES (decision 6: cheats always, no skip). Total = zip central-directory entry count (Kasia b). Placeholders: 12 400, 41 000, 72%, 5 of 9.

### 2.7 ra-2d  Unpacking cores tar.gz
Title "Installing RetroArch"; "Step 7 of 9  -  Unpacking" 68% (0.20 of step 7); package "Unpacking emulator cores - 120 of 290 MB", "Package 3 of 9";
bottom bar 120/290 = 41%; why "The cores are unpacked onto the stick, ready to run."; no foot line; Stop shown.
MB = compressed bytes of the tar.gz read so far / archive size (Kasia b). Placeholders: 120, 290, 68%, 3 of 9.

### 2.8 ra-2c  Removing (NO Stop)
Title "Removing RetroArch"; "Step 1 of 1  -  Removing files" 42%; package "Removing emulator cores - 3 120 of 7 400 files", "Package 2 of 7";
bottom bar 3120/7400 = 42%; why "Your games, saves and settings are not touched."; no foot line.
Footer: NO hint glyph, NO Stop. Instead the plain text "Please wait - this takes about half a minute." medium 20 HINT at x 272, centred on the footer line.
Back/circle does nothing during Remove (decision 4). Totals: never from a walk at Remove start; from the per-package file list written at install time, or
count while deleting and show "N files" with the package bar by packages done (Kasia a). Never on the UI thread.
Placeholders: 3 120, 7 400, 42%, 2 of 7 (7 packages = the 7 "will be deleted" items of 3a; 8 with BIOS).

### 2.9 ra-3a / ra-3b  Remove confirm (3b = "Also delete the BIOS files" on)
Title "Remove RetroArch?". Two columns from y0+88: left header at x 272, right header at x 650 (PX+PW/2+10), bold 19 HEAD, upper case:
"WILL BE DELETED" | "STAYS ON THE STICK". Items from +36, pitch 30, medium 20:
- Left (TEXT), each prefixed with an en dash and two spaces "–  ": RetroArch itself, Emulator cores, Menu assets, Databases, Cheats, Overlays, Shaders; in 3b also "BIOS files" (last).
- Right (SEC), each with a drawn tick (polyline (x,ty) (x+4,ty+4) (x+12,ty-5), width 2, HEAD, ty = row top+13) and text at x+22 (x = 650): Your games, Playlists, Covers,
  Saves (memory cards), Save states, Settings, BIOS files (3a only; in 3b it moves to the left, 6 items remain).
- Vertical divider at x 628, HEAD alpha 80, 1 px, from (first row top - 30) to (rows*30 - 6 below it).
- 3b only, below the columns (+6): "Cores that need these BIOS files won't start until you copy them back." medium 17 HINT at x 272.
- Hints: [cross] "Remove" [circle] "Cancel". Default: Remove (cross). Rows = max(left,right) (7 in 3a, 8 in 3b).
Static text; no sizes.

### 2.10 ra-4a / 4b / 4c  Result boxes (h 240, one OK each)
Title (bold 28) at y0+22, rule at y0+66, two lines from y0+88, pitch 32, medium 21: line 1 TEXT, line 2 SEC. Hint: [cross] "OK" (the only button, default).
- 4a: "RetroArch is installed" / "Version 1.23.0 is ready - start it from the System menu." / "Free on the stick: 15.9 GB". (placeholders: 1.23.0, 15.9 GB)
- 4b: "Installation failed" + chip "ERROR" (x = 264 + title width + 16, y0+28) / "Not enough free space: it needs 2.6 GB, the stick has 1.1 GB." / "Nothing was changed - free some space and try again." (placeholders 2.6 / 1.1 GB; the message is the example for the no-space cause only)
- 4c: "Installation stopped" / "The parts downloaded so far are kept." / "Choose Install again to carry on. RetroArch was not changed."

### 2.11 Where Stop shows (decision 4 / Kasia)
Shown: 2a, 2b, 2d (every Install progress screen), hint "Stop" [circle]. Not shown: 2c Remove (and 3a/3b/4*/1*). Stop keeps downloaded parts (4c text).

## 3. CRT variant (ra-crt-2a)
Same 2a frame, rendered 1280x720 then resized to 960x720 (x scale 0.75, y 1.0, Lanczos): the CRT shows the 16:9 frame squeezed to 4:3. It is a legibility check only:
no separate layout, no different strings, same colours. All text stayed readable (smallest 17 px high text becomes ~12.75 px wide-equivalent). The developer needs NO CRT-specific code,
but must not make the layout wider than the 736 px content width (1c-pl is already at 709).

## 4. Generator helpers to reuse (all in `make_ra_manager_ui.py`; they mirror launcher widgets)
`hexc(h, a)` colour from theme hex; `font(bold, size)` Red Hat Text medium/bold; `nine(key, w, h)` and `put(scr, key, x, y, w, h)` 9-slice theme frame (honours slice + bleed);
`backdrop()` dimmed launcher; `hints(s, d, items, y)` glyph + label pairs at x 272; `rule(d, y)`; `chip(s, d, x, y, label, h=24)`; `bar(s, x, y, w, frac, h=14)`; `panel(h, top=None)`;
`gib(n)` "{:.1f} GB"; `num(n, lang)` space-grouped integer; screen builders `screen1(variant)`, `screen1c(lang)`, `screen2(name, lang)`, `screen3(bios)`, `screen4(kind)`; data tables
`STEPS`, `PL`, `PROGRESS`, `PL_2A`, `NOTICE`, `GOES`, `STAYS`, constants `PX=240, PW=800, PAD=32`. The launcher already has the panel, selection, chip, progress frames, toggle icons and hint glyphs
(per the generator docstring); new code should only add the progress layout (2.4), 3a/3b two-column list and the result boxes.

## 5. Placeholder numbers (the code shows real values)
1a: v1.22.2, v1.23.0, "about 640 MB", 18.4 / 58.2 GB. 1b: 21.0 / 58.2 GB. 1c: "about 400 MB", "10-15 minutes" (kept as written, measured P4).
2a: 128 / 289 MB, 27%, "About 1 min left", 2.4 MB/s, step 3 of 9, package 3 of 9. 2b: 12 400 / 41 000 files, 72%, package 5 of 9. 2d: 120 / 290 MB, 68%, package 3 of 9.
2c: 3 120 / 7 400 files, 42%, package 2 of 7. 4a: 1.23.0, 15.9 GB. 4b: 2.6 GB, 1.1 GB. 3a/3b and 4c: no numbers.

## 6. Contradictions, gaps and risks (PNG/generator vs decisions and notes)
Nothing breaks decisions 2-6 outright (Kasia's check holds for the panel/Stop/notice). The following is where the mockups disagree with, or do not cover, a decision or note:
1. Decision 3 / Kasia: "Step i of 9" assumes 9 steps. The generator's 9 phase names are invented for the mockup (the real job runs download-cores, unpack, merge-config...). Real step list and count must come from the job; Remove is "Step 1 of 1", which makes the top bar a duplicate of the bottom one (2c: 42% and 42%).
2. "Package i of n" is inconsistent: 2a package 3 of 9, 2b package 5 of 9, 2d package 3 of 9 under the same "Step 7 of 9" (2b and 2d are both "Unpacking" with different package numbers, and 2d "3 of 9" repeats 2a's). Package numbering/ordering is undefined; n=9 coincides with the step count. 2c "of 7" vs 3b with BIOS = 8 deleted items.
3. Kasia note a: 2c shows a hard total "7 400 files". The notes allow a count-while-deleting fallback ("N files" + packages bar), and installs made before the file list existed have no list: no mockup for "N files" without a total, nor for a missing list.
4. Kasia note b: 2a shows MB with a total; the mockup has no state while sizes are unknown (before HEAD/Content-Length returns) and none for the 6 libretro zips ("Downloading assets"). Step phases "Checking space", "Merging settings", "Installing" and "Downloading libraries/apps/assets" have no mockup: unit (MB/files) and why-line are not given. The why line exists only for 2a, 2b, 2c, 2d ("one why line per phase", decision 5, is not delivered for the other phases).
5. Decision 4: Stop is shown on 2b/2d (unpack), but 4c says "RetroArch was not changed" and the only tested Stop was during download (P5). A Stop during unpack/merge may leave a half-installed state; the 4c wording may then be untrue. No "Stopping..." state is mocked (the PSC run says Stop ends "in seconds"). 2c footer promises "about half a minute" (measured ~25 s on the PSC), a fixed claim on every device.
5b. Decision 4 also says a half-done Remove is finished by the next Install/Remove; no screen or text tells the user of this.
6. Decision 2 / 1a: the bar is labelled "free" but filled with the USED part (1 - free/total, 68% filled while 18.4 of 58.2 GB is free). Probably intended, but ambiguous. 1a still shows a download size for Update ("about 640 MB") - allowed by Kasia c (it is a download, not the RetroArch size).
7. Placeholders that disagree with each other: 1a "640 MB" vs 1c "400 MB" vs 4b "needs 2.6 GB" vs 2a total 289 MB: Kasia c says placeholders, the code shows real values; 1c keeps the literal "about 400 MB / 10-15 min".
8. Polish: only 1c-pl and 2a-pl are mocked. PSC P8 says panel, progress and the Done box are in Polish; no PL text exists for 1a, 1b, 2b, 2c, 2d, 3a, 3b, 4a-c, and the generator's `PL` dict covers only 4 words (step names other than "Downloading cores" stay English in the generator). The developer needs the launcher's own PL strings. Number formats: generator `num()` ignores language (always space-grouped), `gib()` uses a decimal point, the PL 2.4 -> "2,4" was typed by hand: PL needs a decimal comma for GB/MB/speed everywhere.
9. Layout limits: no wrapping/overflow rule exists (single lines only). 1c-pl is at 709 of 736 px; the 2a package line with a bigger size ("1 289 of 1 289 MB") or a longer package name can collide with the right-aligned "Package i of n".
10. The toggle is mocked only OFF; ON state, the focused-toggle look and how Remove reads the toggle are not mocked (3b is the ON result). The focus rows: only Update focused (1a); the default focus for "installed, no update", and for "not installed, network OK" is not specified (Install enabled state is not mocked at all).
11. 2c has a different footer from the other progress screens (text instead of hint row) - deliberate (no Stop) but it breaks "one layout for every phase" slightly.
12. 4b shows one error cause only (no space); other failures (network lost - P6 not run, checksum, unpack) have no text. 4a "Version 1.23.0" implies an Update; the fresh-install text is not distinguished.
13. The generator's sample 4a shows free space 15.9 GB (= the stick after the install), 4b 1.1 GB: real values from statvfs.
