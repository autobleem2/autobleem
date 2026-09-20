#!/usr/bin/env python3
"""Regenerate the machine-readable files and the landing page of the download repository.

Run on the server over the repository directory after every publish (tools/repo_publish.sh does it):

    repo_index.py /home/claude/autobleem-repo --base-url https://autobleem.retromenele.pl

Reads what is there (CLAUDE.md, "The download repository", has the layout) and writes:

    releases/<tag>/release.json        the packages of that release: name, size, sha256, url
    releases/latest.json               the newest stable release's release.json
    releases/unstable.json             the one pre-release kept, same shape
    rpi/retroarch/latest.json          the newest RetroArch build per architecture
    psc/retroarch/latest.json          the newest RetroArch build for the PlayStation Classic (psc/retroarch/<tag>/)
    psc/cores/latest.json              the newest cores tarball for the console (psc/cores/cores-psc-<date>.tar.gz)
    psc/libs/latest.json               the newest runtime-library pack for the console's apps (psc/libs/libs-psc-<date>.tar.gz)
    psc/apps/latest.json               the newest pack of the console's third-party Apps (psc/apps/apps-psc-<date>.tar.gz)
    psc/bios/latest.json               the console's BIOS list (psc/bios/biospack.txt: what the installer fetches from
                                       RetroBIOS into RetroArch/bios - only the list is here, never a BIOS file)
    rpi/cores/latest.json              the newest cores tarball per architecture (rpi/cores/<arch>/)
    samples/latest.json                the newest sample-games pack (samples/samples-<date>.tar.gz, tools/build_samples.py)
    rpi-imager/os_list.json            the newest images' Imager metadata with real urls (from the
                                       rpi_imager_repo.json make_rpi_image.sh wrote next to them)
    index.html                         the landing page
    rpi-install.html                   the Raspberry Pi manual: which image for which Pi, the setup, games

Every file's sha256 comes from its `<name>.sha256` sidecar (sha256sum format) when there is one, else it
is computed and the sidecar written.

Retention (the owner's rule, 2026-09-19): pre-release builds are not kept - a new one replaces the previous
one, in releases/ and in rpi-imager/images/ alike. The page shows the latest stable release and that one
pre-release (its own panel, marked as a development build; unstable.json for machines), and the newest
image set even when it is a pre-release. Of the RetroArch builds only the newest version is kept.
"""

import argparse
import hashlib
import html
import json
import os
import re
import shutil
import sys
from datetime import datetime, timezone

# bump on every change: tools/repo_publish.sh only replaces the copy the repository runs with a newer one
INDEX_VERSION = 18

# the release packages, by the name they carry (tools/make_*_package.sh, ci/build.sh)
PACKAGE_KINDS = [
    ("psc", re.compile(r"^autobleem-psc-.*\.zip$"), "PlayStation Classic (USB stick zip)"),
    ("psc-fs", re.compile(r"^autobleem-psc-.*\.tar\.gz$"),
     "PlayStation Classic, the stick's file system for the installer (no RetroArch - added from the packs below)"),
    ("rpi", re.compile(r"^autobleem-rpi(-armhf)?(-v.*)?\.tar\.gz$"), "Raspberry Pi, 32-bit OS (tarball + install.sh)"),
    ("rpi64", re.compile(r"^autobleem-rpi-arm64.*\.tar\.gz$"), "Raspberry Pi, 64-bit OS (tarball + install.sh)"),
    ("win", re.compile(r"^autobleem-win-.*\.zip$"), "Windows (launcher, for a look on a PC)"),
    ("updateroms", re.compile(r"^UpdateRoms-.*\.zip$"), "UpdateRoms for Windows (scan a stick or card on a PC)"),
]
IMAGE_RE = re.compile(r"^autobleem-(?P<version>.+)-rpi-(?P<arch>armhf|arm64)\.img\.xz$")
RETROARCH_RE = re.compile(r"^retroarch-(?P<tag>v[0-9][^-]*)-(?P<arch>armhf|arm64)\.tar\.gz$")
CORES_RE = re.compile(r"^cores-(?P<arch>armhf|arm64)-(?P<date>[0-9]{8})\.tar\.gz$")
# the console build's tag is the RetroArch version plus a build number (github.com/autobleem/retroarch-psc)
PSC_RETROARCH_RE = re.compile(r"^retroarch-psc-(?P<tag>v[0-9][0-9.]*-[0-9]+)\.zip$")
PSC_CORES_RE = re.compile(r"^cores-psc-(?P<date>[0-9]{8})\.tar\.gz$")
PSC_LIBS_RE = re.compile(r"^libs-psc-(?P<date>[0-9]{8})\.tar\.gz$")
PSC_APPS_RE = re.compile(r"^apps-psc-(?P<date>[0-9]{8})\.tar\.gz$")
SAMPLES_RE = re.compile(r"^samples-(?P<date>[0-9]{8})\.tar\.gz$")


# version folder -> its mtime, filled in as the tree is read: two builds of the same pre-release label
# (v2.0.0-pre0-933bd2f, v2.0.0-pre0-1ba1e84) differ only by a commit hash, which has no order - the one
# published later is the newer one
PUBLISHED_AT = {}


def version_key(tag):
    """v2.0.0-pre0-933bd2f -> sortable; a tag with a suffix sorts before the same version without one;
    a trailing commit hash is ignored and the publish time decides instead."""
    m = re.match(r"^v?(\d+)\.(\d+)(?:\.(\d+))?(?:-(.*))?$", tag)
    if not m:
        return (0, 0, 0, 0, tag, PUBLISHED_AT.get(tag, 0))
    major, minor, patch, suffix = m.groups()
    label = re.sub(r"-[0-9a-f]{7,40}$", "", suffix or "")
    return (int(major), int(minor), int(patch or 0), 0 if suffix else 1, label, PUBLISHED_AT.get(tag, 0))


def note_published(folder):
    PUBLISHED_AT[os.path.basename(folder)] = os.path.getmtime(folder)


def is_prerelease(tag):
    return bool(re.search(r"-(pre|rc|alpha|beta)", tag))


