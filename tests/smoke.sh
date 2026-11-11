#!/bin/bash
# tests/smoke.sh <phase> [dump]: the AppImage end to end on this PC, from a clean data folder, with only the
# AppImage's own tools reachable (PATH holds a few shell utilities for AppRun - no compiler, no Python).
#   setup      fresh data root; setup from the extracted dump (LARECOMP_SETUP_DIR); waits for [setup] ready; asserts
#              the bundled generator + zig were used, the game library was built, AppRun seeded plugins / mods (off)
#   interrupt  fresh data root; setup killed 60 s into the recompile; started again (no crash, not "ready"), then
#              set up again to [setup] ready
#   stale      the setup phase's data root with the game library's id changed: the start rebuilds it
# dump: an extracted Arceus 1.1.1 (main, main.npdm, ... next to bin/). Default: the folder this repo sits in.
# It is only read: the test links its files into a clean exefs/ + romfs/ view (the folder may hold other things).
set -euo pipefail
cd "$(dirname "$0")/.."
REPO=$PWD
phase=${1:?phase: setup, interrupt or stale}
DUMP=$(readlink -f "${2:-..}")
W=$REPO/work/smoke
IMG_SRC=$(readlink -f "$(ls dist/PLA-Recomp-*-x86_64.AppImage | tail -1)")
say() { printf '[smoke] %s\n' "$*"; }
die() { printf '[smoke] FAIL: %s\n' "$*"; exit 1; }

# ---- the dump, as a clean view (links only)
view() {
    local v=$W/dump
    rm -rf "$v" && mkdir -p "$v/exefs" "$v/romfs"
    for f in main main.npdm rtld sdk subsdk0 subsdk1; do
        [ -f "$DUMP/$f" ] || die "no $DUMP/$f (an extracted Arceus 1.1.1 dump?)"
        ln -s "$DUMP/$f" "$v/exefs/$f"
    done
    for d in bin system_resource; do [ -d "$DUMP/$d" ] && ln -s "$DUMP/$d" "$v/romfs/$d"; done
    echo "$v"
}

# ---- the AppImage in a folder with a space, a PATH with no compiler / Python
mkdir -p "$W/My Games" "$W/bin"
cp -f "$IMG_SRC" "$W/My Games/"
IMG="$W/My Games/$(basename "$IMG_SRC")"
for t in sh dirname readlink mkdir cmp cp mv chmod basename cat; do ln -sf "$(command -v $t)" "$W/bin/$t"; done
for t in fusermount3 fusermount; do command -v $t >/dev/null && ln -sf "$(command -v $t)" "$W/bin/$t"; done  # (the AppImage mounts itself)
for t in cc gcc clang python python3 zig; do [ -e "$W/bin/$t" ] && die "$t on the test PATH"; done

# run <data root> <log> [VAR=value...]: the AppImage in the background, its pid in $APP. The runtime writes its own log
# to <data root>/lasrecomp.log (that run's only); it's copied to <log> as the run goes (the AppImage's output too).
run() {
    local root=$1 log=$2; shift 2
    mkdir -p "$root" && rm -f "$root/lasrecomp.log"
    env -i HOME="$HOME" DISPLAY="${DISPLAY:-}" XAUTHORITY="${XAUTHORITY:-}" WAYLAND_DISPLAY="${WAYLAND_DISPLAY:-}" \
        XDG_RUNTIME_DIR="${XDG_RUNTIME_DIR:-}" PATH="$W/bin" LARECOMP_DATA_ROOT="$root" "$@" \
        "$IMG" > "$log.out" 2>&1 &
    APP=$!
    RUNLOG=$root/lasrecomp.log OUTLOG=$log
}
sync_log() { cat "$OUTLOG.out" "$RUNLOG" > "$OUTLOG" 2>/dev/null || true; }
# wait_for <log> <pattern> <seconds>: the pattern appears (0), the app exits or time runs out (1)
wait_for() {
    local log=$1 pat=$2 secs=$3 t=0
    while [ $t -lt "$secs" ]; do
        sync_log
        grep -qE "$pat" "$log" 2>/dev/null && return 0
        kill -0 "$APP" 2>/dev/null || { sync_log; grep -qE "$pat" "$log" && return 0; return 1; }
        sleep 2; t=$((t + 2))
    done
    return 1
}
stop() {  # (AppRun execs lasrecomp from the AppImage's mount; its children: the generator, zig)
    kill "$APP" 2>/dev/null || true
    pkill -f '/tmp/[.]mount_PLA-Re.*/lasrecomp' 2>/dev/null || true
    sleep 3
    pkill -f '/tmp/[.]mount_PLA-Re' 2>/dev/null || true
    wait "$APP" 2>/dev/null || true
    sync_log
}

