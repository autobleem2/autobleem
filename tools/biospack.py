#!/usr/bin/env python3
"""Build payload_rpi/system/biospack.txt (or biospack-arm64.txt), the BIOS pack install.sh downloads.

The files come from RetroBIOS (github.com/Abdess/retrobios), a source-verified BIOS collection with a
manifest per platform - install/retroarch.json lists every file the RetroArch pack carries, with its size,
SHA-256 and where it is served from. Which cores exist for the target picks which of those files matter:
for armhf, RetroBIOS's own install/targets/retroarch.json has a "linux-armhf" list. It has no matching
64-bit Linux target (only android-arm64-v8a, osx-arm64, ios-arm64 - none of them our target), so --arch
arm64 reads the real core list straight from buildbot.libretro.com/nightly/linux/aarch64/latest instead -
what install.sh itself downloads cores from, so "which cores exist" is never in question. The full RetroArch
pack is 5.8 GB and even one architecture's slice of it mostly not BIOS at all: arcade sound-sample zips, the
MAME history/mameinfo/cheat text files in triplicate, files for cores that have no build on that
architecture. This script keeps what the systems AutoBleem sets up on the Pi need (every system install.sh
makes a roms/ folder for, arcade, ScummVM, Doom) and writes a flat manifest that install.sh's
download_bios_pack() fetches file by file with wget and checks with sha256sum - no BIOS file is ever checked
in here, and the pack is pinned to one RetroBIOS commit so the same manifest comes out every time.

    python tools/biospack.py                 # rewrite payload_rpi/system/biospack.txt (armhf) from the pinned commit
    python tools/biospack.py --arch arm64    # rewrite payload_rpi/system/biospack-arm64.txt
    python tools/biospack.py --ref main      # try RetroBIOS's current main (then update RETROBIOS_REF)
    python tools/biospack.py --list          # print what is in and out, per system, and stop
    python tools/biospack.py --arch arm64 --list
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
RETROBIOS_TARGET = "linux-armhf"  # RetroBIOS's own per-target core list - armhf only, see ARCHES below

# buildbot.libretro.com's directory name for each architecture (not the same as Debian's - arm64 is
# "aarch64" there) and the manifest file install.sh looks for on that architecture.
BUILDBOT_INDEX = "https://buildbot.libretro.com/nightly/linux/{buildbot_arch}/latest/.index-extended"
ARCHES = {
    "armhf": {"buildbot_arch": "armhf", "manifest_name": "biospack.txt"},
    "arm64": {"buildbot_arch": "aarch64", "manifest_name": "biospack-arm64.txt"},
}

MANIFEST_DIR = os.path.join(os.path.dirname(__file__), "..", "payload_rpi", "system")

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


# Files a core wants in system/ that are not BIOS and so not in RetroBIOS: fetched from the core's own
# repository at a pinned commit. blueMSX will not start without the machine definition
# (Machines/<machine>/config.ini) of the machine it picks - "COL - ColecoVision" for a .col, "MSX2+" for an
# MSX cartridge, and so on - and these live in the core's system/bluemsx tree, next to the databases.
# Where RetroBIOS carries the same file (the ROMs), RetroBIOS's copy wins.
EXTRA_SOURCES = [
    {
        "repo": "libretro/blueMSX-libretro",
        "ref": "e3086eb5d36d77fa11704cf53dc176686e70127d",  # 2026-09
        "strip": "system/bluemsx/",
        "folders": [
            "system/bluemsx/Machines/MSX/",
            "system/bluemsx/Machines/MSX2/",
            "system/bluemsx/Machines/MSX2+/",
            "system/bluemsx/Machines/MSXturboR/",
            "system/bluemsx/Machines/MSX - C-BIOS/",
            "system/bluemsx/Machines/MSX2 - C-BIOS/",
            "system/bluemsx/Machines/MSX2+ - C-BIOS/",
            "system/bluemsx/Machines/COL - ColecoVision/",
            "system/bluemsx/Machines/COL - Spectravideo SVI-603 Coleco/",
            "system/bluemsx/Machines/SEGA - SG-1000/",
            "system/bluemsx/Machines/SEGA - SC-3000/",
            "system/bluemsx/Machines/SEGA - SF-7000/",
            "system/bluemsx/Machines/SVI - Spectravideo SVI-318/",
            "system/bluemsx/Machines/SVI - Spectravideo SVI-328/",
            "system/bluemsx/Machines/SVI - Spectravideo SVI-328 MK2/",
        ],
        "why": "blueMSX's machine definitions",
    },
]


def fetch_json(url, what):
    try:
        with urllib.request.urlopen(url, timeout=60) as resp:
            return json.load(resp)
    except Exception as exc:  # noqa: BLE001 - one message for every way a download can fail
        sys.exit(f"cannot fetch the RetroBIOS {what} from {url}: {exc}")


def file_url(entry, ref):
    if entry.get("url"):
        return entry["url"]
    if entry.get("release_asset"):
        return RELEASE_BASE.format(asset=urllib.parse.quote(entry["release_asset"], safe=""))
    return RAW_BASE.format(ref=ref) + urllib.parse.quote(entry["repo_path"], safe="/")


def system_of(entry):
    if not entry.get("repo_path"):
        return None
    parts = entry["repo_path"].split("/")
    return "/".join(parts[1:3]) if parts[0] == "bios" and len(parts) >= 4 else None


def excluded(dest):
    name = dest.rsplit("/", 1)[-1]
    return dest.startswith(EXCLUDE_PREFIXES) or name in EXCLUDE_NAMES or dest.endswith(EXCLUDE_SUFFIXES)


def buildbot_cores(buildbot_arch):
    """The core names actually built for this architecture, straight from the buildbot's own nightly
    listing - what install.sh's download_retroarch_content() itself downloads from. Used instead of
    RetroBIOS's targets/retroarch.json for an architecture RetroBIOS has no matching entry for (arm64:
    it only lists android-arm64-v8a/osx-arm64/ios-arm64, none of them this target)."""
    url = BUILDBOT_INDEX.format(buildbot_arch=buildbot_arch)
    try:
        with urllib.request.urlopen(url, timeout=60) as resp:
            text = resp.read().decode("utf-8", "replace")
    except Exception as exc:  # noqa: BLE001
        sys.exit(f"cannot fetch the buildbot core list from {url}: {exc}")
    suffix = "_libretro.so.zip"
    cores = {line.split()[-1][: -len(suffix)]
             for line in text.splitlines() if line.strip().endswith(suffix)}
    if not cores:
        sys.exit(f"no cores found at {url} - has buildbot.libretro.com's layout changed?")
    return cores


def select(manifest, target_cores, arch_label):
    """The pack: (kept, dropped) lists of manifest entries, each with a 'why' for --list."""
    cores = set(target_cores) - DROP_CORES
    kept, dropped = [], []
    for entry in manifest["files"]:
        system = system_of(entry)
        if system not in SYSTEMS:
            dropped.append((entry, "system not in the pack"))
        elif entry["cores"] is not None and not set(entry["cores"]) & cores:
            dropped.append((entry, "no core for it on " + arch_label))
        elif excluded(entry["dest"]):
            dropped.append((entry, "excluded path"))
        else:
            kept.append((entry, SYSTEMS[system]))
    kept.sort(key=lambda item: (system_of(item[0]), item[0]["dest"].lower()))
    return kept, dropped


def extra_entries(source):
    """Manifest entries for one EXTRA_SOURCES repository: the tree listing from GitHub's API, then each file
    downloaded once here to get its SHA-256 (the API only has git's blob id)."""
    tree = fetch_json(f"https://api.github.com/repos/{source['repo']}/git/trees/{source['ref']}?recursive=1",
                      source["repo"] + " tree")
    entries = []
    for node in tree["tree"]:
        if node["type"] != "blob" or not node["path"].startswith(tuple(source["folders"])):
            continue
        url = (f"https://raw.githubusercontent.com/{source['repo']}/{source['ref']}/"
               + urllib.parse.quote(node["path"], safe="/"))
        try:
            with urllib.request.urlopen(url, timeout=60) as resp:
                data = resp.read()
        except Exception as exc:  # noqa: BLE001
            sys.exit(f"cannot fetch {url}: {exc}")
        entries.append(({"dest": node["path"][len(source["strip"]):], "size": len(data),
                         "sha256": hashlib.sha256(data).hexdigest(), "url": url, "repo_path": None,
                         "cores": None}, source["why"]))
    if not entries:
        sys.exit(f"{source['repo']}@{source['ref']}: none of the folders exist any more")
    return entries


def add_extras(kept):
    """The EXTRA_SOURCES files, after RetroBIOS's: a path RetroBIOS already provides is left to it."""
    have = {e["dest"] for e, _ in kept}
    for source in EXTRA_SOURCES:
        for entry, why in extra_entries(source):
            if entry["dest"] not in have:
                kept.append((entry, why))
                have.add(entry["dest"])
    return kept


def mib(size):
    return size / (1024 * 1024)


def print_listing(kept, dropped):
    by_system = defaultdict(list)
    for entry, why in kept:
        by_system[system_of(entry) or why].append(entry)
    print(f"In the pack ({len(kept)} files, {mib(sum(e['size'] for e, _ in kept)):.1f} MB):")
    for system in sorted(by_system):
        entries = by_system[system]
        print(f"  {mib(sum(e['size'] for e in entries)):7.1f} MB {len(entries):4d}  {system}  - {SYSTEMS.get(system, 'from the core')}")
    print()
    print("Left out, the biggest first:")
    for entry, why in sorted(dropped, key=lambda item: -item[0]["size"])[:30]:
        print(f"  {mib(entry['size']):7.1f} MB  {entry['dest']:50s} {why}")


def write_manifest(kept, ref, generated, path):
    total = sum(e["size"] for e, _ in kept)
    lines = [
        "# AutoBleem's BIOS pack for the Raspberry Pi: what install.sh downloads into RetroArch/system/.",
        f"# Built by tools/biospack.py from RetroBIOS (github.com/Abdess/retrobios) at {ref},",
        f"# {generated}. {len(kept)} files, {total} bytes ({mib(total):.0f} MB), plus what the cores' own",
        "# repositories carry in their system/ trees (EXTRA_SOURCES in the script: blueMSX's Machines).",
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
    parser.add_argument("--arch", choices=sorted(ARCHES), default="armhf",
                         help="which Pi architecture's core list to build the pack for (default: armhf)")
    parser.add_argument("--ref", default=RETROBIOS_REF, help="RetroBIOS commit or branch to read (default: the pinned one)")
    parser.add_argument("--out", default=None, help="where to write the manifest (default: payload_rpi/system/"
                         "biospack.txt for armhf, biospack-arm64.txt for arm64)")
    parser.add_argument("--list", action="store_true", help="print the selection and change nothing")
    parser.add_argument("--check", metavar="DIR", help="verify a RetroArch/system/ folder against the manifest")
    args = parser.parse_args()
    if args.out is None:
        args.out = os.path.normpath(os.path.join(MANIFEST_DIR, ARCHES[args.arch]["manifest_name"]))

    if args.check:
        sys.exit(0 if check_dir(args.out, args.check) else 1)

    base = RAW_BASE.format(ref=args.ref)
    manifest = fetch_json(base + "install/retroarch.json", "manifest")
    if args.arch == "armhf":
        targets = fetch_json(base + "install/targets/retroarch.json", "targets")
        if RETROBIOS_TARGET not in targets:
            sys.exit(f"RetroBIOS has no '{RETROBIOS_TARGET}' target any more; targets: {', '.join(sorted(targets))}")
        target_cores, arch_label = targets[RETROBIOS_TARGET], RETROBIOS_TARGET
    else:
        buildbot_arch = ARCHES[args.arch]["buildbot_arch"]
        target_cores = buildbot_cores(buildbot_arch)
        arch_label = f"the {buildbot_arch} buildbot"
    unknown = sorted(s for s in SYSTEMS if not any(system_of(e) == s for e in manifest["files"]))
    if unknown:
        print("warning: no files under these folders any more: " + ", ".join(unknown), file=sys.stderr)

    kept, dropped = select(manifest, target_cores, arch_label)
    kept = add_extras(kept)
    if args.list:
        print_listing(kept, dropped)
        return
    generated = datetime.now(timezone.utc).strftime("%Y-%m-%d")
    total = write_manifest(kept, args.ref, generated, args.out)
    print(f"{args.out}: {len(kept)} files, {mib(total):.1f} MB, RetroBIOS {args.ref}")


if __name__ == "__main__":
    main()
