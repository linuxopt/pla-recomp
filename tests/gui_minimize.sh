#!/bin/bash
# tests/gui_minimize.sh [AppImage]: the launcher survives being minimized on its home page. (Upstream's banner divided
# by its width: a minimized window is 0 wide, the NaN became an array index and the launcher crashed - patches/0005.)
# Needs an X11 desktop and xdotool. A fresh data folder; only this test's own process is touched.
set -euo pipefail
cd "$(dirname "$0")/.."
IMG=$(readlink -f "${1:-$(ls dist/PLA-Recomp-*-x86_64.AppImage | tail -1)}")
command -v xdotool >/dev/null || { echo "gui_minimize: needs xdotool"; exit 2; }
T=$(mktemp -d "${TMPDIR:-/tmp}/minimize.XXXXXX")
cp "$IMG" "$T/test.AppImage"
LARECOMP_DATA_ROOT="$T/data" "$T/test.AppImage" > "$T/out" 2>&1 &
APP=$!
tree() { local c; for c in $(pgrep -P "$1"); do tree "$c"; done; echo "$1"; }
cleanup() { kill $(tree "$APP" 2>/dev/null) 2>/dev/null || true; wait "$APP" 2>/dev/null || true; rm -rf "$T"; }
trap cleanup EXIT
timeout 60 bash -c "until grep -q '\[launcher\] this PC' '$T/data/lasrecomp.log' 2>/dev/null; do sleep 1; done" ||
    { echo "gui_minimize: the launcher didn't start"; exit 1; }
P=$(tree "$APP" | while read -r p; do [ "$(basename "$(readlink -f /proc/$p/exe 2>/dev/null)")" = lasrecomp ] && echo "$p"; done | head -1)
WIN=$(xdotool search --pid "$P" 2>/dev/null | head -1)
[ -n "$WIN" ] || { echo "gui_minimize: no launcher window"; exit 1; }
sleep 2
xdotool windowminimize "$WIN"
sleep 10
kill -0 "$P" 2>/dev/null || { echo "gui_minimize: FAIL - the launcher died after being minimized"; exit 1; }
xdotool windowmap "$WIN" 2>/dev/null || true
sleep 2
kill -0 "$P" 2>/dev/null || { echo "gui_minimize: FAIL - the launcher died when restored"; exit 1; }
echo "gui_minimize: ok (minimized 10 s and restored, still running)"
