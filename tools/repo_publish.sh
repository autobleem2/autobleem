#!/usr/bin/env bash
# Publish files to the download repository (CLAUDE.md, "The download repository") and regenerate its index.
#
#   tools/repo_publish.sh release v2.0.0 dist/psc/*.zip dist/rpi/*.tar.gz ...   -> releases/v2.0.0/
#   tools/repo_publish.sh image v2.0.0-pre0-933bd2f build_rpi_image/*.img.xz build_rpi_image/rpi_imager_repo.json
#                                                                              -> rpi-imager/images/<version>/
#   tools/repo_publish.sh retroarch v1.22.2 retroarch-v1.22.2-armhf.tar.gz     -> rpi/retroarch/v1.22.2/
#   tools/repo_publish.sh cores armhf build_cores/dist/cores-armhf-*.tar.gz    -> rpi/cores/armhf/
#   tools/repo_publish.sh pc-image v2.0.0-pre0-933bd2f build_pc_image/out/*.img.xz  -> pc/images/<version>/ (the PC stick)
#   tools/repo_publish.sh pc-retroarch v1.22.2 retroarch-v1.22.2-i386.tar.gz    -> pc/retroarch/v1.22.2/
#   tools/repo_publish.sh pc-cores i386 build_cores/dist/cores-i386-*.tar.gz    -> pc/cores/i386/
#   tools/repo_publish.sh psc-retroarch v1.22.2-1 dist/release/retroarch-psc-v1.22.2-1.zip dist/release/manifest.json
#                                                                              -> psc/retroarch/v1.22.2-1/ (the retroarch-psc repo's `make publish`)
#   tools/repo_publish.sh psc-cores dist/release/cores-psc-20260920.tar.gz dist/release/cores-psc-20260920.json
#                                                                              -> psc/cores/ (newest date kept)
#   tools/repo_publish.sh psc-libs dist/release/libs-psc-20260920.tar.gz dist/release/libs-psc-20260920.json
#                                                                              -> psc/libs/ (newest date kept)
#   tools/repo_publish.sh psc-apps dist/release/apps-psc-20260920.tar.gz dist/release/apps-psc-20260920.json
#                                                                              -> psc/apps/ (newest date kept; tools/pack_psc_apps.py)
#   tools/repo_publish.sh win-retroarch 1.22.2 build_retroarch/dist/retroarch-win64-1.22.2.tar.gz -> win/retroarch/1.22.2/
#   tools/repo_publish.sh win-cores build_cores/dist/cores-win64-20260920.tar.gz    -> win/cores/ (the Windows product's
#                                                                                      RetroArch, cores and BIOS list -
#                                                                                      AutoBleemWinSetup reads them)
#   tools/repo_publish.sh win-bios src/win/biospack-win64.txt                       -> win/bios/
#   tools/repo_publish.sh psc-bios payload/RetroArch/bios/biospack.txt          -> psc/bios/ (the BIOS list the installer
#                                                                                 fetches RetroBIOS's files by; the list only)
#   tools/repo_publish.sh samples build_samples/samples-20260920.tar.gz build_samples/samples-20260920.json
#                                                                              -> samples/ (newest date kept)
#   tools/repo_publish.sh pcsx r26-20-gb9801962 ../pcsx-abnxt/dist/packages/*   -> emu/pcsx-abnxt/<version>/ (the pcsx-abnxt
#                                                                                 repository's tools/make_packages.sh; newest kept)
#   tools/repo_publish.sh pcsx-ab 20260920-fc8c992 ../pcsx-ab2/dist/packages/*  -> emu/pcsx-ab/<version>/ (the same, the classic emulator)
#   tools/repo_publish.sh manuals build_manuals/*/*.pdf                        -> manuals/ (the user manuals, one PDF
#                                                                                 per language - tools/build_manuals.py)
#   tools/repo_publish.sh db db/covers*.db                                     -> db/
#   tools/repo_publish.sh assets                                               -> assets/ (tools/repo_assets.py)
#   tools/repo_publish.sh index                                                just regenerate the index
#
# The page generator travels with every publish, three-way merged with the repository's copy (see
# tools/repo_index_merge.py) - never copied over it.
#
# The files go over ssh (rsync to $REPO_HOST, "psc-build" in ~/.ssh/config, into $REPO_DIR) with a .sha256
# next to each; then tools/repo_index.py runs on the server over the whole tree (it is uploaded as
# <repo>/.tools/repo_index.py so the server needs no checkout). --local skips ssh and copies within this
# machine - what a CI job on the server does, with $REPO_DIR bind-mounted.
#
# AB_REPO_URL is what the generated urls start with (default: the domain; http://212.71.244.78:9090 is the
# same tree without TLS). Retention is the index script's: a pre-release replaces the previous pre-release
# (releases and image sets), only the newest RetroArch build (Pi and PSC alike) and cores tarball are kept, stable
# releases stay.
set -euo pipefail

