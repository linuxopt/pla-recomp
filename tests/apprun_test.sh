#!/bin/bash
# tests/apprun_test.sh: appimage/AppRun on its own, in a fake AppDir whose lasrecomp is a stub that prints the data folder
# it was handed - no game, no AppImage. Checks: the data folder is pinned (LARECOMP_DATA_ROOT, so the runtime never takes
# an extracted/ in the folder it was started from), the shipped plugins / mods / cheats are seeded, and the bundled
# cheats are seeded once (a player who deleted them doesn't get them back).
set -euo pipefail
cd "$(dirname "$0")/.."
T=$(mktemp -d "${TMPDIR:-/tmp}/apprun test é XXXXXX")
trap 'rm -rf "$T"' EXIT
A=$T/AppDir
mkdir -p "$A/plugins" "$A/mods/Lush Foliage" "$A/cheats" "$A/lua-examples" "$T/home" "$T/start/extracted"
cp appimage/AppRun "$A/AppRun"
printf '#!/bin/sh\necho "root=$LARECOMP_DATA_ROOT"\n' > "$A/lasrecomp" && chmod +x "$A/lasrecomp" "$A/AppRun"
echo plugin > "$A/plugins/libtest.so"; echo ini > "$A/mods/Lush Foliage/mod.ini"; echo ex > "$A/lua-examples/x.lua"
echo '[A cheat]' > "$A/cheats/AEE8F150DDA1B5A8.txt"; echo credits > "$A/cheats/CREDITS.txt"
fail=0
check() { if eval "$1"; then echo "ok   $2"; else echo "FAIL $2"; fail=1; fi; }
run() { (cd "$T/start" && env -i HOME="$T/home" PATH="$PATH" "$@" "$A/AppRun"); }

out=$(run)
D="$T/home/.local/share/plarecomp"
check '[ "$out" = "root=$D" ]' "data folder pinned for the runtime (not the start folder's extracted/)"
out=$(run XDG_DATA_HOME="$T/xdg")
check '[ "$out" = "root=$T/xdg/plarecomp" ]' "XDG_DATA_HOME honoured"
out=$(run LARECOMP_DATA_ROOT="$T/mine")
check '[ "$out" = "root=$T/mine" ]' "LARECOMP_DATA_ROOT kept as given"
check '[ -f "$D/plugins/libtest.so" ] && [ -f "$D/mods/Lush Foliage/mod.ini" ] && [ -f "$D/lua-examples/x.lua" ]' "plugins, mods, examples seeded"
check '[ -f "$D/cheats/AEE8F150DDA1B5A8.txt" ] && [ -f "$D/cheats/CREDITS.txt" ]' "bundled cheats seeded"
rm "$D/cheats/AEE8F150DDA1B5A8.txt"
run >/dev/null
check '[ ! -e "$D/cheats/AEE8F150DDA1B5A8.txt" ]' "bundled cheats deleted by the player stay deleted"
check '[ -d "$T/start/extracted" ] && [ -z "$(ls -A "$T/start/extracted")" ]' "the start folder's extracted/ untouched"
[ $fail = 0 ] && echo "apprun: ok"
exit $fail
