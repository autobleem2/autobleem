#!/usr/bin/env python3
"""Build payload_rpi/system/biospack.txt, the BIOS pack the Raspberry Pi installer downloads.

The files come from RetroBIOS (github.com/Abdess/retrobios), a source-verified BIOS collection with a
manifest per platform - install/retroarch.json lists every file the RetroArch pack carries, with its size,
SHA-256 and where it is served from, and install/targets/retroarch.json says which cores exist per hardware
target. The full RetroArch pack is 5.8 GB and the linux-armhf slice of it still 2.6 GB, most of which is not
BIOS at all: arcade sound-sample zips, the MAME history/mameinfo/cheat text files in triplicate, files for
cores that have no armhf build. This script keeps what the systems AutoBleem sets up on the Pi need (every
system install.sh makes a roms/ folder for, arcade, ScummVM, Doom) and writes a flat manifest that install.sh's
download_bios_pack() fetches file by file with wget and checks with sha256sum - no BIOS file is ever checked
in here, and the pack is pinned to one RetroBIOS commit so the same manifest comes out every time.

    python tools/biospack.py                 # rewrite payload_rpi/system/biospack.txt from the pinned commit
    python tools/biospack.py --ref main      # try RetroBIOS's current main (then update RETROBIOS_REF)
    python tools/biospack.py --list          # print what is in and out, per system, and stop
    python tools/biospack.py --check DIR     # verify a RetroArch/system/ folder against the manifest

Only the standard library is needed.
"""

import argparse
import hashlib
import json
import os
import sys
import urllib.parse
import urllib.request
from collections import defaultdict
from datetime import datetime, timezone

# The RetroBIOS commit the pack is built from. Bump it deliberately (--ref main to preview the change).
RETROBIOS_REF = "73be130e651eed55b305e92654eaeffa61d9d2c1"  # 2026-09-14

RAW_BASE = "https://raw.githubusercontent.com/Abdess/retrobios/{ref}/"
RELEASE_BASE = "https://github.com/Abdess/retrobios/releases/download/large-files/{asset}"
TARGET = "linux-armhf"  # buildbot.libretro.com/nightly/linux/armhf - the cores install.sh downloads

MANIFEST_OUT = os.path.join(os.path.dirname(__file__), "..", "payload_rpi", "system", "biospack.txt")

# RetroBIOS keeps its files as bios/<Vendor>/<System>/...; these are the folders the pack draws from. The
# comment says what on the Pi wants the files. A system with a roms/ folder in install.sh but nothing here
# (Virtual Boy, Neo Geo Pocket, Vectrex, DOS, ...) simply has no BIOS to ship.
SYSTEMS = {
    # consoles and handhelds
    "Sony/PlayStation": "pcsx_rearmed, and pcsx-ab's romw.bin/romJP.bin",
    "3DO Company/3DO": "opera",
    "Atari/2600": "stella (the KidVid tapes are excluded below)",
    "Atari/5200": "atari800",
    "Atari/7800": "prosystem",
    "Atari/Lynx": "handy, mednafen_lynx",
    "Coleco/ColecoVision": "bluemsx",
    "Magnavox/Odyssey2": "o2em",
    "Philips/Videopac": "o2em",
    "Philips/Videopac+": "o2em",
    "Mattel/Intellivision": "freeintv",
    "NEC/PC Engine": "mednafen_pce (the CD system cards)",
    "Nintendo/Game Boy": "gambatte, sameboy, mgba boot ROMs",
    "Nintendo/Game Boy Color": "same",
    "Nintendo/Game Boy Advance": "mgba, gpsp, vbam",
    "Nintendo/NES": "fceumm, nestopia, mesen",
    "Nintendo/SNES": "snes9x, bsnes (the coprocessor ROMs)",
    "Nintendo/Super Game Boy": "snes9x, bsnes",
    "Nintendo/Satellaview": "snes9x, bsnes",
    "Nintendo/SuFami Turbo": "snes9x, bsnes",
    "Nintendo/Pokemon Mini": "pokemini",
    "Sega/Master System": "genesis_plus_gx, picodrive",
    "Sega/Game Gear": "genesis_plus_gx",
    "Sega/Mega Drive": "genesis_plus_gx, picodrive",
    "Sega/Mega CD": "genesis_plus_gx, picodrive",
    "Sega/32X": "picodrive",
    "SNK/Neo Geo": "fbneo's AES set, neocd's universe BIOS",
    "SNK/Neo Geo CD": "neocd",
    "Bally/Astrocade": "fbneo",
    # home computers
    "Commodore/Amiga": "puae (the Kickstart ROMs, CD32 and CDTV included)",
    "Commodore/C64": "vice_x64 (JiffyDOS and the alternative kernals; the stock ROMs are built in)",
    "Commodore/C128": "vice_x64, vice_xvic (same)",
    "Microsoft/MSX": "fmsx, bluemsx",
    "Other/bluemsx": "bluemsx",
    "Other/msx-emu": "bluemsx",
    "Sinclair/ZX Spectrum": "fuse",
    "Other/zesarux": "fuse",
    "Atari/400-800": "atari800",
    "NEC/PC-98": "np2kai, nekop2",
    "IBM/PC": "nekop2",
    "NEC/PC-88": "quasi88",
    "Sharp/X68000": "px68k",
    "Sharp/X1": "x1",
    "Elektronika/BK": "bk",
    # arcade and engines
    "Arcade/Arcade": "fbneo, fbalpha2012, mame2000/2003/2003_plus: the BIOS sets the games need",
    "Arcade/MAME": "same (the pgm BIOS; the .dat text files are excluded below)",
    "Arcade/FBNeo": "fceumm's disksys.rom (Famicom Disk System) lives here, and fuse's Spectrum ROMs",
    "Sega/Arcade": "cannonball (OutRun)",
    "Other/ScummVM": "scummvm's engine data and themes",
    "ScummVM/.variants": "scummvm's bundled data",
    "Id Software/Doom": "prboom's prboom.wad",
    "Id Software/Wolfenstein 3D": "ecwolf's ecwolf.pk3",
}