REPO_HOST="${REPO_HOST:-psc-build}"
REPO_DIR="${REPO_DIR:-/home/claude/autobleem-repo}"
AB_REPO_URL="${AB_REPO_URL:-https://autobleem.retromenele.pl}"
LOCAL=0
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

usage() { sed -n '2,23p' "${BASH_SOURCE[0]}" | sed 's/^# \{0,1\}//'; exit "${1:-0}"; }

while [ $# -gt 0 ]; do
    case "$1" in
        --local) LOCAL=1; shift ;;
        -h|--help) usage ;;
        *) break ;;
    esac
done
[ $# -ge 1 ] || usage 1
KIND="$1"; shift

# where the files of this kind land, relative to the repository root
case "$KIND" in
    release)   [ $# -ge 2 ] || usage 1; VERSION="$1"; shift; DEST="releases/$VERSION" ;;
    image)     [ $# -ge 2 ] || usage 1; VERSION="$1"; shift; DEST="rpi-imager/images/$VERSION" ;;
    retroarch) [ $# -ge 2 ] || usage 1; VERSION="$1"; shift; DEST="rpi/retroarch/$VERSION" ;;
    cores)     [ $# -ge 2 ] || usage 1; VERSION="$1"; shift; DEST="rpi/cores/$VERSION" ;;
    pc-image)  [ $# -ge 2 ] || usage 1; VERSION="$1"; shift; DEST="pc/images/$VERSION" ;;
    pc-retroarch) [ $# -ge 2 ] || usage 1; VERSION="$1"; shift; DEST="pc/retroarch/$VERSION" ;;
    pc-cores)  [ $# -ge 2 ] || usage 1; VERSION="$1"; shift; DEST="pc/cores/$VERSION" ;;
    psc-retroarch) [ $# -ge 2 ] || usage 1; VERSION="$1"; shift; DEST="psc/retroarch/$VERSION" ;;
    psc-cores) [ $# -ge 1 ] || usage 1; DEST="psc/cores" ;;
    psc-libs)  [ $# -ge 1 ] || usage 1; DEST="psc/libs" ;;
    psc-apps)  [ $# -ge 1 ] || usage 1; DEST="psc/apps" ;;
    psc-bios)  [ $# -ge 1 ] || usage 1; DEST="psc/bios" ;;
    win-retroarch) [ $# -ge 2 ] || usage 1; VERSION="$1"; shift; DEST="win/retroarch/$VERSION" ;;
    win-cores) [ $# -ge 1 ] || usage 1; DEST="win/cores" ;;
    win-bios)  [ $# -ge 1 ] || usage 1; DEST="win/bios" ;;
    samples)   [ $# -ge 1 ] || usage 1; DEST="samples" ;;
    pcsx)      [ $# -ge 2 ] || usage 1; VERSION="$1"; shift; DEST="emu/pcsx-abnxt/$VERSION" ;;
    pcsx-ab)   [ $# -ge 2 ] || usage 1; VERSION="$1"; shift; DEST="emu/pcsx-ab/$VERSION" ;;
    manuals)   [ $# -ge 1 ] || usage 1; DEST="manuals" ;;
    db)        [ $# -ge 1 ] || usage 1; DEST="db" ;;
    assets)    DEST="assets" ;;
    index)     DEST="" ;;
    *)         echo "unknown kind: $KIND" >&2; usage 1 ;;
esac

