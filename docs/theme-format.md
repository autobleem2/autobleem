# AutoBleem themes: `theme.json`

A theme is a folder under `/media/Themes/<name>/` with a `theme.json` and the files it names. Everything in
the file is optional: a key that is missing is taken from `Themes/default/theme.json`, and a file that is
named but not in the folder is taken from the default theme too. So a theme can be as small as a
background and a logo. Paths in the file are relative to the theme folder; `/` is the separator.

Written 2026-09-17, when the format replaced `theme.ini`. The code that reads it is
`ableem::ThemeSpec` (`lib_ableem/include/ableem/engine/theme_spec.h`) and `Theme`
(`src/code/core/services/theme.*`).

## Layout

```
Themes/<name>/
  theme.json
  background.jpg  ab.png  saira-semicondensed-medium.ttf  mel.ogg  cross.png ...   the classic UI's files - names are free,
                                                              theme.json says which is which
  images/launcher_background.png  launcher_footer.png  play_button.png  play_text.png
         settings_panel.png  meta_panel.png  arrow.png  hint_cross.png  hint_circle.png
         hint_triangle.png  menu_settings.png  menu_guide.png  menu_memcard.png  menu_resume.png
         memcard_grid.png  memcard_pencil.png                 the EvolutionUI launcher (names are free
                                                              here too; these are what the converter picks)
  font/medium.ttf  font/bold.ttf                              optional - the launcher's own pair
  sounds/cursor.wav  cancel.wav  home_up.wav  home_down.wav  resume_new.wav      optional
  credit.txt                                                  anything else at the root is left alone
```

## The file

`payload/Themes/aergb/theme.json`, with every section:

```json
{
  "format": 1,
  "music": { "file": "mel.ogg", "loop": true },
  "classic": {
    "background": "background.jpg",
    "logo": { "file": "ab.png", "x": 520, "y": 0, "w": 240, "h": 180 },
    "font": { "file": "saira-semicondensed-medium.ttf", "size": 22 },
    "menuLines": 13,
    "menuPanel": { "x": 30, "y": 10, "w": 1220, "h": 530, "color": "#000000", "alpha": 170 },
    "statusBar": { "x": 0, "y": -670, "w": 1280, "h": 30, "textY": 662, "color": "#000000", "alpha": 170 },
    "textColor": "#ffffff",
    "textShadow": true,
    "keyboardKey": { "color": "#787878", "alpha": 170 },
    "labelColor": "#b4b4b4",
    "freeSpaceText": { "x": 180, "y": 35 },
    "editorCover": { "x": 95, "y": 130 },
    "buttons": { "cross": "cross.png", "circle": "circle.png", "square": "square.png",
                 "triangle": "triangle.png", "start": "start.png", "select": "select.png",
                 "l1": "l1.png", "r1": "r1.png", "l2": "l2.png", "r2": "r2.png",
                 "check": "on.png", "uncheck": "off.png", "esc": "esc.png", "enter": "enter.png", "tab": "tab.png" }
  },
  "launcher": {
    "background": "images/launcher_background.png",
    "footer": "images/launcher_footer.png",
    "playButton": "images/play_button.png",
    "playText": "images/play_text.png",
    "settingsPanel": "images/settings_panel.png",
    "metaPanel": "images/meta_panel.png",
    "metaPanelSlides": true,
    "textShadow": true,
    "arrow": "images/arrow.png",
    "hints": { "cross": "images/hint_cross.png", "circle": "images/hint_circle.png", "triangle": "images/hint_triangle.png" },
    "menuIcons": { "settings": "images/menu_settings.png", "guide": "images/menu_guide.png",
                   "memcard": "images/menu_memcard.png", "resume": "images/menu_resume.png",
                   "resumePicture": { "x": 25, "y": 33, "w": 68, "h": 52 },
                   "resumeSlotLabel": { "x": 22, "y": 18 } },
    "memcardManager": { "grid": "images/memcard_grid.png", "pencil": "images/memcard_pencil.png" },
    "fonts": { "medium": "font/SST-Medium.ttf", "bold": "font/SST-Bold.ttf" },
    "colors": { "text": "#ffffff", "secondary": "#646464", "hint": "#646464", "selection": "#4fc3f7" }
  },
  "sounds": { "cursor": "sounds/cursor.wav", "cancel": "sounds/cancel.wav", "homeUp": "sounds/home_up.wav",
              "homeDown": "sounds/home_down.wav", "resume": "sounds/resume_new.wav" }
}
```

