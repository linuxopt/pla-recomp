#!/bin/bash
# tests/zig_shared.sh: the bundled zig builds a library shaped like the game code (lasgame.so) the way
# runtime/gamebuild.c does on the player's PC - compiled with the game code's flags and the PGO profile, linked
# -shared with the runtime's functions left for the executable (-rdynamic) to provide - and an -rdynamic host loads
# it and calls through. Run in the build container: ./build.sh shell tests/zig_shared.sh
# ZIG_DIR: another zig folder (the AppDir's trimmed one). ZIG_LINK_EXTRA: extra link flags to try (the fallback, if plain -shared doesn't link).
set -euo pipefail
cd "$(dirname "$0")/.."
ZIG=${ZIG_DIR:-$(ls -d cache/zig/zig-x86_64-linux-0.17.0 2>/dev/null || true)}
if [ ! -x "$ZIG/zig" ]; then
    mkdir -p work/zigtest-unpack && tar -xf cache/zig/zig-x86_64-linux-0.17.0.tar.xz -C work/zigtest-unpack
    ZIG=work/zigtest-unpack/zig-x86_64-linux-0.17.0
fi
ZIG=$(readlink -f "$ZIG")/zig
T=$(mktemp -d "${TMPDIR:-/tmp}/zigtest.XXXXXX")
trap 'rm -rf "$T"' EXIT
export ZIG_GLOBAL_CACHE_DIR=$T/zig-cache ZIG_LOCAL_CACHE_DIR=$T/zig-cache
zstd -q -d -f cache/upstream/data/pgo/pla.clang21.profdata.zst -o "$T/pgo.profdata" 2>/dev/null ||
    git -C cache/upstream show bc830dd:data/pgo/pla.clang21.profdata.zst | zstd -q -d -o "$T/pgo.profdata"

cat > "$T/host.c" <<'EOF'
#include <dlfcn.h>
#include <stdio.h>
int rt_hello(int x) { return x + 1; }  /* a runtime function the game code calls */
int main(int argc, char **argv) {
    void *g = dlopen(argv[1], RTLD_NOW);
    if (!g) { printf("dlopen: %s\n", dlerror()); return 1; }
    int (*entry)(void) = (int (*)(void))dlsym(g, "game_entry");
    if (!entry) { printf("no game_entry\n"); return 1; }
    printf("%d\n", entry());
    return 0;
}
EOF
cat > "$T/g.c" <<'EOF'
int rt_hello(int);
__attribute__((visibility("default"))) int game_entry(void) { return rt_hello(41); }
EOF
clang -rdynamic "$T/host.c" -ldl -o "$T/host"
"$ZIG" cc -O2 -fno-strict-aliasing -fdenormal-fp-math=dynamic -fvisibility=hidden -mcpu=x86_64_v3 -fPIC \
    -fprofile-instr-use="$T/pgo.profdata" -Wno-profile-instr-unprofiled -Wno-profile-instr-out-of-date \
    -Wno-backend-plugin -c "$T/g.c" -o "$T/g.o"
# shellcheck disable=SC2086
"$ZIG" cc -shared ${ZIG_LINK_EXTRA:-} -o "$T/g.so" "$T/g.o"
out=$("$T/host" "$T/g.so")
[ "$out" = 42 ] || { echo "zig_shared: FAIL (host printed '$out')"; exit 1; }
echo "zig_shared: ok ($out)"
