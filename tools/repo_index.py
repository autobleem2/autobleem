#!/usr/bin/env python3
"""Regenerate the machine-readable files and the landing page of the download repository.

Run on the server over the repository directory after every publish (tools/repo_publish.sh does it):

    repo_index.py /home/claude/autobleem-repo --base-url https://autobleem.retromenele.pl

Reads what is there (docs/repo-server-plan.md has the layout) and writes:

    releases/<tag>/release.json        the packages of that release: name, size, sha256, url
    releases/latest.json               the newest stable release's release.json
    releases/unstable.json             the one pre-release kept, same shape
    rpi/retroarch/latest.json          the newest RetroArch build per architecture
    rpi/cores/latest.json              the newest cores tarball per architecture (rpi/cores/<arch>/)
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
INDEX_VERSION = 7

# the five release packages, by the name they carry (tools/make_*_package.sh, ci/build.sh)
PACKAGE_KINDS = [
    ("psc", re.compile(r"^autobleem-psc-.*\.zip$"), "PlayStation Classic (USB stick zip)"),
    ("rpi", re.compile(r"^autobleem-rpi(-armhf)?(-v.*)?\.tar\.gz$"), "Raspberry Pi, 32-bit OS (tarball + install.sh)"),
    ("rpi64", re.compile(r"^autobleem-rpi-arm64.*\.tar\.gz$"), "Raspberry Pi, 64-bit OS (tarball + install.sh)"),
    ("win", re.compile(r"^autobleem-win-.*\.zip$"), "Windows (launcher, for a look on a PC)"),
    ("updateroms", re.compile(r"^UpdateRoms-.*\.zip$"), "UpdateRoms for Windows (scan a stick or card on a PC)"),
]
IMAGE_RE = re.compile(r"^autobleem-(?P<version>.+)-rpi-(?P<arch>armhf|arm64)\.img\.xz$")
RETROARCH_RE = re.compile(r"^retroarch-(?P<tag>v[0-9][^-]*)-(?P<arch>armhf|arm64)\.tar\.gz$")
CORES_RE = re.compile(r"^cores-(?P<arch>armhf|arm64)-(?P<date>[0-9]{8})\.tar\.gz$")


def version_key(tag):
    """v2.0.0-pre0-933bd2f -> sortable; a tag with a suffix sorts before the same version without one."""
    m = re.match(r"^v?(\d+)\.(\d+)(?:\.(\d+))?(?:-(.*))?$", tag)
    if not m:
        return (0, 0, 0, 0, tag)
    major, minor, patch, suffix = m.groups()
    return (int(major), int(minor), int(patch or 0), 0 if suffix else 1, suffix or "")


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
ul,ol{line-height:1.55;padding-left:1.4rem}li{margin:.3rem 0}
footer{color:var(--dim);font-size:.8rem;text-align:center;margin-top:2rem}
"""