| Key | Meaning |
|---|---|
| `format` | `1`. |
| `music` | The background track. `loop` false plays it once. `"music": null` is a theme with no music at all - it also silences a track the user picked in Options. |
| `classic.background` | The classic UI's full-screen background (menus, splash, dialogs). |
| `classic.logo` | The logo file and the rect it is drawn in. Dialogs draw it at a third of the size at the menu panel's origin. |
| `classic.font` | The classic UI's ttf and point size. Also sizes the list menus' rows. |
| `classic.menuLines` | Visible rows in a list menu. |
| `classic.menuPanel` | The translucent panel behind a menu: rect, fill colour, alpha. |
| `classic.statusBar` | The status line at the bottom: its bar (rect, colour, alpha) and `textY`, where the text is drawn. Every shipped theme keeps the bar off screen (`y: -670`), so only the text shows. |
| `classic.textColor` | The classic UI's text and selection-box colour. |
| `classic.textShadow` | `false` drops the dark halo drawn under the classic UI's text (it keeps white text readable on a light background; on by default). |
| `classic.keyboardKey` | The on-screen keyboard's key fill; its alpha is also the label box's. |
| `classic.labelColor` | The label box (dialog titles) fill. |
| `classic.freeSpaceText` | Where "Free space: ..." is drawn. |
| `classic.editorCover` | Where the game editor draws the cover. |
| `classic.buttons` | One image per `\|@X\|` marker in UI strings: `cross circle square triangle start select l1 r1 l2 r2 check uncheck esc enter tab`. |
| `launcher.*` | The EvolutionUI launcher's images, by what they are on screen. `metaPanelSlides: false` keeps the metadata panel in place when the menu opens (for a background drawn around it). `textShadow: false` drops the dark halo drawn under the launcher's text (it is what keeps white text readable on a light background; on by default). |
| `launcher.snapPanel` | `{ "x", "y", "w", "h" }`: where the selected game's screenshot (RetroArch's thumbnails `Named_Snaps`, or the user's own from `retroarch/screenshots`) is drawn, aspect-fit inside the rect. Leave it out and no screenshot is drawn. |
| `launcher.hints` | The cross/circle/triangle icons in the footer. |
| `launcher.menuIcons` | The launcher menu's four icons, 118x118 each. `resumePicture` (optional, `{ "x", "y", "w", "h" }` in the icon's own pixels) is where the save state's picture is pasted on the resume icon - draw the icon's frame around it; unset means (25, 33) 68x52, the original icon's window. `resumeSlotLabel` (optional, `{ "x", "y" }`, the same pixels) is where the resume-slot picker writes "Slot n" on its 2.7x copy of the icon, left-aligned; unset means (22, 18), the original spot above the original window - a theme that moves the window (ab2 puts a screen at the top of its tile) moves the label with it. |
| `launcher.memcardManager` | The memory-card manager's block grid and cursor. |
| `launcher.fonts` | The launcher's medium and bold ttf. Without them the shipped pair is used: `resources/fonts/OpenSans-Medium.ttf` / `-Bold.ttf` (OFL; the console's SST fonts were, until 2026-09-21). |
| `launcher.colors` | The launcher's text colour and the secondary (dimmer) one. `hint` is the colour of the footer's "Enter" / "Cancel" / "Button Guide" labels; leave it out and they take `secondary`. A light `hint` gets the dark halo like any other light text (unless `textShadow` is off). `selection` is the resume-slot picker's colour for the selected slot: a halo in it around the slot's tile, the other tiles dimmed; leave it out and the selected slot's tile is tinted red instead, the original way, which only shows on a white tile. |
| `sounds` | The five UI sounds. |

Colours are `"#rrggbb"`; alphas are 0-255.

## Installing a theme from a zip

Copy `<name>.zip` into the themes directory next to the theme folders. The next time the app looks at the
themes (at start, or when the Options menu lists them) it unpacks the archive to `<name>/` and deletes
the zip. The theme's files may be at the archive's root or inside one folder (the usual way a zip is
made - the folder's own name does not matter, the zip's name is the theme's; `__MACOSX` and other
dot/underscore folders are ignored). A zip in the old `theme.ini` layout is converted afterwards like any
folder. A zip that replaces an existing `<name>/` is an update: the folder is replaced. A file that is not
an archive, or holds no theme, is renamed `<name>.zip.bad` and left alone. Entries with `..` or absolute
names are refused, and nothing from such an archive is written.

