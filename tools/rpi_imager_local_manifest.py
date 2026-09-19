#!/usr/bin/env python3
"""Write a Raspberry Pi Imager manifest for locally built AutoBleem images.

Raspberry Pi Imager offers its OS customisation screen (hostname, user, WiFi, SSH, locale) only for images
it has metadata for. A local .img.xz picked through "Use custom" has none, so the screen never appears -
unless Imager is pointed at a manifest that describes the file: the same JSON its online catalogue uses,
with file:// URLs. That is what this writes, the way Imager's own doc/local_json/create_local_json.py does
for the official images (a *.rpi-imager-manifest file; double-click it, or App Options -> Content
Repository -> Use custom file, or `rpi-imager --repo <file>`).

The size/hash fields come from the rpi_imager_repo.json that tools/make_rpi_image.sh wrote next to the
image (--repo). Without it, or for an architecture it has no real values for, they are computed here from
the image file itself - the extract_* pair needs the whole .xz decompressed once, about a minute.

    python tools/rpi_imager_local_manifest.py --repo build_rpi_image/rpi_imager_repo.json \\
        --arm64 build_rpi_image/autobleem-rpi-image-arm64.img.xz \\
        --armhf build_rpi_image/autobleem-rpi-image-armhf.img.xz \\
        -o build_rpi_image/os_list_local.rpi-imager-manifest
"""
import argparse
import hashlib
import json
import lzma
import os
import pathlib
import sys

ENTRY_NAMES = {"armhf": "AutoBleem (32-bit)", "arm64": "AutoBleem (64-bit)"}
CHUNK = 1024 * 1024


def sha256_of_file(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(CHUNK), b""):
            h.update(chunk)
    return h.hexdigest()


def sha256_and_size_of_extracted(path):
    """Decompress the .xz once, streaming, for extract_sha256/extract_size."""
    h = hashlib.sha256()
    size = 0
    with lzma.open(path, "rb") as f:
        for chunk in iter(lambda: f.read(CHUNK), b""):
            h.update(chunk)
            size += len(chunk)
    return h.hexdigest(), size


def is_placeholder(value):
    return isinstance(value, str) and value.startswith("__") and value.endswith("__")


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--repo", help="rpi_imager_repo.json written by tools/make_rpi_image.sh (default: the "
                                    "checked-in tools/rpi_imager_repo.json template, which forces computing "
                                    "every size/hash here)")
    ap.add_argument("--armhf", help="the 32-bit image (.img.xz)")
    ap.add_argument("--arm64", help="the 64-bit image (.img.xz)")
    ap.add_argument("--icon", help="an icon URL or local image file for the entries (optional)")
    ap.add_argument("--no-verify", action="store_true",
                    help="do not check the image's sha256 against the repo JSON (saves a few seconds)")
    ap.add_argument("-o", "--output", default="os_list_local.rpi-imager-manifest")
    args = ap.parse_args()

    images = {arch: getattr(args, arch) for arch in ENTRY_NAMES if getattr(args, arch)}
    if not images:
        ap.error("give at least one of --armhf / --arm64")
    for arch, path in images.items():
        if not os.path.isfile(path):
            ap.error(f"--{arch}: no such file: {path}")

    repo_path = args.repo or os.path.join(os.path.dirname(os.path.abspath(__file__)), "rpi_imager_repo.json")
    with open(repo_path, encoding="utf-8") as f:
        repo = json.load(f)

    icon = None
    if args.icon:
        icon = pathlib.Path(os.path.abspath(args.icon)).as_uri() if os.path.isfile(args.icon) else args.icon

    out_entries = []
    done = set()
    for entry in repo.get("os_list", []):
        arch = next((a for a, n in ENTRY_NAMES.items() if entry.get("name") == n), None)
        if arch is None or arch not in images:
            continue
        done.add(arch)
        path = os.path.abspath(images[arch])
        e = {k: v for k, v in entry.items() if not is_placeholder(v)}
        e["url"] = pathlib.Path(path).as_uri()
        if icon:
            e["icon"] = icon

        download_size = os.path.getsize(path)
        need_download = e.get("image_download_size") != download_size or "image_download_sha256" not in e
        if need_download or not args.no_verify:
            print(f"{arch}: hashing {os.path.basename(path)} ...", file=sys.stderr)
            digest = sha256_of_file(path)
            if not need_download and e.get("image_download_sha256") != digest:
                sys.exit(f"{arch}: {path} does not match the repo JSON's image_download_sha256 - "
                         f"wrong file for this JSON? (--no-verify to write it anyway)")
            e["image_download_size"] = download_size
            e["image_download_sha256"] = digest
        if "extract_sha256" not in e or "extract_size" not in e:
            print(f"{arch}: decompressing {os.path.basename(path)} for extract_size/extract_sha256 "
                  f"(about a minute) ...", file=sys.stderr)
            e["extract_sha256"], e["extract_size"] = sha256_and_size_of_extracted(path)
        out_entries.append(e)

    missing = [a for a in images if a not in done]
    if missing:
        sys.exit(f"no os_list entry named {[ENTRY_NAMES[a] for a in missing]} in {repo_path}")

    manifest = {k: v for k, v in repo.items() if k not in ("//", "os_list")}
    manifest["os_list"] = out_entries
    with open(args.output, "w", encoding="utf-8") as f:
        json.dump(manifest, f, indent=2)
        f.write("\n")
    print(f"wrote {args.output} ({len(out_entries)} image(s))")
    print("Open it in Raspberry Pi Imager: double-click it, or App Options -> Content Repository -> "
          "Use custom file, or `rpi-imager --repo <file>`; the AutoBleem entries then offer OS customisation.")


if __name__ == "__main__":
    main()