def sha256_of(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def sidecar_sha256(path):
    """The hash from <path>.sha256 (sha256sum format), computed and written when missing."""
    sidecar = path + ".sha256"
    if os.path.isfile(sidecar):
        with open(sidecar, encoding="utf-8") as f:
            first = f.readline().split()
        if first and re.fullmatch(r"[0-9a-f]{64}", first[0]):
            return first[0]
    digest = sha256_of(path)
    with open(sidecar, "w", encoding="utf-8") as f:
        f.write("%s  %s\n" % (digest, os.path.basename(path)))
    return digest


def file_entry(repo, base_url, path):
    rel = os.path.relpath(path, repo).replace(os.sep, "/")
    return {
        "name": os.path.basename(path),
        "size": os.path.getsize(path),
        "sha256": sidecar_sha256(path),
        "url": base_url + "/" + rel,
    }


def data_files(directory):
    """The regular files of a directory that are not sidecars, json or hidden, sorted."""
    if not os.path.isdir(directory):
        return []
    names = sorted(os.listdir(directory))
    return [
        os.path.join(directory, n)
        for n in names
        if os.path.isfile(os.path.join(directory, n))
        and not n.startswith(".")
        and not n.endswith(".sha256")
        and not n.endswith(".json")
        and n != "SHA256SUMS"
    ]


def write_json(path, data):
    with open(path, "w", encoding="utf-8") as f:
        json.dump(data, f, indent=2)
        f.write("\n")


def prune(folders, keep, what):
    """Delete the folders not in `keep` (a list of version folders), saying which."""
    for folder in folders:
        if folder not in keep and os.path.isdir(folder):
            print("pruning %s %s" % (what, os.path.basename(folder)))
            shutil.rmtree(folder)


def newest_of(versions):
    return sorted(versions, key=version_key)[-1] if versions else None


def human(size):
    for unit in ("B", "KB", "MB", "GB"):
        if size < 1024 or unit == "GB":
            return "%.0f %s" % (size, unit) if unit == "B" else "%.1f %s" % (size, unit)
        size /= 1024.0
    return "%d" % size


#*******************************
# releases
#*******************************
def index_releases(repo, base_url):
    root = os.path.join(repo, "releases")
    releases = []
    if os.path.isdir(root):
        for tag in os.listdir(root):
            if os.path.isdir(os.path.join(root, tag)):
                note_published(os.path.join(root, tag))
        for tag in sorted(os.listdir(root), key=version_key):
            folder = os.path.join(root, tag)
            if not os.path.isdir(folder) or not tag.startswith("v"):
                continue
            files = {}
            others = []
            for path in data_files(folder):
                entry = file_entry(repo, base_url, path)
                for kind, pattern, _ in PACKAGE_KINDS:
                    if pattern.match(entry["name"]) and kind not in files:
                        files[kind] = entry
                        break
                else:
                    others.append(entry)
            if not files and not others:
                continue
            with open(os.path.join(folder, "SHA256SUMS"), "w", encoding="utf-8") as f:
                for entry in list(files.values()) + others:
                    f.write("%s  %s\n" % (entry["sha256"], entry["name"]))
            release = {
                "version": tag,
                "prerelease": is_prerelease(tag),
                "date": datetime.fromtimestamp(os.path.getmtime(folder), timezone.utc).strftime("%Y-%m-%d"),
                "files": files,
                "other_files": others,
            }
            write_json(os.path.join(folder, "release.json"), release)
            releases.append(release)
    # one pre-release at most: the newest; every stable release stays
    stable = [r for r in releases if not r["prerelease"]]
    pre = [r for r in releases if r["prerelease"]]
    if len(pre) > 1:
        prune([os.path.join(root, r["version"]) for r in pre[:-1]], [], "pre-release")
        pre = pre[-1:]
    for name, which in (("latest.json", stable), ("unstable.json", pre)):
        path = os.path.join(root, name)
        if which:
            write_json(path, which[-1])
        elif os.path.isfile(path):
            os.remove(path)
    return stable + pre


#*******************************
# RetroArch builds
#*******************************
def index_retroarch(repo, base_url):
    root = os.path.join(repo, "rpi", "retroarch")
    builds = {}  # tag -> arch -> entry
    if os.path.isdir(root):
        for tag in os.listdir(root):
            folder = os.path.join(root, tag)
            if not os.path.isdir(folder):
                continue
            note_published(folder)
            for path in data_files(folder):
                m = RETROARCH_RE.match(os.path.basename(path))
                if m:
                    builds.setdefault(tag, {})[m.group("arch")] = file_entry(repo, base_url, path)
    if builds:
        newest = newest_of(builds)
        prune([os.path.join(root, t) for t in builds if t != newest], [], "RetroArch build")
        builds = {newest: builds[newest]}
        latest = {"version": newest}
        latest.update(builds[newest])
        write_json(os.path.join(root, "latest.json"), latest)
    return builds


#*******************************
# RetroArch for the PlayStation Classic
#*******************************
def psc_version_key(tag):
    """v1.22.2-3 -> ((1, 22, 2), 3): the RetroArch version, then our build number."""
    m = re.match(r"^v?(\d+)\.(\d+)\.(\d+)-(\d+)$", tag)
    if not m:
        return ((0, 0, 0), 0, tag)
    return (tuple(int(x) for x in m.groups()[:3]), int(m.group(4)), tag)


def index_psc_retroarch(repo, base_url):
    """psc/retroarch/<tag>/retroarch-psc-<tag>.zip + manifest.json (from the retroarch-psc repository's
    `make package-retroarch`) - the newest kept, the rest deleted, latest.json = the newest."""
    root = os.path.join(repo, "psc", "retroarch")
    builds = {}  # tag -> {"zip": entry, "manifest": url}
    if os.path.isdir(root):
        for tag in os.listdir(root):
            folder = os.path.join(root, tag)
            if not os.path.isdir(folder):
                continue
            for path in data_files(folder):
                m = PSC_RETROARCH_RE.match(os.path.basename(path))
                if m and m.group("tag") == tag:
                    builds[tag] = {"zip": file_entry(repo, base_url, path)}
                    manifest = os.path.join(folder, "manifest.json")
                    if os.path.isfile(manifest):
                        builds[tag]["manifest"] = base_url + "/psc/retroarch/%s/manifest.json" % tag
    if builds:
        newest = sorted(builds, key=psc_version_key)[-1]
        prune([os.path.join(root, t) for t in builds if t != newest], [], "PSC RetroArch build")
        builds = {newest: builds[newest]}
        latest = {"version": newest}
        latest.update(builds[newest])
        write_json(os.path.join(root, "latest.json"), latest)
    return builds


def index_psc_dated(repo, base_url, kind, pattern, what):
    """psc/<kind>/<kind>-psc-<date>.tar.gz (+ <kind>-psc-<date>.json, the list inside) - the newest kept."""
    root = os.path.join(repo, "psc", kind)
    dated = {}
    for path in data_files(root):
        m = pattern.match(os.path.basename(path))
        if m:
            dated[m.group("date")] = path
    if not dated:
        return None
    newest = max(dated)
    for date, path in dated.items():
        if date != newest:
            print("pruning PSC %s %s" % (what, os.path.basename(path)))
            for f in (path, path + ".sha256", os.path.join(root, "%s-psc-%s.json" % (kind, date))):
                if os.path.isfile(f):
                    os.remove(f)
    entry = file_entry(repo, base_url, dated[newest])
    entry["date"] = newest
    manifest = os.path.join(root, "%s-psc-%s.json" % (kind, newest))
    if os.path.isfile(manifest):
        entry["manifest"] = base_url + "/psc/%s/%s-psc-%s.json" % (kind, kind, newest)
        try:
            with open(manifest, encoding="utf-8") as f:
                entry["count"] = json.load(f).get("count")
        except (OSError, ValueError):
            pass
    write_json(os.path.join(root, "latest.json"), entry)
    return entry


def index_psc_cores(repo, base_url):
    """psc/cores/: RetroBoot 1.2's cores as retroarch-psc's tools/pack_retroboot_cores.py packs them."""
    return index_psc_dated(repo, base_url, "cores", PSC_CORES_RE, "cores tarball")


def index_psc_libs(repo, base_url):
    """psc/libs/: the runtime libraries RetroBoot's apps and AutoBleem's Apps need beyond the firmware
    (retroarch-psc's tools/pack_retroboot_libs.py)."""
    return index_psc_dated(repo, base_url, "libs", PSC_LIBS_RE, "library pack")


def index_psc_bios(repo, base_url):
    """psc/bios/biospack.txt - the console's BIOS manifest (tools/biospack.py --arch psc): one line per file,
    <sha256> <size> <url> <path>, the URLs pointing at RetroBIOS. latest.json says how many files and bytes
    the installer would fetch, and which RetroBIOS commit the list is from."""
    path = os.path.join(repo, "psc", "bios", "biospack.txt")
    if not os.path.isfile(path):
        return None
    entry = file_entry(repo, base_url, path)
    count = total = 0
    with open(path, encoding="utf-8") as f:
        for line in f:
            if line.startswith("# Built by tools/biospack.py"):
                m = re.search(r"at ([0-9a-f]{7,40}),", line)
                if m:
                    entry["retrobios_ref"] = m.group(1)
            if line.startswith("#") or not line.strip():
                continue
            parts = line.rstrip("\n").split(" ", 3)
            if len(parts) == 4:
                count += 1
                total += int(parts[1])
    entry["count"] = count
    entry["total_bytes"] = total
    write_json(os.path.join(repo, "psc", "bios", "latest.json"), entry)
    return entry


def index_psc_apps(repo, base_url):
    """psc/apps/: the console's third-party Apps (amiberry, doom, eduke32, ...) as tools/pack_psc_apps.py
    packs them - Apps/<name>/ folders, self-contained, laid out for the stick."""
    return index_psc_dated(repo, base_url, "apps", PSC_APPS_RE, "apps pack")


#*******************************
# cores tarballs
#*******************************
def index_cores(repo, base_url):
    """rpi/cores/<arch>/cores-<arch>-<date>.tar.gz - the newest per architecture, the rest deleted."""
    root = os.path.join(repo, "rpi", "cores")
    latest = {}
    for arch in ("armhf", "arm64"):
        folder = os.path.join(root, arch)
        dated = {}
        for path in data_files(folder):
            m = CORES_RE.match(os.path.basename(path))
            if m and m.group("arch") == arch:
                dated[m.group("date")] = path
        if not dated:
            continue
        newest = max(dated)
        for date, path in dated.items():
            if date != newest:
                print("pruning cores tarball %s" % os.path.basename(path))
                os.remove(path)
                if os.path.isfile(path + ".sha256"):
                    os.remove(path + ".sha256")
        entry = file_entry(repo, base_url, dated[newest])
        entry["date"] = newest
        latest[arch] = entry
    if latest:
        write_json(os.path.join(root, "latest.json"), latest)
    return latest


#*******************************
# Raspberry Pi images
#*******************************
def index_images(repo, base_url):
    root = os.path.join(repo, "rpi-imager", "images")
    versions = {}  # version -> {arch: entry}
    if os.path.isdir(root):
        for version in os.listdir(root):
            folder = os.path.join(root, version)
            if not os.path.isdir(folder):
                continue
            note_published(folder)
            for path in data_files(folder):
                m = IMAGE_RE.match(os.path.basename(path))
                if m:
                    versions.setdefault(version, {})[m.group("arch")] = file_entry(repo, base_url, path)
    if not versions:
        return versions
    # one pre-release image set at most, the newest; Imager gets the newest stable set, else that one
    pre = [v for v in versions if is_prerelease(v)]
    stable = [v for v in versions if not is_prerelease(v)]
    if len(pre) > 1:
        keep = newest_of(pre)
        prune([os.path.join(root, v) for v in pre if v != keep], [], "pre-release image set")
        versions = {v: f for v, f in versions.items() if v == keep or v in stable}
    newest = newest_of(stable) or newest_of(versions)
    # make_rpi_image.sh's rpi_imager_repo.json for that version, with the placeholders filled in
    template = os.path.join(root, newest, "rpi_imager_repo.json")
    if os.path.isfile(template):
        with open(template, encoding="utf-8") as f:
            os_list = json.load(f)
        icon = base_url + "/rpi-imager/icon.png"
        for entry in os_list.get("os_list", []):
            # which image this entry is: by the download hash the template already carries, else by the
            # architecture named in its url placeholder (__ARMHF_IMAGE_URL__) or name
            match = None
            for arch, file in versions[newest].items():
                if entry.get("image_download_sha256") == file["sha256"]:
                    match = file
            if match is None:
                hint = (entry.get("url", "") + " " + entry.get("name", "")).lower()
                for arch, file in versions[newest].items():
                    if arch in hint or (arch == "armhf" and "32-bit" in hint) or (arch == "arm64" and "64-bit" in hint):
                        match = file
            if match is not None:
                entry["url"] = match["url"]
                entry["image_download_sha256"] = match["sha256"]
                entry["image_download_size"] = match["size"]
            if os.path.isfile(os.path.join(repo, "rpi-imager", "icon.png")):
                entry["icon"] = icon
        write_json(os.path.join(repo, "rpi-imager", "os_list.json"), os_list)
    return versions


#*******************************
# sample games
#*******************************
def index_samples(repo, base_url):
    """samples/samples-<date>.tar.gz (+ samples-<date>.json, what is inside - tools/build_samples.py) - the
    newest kept; latest.json is what payload_rpi/install.sh reads (url, sha256, date, the games)."""
    root = os.path.join(repo, "samples")
    dated = {}
    for path in data_files(root):
        m = SAMPLES_RE.match(os.path.basename(path))
        if m:
            dated[m.group("date")] = path
    if not dated:
        return None
    newest = max(dated)
    for date, path in dated.items():
        if date != newest:
            print("pruning sample pack %s" % os.path.basename(path))
            for f in (path, path + ".sha256", os.path.join(root, "samples-%s.json" % date)):
                if os.path.isfile(f):
                    os.remove(f)
    entry = file_entry(repo, base_url, dated[newest])
    entry["date"] = newest
    manifest = os.path.join(root, "samples-%s.json" % newest)
    if os.path.isfile(manifest):
        entry["manifest"] = base_url + "/samples/samples-%s.json" % newest
        try:
            with open(manifest, encoding="utf-8") as f:
                entry["games"] = json.load(f).get("games", [])
        except (OSError, ValueError):
            pass
    write_json(os.path.join(root, "latest.json"), entry)
    return entry


#*******************************
# cover databases
#*******************************
def index_db(repo, base_url):
    return [file_entry(repo, base_url, p) for p in data_files(os.path.join(repo, "db"))]


#*******************************
# the landing page
#*******************************
# Styled after the ab2 theme: its background (the logo is painted into it) as the hero, its navy/cyan
# palette, its Selawik Light font - all under /assets, staged by tools/repo_assets.py.
PAGE_CSS = """
@font-face{font-family:Selawik;src:url(/assets/selawik-light.ttf) format('truetype');font-weight:300;font-display:swap}
:root{--navy:#061a3a;--panel:rgba(4,22,56,.78);--line:rgba(80,200,255,.35);--cyan:#4fc8ff;--ink:#e8f2ff;--dim:#9fb8d6}
*{box-sizing:border-box}
body{margin:0;font-family:Selawik,"Segoe UI",system-ui,sans-serif;font-weight:300;color:var(--ink);
  background:var(--navy) radial-gradient(ellipse at 50% 0,#0b3a7a 0,#071f47 45%,#040f26 100%) fixed}
.hero{position:relative;height:min(46vw,590px);background:url(/assets/hero.jpg) center 30%/cover no-repeat}
.hero:after{content:"";position:absolute;inset:0;background:linear-gradient(to bottom,rgba(6,26,58,0) 75%,var(--navy) 100%)}
main{max-width:64rem;margin:-1.5rem auto 3rem;padding:0 1rem;position:relative}
.panel{background:var(--panel);border:1px solid var(--line);border-radius:6px;padding:1.2rem 1.4rem;margin:1.2rem 0;
  box-shadow:0 0 24px rgba(0,120,220,.15),inset 0 0 0 1px rgba(255,255,255,.03)}
h1{font-weight:300;font-size:1.9rem;letter-spacing:.06em;text-transform:uppercase;margin:0 0 .4rem;color:#fff}
h2{font-weight:300;font-size:1.35rem;letter-spacing:.05em;text-transform:uppercase;margin:0 0 .8rem;color:var(--cyan)}
h2 small,h1 small{font-size:.7em;color:var(--dim);letter-spacing:0;text-transform:none;margin-left:.6rem}
p{line-height:1.5;margin:.4rem 0 .8rem}
ul.what{line-height:1.5;margin:.3rem 0 .9rem;padding-left:1.2rem}ul.what li{margin:.25rem 0}
ul.what b{color:var(--ink);font-weight:600}
a{color:var(--cyan);text-decoration:none}a:hover{color:#fff;text-decoration:underline}
table{border-collapse:collapse;width:100%}
td,th{text-align:left;padding:.45rem .6rem;border-bottom:1px solid rgba(80,200,255,.14);vertical-align:top}
th{font-weight:300;color:var(--dim);font-size:.85rem;letter-spacing:.06em;text-transform:uppercase}
tr:last-child td{border-bottom:0}
td.size{white-space:nowrap;color:var(--dim);text-align:right}
a.dl{display:inline-block;padding:.25rem .7rem;border:1px solid var(--line);border-radius:4px;background:rgba(79,200,255,.08)}
a.dl:hover{background:rgba(79,200,255,.2);text-decoration:none}
code{font-family:ui-monospace,Consolas,monospace;font-size:.9em;color:#fff;background:rgba(255,255,255,.07);padding:.05em .35em;border-radius:3px}
.older{color:var(--dim);font-size:.9rem}
h2.plat{margin:2.4rem 0 .2rem;padding-bottom:.3rem;border-bottom:1px solid var(--line);color:#fff;font-size:1.6rem}
h3{font-weight:300;font-size:1rem;letter-spacing:.05em;text-transform:uppercase;margin:1rem 0 .4rem;color:var(--dim)}
.panel.inputs{background:rgba(4,22,56,.5);border-style:dashed}
.panel.inputs h2{color:var(--dim)}
h3 small{letter-spacing:0;text-transform:none}
nav.tabs{display:flex;flex-wrap:wrap;gap:.4rem;margin:1.4rem 0 .2rem}
nav.tabs a{padding:.5rem 1rem;border:1px solid var(--line);border-bottom:0;border-radius:6px 6px 0 0;
  background:rgba(4,22,56,.5);color:var(--dim);font-size:1rem;letter-spacing:.05em;text-transform:uppercase}
nav.tabs a:hover{color:#fff;text-decoration:none}
nav.tabs a.active{background:var(--panel);color:var(--cyan);border-color:var(--cyan)}
body.js section.tab{display:none}
body.js section.tab.active{display:block}
body.js section.tab h2.plat{display:none}
ul,ol{line-height:1.55;padding-left:1.4rem}li{margin:.3rem 0}
footer{color:var(--dim);font-size:.8rem;text-align:center;margin-top:2rem}
"""


def render_index(base_url, releases, builds, cores, images, dbs, psc_builds, psc_cores, samples=None, psc_libs=None,
                 psc_apps=None, psc_bios=None):
    """The page: a section per platform, each with what a user installs from (the image, the package)
    and, under it, the build inputs - what the installer, the image build or the CI fetch: RetroArch
    builds, cores, the Pi tarball with install.sh, the cover databases."""
    e = html.escape

    def row(label, f, cls="dl"):
        return "<tr><td>%s</td><td><a class=\"%s\" href=\"%s\">%s</a></td><td class=\"size\">%s</td></tr>" % (
            e(label), cls, e(f["url"]), e(f["name"]), human(f["size"]))

    def date_of(d):
        return "%s-%s-%s" % (d[:4], d[4:6], d[6:])

    def table(rows, head=("", "File", "")):
        return "<table><tr>%s</tr>%s</table>" % ("".join("<th>%s</th>" % h for h in head), "".join(rows))

    stable = [r for r in releases if not r["prerelease"]]
    pre = [r for r in releases if r["prerelease"]]

    def release_rows(kinds, which):
        """The packages of the given kinds from a release, labelled with their kind."""
        return [row(kind_title, which["files"][kind]) for kind, _, kind_title in PACKAGE_KINDS
                if kind in kinds and kind in which["files"]]

    def release_block(kinds):
        """The latest stable release's packages of these kinds, then the one pre-release's; [] if none."""
        out = []
        if stable and release_rows(kinds, stable[-1]):
            out.append("<h3>Release <small>%s &middot; %s</small></h3>" % (e(stable[-1]["version"]), e(stable[-1]["date"])))
            out.append(table(release_rows(kinds, stable[-1])))
        if pre and release_rows(kinds, pre[-1]):
            out.append("<h3>Pre-release <small>%s &middot; %s &middot; a development build, not a release</small></h3>"
                       % (e(pre[-1]["version"]), e(pre[-1]["date"])))
            out.append(table(release_rows(kinds, pre[-1])))
        return out

    def older():
        if len(stable) > 1:
            return ["<p class=\"older\">Older releases: %s</p>" % ", ".join(
                "<a href=\"/releases/%s/\">%s</a>" % (e(r["version"]), e(r["version"])) for r in reversed(stable[:-1]))]
        return []

    out = []
    out.append("<!DOCTYPE html><html lang=\"en\"><head><meta charset=\"utf-8\">")
    out.append("<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">")
    out.append("<title>AutoBleem downloads</title><link rel=\"icon\" href=\"/assets/icon.png\">")
    out.append("<style>%s</style></head><body>" % PAGE_CSS)
    out.append("<div class=\"hero\"></div><main>")
    out.append("<div class=\"panel\"><h1>Downloads</h1>"
               "<p><a href=\"https://github.com/autobleem/AutoBleem2\">AutoBleem</a>, the game launcher for the "
               "PlayStation Classic and the Raspberry Pi.</p>"
               "<p>Each platform's tab has two parts:</p>"
               "<ul class=\"what\">"
               "<li><b>Install</b> - what you install from: the console's USB stick package, the Pi images, the "
               "Windows programs.</li>"
               "<li><b>Build inputs</b> - the pieces the installers, the image build and the CI fetch from here: "
               "RetroArch builds, cores, libraries, apps, the cover databases.</li>"
               "</ul>"
               "<p>Every file has a <code>.sha256</code> next to it. <a href=\"/releases/\">Browse</a> the tree for "
               "older versions; for machines there is <a href=\"/releases/latest.json\">releases/latest.json</a>.</p>"
               "</div>")

    # ---- PlayStation Classic ----
    out.append("<h2 class=\"plat\" id=\"psc\">PlayStation Classic</h2>")
    out.append("<div class=\"panel\"><h2>Install</h2>"
               "<p>The USB stick package.</p>"
               "<p>Unzip it onto the root of a FAT32 stick named <code>SONY</code>, then boot the console with the "
               "stick in the second controller port.</p>")
    block = release_block(("psc",))
    out += block if block else ["<p>Nothing published yet.</p>"]
    out += older()
    out.append("</div>")
    rows = []
    if psc_builds:
        newest = sorted(psc_builds, key=psc_version_key)[-1]
        b = psc_builds[newest]
        rows.append(row("RetroArch %s%s" % (newest, " (manifest)" if b.get("manifest") else ""), b["zip"]))
    if psc_cores:
        rows.append(row("RetroArch cores, %s%s" % (date_of(psc_cores["date"]),
                                                    ", %d cores" % psc_cores["count"] if psc_cores.get("count") else ""), psc_cores))
    if psc_libs:
        rows.append(row("Runtime libraries for the apps, %s%s" % (date_of(psc_libs["date"]),
                                                                   ", %d libraries" % psc_libs["count"] if psc_libs.get("count") else ""), psc_libs))
    if psc_apps:
        rows.append(row("Apps, %s%s" % (date_of(psc_apps["date"]),
                                        ", %d apps" % psc_apps["count"] if psc_apps.get("count") else ""), psc_apps))
    if psc_bios:
        rows.append(row("BIOS list: %d files, %d MB, fetched from RetroBIOS by the installer"
                        % (psc_bios["count"], psc_bios["total_bytes"] // (1024 * 1024)), psc_bios))
    if rows or release_block(("psc-fs",)):
        def links(latest, manifest, what="the list"):
            out = "<a href=\"%s\">latest.json</a>" % latest
            if manifest:
                out += ", <a href=\"%s\">%s</a>" % (e(manifest), what)
            return " (" + out + ")"
        out.append("<div class=\"panel inputs\"><h2>Build inputs</h2>"
                   "<p>What the PC installer lays out on a stick, piece by piece:</p>"
                   "<ul class=\"what\">"
                   "<li><b>The stick's file system</b> - the launcher, pcsx-ab, the scripts, the themes, the console "
                   "tools. One tarball per release, without RetroArch.</li>"
                   "<li><b>RetroArch</b> (<code>RetroArch/bin</code>) - built for the console's firmware (glibc 2.24, "
                   "Wayland, GLES, ALSA, udev) with the PSC patches; it loads xz-compressed cores as they are%s.</li>"
                   "<li><b>Cores</b> - with their info files. RetroBoot 1.2's set for now: the ones that run on a "
                   "stock console%s.</li>"
                   "<li><b>Runtime libraries</b> (<code>Autobleem/lib</code>) - what the Apps need beyond the "
                   "firmware: SDL2_image/mixer/ttf, freetype, png, vorbis; and the xpad kernel module for Xbox "
                   "pads%s.</li>"
                   "<li><b>Apps</b> (<code>Apps/</code>) - Amiberry, Doom, Duke Nukem 3D, OpenBOR, Tyrian, Prince of "
                   "Persia, Shadow Warrior, Wolfenstein 3D, each self-contained%s.</li>"
                   "<li><b>BIOS list</b> (<code>RetroArch/bios</code>) - the installer fetches the BIOS files from "
                   "RetroBIOS by this list, file by file. No BIOS file is on this site%s.</li>"
                   "</ul>"
                   % (links("/psc/retroarch/latest.json", b.get("manifest") if psc_builds else None, "manifest.json"),
                      links("/psc/cores/latest.json", psc_cores.get("manifest") if psc_cores else None),
                      links("/psc/libs/latest.json", psc_libs.get("manifest") if psc_libs else None),
                      links("/psc/apps/latest.json", psc_apps.get("manifest") if psc_apps else None),
                      links("/psc/bios/latest.json", None)))
        out += release_block(("psc-fs",))
        if rows:
            out.append(table(rows))
        out.append("</div>")

    # ---- Raspberry Pi ----
    out.append("<h2 class=\"plat\" id=\"rpi\">Raspberry Pi</h2>")
    out.append("<div class=\"panel\"><h2>Install</h2>")
    if images:
        stable_images = {v: f for v, f in images.items() if not is_prerelease(v)}
        newest = newest_of(stable_images) or newest_of(images)
        out.append("<p>Flash an image with <a href=\"https://www.raspberrypi.com/software/\">Raspberry Pi Imager</a>, "
                   "one of two ways:</p>"
                   "<ul class=\"what\">"
                   "<li><b>Use custom</b> - with an image downloaded from here.</li>"
                   "<li><b>This repository</b> - add it under <em>App Options &rarr; Content Repository</em>: "
                   "<code>%s/rpi-imager/os_list.json</code>. The images then appear in Imager's own list.</li>"
                   "</ul>"
                   "<p>The first boot finishes the install; a network connection is needed for it.</p>"
                   "<p><a href=\"/rpi-install.html\">Which image for which Pi, and the whole setup, step by step.</a></p>"
                   % e(base_url))
        out.append("<h3>Images <small>%s%s</small></h3>" % (e(newest), " &middot; pre-release" if is_prerelease(newest) else ""))
        rows = []
        for arch, title in (("armhf", "32-bit Raspberry Pi OS (Pi 2/3/4/400/Zero 2)"),
                            ("arm64", "64-bit Raspberry Pi OS (Pi 3/4/5/400/Zero 2)")):
            f = images[newest].get(arch)
            if f:
                rows.append(row(title, f))
        out.append(table(rows, ("OS", "File", "")))
    else:
        out.append("<p>No image published yet.</p>")
    out.append("</div>")
    out.append("<div class=\"panel inputs\"><h2>Build inputs</h2>"
               "<p><b>The package</b> - what the image carries and <code>install.sh</code> installs from. It is also "
               "the way onto a Pi already running Raspberry Pi OS Lite: unpack it and run the script.</p>"
               "<p>What the installer downloads:</p>"
               "<ul class=\"what\">"
               "<li><b>RetroArch</b>, prebuilt for each architecture (<a href=\"/rpi/retroarch/latest.json\">latest.json</a>). "
               "<code>--retroarch prebuilt</code> takes it instead of building from source.</li>"
               "<li><b>Cores</b> - every core libretro's buildbot has for the architecture, with the info, assets, "
               "autoconfig, database, cheats, overlays and shaders bundles: one download instead of about 130 "
               "(<a href=\"/rpi/cores/latest.json\">latest.json</a>).</li>"
               "</ul>")
    block = release_block(("rpi", "rpi64"))
    out += block
    rows = []
    if builds:
        newest = newest_of(builds)
        for arch in ("armhf", "arm64"):
            f = builds[newest].get(arch)
            if f:
                rows.append(row("RetroArch %s, %s" % (newest, arch), f))
    if cores:
        for arch in ("armhf", "arm64"):
            f = cores.get(arch)
            if f:
                rows.append(row("RetroArch cores, %s, %s" % (arch, date_of(f["date"])), f))
    if rows:
        out.append("<h3>RetroArch</h3>" + table(rows))
    if not block and not rows:
        out.append("<p>Nothing published yet.</p>")
    out.append("</div>")

    # ---- PC ----
    out.append("<h2 class=\"plat\" id=\"pc\">PC</h2>")
    out.append("<div class=\"panel\"><h2>Install</h2>"
               "<p>Two Windows programs:</p>"
               "<ul class=\"what\">"
               "<li><b>The launcher</b> - AutoBleem itself, for a look on a PC.</li>"
               "<li><b>UpdateRoms</b> - prepares a console stick or a Pi card in a card reader: the playlists, "
               "names from RetroArch's databases, box art.</li>"
               "</ul>")
    block = release_block(("win", "updateroms"))
    out += block if block else ["<p>Nothing published yet.</p>"]
    out.append("</div>")

    # ---- shared build inputs ----
    if dbs or samples:
        out.append("<h2 class=\"plat\" id=\"inputs\">Every platform</h2>")
    if dbs:
        out.append("<div class=\"panel inputs\"><h2>Build inputs</h2>"
                   "<p><b>The cover art databases</b> - the launcher's PS1 covers, by region (<a href=\"/db/\">db/</a>).</p>"
                   "<p>Baked into the console and Windows packages; the Pi installer fetches them.</p>")
        out.append(table([row("", f) for f in dbs]) + "</div>")
    if samples:
        games = samples.get("games") or []
        names = {"psx": "PlayStation", "nes": "NES", "snes": "Super NES", "md": "Mega Drive"}
        out.append("<div class=\"panel inputs\"><h2>Sample games</h2>"
                   "<p>What the Pi installer puts on the shelf, so the first start is not an empty one: homebrew "
                   "whose licence allows redistribution, in one small pack "
                   "(<a href=\"/samples/\">samples/</a>, <a href=\"/samples/latest.json\">latest.json</a>%s).</p>"
                   "<p>Unpack it onto a console stick or a Pi card as it is - the games sit where the launcher looks.</p>"
                   % (", <a href=\"%s\">the list</a>" % e(samples["manifest"]) if samples.get("manifest") else ""))
        rows = [row("Sample pack, %s%s" % (date_of(samples["date"]), ", %d games" % len(games) if games else ""), samples)]
        out.append(table(rows))
        if games:
            out.append("<table><tr><th>Game</th><th>System</th><th>By</th><th>Licence</th></tr>")
            for g in games:
                out.append("<tr><td><a href=\"%s\">%s</a></td><td>%s</td><td>%s</td><td><a href=\"%s\">%s</a></td></tr>"
                           % (e(g.get("source", "")), e(g.get("title", "")), e(names.get(g.get("system"), g.get("system", ""))),
                              e(g.get("author", "")), e(g.get("licence_url", "")), e(g.get("licence", ""))))
            out.append("</table>")
        out.append("</div>")

    out = tabbed(out)
    out.append("<footer>Generated %s UTC &middot; theme: ab2</footer></main></body></html>"
               % datetime.now(timezone.utc).strftime("%Y-%m-%d %H:%M"))
    return "\n".join(out) + "\n"


def tabbed(out):
    """The page's platform sections - everything from each <h2 class="plat" id=...> to the next - as tabs:
    a tab bar after the intro panel, one <section class="tab"> per platform, a few lines of script that
    show the one named in the URL's #hash (the first otherwise) and keep the hash in step. Without script
    every section is shown in turn, headings and all, as before."""
    html_text = "\n".join(out)
    parts = re.split(r'<h2 class="plat" id="([a-z]+)">([^<]+)</h2>', html_text)
    if len(parts) < 3:
        return out
    head, rest = parts[0], parts[1:]
    tabs = [(rest[i], rest[i + 1], rest[i + 2]) for i in range(0, len(rest), 3)]
    bar = "<nav class=\"tabs\" role=\"tablist\">" + "".join(
        "<a href=\"#%s\" data-tab=\"%s\" role=\"tab\">%s</a>" % (tid, tid, title) for tid, title, _ in tabs) + "</nav>"
    sections = "".join(
        "<section class=\"tab\" id=\"%s\"><h2 class=\"plat\">%s</h2>%s</section>" % (tid, title, body)
        for tid, title, body in tabs)
    script = """<script>
(function(){
  var tabs=document.querySelectorAll('nav.tabs a'), secs=document.querySelectorAll('section.tab');
  if(!tabs.length) return;
  document.body.classList.add('js');
  function show(id){
    var found=false;
    secs.forEach(function(s){ var on=(s.id===id); s.classList.toggle('active',on); if(on) found=true; });
    if(!found){ show(secs[0].id); return; }
    tabs.forEach(function(a){ a.classList.toggle('active', a.dataset.tab===id); });
  }
  tabs.forEach(function(a){ a.addEventListener('click', function(ev){
    ev.preventDefault(); history.replaceState(null,'','#'+a.dataset.tab); show(a.dataset.tab); }); });
  window.addEventListener('hashchange', function(){ show(location.hash.slice(1)); });
  show(location.hash.slice(1));
})();
</script>"""
    return [head, bar, sections, script]


#*******************************
# the Raspberry Pi manual page
#*******************************
RPI_MODELS = [
    # model, 32-bit image, 64-bit image, note
    ("Raspberry Pi 5", "yes", "yes", ""),
    ("Raspberry Pi 4 Model B", "yes", "yes", ""),
    ("Raspberry Pi 400", "yes", "yes", "the test machine - both images are tried here"),
    ("Raspberry Pi 3 Model B / B+ / A+", "yes", "yes", "1 GB of RAM (512 MB on the A+): fine for the launcher and PS1"),
    ("Raspberry Pi Zero 2 W", "yes", "yes", "512 MB of RAM; PS1 runs, the heavier RetroArch cores do not"),
    ("Raspberry Pi 2 Model B", "yes", "v1.2 only", "v1.1 has a 32-bit-only CPU; slow for anything 3D"),
    ("Compute Module 3 / 4 / 5", "yes", "yes", "with a carrier board that has HDMI and USB"),
    ("Raspberry Pi 1, Zero, Zero W", "no", "no", "ARMv6 - neither image runs on these"),
]


def render_rpi_install(base_url, images):
    e = html.escape
    newest = newest_of(images) if images else None
    out = []
    out.append("<!DOCTYPE html><html lang=\"en\"><head><meta charset=\"utf-8\">")
    out.append("<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">")
    out.append("<title>AutoBleem on a Raspberry Pi</title><link rel=\"icon\" href=\"/assets/icon.png\">")
    out.append("<style>%s</style></head><body>" % PAGE_CSS)
    out.append("<div class=\"hero\"></div><main>")
    out.append("<div class=\"panel\"><h1>AutoBleem on a Raspberry Pi</h1>"
               "<p>AutoBleem turns a Raspberry Pi into a PlayStation Classic-style console: it boots straight into "
               "the game carousel, plays PlayStation games with its own emulator, and - with RetroArch - the "
               "other systems too. The image is Raspberry Pi OS Lite with AutoBleem's setup added; the first boot "
               "finishes the installation by itself. <a href=\"/\">&larr; Downloads</a></p></div>")

    out.append("<div class=\"panel\"><h2>Which image</h2>"
               "<p>Two images, one per flavour of Raspberry Pi OS. <strong>The 32-bit image is the one to take</strong> "
               "unless you have a reason not to: AutoBleem's PlayStation emulator has its fast ARM dynamic "
               "recompiler only on 32-bit, and every Pi from the 2 up runs it. The 64-bit image runs the "
               "same launcher with a slower (interpreted) PlayStation emulator, but RetroArch has about twice "
               "as many cores built for 64-bit ARM - pick it for the other systems on a Pi 4, 400 or 5.</p>"
               "<table><tr><th>Model</th><th>32-bit image</th><th>64-bit image</th><th>Notes</th></tr>")
    for model, b32, b64, note in RPI_MODELS:
        out.append("<tr><td>%s</td><td>%s</td><td>%s</td><td>%s</td></tr>" % (e(model), e(b32), e(b64), e(note)))
    out.append("</table>")
    if newest:
        out.append("<p style=\"margin-top:1rem\">")
        for arch, title in (("armhf", "32-bit image"), ("arm64", "64-bit image")):
            f = images[newest].get(arch)
            if f:
                out.append("<a class=\"dl\" href=\"%s\">%s (%s)</a> " % (e(f["url"]), title, human(f["size"])))
        out.append("</p>")
    out.append("</div>")

    out.append("<div class=\"panel\"><h2>You need</h2><ul>"
               "<li>A Raspberry Pi from the table, its power supply, and an HDMI screen.</li>"
               "<li>A microSD card of <strong>16 GB or more</strong> - 32 GB and up if you want room for games "
               "(the system takes 8 GB, the rest becomes the games partition).</li>"
               "<li>A USB or Bluetooth gamepad - a DualShock 4, an Xbox pad, an 8BitDo, any pad the Pi sees as a game "
               "controller. The launcher is driven with the pad; a keyboard is only for the first boot.</li>"
               "<li><strong>Internet on the first boot</strong> (Ethernet or WiFi): the setup downloads RetroArch, "
               "its cores and the BIOS files, about a gigabyte in all.</li>"
               "<li><a href=\"https://www.raspberrypi.com/software/\">Raspberry Pi Imager</a> on a PC or Mac.</li>"
               "</ul></div>")

    out.append("<div class=\"panel\"><h2>Flashing the card</h2><ol>"
               "<li>In Imager, choose your Raspberry Pi model, then under <em>Operating System</em> pick "
               "<em>Use custom</em> and the downloaded <code>.img.xz</code> - or first add this site under "
               "<em>App Options &rarr; Content Repository</em> (<code>%s/rpi-imager/os_list.json</code>) and pick "
               "AutoBleem from the list.</li>"
               "<li>Choose the card under <em>Storage</em>.</li>"
               "<li>Say <strong>yes to customisation</strong> when Imager offers it: set a user name and password, "
               "your <strong>WiFi</strong> network and country, and enable <strong>SSH</strong>. With these preset the "
               "first boot needs no keyboard at all. (Skipping is fine too - the first boot then asks for the WiFi "
               "on the screen.)</li>"
               "<li>Write, then put the card in the Pi and power it on.</li></ol></div>" % e(base_url))

    out.append("<div class=\"panel\"><h2>The first boot</h2>"
               "<p>Raspberry Pi OS starts once to apply your presets, then AutoBleem's setup takes the screen:</p><ol>"
               "<li>It waits for the network. Without one it lists the WiFi networks it can see and asks for the "
               "password (or press <code>e</code> after plugging in an Ethernet cable).</li>"
               "<li>It asks whether to install <strong>RetroArch</strong>. <code>Y</code> (or a minute of silence) gives "
               "the full install with about 130 systems; <code>n</code> a lean PlayStation-only AutoBleem. RetroArch "
               "can be added later by running the installer again.</li>"
               "<li>It grows the system partition, creates the <code>AUTOBLEEM</code> games partition on the rest of "
               "the card, downloads RetroArch (ready-built from this site - or, when the site cannot be reached, "
               "builds it, which takes 10-40 minutes), its cores and the BIOS files, and reboots. Everything is "
               "shown on the screen; nothing needs pressing.</li>"
               "<li>The Pi comes up in the game carousel. It is empty until you add games.</li></ol>"
               "<p>If something fails (no network, a download that would not finish), the setup says so, gives the "
               "login prompt back and simply tries again on the next boot.</p></div>")

    out.append("<div class=\"panel\"><h2>Adding games</h2>"
               "<p>The <code>AUTOBLEEM</code> partition is exFAT, so any PC or Mac reads it: put the card in a reader, "
               "or copy over the network with SFTP (WinSCP, FileZilla, <code>scp</code>) to "
               "<code>/media/autobleem/</code> on the Pi once SSH is enabled.</p><ul>"
               "<li><strong>PlayStation games</strong>: drop the game files - <code>.cue</code> + <code>.bin</code>, "
               "<code>.chd</code> or <code>.pbp</code> - straight into <code>Games/</code>; the launcher sorts each game "
               "into its own folder by itself. A multi-disc game is one folder with every disc in it (make that "
               "folder yourself, or name the discs <em>Game (Disc 1)</em>, <em>Game (Disc 2)</em> and the launcher merges "
               "them). Cover art and the title come on their own (the launcher looks the game up and fetches its "
               "cover online); your own <code>&lt;name&gt;.png</code> next to the game wins.</li>"
               "<li><strong>Other systems</strong> (with RetroArch): into <code>RetroArch/roms/&lt;system&gt;/</code> - a "
               "folder per system is already there, named as RetroArch names them ("
               "<em>Nintendo - Nintendo Entertainment System</em>, <em>Sega - Mega Drive - Genesis</em>, ...).</li>"
               "<li><strong>BIOS files</strong>: the setup installs the PlayStation BIOS and RetroArch's system files; "
               "your own go to <code>System/Bios/</code> and <code>RetroArch/system/</code>.</li></ul>"
               "<p>The launcher watches its folders: copy a game in while it runs and it appears in the carousel by "
               "itself, sorted into its folder, cover and all.</p></div>")

    out.append("<div class=\"panel\"><h2>Options and updates</h2>"
               "<p><code>autobleem.txt</code> on the card's small boot partition (readable on any PC) holds the "
               "first-boot options as <code>key=value</code>: the system partition's size, the HDMI mode, the "
               "RetroArch answer, whether to mirror the box art. Edit it before the first boot; comments in the file "
               "explain each key.</p>"
               "<p>To update an installed Pi, download the Raspberry Pi tarball from the <a href=\"/\">downloads</a> "
               "page, unpack it on the Pi and run <code>sudo bash install.sh</code> - it keeps the games partition "
               "and everything on it, and skips what is already installed.</p>"
               "<p>Questions and bug reports: <a href=\"https://github.com/autobleem/AutoBleem2\">github.com/autobleem/AutoBleem2</a>. "
               "The Pi's logs are in <code>System/Logs/</code> on the games partition.</p></div>")

    out.append("<footer>Generated %s UTC &middot; theme: ab2</footer></main></body></html>"
               % datetime.now(timezone.utc).strftime("%Y-%m-%d %H:%M"))
    return "\n".join(out) + "\n"


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("repo", help="the repository directory")
    ap.add_argument("--base-url", default=os.environ.get("AB_REPO_URL", "https://autobleem.retromenele.pl"),
                    help="what the urls in the json files start with (default: $AB_REPO_URL or the server's address)")
    args = ap.parse_args()
    repo = os.path.abspath(args.repo)
    if not os.path.isdir(repo):
        sys.exit("not a directory: %s" % repo)
    base_url = args.base_url.rstrip("/")

    releases = index_releases(repo, base_url)
    builds = index_retroarch(repo, base_url)
    cores = index_cores(repo, base_url)
    psc_builds = index_psc_retroarch(repo, base_url)
    psc_cores = index_psc_cores(repo, base_url)
    psc_libs = index_psc_libs(repo, base_url)
    psc_apps = index_psc_apps(repo, base_url)
    psc_bios = index_psc_bios(repo, base_url)
    images = index_images(repo, base_url)
    dbs = index_db(repo, base_url)
    samples = index_samples(repo, base_url)
    for name, page in (("index.html", render_index(base_url, releases, builds, cores, images, dbs, psc_builds, psc_cores, samples, psc_libs, psc_apps, psc_bios)),
                       ("rpi-install.html", render_rpi_install(base_url, images))):
        tmp = os.path.join(repo, ".%s.tmp" % name)
        with open(tmp, "w", encoding="utf-8") as f:
            f.write(page)
        os.replace(tmp, os.path.join(repo, name))
    print("%s: %d releases, %d RetroArch builds, %d cores tarballs, %d PSC RetroArch builds, %s PSC cores, %d image sets, %d databases, %s sample pack" % (
        repo, len(releases), len(builds), len(cores), len(psc_builds), "1" if psc_cores else "0", len(images), len(dbs),
        "1" if samples else "0"))


if __name__ == "__main__":
    main()