`theme_convert <themesDir>` does the same installs first, then the conversions.

## Converting an old theme

Before this format a theme was `theme.ini` next to a full copy of the console's `/usr/sony/share/data`
tree (some 330 files) that the app read 17 images, 2 fonts and 5 sounds from by their PSC names. Such a
folder still works: **the app converts it in place the first time it loads it** (`ThemeConverter`,
`src/code/core/services/theme_converter.*`), and `theme_convert <themesDir>` (built with the tests,
`build_win/theme_convert.exe`) does the same ahead of time for every folder under a themes directory.

The conversion writes `theme.json`, renames the launcher images to the names above, and deletes
`theme.ini`, `colors.ini` and every file under `images/`, `sounds/` and `font/` the json does not name.
Files at the theme's root that the json does not name are kept. A folder with launcher images but no
`theme.ini` converts too.

`theme.ini` key by key (keys are case-insensitive):

| `theme.ini` | `theme.json` |
|---|---|
| `Music`, `Loop` (`1` loop, `0` once, `-1` no music) | `music.file`, `music.loop`; `-1` -> `"music": null` |
| `Background` | `classic.background` |
| `Logo`, `Lpositionx/y`, `Lw/Lh` | `classic.logo {file,x,y,w,h}` |
| `Font`, `Fsize` | `classic.font {file,size}` |
| `Lines` | `classic.menuLines` |
| `Opscreenx/y/w/h`, `Main_bg`, `Mainalpha` | `classic.menuPanel {x,y,w,h,color,alpha}` |
| `Textx/y/w/h`, `Text_bg`, `Textalpha`, `Ttop` | `classic.statusBar {x,y,w,h,color,alpha,textY}` |
| `Text_fg` | `classic.textColor` |
| `Key_bg`, `Keyalpha` | `classic.keyboardKey {color,alpha}` |
| `Label_bg` | `classic.labelColor` |
| `Fsposx/y` | `classic.freeSpaceText {x,y}` |
| `Ecoverx/y` | `classic.editorCover {x,y}` |
| `Circle Cross Square Triangle Start Select L1 R1 L2 R2 Check Uncheck Esc Enter Tab` | `classic.buttons.*` |
| `Iconw Iconh IconRescan IconExit Lspositionx/y Lsw Lsh Maxw` | nothing read them; dropped |
| `colors.ini`: `fg`, `sec` | `launcher.colors {text,secondary}` |

A rect is taken only when all four of its keys are there; colours (`r,g,b`) become `#rrggbb`.

| PSC image | role |
|---|---|
| `GR/AB_BG.png` if present, else `GR/JP_US_BG.png` | `launcher.background` (an `AB_BG` sets `metaPanelSlides: false`) |
| `GR/Footer_AB.png` else `GR/Footer.png` | `launcher.footer` |
| `GR/Acid_C_Btn.png`, `BMP_Text/Play_Text.png` | `launcher.playButton`, `launcher.playText` |
| `CB/Function_AB.png` else `CB/Function_BG.png` | `launcher.settingsPanel` |
| `CB/PlayerOne.png` | `launcher.metaPanel` |
| `GR/arrow.png` | `launcher.arrow` |
| `GR/X_Btn_ICN.png`, `GR/Circle_Btn_ICN.png`, `GR/Tri_Btn_ICN.png` | `launcher.hints.*` |
| `CB/Setting_ICN.png`, `CB/Manual_ICN.png`, `CB/MemoryCard_ICN.png`, `CB/Resume.png` | `launcher.menuIcons.*` |
| `MC/Dot_Matrix.png`, `MC/Pencil_Carsor.png` | `launcher.memcardManager.*` |
| `font/SST-Medium.ttf`, `font/SST-Bold.ttf` | `launcher.fonts.*` (names kept) |
| `sounds/{cursor,cancel,home_up,home_down,resume_new}.wav` | `sounds.*` (names kept) |

What went with the old layout: the classic UI's "Start" / "Original" paths used to bind-mount the theme's
`images/`, `sounds/` and `font/` over the stock SonyUI's data (`rc/selection.sh`). Themes no longer carry
that data, so the stock SonyUI is not re-skinned.
