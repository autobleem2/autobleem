#!/usr/bin/env bash
# Run a command in the autobleem-build image with this checkout mounted at its own path, as the calling user
# (so nothing in the tree ends up root-owned, and a build_*/ made in the container is the same build dir
# to a cmake run outside it - CMake caches the absolute source and build paths).
#
#   docker/run.sh ci/build.sh psc        one target (see ci/build.sh for the list)
#   docker/run.sh ci/build.sh all
#   docker/run.sh                         an interactive shell in the image
#   AB_BUILD_IMAGE=ghcr.io/autobleem/autobleem-build:latest docker/run.sh ...
set -euo pipefail
cd "$(dirname "$0")/.."

IMAGE="${AB_BUILD_IMAGE:-autobleem-build:latest}"
OPTS=()
[ -t 0 ] && OPTS+=(-it)
# every AB_* variable goes through: the ci/build.sh knobs, and the AB_GIT_* facts for a tree without .git
for v in "${!AB_@}"; do OPTS+=(-e "$v"); done
# the pcsx-ab checkout ci/build.sh builds the emulator from, mounted at its own path too (a sibling
# directory is outside this tree's mount); the same places ci/build.sh looks
pcsx="${AB_PCSX_DIR:-}"
if [ -z "$pcsx" ]; then
    for d in ../pcsx-ab ../pcsx-ab2 ../pcsx-rearmed-develop; do
        [ -f "$d/ci/build.sh" ] && { pcsx="$(cd "$d" && pwd)"; break; }
    done
fi
[ -n "$pcsx" ] && [ -d "$pcsx" ] && OPTS+=(-v "$pcsx:$pcsx")
exec docker run --rm "${OPTS[@]}" \
    -v "$PWD:$PWD" -w "$PWD" \
    -u "$(id -u):$(id -g)" -e HOME=/tmp \
    "$IMAGE" "$@"
