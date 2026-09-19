#!/usr/bin/env bash
# Build the autobleem-build image (docker/Dockerfile) on a Docker host - the build server, in practice.
#
#   docker/build-image.sh                      -> autobleem-build:latest (and :<git sha>)
#   docker/build-image.sh --target pi          one of the stages: base, native, pi, mingw, db, psc, all
#   docker/build-image.sh --covers DIR         where coversJ.db/coversP.db/coversU.db are (see below)
#   docker/build-image.sh --no-cache           rebuild every layer
#   docker/build-image.sh --build-arg K=V ...  passed through (the versions in the Dockerfile)
#
# The cover databases are baked into the image (the db stage). They are not in git - this stages them into
# docker/db/ (git-ignored) from --covers, else $AB_COVERS_DIR, else the checkout's db/ when those are the
# real files and not tools/make_usb.py's 5 KB stubs, else the build server's own copy.
set -euo pipefail
cd "$(dirname "$0")"

TAG="${AB_BUILD_IMAGE:-autobleem-build}"
TARGET=all
COVERS="${AB_COVERS_DIR:-}"
EXTRA=()
while [ $# -gt 0 ]; do
    case "$1" in
        --target) TARGET="$2"; shift 2 ;;
        --covers) COVERS="$2"; shift 2 ;;
        --tag) TAG="$2"; shift 2 ;;
        --no-cache) EXTRA+=(--no-cache); shift ;;
        --build-arg) EXTRA+=(--build-arg "$2"); shift 2 ;;
        *) echo "unknown option: $1" >&2; exit 2 ;;
    esac
done

real_covers() { # real_covers DIR - the three files, and not stubs
    local f
    for f in coversJ.db coversP.db coversU.db; do
        [ -f "$1/$f" ] && [ "$(stat -c %s "$1/$f")" -gt 1000000 ] || return 1
    done
}
if [ -z "$COVERS" ]; then
    for dir in ../db /AutoBleem/BUILD/data_that_gets_copied/cover_databases; do
        if real_covers "$dir"; then COVERS="$dir"; break; fi
    done
fi
if [ -z "$COVERS" ] || ! real_covers "$COVERS"; then
    echo "cover databases not found - give --covers DIR (coversJ.db, coversP.db, coversU.db, the real ones)" >&2
    exit 1
fi
mkdir -p db
for f in coversJ.db coversP.db coversU.db; do
    if ! cmp -s "$COVERS/$f" "db/$f"; then cp "$COVERS/$f" "db/$f"; fi
done
echo "==> cover databases from $COVERS ($(du -sh db | cut -f1))"

SHA="$(git -C .. rev-parse --short HEAD 2>/dev/null || echo unknown)"
echo "==> docker build --target $TARGET -t $TAG:latest -t $TAG:$SHA"
DOCKER_BUILDKIT=1 docker build --target "$TARGET" -t "$TAG:latest" -t "$TAG:$SHA" "${EXTRA[@]}" .
docker image ls "$TAG"