def render_index(base_url, releases, builds, cores, images, dbs):
    e = html.escape

    def row(label, f, cls="dl"):
        return "<tr><td>%s</td><td><a class=\"%s\" href=\"%s\">%s</a></td><td class=\"size\">%s</td></tr>" % (
            e(label), cls, e(f["url"]), e(f["name"]), human(f["size"]))

    out = []
    out.append("<!DOCTYPE html><html lang=\"en\"><head><meta charset=\"utf-8\">")
    out.append("<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">")
    out.append("<title>AutoBleem downloads</title><link rel=\"icon\" href=\"/assets/icon.png\">")
    out.append("<style>%s</style></head><body>" % PAGE_CSS)
    out.append("<div class=\"hero\"></div><main>")
    out.append("<div class=\"panel\"><h1>Downloads</h1>"
               "<p>Packages, Raspberry Pi images and build inputs for "
               "<a href=\"https://github.com/autobleem/AutoBleem2\">AutoBleem</a>, the game launcher for the "
               "PlayStation Classic and the Raspberry Pi. Every file has a <code>.sha256</code> next to it; "
               "<a href=\"/releases/\">browse</a> the tree for older versions. Machine-readable: "
               "<a href=\"/releases/latest.json\">releases/latest.json</a>.</p></div>")

    # the latest stable release and, in its own panel, the one pre-release that is kept
    stable = [r for r in releases if not r["prerelease"]]
    pre = [r for r in releases if r["prerelease"]]
    if not stable and not pre:
        out.append("<div class=\"panel\"><h2>Releases</h2><p>Nothing published yet.</p></div>")
    for title, which, note in (("Latest release", stable, ""),
                               ("Pre-release", pre, " &middot; a development build, not a release")):
        if not which:
            continue
        latest = which[-1]
        out.append("<div class=\"panel\"><h2>%s <small>%s &middot; %s%s</small></h2>" % (
            title, e(latest["version"]), e(latest["date"]), note))
        out.append("<table><tr><th>Target</th><th>File</th><th></th></tr>")
        for kind, _, kind_title in PACKAGE_KINDS:
            f = latest["files"].get(kind)
            if f:
                out.append(row(kind_title, f))
        out.append("</table>")
        if len(which) > 1:
            out.append("<p class=\"older\">Older: %s</p>" % ", ".join(
                "<a href=\"/releases/%s/\">%s</a>" % (e(r["version"]), e(r["version"])) for r in reversed(which[:-1])))
        out.append("</div>")

    # the newest stable image set, else the one pre-release set (the owner wants that one on the page)
    if images:
        stable_images = {v: f for v, f in images.items() if not is_prerelease(v)}
        newest = newest_of(stable_images) or newest_of(images)
        out.append("<div class=\"panel\"><h2>Raspberry Pi images <small>%s%s</small></h2>" % (
            e(newest), " (pre-release)" if is_prerelease(newest) else ""))
        out.append("<p>Flash with <a href=\"https://www.raspberrypi.com/software/\">Raspberry Pi Imager</a>: "
                   "<em>Use custom</em> with a downloaded file, or add this repository under "
                   "<em>App Options &rarr; Content Repository</em>: <code>%s/rpi-imager/os_list.json</code>. "
                   "The first boot finishes the install (a network connection is needed). "
                   "<a href=\"/rpi-install.html\">Which image for which Pi, and the whole setup, step by step</a>."
                   "</p>" % e(base_url))
        out.append("<table><tr><th>OS</th><th>File</th><th></th></tr>")
        for arch, title in (("armhf", "32-bit Raspberry Pi OS (Pi 2/3/4/400/Zero 2)"),
                            ("arm64", "64-bit Raspberry Pi OS (Pi 3/4/5/400/Zero 2)")):
            f = images[newest].get(arch)
            if f:
                out.append(row(title, f))
        out.append("</table></div>")

    if builds:
        newest = newest_of(builds)
        out.append("<div class=\"panel\"><h2>RetroArch for the Pi installer <small>%s</small></h2>" % e(newest))
        out.append("<p>What <code>install.sh --retroarch prebuilt</code> downloads instead of building from source "
                   "(<a href=\"/rpi/retroarch/latest.json\">latest.json</a>).</p><table>")
        for arch in ("armhf", "arm64"):
            f = builds[newest].get(arch)
            if f:
                out.append(row(arch, f))
        out.append("</table></div>")

    if cores:
        out.append("<div class=\"panel\"><h2>RetroArch cores for the Pi installer</h2>"
                   "<p>Every core libretro's buildbot has for the architecture, with the info, assets, autoconfig, "
                   "database, cheats, overlays and shaders bundles - one download instead of ~130 "
                   "(<a href=\"/rpi/cores/latest.json\">latest.json</a>).</p><table>")
        for arch in ("armhf", "arm64"):
            f = cores.get(arch)
            if f:
                out.append(row("%s, %s-%s-%s" % (arch, f["date"][:4], f["date"][4:6], f["date"][6:]), f))
        out.append("</table></div>")

    if dbs:
        out.append("<div class=\"panel\"><h2>Cover databases</h2><p>The launcher's PS1 cover art databases, "
                   "a build input (<a href=\"/db/\">db/</a>).</p><table>")
        for f in dbs:
            out.append(row("", f))
        out.append("</table></div>")

    out.append("<footer>Generated %s UTC &middot; theme: ab2</footer></main></body></html>"
               % datetime.now(timezone.utc).strftime("%Y-%m-%d %H:%M"))
    return "\n".join(out) + "\n"


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
    images = index_images(repo, base_url)
    dbs = index_db(repo, base_url)
    for name, page in (("index.html", render_index(base_url, releases, builds, cores, images, dbs)),
                       ("rpi-install.html", render_rpi_install(base_url, images))):
        tmp = os.path.join(repo, ".%s.tmp" % name)
        with open(tmp, "w", encoding="utf-8") as f:
            f.write(page)
        os.replace(tmp, os.path.join(repo, name))
    print("%s: %d releases, %d RetroArch builds, %d cores tarballs, %d image sets, %d databases" % (
        repo, len(releases), len(builds), len(cores), len(images), len(dbs)))


if __name__ == "__main__":
    main()
