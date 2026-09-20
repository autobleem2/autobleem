#!/usr/bin/env python3
"""Pack the sample games (tools/samples/samples.json) for the download repository.

    python3 tools/build_samples.py [--out DIR] [--cache DIR]

Downloads every game's files from its upstream release (each is sha256-checked against the manifest, a zip
member is taken out of its zip), lays them out exactly as they land on a Pi's data partition, and packs

    <out>/samples-<YYYYMMDD>.tar.gz     the pack (tools/repo_publish.sh samples ... -> samples/)
    <out>/samples-<YYYYMMDD>.json       what is in it, for the site's page and install.sh's summary

The tree inside:

    Games/<title>/<title>.cue + .bin, Game.ini (locked: Automation=0, so the scanner keeps the title,
        publisher, year and players written here - see UsbGame/GameScanner), <title>.png (the cover; the
        carousel takes a PNG next to the game before anything else)
    RetroArch/roms/<system folder>/<label>.<ext>          the launcher's ROM scan labels a ROM no rdb
        knows by its file stem, so the file is named as the game should be listed
    RetroArch/thumbnails/<db>/Named_Boxarts/<label>.png   its box art, found by that label
    SAMPLES.md                                            what the games are and under which licences

Only the standard library: it runs on the build server's Docker image and on the PC alike. The covers are
tools/samples/covers/ (drawn by tools/samples/make_covers.py, which needs Pillow - hence checked in).
"""
import argparse
import datetime
import hashlib
import io
import json
import os
import shutil
import sys
import tarfile
import tempfile
import urllib.request
import zipfile

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
SAMPLES_DIR = os.path.join(HERE, "samples")

# RetroArch's database names, which are the ROM folder names install.sh creates (RA_ROM_SYSTEMS) and the
# thumbnails tree's folder names
SYSTEM_DB = {
    "nes": "Nintendo - Nintendo Entertainment System",
    "snes": "Nintendo - Super Nintendo Entertainment System",
    "md": "Sega - Mega Drive - Genesis",
}


