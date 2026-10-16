# RetroArch manager dialogs - designer's answers to ui-spec.md section 6 (AUTOBLEEM-39 part 2)

Author: Kasia (designer), 2026-10-16. Answers items 1-13 of `ui-spec.md` section 6 (Foundry 83ef1d5); item 5's 2c text is the
owner's choice (2026-10-16 01:16). Labels: **DESIGN** = decided, build it; **DEV+VM** = the developer decides, Kasia checks
it on the VM shots. Mockups: `!autobleem/out/ra-manager/ui/` (generator autobleem-design `feature/platform24-ra-manager-ui`).

1. **Steps** (DESIGN). The step list and n come from the real job, in its own order; the mockup's 9 names are placeholders.
   When n = 1 (Remove), hide the whole-job line and its bar; the package line + bar move up to y0+84. The panel height may
   shrink by the saved 62 px (DEV+VM).

2. **Packages** (DESIGN). A package = one archive of the job, numbered in download order; the same package keeps its number
   in its download and its unpack; n = the number of archives (independent of the step count). Remove: package = one
   "will be deleted" group actually present (7, or 8 with BIOS). The mockup numbers are placeholders.

3. **No total known** (DESIGN). Remove without a file list (an old install): count while deleting -
   "Removing emulator cores - 3 120 files" (no "of ..."), bar = packages done / packages. The same fallback for any package
   whose total is unknown. Never a pulsing bar.

4. **The other phases** (DESIGN). All package sizes are fetched in "Checking space" (it needs them for the free-space check
   anyway), so from "Downloading" on the totals are known; if one is still unknown, use the item 3 fallback ("12 MB", bar by
   packages). Package line unit and why-line per phase (EN; all languages via `lang_tools.py add` in the same commit):

   | Phase | Package line | Why-line |
   |---|---|---|
   | Checking space | "Checking sizes - 4 of 9 packages" | "We make sure the stick has room before anything is downloaded." |
   | Downloading RetroArch | MB | "RetroArch itself - the program that runs the cores." |
   | Downloading cores | as ra-2a | as ra-2a |
   | Downloading libraries | MB | "Libraries RetroArch needs to run on this console." |
   | Downloading apps | MB | "Small tools that come with RetroArch." |
   | Downloading assets | MB | "Pictures, fonts and icons for RetroArch's own menus." |
   | Unpacking | per package, as ra-2b / ra-2d (zip = files, tar.gz = MB) | as ra-2b / ra-2d |
   | Merging settings | files | "Your RetroArch settings are kept; only new options are added." |
   | Installing | files | "The new files are put in place." |

   DEV+VM: if the real job has other phases, the developer names them in the same style; Kasia checks the text on the shots.

5. **Stop during unpack / merge, "Stopping..."**
   - DESIGN: Stop shows on every phase BEFORE "Merging settings"; it is hidden during "Merging settings" and "Installing"
     (short, they must finish). A Stop during unpack ends after the current package.
   - DESIGN: after Stop is pressed, the footer hint is replaced by the text "Stopping..." (medium 20 HINT, like the 2c
     footer) until the job ends; then ra-4c.
   - DEV+VM: ra-4c "RetroArch was not changed" holds only if the install works in a staging place until "Installing"; if
     the code changes files earlier, say so and Kasia gives a different second line.
   - **2c footer text (owner 2026-10-16 01:16): "Please wait - this takes under a minute."** (PL "Proszę czekać - to potrwa
     poniżej minuty."), all languages via `lang_tools.py add`. The mockup ra-2c shows it (generator dfc4e97).

5b. **A half-done Remove** (DESIGN). No new screen: the next Install/Remove shows "Removing leftovers" as its first package
   (files), why-line "A Remove that was cut short is finished first."

6. **The free bar** (DESIGN). Filled = USED, like any disk bar; intended - keep it.

7. **Placeholders** - real values from the catalog / statvfs; ra-1c keeps "about 400 MB / 10-15 minutes" literal.

8. **Polish and the other languages**
   - DESIGN: every string through the launcher's translations, all languages in the same commit. Number grouping = a plain
     space in all languages (as the mockups); the decimal separator follows the language (PL comma: 2,4 MB/s, 18,4 GB).
   - DEV+VM: PL shots of 1a, 2b, 2c, 3a, 4a-c for Kasia's check.

9. **Overflow** (DESIGN). Progress / package lines: the package name is cut with "..." so there are at least 24 px before
   the right-aligned "Package i of n". ra-1c and ra-4*: a too-long translated line may wrap to 2 lines (the panel grows by
   30 px per extra line), never cut. DEV+VM: shots of the longest language.

10. **Toggle and focus** (DESIGN). Toggle ON = `icons/switch_on.png`; a focused toggle row = the same `selection` frame as
    the other rows; Remove reads the toggle at confirm (ra-3a off / ra-3b on). Default focus:
    - installed + update: Update;
    - installed, up to date: Remove (Update sub-line "Up to date"; Remove only opens the ra-3a confirm);
    - not installed, network OK: Install, sub-line "About 400 MB to download - takes 10-15 minutes";
    - nothing enabled (ra-1b): no focus, as drawn.

11. **The 2c footer** (DESIGN) stays as drawn (no Stop, so plain text instead of a hint).

12. **Other errors and fresh install vs update** (DESIGN). The ra-4b layout, title "Installation failed" + ERROR chip:

    | Cause | Line 1 | Line 2 |
    |---|---|---|
    | Network lost | "The network connection was lost." | "The downloaded parts are kept - choose Install again to carry on." |
    | Damaged file | "A downloaded file was damaged." | "It will be downloaded again - choose Install again." |
    | Unpack failed | "A package could not be unpacked." | "RetroArch was not changed - try Install again." |
    | Remove error (title "Removing failed") | "Some files could not be deleted." | "Choose Remove again to finish." |

    ra-4a: fresh install "RetroArch is installed" / update "RetroArch is updated", both with
    "Version {v} is ready - start it from the System menu." and the free space line.

13. **Free space values** - as written: real values from statvfs.
