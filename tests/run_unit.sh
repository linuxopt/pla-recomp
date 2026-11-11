#!/bin/bash
# tests/run_unit.sh: the unit tests against the patched upstream checkout in work/src (scripts/build-runtime.sh, or a
# checkout being patched by hand). Run in the build container: ./build.sh shell tests/run_unit.sh
set -euo pipefail
cd "$(dirname "$0")/.."
S=work/src/runtime
[ -f "$S/nxpkg.c" ] || { echo "run_unit: no upstream checkout in work/src" >&2; exit 1; }
B=$(mktemp -d "${TMPDIR:-/tmp}/unit.XXXXXX")
trap 'rm -rf "$B"' EXIT
clang -std=gnu11 -O1 -g -Wall -Wno-unused-function -I"$S" -I"$S/nvn" -Iwork/src/third_party -I/usr/local/include \
    tests/unit_test.c "$S/nxpkg.c" "$S/gamesetup.c" "$S/settings.c" "$S/cheats.c" work/src/third_party/miniz/miniz.c tests/unit_stubs.c -o "$B/unit" \
    $(pkg-config --libs sdl3) -lpthread -lm
"$B/unit"
# the cheats (patches/0006): parser, VM and the player's cheat files; miniz reads / writes the test's zip
clang -std=gnu11 -O1 -g -Wall -Wno-unused-function -I"$S" -Iwork/src/third_party \
    tests/cheats_test.c "$S/cheats.c" work/src/third_party/miniz/miniz.c -o "$B/cheats" -lpthread -lm
"$B/cheats"
# the field of view (patches/0008): the hook against fake camera objects at guest addresses, the setting
clang -std=gnu11 -O1 -g -Wall -Wno-unused-function -I"$S" -I"$S/nvn" -Iwork/src/third_party -I/usr/local/include \
    tests/fov_test.c "$S/hle/pla_fov.c" "$S/settings.c" tests/unit_stubs.c -o "$B/fov" $(pkg-config --libs sdl3) -lm
"$B/fov"
# frame rates above 60 kept as saved (patches/0019): its own program - settings are read once per process
clang -std=gnu11 -O1 -g -Wall -Wno-unused-function -I"$S" -I"$S/nvn" -Iwork/src/third_party -I/usr/local/include \
    tests/settings_test.c "$S/settings.c" tests/unit_stubs.c -o "$B/settings" $(pkg-config --libs sdl3) -lm
"$B/settings"
# a cutscene's expired weak reference (patches/0020): the hook against fake objects at guest addresses
clang -std=gnu11 -O1 -g -Wall -Wno-unused-function -I"$S" -Iwork/src/third_party \
    tests/weakref_test.c "$S/hle/pla_weakref.c" -o "$B/weakref"
"$B/weakref"