# Cores from the target's list that the pack does not serve. The current-year `mame` core is on the armhf
# buildbot but is far too heavy for a Pi, and the device ROM sets only it loads are the biggest arcade files.
DROP_CORES = {"mame"}

# Paths (relative to system/) that are out whatever folder they come from. Not BIOS: the arcade sound
# samples, MAME's history/mameinfo/cheat text, stella's KidVid audio, x86 MIDI libraries; no core on
# armhf: the Dreamcast/Naomi and ST-V sets (flycast, kronos).
EXCLUDE_PREFIXES = ("fba2012/samples/", "fbneo/samples/", "dc/", "kronos/")
EXCLUDE_NAMES = {"history.dat", "mameinfo.dat", "cheat.dat"}
EXCLUDE_SUFFIXES = (".wav", ".dll", ".dylib", ".so")


def fetch_json(url, what):
    try:
        with urllib.request.urlopen(url, timeout=60) as resp:
            return json.load(resp)
    except Exception as exc:  # noqa: BLE001 - one message for every way a download can fail
        sys.exit(f"cannot fetch the RetroBIOS {what} from {url}: {exc}")


def file_url(entry, ref):
    if entry.get("release_asset"):
        return RELEASE_BASE.format(asset=urllib.parse.quote(entry["release_asset"], safe=""))
    return RAW_BASE.format(ref=ref) + urllib.parse.quote(entry["repo_path"], safe="/")


def system_of(entry):
    parts = entry["repo_path"].split("/")
    return "/".join(parts[1:3]) if parts[0] == "bios" and len(parts) >= 4 else None


def excluded(dest):
    name = dest.rsplit("/", 1)[-1]
    return dest.startswith(EXCLUDE_PREFIXES) or name in EXCLUDE_NAMES or dest.endswith(EXCLUDE_SUFFIXES)


def select(manifest, target_cores):
    """The pack: (kept, dropped) lists of manifest entries, each with a 'why' for --list."""
    cores = set(target_cores) - DROP_CORES
    kept, dropped = [], []
    for entry in manifest["files"]:
        system = system_of(entry)
        if system not in SYSTEMS:
            dropped.append((entry, "system not in the pack"))
        elif entry["cores"] is not None and not set(entry["cores"]) & cores:
            dropped.append((entry, "no core for it on " + TARGET))
        elif excluded(entry["dest"]):
            dropped.append((entry, "excluded path"))
        else:
            kept.append((entry, SYSTEMS[system]))
    kept.sort(key=lambda item: (system_of(item[0]), item[0]["dest"].lower()))
    return kept, dropped


