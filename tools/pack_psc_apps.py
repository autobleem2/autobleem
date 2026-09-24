#!/usr/bin/env python3
"""Pack the console's third-party Apps - what an AutoBleem stick's Apps/ folder carries beyond the two
console tools this repository builds (pscbios, abflashkit).

    python tools/pack_psc_apps.py F:/Apps [--out dist/release] [--date YYYYMMDD]

Each app is a folder with an app.ini (title, author, startup, image, readme - what the launcher's Apps
set shows) and everything its run.sh needs next to it: the binary, its data, its launch script. They
were RetroBoot 1.2's apps (amiberry, crispy-doom, eduke32, openbor, opentyrian, sdlpop, shadowwarrior,
wolf4sdl), made self-contained by tools/install_autobleem.py's layout stage - the parts that lived under
retroarch/apps/<name> moved in, the scripts pointed at /media/Apps/<name>, /media/System/Logs and
Autobleem/rc/app_env.sh (the libraries from the site's libs pack, on the path). The pack is
apps-psc-<date>.tar.gz, laid out as Apps/<name>/..., plus apps-psc-<date>.json listing every app with its
app.ini fields, file count, size and the sha256 of every file; tools/repo_publish.sh psc-apps puts both
on the download repository (psc/apps/, the newest date kept). pscbios and abflashkit are skipped - they
ship with every release.

Only the standard library is needed.
"""
import argparse
import datetime
import hashlib
import io
import json
import os
import sys
import shutil
import tarfile
import zipfile

OURS = {"pscbios", "abflashkit"}  # built here, in every release package


