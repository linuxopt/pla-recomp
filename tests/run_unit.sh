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
    tests/nxpkg_files_test.c "$S/nxpkg.c" -o "$B/unit" -lm
"$B/unit"
