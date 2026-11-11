#!/bin/bash
# tests/compat.sh [AppImage]: the AppImage on a stock Debian 13 (glibc 2.41, the oldest target) with only what a
# desktop has - the display, sound and GPU libraries the AppImage leaves to the system - and no build tools:
# every library resolves, and the launcher starts and draws (software Vulkan: lavapipe, a virtual X display).
# Runs on the host: podman, the AppImage mounted read-only.
set -euo pipefail
cd "$(dirname "$0")/.."
IMG=$(readlink -f "${1:-$(ls dist/PLA-Recomp-*-x86_64.AppImage | tail -1)}")
# a desktop's libraries (what assemble.sh leaves to the system) + lavapipe + a virtual X server
PKGS="libvulkan1 mesa-vulkan-drivers libgl1 libegl1 libgbm1 libdrm2 libx11-6 libx11-xcb1 libxcb1 libxcb-render0 libxcb-shm0 libxext6 libxrandr2
      libxcursor1 libxi6 libxss1 libxfixes3 libxtst6 libxkbcommon0 libwayland-client0 libwayland-cursor0 libwayland-egl1
      libdecor-0-0 libasound2t64 libpulse0 libpipewire-0.3-0t64 libdbus-1-3 libudev1 xvfb xauth"
podman run --rm --network=host -v "$IMG":/app.AppImage:ro,Z docker.io/library/debian:trixie bash -c "
set -e
export DEBIAN_FRONTEND=noninteractive
apt-get update -qq && apt-get install -y -qq --no-install-recommends $(echo $PKGS) >/dev/null
for t in clang gcc cc python3 zig; do command -v \$t >/dev/null && { echo \"compat: \$t is installed (should not be)\"; exit 1; }; done
cd /tmp && /app.AppImage --appimage-extract >/dev/null
missing=\$(for f in squashfs-root/lasrecomp squashfs-root/lib/* squashfs-root/plugins/*; do ldd \$f 2>/dev/null | grep 'not found'; done | sort -u)
[ -z \"\$missing\" ] || { echo \"compat: unresolved libraries:\"; echo \"\$missing\"; exit 1; }
export LARECOMP_DATA_ROOT=/tmp/data VK_ICD_FILENAMES=\$(ls /usr/share/vulkan/icd.d/lvp_icd*.json | head -1)
timeout 40 xvfb-run -a -s '-screen 0 1280x720x24' squashfs-root/AppRun > /tmp/run.out 2>&1 || true
cat /tmp/data/lasrecomp.log 2>/dev/null > /tmp/all.log; cat /tmp/run.out >> /tmp/all.log
grep -E 'error while loading shared libraries|GLIBC_2[.][0-9]+.. not found|version .GLIBCXX' /tmp/all.log && { echo 'compat: loader errors'; exit 1; }
grep -q '\[paths\] data: /tmp/data' /tmp/all.log || { echo 'compat: the runtime did not start'; tail -20 /tmp/all.log; exit 1; }
grep -q '\[launcher\] this PC' /tmp/all.log || { echo 'compat: the launcher did not come up'; tail -20 /tmp/all.log; exit 1; }
[ -f /tmp/data/plugins/liblegendslua.so ] && [ -f '/tmp/data/mods/Lush Foliage/mod.ini' ] || { echo 'compat: AppRun did not seed'; exit 1; }
echo \"compat: ok (Debian \$(cat /etc/debian_version), \$(ldd --version | head -1 | grep -o '[0-9.]*\$'); \$(grep -o '\[vk\] using .*' /tmp/all.log | head -1))\"
"
