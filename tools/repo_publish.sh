#!/usr/bin/env bash
# Publish files to the download repository (docs/repo-server-plan.md) and regenerate its index.
#
#   tools/repo_publish.sh release v2.0.0 dist/psc/*.zip dist/rpi/*.tar.gz ...   -> releases/v2.0.0/
#   tools/repo_publish.sh image v2.0.0-pre0-933bd2f build_rpi_image/*.img.xz build_rpi_image/rpi_imager_repo.json
#                                                                              -> rpi-imager/images/<version>/
#   tools/repo_publish.sh retroarch v1.22.2 retroarch-v1.22.2-armhf.tar.gz     -> rpi/retroarch/v1.22.2/
#   tools/repo_publish.sh cores armhf build_cores/dist/cores-armhf-*.tar.gz    -> rpi/cores/armhf/
#   tools/repo_publish.sh psc-retroarch v1.22.2-1 dist/release/retroarch-psc-v1.22.2-1.zip dist/release/manifest.json
#                                                                              -> psc/retroarch/v1.22.2-1/ (the retroarch-psc repo's `make publish`)
#   tools/repo_publish.sh db db/covers*.db                                     -> db/
#   tools/repo_publish.sh assets                                               -> assets/ (tools/repo_assets.py)
#   tools/repo_publish.sh index                                                just regenerate the index
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

usage() { sed -n '2,20p' "${BASH_SOURCE[0]}" | sed 's/^# \{0,1\}//'; exit "${1:-0}"; }

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
    psc-retroarch) [ $# -ge 2 ] || usage 1; VERSION="$1"; shift; DEST="psc/retroarch/$VERSION" ;;
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
# the index script travels with every publish, but never backwards: an older copy (the server's rsync'd
# checkout, behind the PC's) must not replace the one the repository already runs - it regenerated the
# page without the manual link once
mkdir -p "$STAGE/.tools"
index_version() { sed -n 's/^INDEX_VERSION = \([0-9]*\).*/\1/p' | head -1; }
mine="$(index_version < "$HERE/repo_index.py")"
if [ "$LOCAL" -eq 1 ]; then
    theirs="$(index_version < "$REPO_DIR/.tools/repo_index.py" 2>/dev/null || true)"
else
    theirs="$(ssh "$REPO_HOST" "cat $REPO_DIR/.tools/repo_index.py 2>/dev/null" | index_version || true)"
fi
if [ "${mine:-0}" -ge "${theirs:-0}" ]; then
    cp "$HERE/repo_index.py" "$STAGE/.tools/"
else
    echo "keeping the repository's repo_index.py (INDEX_VERSION $theirs; this checkout has $mine)" >&2
fi

# the index run on the server (and the image retention)
remote_index() {
    cat <<EOF
set -e
cd "$REPO_DIR"
if [ -f assets/icon.png ]; then mkdir -p rpi-imager && cp assets/icon.png rpi-imager/icon.png; fi
python3 .tools/repo_index.py . --base-url "$AB_REPO_URL"
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
