#!/usr/bin/env bash
# Run a command in the autobleem-build image with this checkout mounted at its own path, as the calling user
# (so nothing in the tree ends up root-owned, and a build_*/ made in the container is the same build dir
# to a cmake run outside it - CMake caches the absolute source and build paths).
#
#   docker/run.sh ci/build.sh psc        one target (see ci/build.sh for the list)
#   docker/run.sh ci/build.sh all
#   docker/run.sh                         an interactive shell in the image
#   AB_BUILD_IMAGE=ghcr.io/autobleem/autobleem-build:latest docker/run.sh ...
#
# Two modes for the image builders (tools/make_pc_image.sh builds a Debian root from packages, which wants
# either user namespaces or real root), both off by default - a build needs neither:
#   docker/run.sh --userns CMD...        still the calling user, but seccomp and AppArmor unconfined so
#                                        mmdebstrap --mode=unshare can make its user namespace and mount
#                                        inside it (the rootless route; needs the kernel to allow
#                                        unprivileged user namespaces)
#   docker/run.sh --privileged CMD...    root inside the container with every device (loop mounts,
#                                        grub-install on a loop device): the --mount route; files the
#                                        command writes into the tree come out root-owned
set -euo pipefail
cd "$(dirname "$0")/.."

IMAGE="${AB_BUILD_IMAGE:-autobleem-build:latest}"
OPTS=()
USER_OPTS=(-u "$(id -u):$(id -g)" -e HOME=/tmp)
USERNS=0
while [ $# -gt 0 ]; do
    case "$1" in
        --userns)
            OPTS+=(--security-opt seccomp=unconfined --security-opt apparmor=unconfined)
            USERNS=1
            shift ;;
        --privileged)
            OPTS+=(--privileged)
            USER_OPTS=(-e HOME=/root)
            shift ;;
        *) break ;;
    esac
done
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
# and the pcsx-abnxt checkout (the next emulator, Autobleem/bin/emunxt), the same way
nxt="${AB_PCSXNXT_DIR:-}"
[ -z "$nxt" ] && [ -f ../pcsx-abnxt/ci/build.sh ] && nxt="$(cd ../pcsx-abnxt && pwd)"
[ -n "$nxt" ] && [ -d "$nxt" ] && OPTS+=(-v "$nxt:$nxt")
# the compiler cache (ci/build.sh puts sccache in front of every compiler): a directory of the host's, so
# a container's compiles are the next container's cache hits - AB_SCCACHE_DIR names it, AB_NO_SCCACHE=1
# leaves it out (ci/build.sh then builds without a launcher)
if [ -z "${AB_NO_SCCACHE:-}" ]; then
    cache="${AB_SCCACHE_DIR:-$HOME/.cache/autobleem-sccache}"
    mkdir -p "$cache"
    OPTS+=(-v "$cache:/tmp/sccache" -e SCCACHE_DIR=/tmp/sccache -e SCCACHE_CACHE_SIZE="${AB_SCCACHE_SIZE:-10G}")
fi
# --userns: the calling uid has no account in the image, and unshare/newuidmap want one with a subordinate
# id range (/etc/subuid, /etc/subgid) to map a user namespace - so the four files are made here and bind-
# mounted over the image's for this run
if [ "$USERNS" -eq 1 ]; then
    ns="$(mktemp -d)"
    uid="$(id -u)"; gid="$(id -g)"
    { docker run --rm "$IMAGE" cat /etc/passwd; printf 'builder:x:%s:%s:builder:/tmp:/bin/bash\n' "$uid" "$gid"; } >"$ns/passwd"
    { docker run --rm "$IMAGE" cat /etc/group;  printf 'builder:x:%s:\n' "$gid"; } >"$ns/group"
    printf 'builder:100000:65536\n' >"$ns/subuid"
    printf 'builder:100000:65536\n' >"$ns/subgid"
    OPTS+=(-v "$ns/passwd:/etc/passwd:ro" -v "$ns/group:/etc/group:ro" -v "$ns/subuid:/etc/subuid:ro" -v "$ns/subgid:/etc/subgid:ro")
    # (exec replaces this shell, so the scratch files are removed by the container's exit instead)
    docker run --rm "${OPTS[@]}" -v "$PWD:$PWD" -w "$PWD" "${USER_OPTS[@]}" "$IMAGE" "$@"
    status=$?
    rm -rf "$ns"
    exit $status
fi
exec docker run --rm "${OPTS[@]}" \
    -v "$PWD:$PWD" -w "$PWD" \
    "${USER_OPTS[@]}" \
    "$IMAGE" "$@"