# a scratch dir with the files to upload and their sidecars, so one rsync does it
STAGE="$(mktemp -d)"
trap 'rm -rf "$STAGE" "$STAGE.assets"' EXIT

if [ "$KIND" = assets ]; then
    python3 "$HERE/repo_assets.py" "$STAGE.assets"
    set -- "$STAGE.assets"/*
fi

if [ -n "$DEST" ]; then
    mkdir -p "$STAGE/$DEST"
    for f in "$@"; do
        [ -f "$f" ] || { echo "not a file: $f" >&2; exit 1; }
        cp "$f" "$STAGE/$DEST/"
        case "$f" in
            *.json|*.txt|*.sha256|*.ttf) ;;
            *) (cd "$STAGE/$DEST" && sha256sum "$(basename "$f")" > "$(basename "$f").sha256") ;;
        esac
    done
    echo "publishing to $DEST: $(cd "$STAGE/$DEST" && ls | grep -v '\.sha256$' | tr '\n' ' ')"
fi
# the index script travels with every publish, merged three-way with the copy the repository runs
# (tools/repo_index_merge.py): two checkouts publishing in turn no longer overwrite each other's page
# generator, and a real conflict stops the publish before anything is copied
mkdir -p "$STAGE/.tools" "$STAGE/.merge"
if [ "$LOCAL" -eq 1 ]; then
    for f in repo_index.py repo_index.base.py repo_index.rev; do
        [ -f "$REPO_DIR/.tools/$f" ] && cp "$REPO_DIR/.tools/$f" "$STAGE/.merge/$f"
    done
else
    ssh "$REPO_HOST" "cd $REPO_DIR/.tools 2>/dev/null && tar cf - repo_index.py repo_index.base.py repo_index.rev 2>/dev/null || true"         | tar xf - -C "$STAGE/.merge" 2>/dev/null || true
fi
if ! python3 "$HERE/repo_index_merge.py" --mine "$HERE/repo_index.py"         --theirs "$STAGE/.merge/repo_index.py" --theirs-base "$STAGE/.merge/repo_index.base.py"         --theirs-rev "$STAGE/.merge/repo_index.rev"         --out "$STAGE/.tools/repo_index.py" --out-base "$STAGE/.tools/repo_index.base.py"         --out-rev "$STAGE/.tools/repo_index.rev"; then
    KEEP="$(mktemp -d "${TMPDIR:-/tmp}/repo_index_conflict.XXXXXX")"
    cp "$STAGE/.tools/repo_index.py" "$KEEP/repo_index.py" 2>/dev/null || true
    cp "$STAGE/.merge/repo_index.py" "$KEEP/repo_index.repository.py" 2>/dev/null || true
    echo "not published: the merged copy with the conflict markers is $KEEP/repo_index.py (the repository's own copy next to it)" >&2
    exit 1
fi
rm -rf "$STAGE/.merge"

# the index run on the server (and the image retention)
remote_index() {
    cat <<EOF
set -e
cd "$REPO_DIR"
if [ -f assets/icon.png ]; then mkdir -p rpi-imager && cp assets/icon.png rpi-imager/icon.png; fi
if ! python3 .tools/repo_index.py . --base-url "$AB_REPO_URL"; then
    if [ -f .tools/repo_index.prev.py ]; then
        echo "the merged repo_index.py failed - the previous copy is restored and run" >&2
        cp .tools/repo_index.prev.py .tools/repo_index.py
        python3 .tools/repo_index.py . --base-url "$AB_REPO_URL"
    fi
    exit 1
fi
cp .tools/repo_index.py .tools/repo_index.prev.py
EOF
}

if [ "$LOCAL" -eq 1 ]; then
    mkdir -p "$REPO_DIR"
    cp -r "$STAGE"/. "$REPO_DIR"/
    bash -c "$(remote_index)"
else
    rsync -rlt --chmod=Du=rwx,Dgo=rx,Fu=rw,Fgo=r "$STAGE"/ "$REPO_HOST:$REPO_DIR/"
    ssh "$REPO_HOST" "$(remote_index)"
fi
echo "done: $AB_REPO_URL/${DEST:+$DEST/}"