def mib(size):
    return size / (1024 * 1024)


def print_listing(kept, dropped):
    by_system = defaultdict(list)
    for entry, _ in kept:
        by_system[system_of(entry)].append(entry)
    print(f"In the pack ({len(kept)} files, {mib(sum(e['size'] for e, _ in kept)):.1f} MB):")
    for system in sorted(by_system):
        entries = by_system[system]
        print(f"  {mib(sum(e['size'] for e in entries)):7.1f} MB {len(entries):4d}  {system}  - {SYSTEMS[system]}")
    print()
    print("Left out, the biggest first:")
    for entry, why in sorted(dropped, key=lambda item: -item[0]["size"])[:30]:
        print(f"  {mib(entry['size']):7.1f} MB  {entry['dest']:50s} {why}")


def write_manifest(kept, ref, generated, path):
    total = sum(e["size"] for e, _ in kept)
    lines = [
        "# AutoBleem's BIOS pack for the Raspberry Pi: what install.sh downloads into RetroArch/system/.",
        f"# Built by tools/biospack.py from RetroBIOS (github.com/Abdess/retrobios) at {ref},",
        f"# {generated}. {len(kept)} files, {total} bytes ({mib(total):.0f} MB).",
        "# One file per line: <sha256> <size> <url> <path under system/>. The path may contain spaces.",
        "# The BIOS files themselves are not part of this repository - see NOTICE in the RetroBIOS repository.",
    ]
    lines.extend(f"{e['sha256']} {e['size']} {file_url(e, ref)} {e['dest']}" for e, _ in kept)
    with open(path, "w", encoding="utf-8", newline="\n") as out:
        out.write("\n".join(lines) + "\n")
    return total


def check_dir(manifest_path, directory):
    """--check: hash every file the manifest names under DIR; missing and mismatched are listed."""
    ok = missing = bad = 0
    with open(manifest_path, encoding="utf-8") as manifest:
        for line in manifest:
            if line.startswith("#") or not line.strip():
                continue
            sha, _size, _url, dest = line.rstrip("\n").split(" ", 3)
            path = os.path.join(directory, dest)
            if not os.path.isfile(path):
                missing += 1
                print(f"missing   {dest}")
                continue
            with open(path, "rb") as handle:
                actual = hashlib.sha256(handle.read()).hexdigest()
            if actual == sha:
                ok += 1
            else:
                bad += 1
                print(f"mismatch  {dest}")
    print(f"{ok} ok, {missing} missing, {bad} mismatched")
    return missing == 0 and bad == 0


def main():
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    parser.add_argument("--ref", default=RETROBIOS_REF, help="RetroBIOS commit or branch to read (default: the pinned one)")
    parser.add_argument("--out", default=os.path.normpath(MANIFEST_OUT), help="where to write the manifest")
    parser.add_argument("--list", action="store_true", help="print the selection and change nothing")
    parser.add_argument("--check", metavar="DIR", help="verify a RetroArch/system/ folder against the manifest")
    args = parser.parse_args()

    if args.check:
        sys.exit(0 if check_dir(args.out, args.check) else 1)

    base = RAW_BASE.format(ref=args.ref)
    manifest = fetch_json(base + "install/retroarch.json", "manifest")
    targets = fetch_json(base + "install/targets/retroarch.json", "targets")
    if TARGET not in targets:
        sys.exit(f"RetroBIOS has no '{TARGET}' target any more; targets: {', '.join(sorted(targets))}")
    unknown = sorted(s for s in SYSTEMS if not any(system_of(e) == s for e in manifest["files"]))
    if unknown:
        print("warning: no files under these folders any more: " + ", ".join(unknown), file=sys.stderr)

    kept, dropped = select(manifest, targets[TARGET])
    if args.list:
        print_listing(kept, dropped)
        return
    generated = datetime.now(timezone.utc).strftime("%Y-%m-%d")
    total = write_manifest(kept, args.ref, generated, args.out)
    print(f"{args.out}: {len(kept)} files, {mib(total):.1f} MB, RetroBIOS {args.ref}")


if __name__ == "__main__":
    main()