def sha256_of(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def read_ini(path):
    values = {}
    with open(path, encoding="utf-8", errors="replace") as f:
        for line in f:
            line = line.strip()
            if "=" in line and not line.startswith("#"):
                key, value = line.split("=", 1)
                values[key.strip().lower()] = value.strip()
    return values


def pack_one(name, folder, ini, out, platform, date):
    """<out>/<name>-<platform>-<version>.zip (Apps/<name>/... with the Unix modes kept), <name>.item.json -
    the Store's descriptor, the site's index_store() fills in the sums - and <name>.png when the App has a
    picture. The version is app.ini's Version=, else the date."""
    version = ini.get("version") or date
    zip_name = "%s-%s-%s.zip" % (name, platform, version)
    zip_path = os.path.join(out, zip_name)
    with zipfile.ZipFile(zip_path, "w", zipfile.ZIP_DEFLATED) as z:
        for root, dirs, names in os.walk(folder):
            dirs.sort()
            for fn in sorted(names):
                if fn.startswith("._") or fn == ".DS_Store":
                    continue
                path = os.path.join(root, fn)
                rel = os.path.relpath(path, folder).replace(os.sep, "/")
                info = zipfile.ZipInfo("Apps/%s/%s" % (name, rel),
                                       datetime.datetime.fromtimestamp(os.path.getmtime(path)).timetuple()[:6])
                mode = 0o755 if rel.endswith(".sh") or "." not in fn else 0o644
                info.external_attr = (0o100000 | mode) << 16
                info.compress_type = zipfile.ZIP_DEFLATED
                with open(path, "rb") as f:
                    z.writestr(info, f.read())
    item = {"id": "app/%s" % name, "kind": "app", "title": ini.get("title", name), "version": version,
            "author": ini.get("author", ""), "files": [{"name": zip_name}]}
    for key in ("licence", "description"):
        if ini.get(key):
            item[key] = ini[key]
    if platform == "psc":
        item["requires"] = ["pack/psc-libs"]  # the console's Apps take their libraries from the libs pack
    image = os.path.join(folder, ini.get("image", "")) if ini.get("image") else ""
    if image and os.path.isfile(image):
        shutil.copyfile(image, os.path.join(out, name + ".png"))
        item["image"] = name + ".png"
    with open(os.path.join(out, name + ".item.json"), "w", encoding="utf-8") as f:
        json.dump(item, f, indent=2, ensure_ascii=False)
        f.write("\n")
    print("  %-14s %-40s %6.1f MB  %s" % (name, item["title"], os.path.getsize(zip_path) / 1e6, zip_name))


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("apps_dir", help="the stick's Apps folder")
    ap.add_argument("--out", default="dist/release")
    ap.add_argument("--date", default=datetime.datetime.now(datetime.timezone.utc).strftime("%Y%m%d"))
    ap.add_argument("--only", nargs="*", help="pack only these app folders")
    ap.add_argument("--per-app", action="store_true",
                    help="one zip per App for the AutoBleem Store instead of the one pack: <out>/<name>-<platform>-"
                         "<version>.zip laid out as Apps/<name>/..., with <name>.item.json and its picture - what "
                         "`repo_publish.sh store <platform>` takes (the launcher's docs/store-plan.md)")
    ap.add_argument("--platform", default="psc", help="the platform key the per-app packages are for")
    args = ap.parse_args()

    apps = []
    for name in sorted(os.listdir(args.apps_dir)):
        folder = os.path.join(args.apps_dir, name)
        if not os.path.isdir(folder) or name in OURS or (args.only and name not in args.only):
            continue
        ini = os.path.join(folder, "app.ini")
        if not os.path.isfile(ini):
            print("skipping %s: no app.ini" % name, file=sys.stderr)
            continue
        apps.append((name, folder, read_ini(ini)))
    if not apps:
        sys.exit("no apps found under %s" % args.apps_dir)

    os.makedirs(args.out, exist_ok=True)
    if args.per_app:
        for name, folder, ini in apps:
            pack_one(name, folder, ini, args.out, args.platform, args.date)
        return
    tar_name = "apps-psc-%s.tar.gz" % args.date
    tar_path = os.path.join(args.out, tar_name)
    entries = []
    with tarfile.open(tar_path, "w:gz", compresslevel=6) as tar:
        for name, folder, ini in apps:
            files = []
            for root, dirs, names in os.walk(folder):
                dirs.sort()
                for fn in sorted(names):
                    path = os.path.join(root, fn)
                    if fn.startswith("._") or fn == ".DS_Store":
                        continue
                    rel = os.path.relpath(path, folder).replace(os.sep, "/")
                    info = tar.gettarinfo(path, arcname="Apps/%s/%s" % (name, rel))
                    info.uid = info.gid = 0
                    info.uname = info.gname = ""
                    info.mode = 0o755 if rel.endswith(".sh") or "." not in fn else 0o644
                    with open(path, "rb") as f:
                        tar.addfile(info, f)
                    files.append({"path": rel, "size": os.path.getsize(path), "sha256": sha256_of(path)})
            entries.append({"name": name, "title": ini.get("title", name), "author": ini.get("author", ""),
                            "startup": ini.get("startup", "run.sh"), "readme": ini.get("readme", ""),
                            "image": ini.get("image", ""), "file_count": len(files),
                            "size": sum(f["size"] for f in files), "files": files})
            print("  %-14s %-40s %4d files %6.1f MB" % (name, entries[-1]["title"], len(files), entries[-1]["size"] / 1e6))
        manifest = {"schema": 1, "target": "psc", "source": "RetroBoot 1.2's apps, made self-contained",
                    "date": args.date, "layout": "Apps/<name>/ on the stick; run.sh sources Autobleem/rc/app_env.sh "
                    "for the libraries of the psc/libs pack (Autobleem/lib/apps)",
                    "count": len(entries), "apps": entries}
        text = json.dumps(manifest, indent=2, ensure_ascii=False).encode("utf-8")
        ti = tarfile.TarInfo("apps.json")
        ti.size = len(text)
        ti.mtime = int(datetime.datetime.now().timestamp())
        tar.addfile(ti, io.BytesIO(text))
    with open(os.path.join(args.out, "apps-psc-%s.json" % args.date), "w", encoding="utf-8") as f:
        json.dump(manifest, f, indent=2, ensure_ascii=False)
        f.write("\n")
    print("%s: %d apps, %.1f MB" % (tar_path, len(entries), os.path.getsize(tar_path) / 1e6))


if __name__ == "__main__":
    main()