def sha256_of(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def fetch(url, cache):
    """The file at url, downloaded into cache once (named by the sha256 of the url)."""
    os.makedirs(cache, exist_ok=True)
    path = os.path.join(cache, hashlib.sha256(url.encode()).hexdigest()[:16] + "-" + os.path.basename(url))
    if not os.path.exists(path):
        print("  downloading %s" % url)
        req = urllib.request.Request(url, headers={"User-Agent": "autobleem-build-samples"})
        with urllib.request.urlopen(req, timeout=60) as r, open(path + ".part", "wb") as f:
            shutil.copyfileobj(r, f)
        os.replace(path + ".part", path)
    return path


def escape_name(name):
    """RetroArch's rule for a playlist label as a file name (ableem::ThumbnailLookup::escapeName)."""
    return "".join("_" if c in "&*/:`<>?\\|" else c for c in name)


def game_ini(entry, disc_name):
    """A locked Game.ini in the scanner's own layout (UsbGame::saveGameIni)."""
    lines = [
        "[Game]",
        "Cached_cover_path=",
        "Cached_snap_path=",
        "Favorite=0",
        "Lightgun=0",
        "Play_using_ra=false",
        "Thumbnail_record_name=",
        "Automation=0",
        "Discs=" + disc_name,
        "Highres=0",
        "Imagetype=0",
        "Memcard=SONY",
        "Players=%d" % entry["players"],
        "Publisher=" + entry.get("publisher", entry["author"]),
        "Region=",
        "Serial=",
        "Title=" + entry["title"],
        "Year=%d" % entry["year"],
    ]
    return "\n".join(lines) + "\n"


def stage_file(spec, cache, dest):
    """One manifest file into dest: downloaded, checked, a zip member taken out when 'member' says so."""
    src = fetch(spec["url"], cache)
    got = sha256_of(src)
    if got != spec["sha256"]:
        sys.exit("sha256 mismatch for %s: %s (manifest says %s)" % (spec["url"], got, spec["sha256"]))
    os.makedirs(os.path.dirname(dest), exist_ok=True)
    if spec.get("member"):
        with zipfile.ZipFile(src) as z, z.open(spec["member"]) as m, open(dest, "wb") as f:
            shutil.copyfileobj(m, f)
    else:
        shutil.copyfile(src, dest)


def stage_psx(entry, cache, stage):
    folder = os.path.join(stage, "Games", entry["title"])
    os.makedirs(folder, exist_ok=True)
    cue_name = None
    for spec in entry["files"]:
        dest = os.path.join(folder, spec["name"])
        stage_file(spec, cache, dest)
        if spec["name"].lower().endswith(".cue"):
            cue_name = spec["name"]
            if spec.get("rewrite_cue"):
                # the cue names the upstream bin; the files here are named after the game
                bins = [s["name"] for s in entry["files"] if s["name"].lower().endswith((".bin", ".img"))]
                with open(dest, encoding="utf-8", errors="replace") as f:
                    text = f.read()
                out = []
                for line in text.splitlines():
                    if line.strip().upper().startswith("FILE ") and bins:
                        line = 'FILE "%s" BINARY' % bins[0]
                    out.append(line)
                with open(dest, "w", encoding="utf-8", newline="\n") as f:
                    f.write("\n".join(out) + "\n")
    if not cue_name:
        sys.exit("%s: a PS1 sample needs a .cue" % entry["id"])
    disc = cue_name[:-4]
    with open(os.path.join(folder, "Game.ini"), "w", encoding="utf-8", newline="\n") as f:
        f.write(game_ini(entry, disc))
    shutil.copyfile(os.path.join(SAMPLES_DIR, "covers", entry["cover"]), os.path.join(folder, disc + ".png"))
    return "Games/%s/" % entry["title"]


def stage_rom(entry, cache, stage):
    db = SYSTEM_DB[entry["system"]]
    roms = os.path.join(stage, "RetroArch", "roms", db)
    art = os.path.join(stage, "RetroArch", "thumbnails", db, "Named_Boxarts")
    os.makedirs(roms, exist_ok=True)
    os.makedirs(art, exist_ok=True)
    for spec in entry["files"]:
        stage_file(spec, cache, os.path.join(roms, spec["name"]))
    label = os.path.splitext(entry["files"][0]["name"])[0]
    shutil.copyfile(os.path.join(SAMPLES_DIR, "covers", entry["cover"]),
                    os.path.join(art, escape_name(label) + ".png"))
    return "RetroArch/roms/%s/%s" % (db, entry["files"][0]["name"])


def samples_md(games, date):
    out = ["# Sample games", "",
           "The games AutoBleem's installer put here so the shelf is not empty on the first start (pack of %s)."
           % date,
           "Every one is homebrew whose licence allows redistribution; the licence text and the source are at the"
           " links. Delete any you do not want - the scan takes them off the shelf.", ""]
    for g in games:
        out.append("## %s (%s)" % (g["title"], {"psx": "PlayStation", "nes": "NES", "snes": "Super NES",
                                                "md": "Mega Drive"}[g["system"]]))
        out.append("")
        out.append("%s  " % g["description"])
        out.append("By %s, %d. Licence: %s (%s). Source: %s  " % (g["author"], g["year"], g["licence"],
                                                                     g["licence_url"], g["source"]))
        out.append("Installed as `%s`" % g["installed"])
        for spec in g["files"]:
            out.append("- %s" % spec["url"])
        out.append("")
    return "\n".join(out)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--out", default=os.path.join(ROOT, "build_samples"))
    ap.add_argument("--cache", default=None, help="where downloads are kept (default: <out>/cache)")
    ap.add_argument("--date", default=datetime.date.today().strftime("%Y%m%d"))
    args = ap.parse_args()
    cache = args.cache or os.path.join(args.out, "cache")
    with open(os.path.join(SAMPLES_DIR, "samples.json"), encoding="utf-8") as f:
        games = json.load(f)["games"]

    stage = tempfile.mkdtemp(prefix="samples-", dir=args.out if os.path.isdir(args.out) else None)
    os.makedirs(args.out, exist_ok=True)
    listing = []
    try:
        for g in games:
            print("%s (%s)" % (g["title"], g["system"]))
            installed = stage_psx(g, cache, stage) if g["system"] == "psx" else stage_rom(g, cache, stage)
            g = dict(g, installed=installed)
            listing.append({k: g[k] for k in ("id", "system", "title", "author", "year", "players",
                                              "licence", "licence_url", "source", "description", "installed")})
        with open(os.path.join(stage, "SAMPLES.md"), "w", encoding="utf-8", newline="\n") as f:
            f.write(samples_md([dict(g, installed=l["installed"]) for g, l in zip(games, listing)], args.date))

        name = "samples-%s" % args.date
        tarball = os.path.join(args.out, name + ".tar.gz")
        with tarfile.open(tarball, "w:gz") as tar:
            for top in sorted(os.listdir(stage)):
                tar.add(os.path.join(stage, top), arcname=top)
        total = sum(os.path.getsize(os.path.join(d, f)) for d, _, fs in os.walk(stage) for f in fs)
        meta = {"date": args.date, "file": os.path.basename(tarball), "sha256": sha256_of(tarball),
                "size": os.path.getsize(tarball), "unpacked_size": total,
                "systems": sorted({g["system"] for g in games}), "games": listing}
        with open(os.path.join(args.out, name + ".json"), "w", encoding="utf-8", newline="\n") as f:
            json.dump(meta, f, indent=2)
            f.write("\n")
        print("==> %s (%.1f MB, %d games, %.1f MB unpacked)" % (tarball, meta["size"] / 1e6, len(games),
                                                                 total / 1e6))
    finally:
        shutil.rmtree(stage, ignore_errors=True)


if __name__ == "__main__":
    main()
