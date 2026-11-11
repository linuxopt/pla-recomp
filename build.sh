#!/bin/bash
# build.sh [image|fetch|runtime|assemble|all] [args...]: the PL:A Recomp AppImage, built in a Debian 13 container
# (podman, rootless) so it runs on older distros than this PC's. Output: dist/PLA-Recomp-<version>-x86_64.AppImage.
# fetch runs on the host (downloads only); the build steps run in the container with this folder mounted at /w.
# Host networking: this kernel has no tun / overlay modules (rootless podman: fuse-overlayfs storage, no pasta).
set -euo pipefail
cd "$(dirname "$0")"
IMAGE=localhost/pla-appimage-build:trixie
step=${1:-all}
[ $# -gt 0 ] && shift

command -v podman >/dev/null || { echo "build.sh: needs podman (e.g. sudo pacman -S podman)" >&2; exit 1; }

image() { podman build --network=host -q -t "$IMAGE" container/ >/dev/null && echo "build.sh: image $IMAGE ready"; }
in_box() { podman run --rm --network=host --userns=keep-id -v "$PWD":/w:Z -w /w "$IMAGE" "$@"; }

case "$step" in
image) image ;;
fetch) scripts/fetch.sh "$@" ;;
runtime) in_box scripts/build-runtime.sh "$@" ;;
assemble) in_box scripts/assemble.sh "$@" ;;
shell) in_box bash "$@" ;;
all) image; scripts/fetch.sh; in_box scripts/build-runtime.sh; in_box scripts/assemble.sh ;;
*) echo "build.sh: unknown step '$step' (image, fetch, runtime, assemble, shell, all)" >&2; exit 1 ;;
esac