assert_ready() {
    local root=$1 log=$2
    grep -q "\[setup\] ready" "$log" || die "no [setup] ready in $log"
    [ -s "$root/extracted/lasgame.so" ] && [ -s "$root/extracted/lasgame.so.id" ] || die "no game library in $root/extracted"
}

case "$phase" in
setup)
    root="$W/data root"; log=$W/setup.log
    rm -rf "$root"
    t0=$(date +%s)
    run "$root" "$log" LARECOMP_SETUP_DIR="$(view)"
    say "setup running (pid $APP, log $log)"
    wait_for "$log" '\[setup\] (ready|failed)' 3600 || { stop; die "setup didn't finish (tail: $(tail -3 "$log"))"; }
    took=$(( $(date +%s) - t0 ))
    stop
    grep -q "\[setup\] failed" "$log" && die "setup failed: $(grep -E 'setup\]|gamebuild\]' "$log" | tail -5)"
    assert_ready "$root" "$log"
    grep -E "\[gamebuild\] generator .*/python/python .*/gen/gen.py, compiler .*/zig/zig cc" "$log" | grep -q "/tmp/.mount_\|squashfs-root" ||
        die "the bundled tools weren't used: $(grep '\[gamebuild\] generator' "$log")"
    for f in plugins/liblegendslua.so plugins/libsteamrelay.so plugins/steamrelay-helper "mods/Lush Foliage/mod.ini" \
             lua-examples/pla_research.lua; do
        [ -e "$root/$f" ] || die "AppRun didn't seed $f"
    done
    grep -qE '^mod = *$' "$root/larecomp.ini" && grep -qE '^plugins_on = *$' "$root/larecomp.ini" ||
        die "mods / plugins not all off in larecomp.ini: $(grep -E '^(mod|plugins_on) =' "$root/larecomp.ini")"
    say "ok: ready in ${took}s; $(grep '\[gamebuild\] generator' "$log" | sed 's/.*\[gamebuild\] //')"
    ;;
interrupt)
    root="$W/data interrupt"; log=$W/interrupt1.log
    rm -rf "$root"
    run "$root" "$log" LARECOMP_SETUP_DIR="$(view)"
    wait_for "$log" '\[gamebuild\] generator' 600 || { stop; die "the recompile didn't start"; }
    sleep 60; stop
    say "killed 60 s into the recompile"
    log=$W/interrupt2.log
    run "$root" "$log"
    sleep 15
    kill -0 "$APP" 2>/dev/null || die "the second start exited: $(tail -5 "$log")"
    [ -s "$root/extracted/lasgame.so" ] && die "a game library exists after the interrupted build"
    stop
    say "second start: running, not ready (no game library)"
    log=$W/interrupt3.log
    run "$root" "$log" LARECOMP_SETUP_DIR="$(view)"
    wait_for "$log" '\[setup\] (ready|failed)' 3600 || { stop; die "setup again didn't finish"; }
    stop
    assert_ready "$root" "$log"
    say "ok: interrupted setup recovers"
    ;;
stale)
    root="$W/data root"; log=$W/stale.log
    [ -s "$root/extracted/lasgame.so.id" ] || die "run the setup phase first"
    echo 0000000000000000 > "$root/extracted/lasgame.so.id"
    run "$root" "$log"
    wait_for "$log" '\[gamebuild\] generator|\[setup\] (ready|failed)' 300 || { stop; die "no rebuild started: $(grep -iE 'gamelib|setup' "$log" | tail -5)"; }
    wait_for "$log" '\[setup\] (ready|failed)|\[gamebuild\] (done|failed)' 3600 || { stop; die "the rebuild didn't finish"; }
    stop
    grep -q "\[gamebuild\] done" "$log" || die "rebuild failed: $(grep gamebuild "$log" | tail -3)"
    [ "$(cat "$root/extracted/lasgame.so.id")" != 0000000000000000 ] || die "the id wasn't rewritten"
    say "ok: a stale game library is rebuilt"
    ;;
*) die "unknown phase $phase" ;;
esac
