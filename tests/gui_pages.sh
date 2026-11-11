#!/bin/bash
# tests/gui_pages.sh [AppImage] [out dir]: screenshots of the launcher's Game Files and Mods pages on a fresh data
# folder, in a stock Debian 13 container (virtual X display, software Vulkan) - nothing on this PC's desktop is
# touched. A stand-in prod.keys in the container's ~/.switch checks the pre-fill. Pictures: <out dir>/gui-*.png.
set -euo pipefail
cd "$(dirname "$0")/.."
IMG=$(readlink -f "${1:-$(ls dist/PLA-Recomp-*-x86_64.AppImage | tail -1)}")
OUT=$(readlink -f "${2:-work/gui}")
mkdir -p "$OUT"
PKGS="libvulkan1 mesa-vulkan-drivers libgl1 libegl1 libgbm1 libdrm2 libx11-6 libx11-xcb1 libxcb1 libxcb-render0 libxcb-shm0
      libxext6 libxrandr2 libxcursor1 libxi6 libxss1 libxfixes3 libxtst6 libxkbcommon0 libwayland-client0 libwayland-cursor0
      libwayland-egl1 libdecor-0-0 libasound2t64 libpulse0 libpipewire-0.3-0t64 libdbus-1-3 libudev1 xvfb xauth xdotool
      imagemagick"
podman run --rm --network=host -v "$IMG":/app.AppImage:ro,Z -v "$OUT":/out:Z docker.io/library/debian:trixie bash -c "
set -e
export DEBIAN_FRONTEND=noninteractive
apt-get update -qq && apt-get install -y -qq --no-install-recommends $(echo $PKGS) >/dev/null
cd /tmp && /app.AppImage --appimage-extract >/dev/null
mkdir -p /root/.switch && echo 'header_key = 00000000000000000000000000000000000000000000000000000000000000ff' > /root/.switch/prod.keys
# a stand-in game header (only main's NSO header with the 1.1.1 build id - no game data) and made-up cheats, so the
# Cheats tab has something to list: one that changes a value, one that patches code (shown as unable to work here)
mkdir -p /tmp/data/extracted/exefs /tmp/data/cheats
printf 'NSO0' > /tmp/data/extracted/exefs/main; head -c 60 /dev/zero >> /tmp/data/extracted/exefs/main
printf '\\x00\\x10\\x00\\x00' | dd of=/tmp/data/extracted/exefs/main bs=1 seek=24 conv=notrunc 2>/dev/null  # (text: offset 0, 0x1000 bytes)
printf '\\xae\\xe8\\xf1\\x50\\xdd\\xa1\\xb5\\xa8' | dd of=/tmp/data/extracted/exefs/main bs=1 seek=64 conv=notrunc 2>/dev/null
printf '[Test: a value]\\n04100000 00000100 00000063\\n[Test: patches code]\\n04000000 00000010 52800020\\n' > /tmp/data/cheats/AEE8F150DDA1B5A8.txt
export LARECOMP_DATA_ROOT=/tmp/data VK_ICD_FILENAMES=\$(ls /usr/share/vulkan/icd.d/lvp_icd*.json | head -1)
Xvfb :9 -screen 0 1280x720x24 >/dev/null 2>&1 & sleep 2
export DISPLAY=:9
squashfs-root/AppRun > /tmp/run.out 2>&1 &
for i in \$(seq 60); do grep -q 'launcher. this PC' /tmp/data/lasrecomp.log 2>/dev/null && break; sleep 1; done
sleep 4
W=\$(xdotool search --name 'PL:A Recomp' | head -1)
shot() { import -window root /out/gui-\$1.png; }
click() { xdotool mousemove --window \$W \$1 \$2; sleep 0.5; xdotool mousedown 1; sleep 0.2; xdotool mouseup 1; }  # (ImGui: the pointer there a frame before the press)
shot home
click 1049 334; sleep 3; shot files
click 1153 43; sleep 2
click 1049 402; sleep 3; shot mods
click 159 131; sleep 2; shot cheats
click 1153 43; sleep 2
click 1049 198; sleep 3; shot options
click 193 131; sleep 2; shot graphics
grep -E 'setup_|plugins_on|^mod ' /tmp/data/larecomp.ini > /out/gui-ini.txt || true
echo gui_pages: done
"
ls "$OUT"
