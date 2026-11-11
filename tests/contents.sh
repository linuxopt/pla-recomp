#!/bin/bash
# tests/contents.sh [AppImage]: the AppImage holds everything the spec lists, nothing from the game, and no ELF built
# against a glibc newer than 2.41. Extracts into a temp folder.
set -euo pipefail
cd "$(dirname "$0")/.."
IMG=$(readlink -f "${1:-$(ls dist/PLA-Recomp-*-x86_64.AppImage | tail -1)}")
T=$(mktemp -d "${TMPDIR:-/tmp}/contents.XXXXXX")
trap 'rm -rf "$T"' EXIT
(cd "$T" && "$IMG" --appimage-extract >/dev/null)
R=$T/squashfs-root
fail=0
for f in AppRun lasrecomp plarecomp.desktop plarecomp.png .DirIcon icon.png Baloo2-SemiBold.ttf README.txt \
         zig/zig python/python gen/gen.py gen/lifter.py gen/patches.py \
         gamelib-src/cpu.h gamelib-src/simd.h gamelib-src/simd_fp.h gamelib-src/gamelib.h gamelib-src/gamelib_entry.c \
         gamelib-src/hle_symbols.txt gamelib-src/pgo.profdata \
         plugins/liblegendslua.so plugins/libsteamrelay.so plugins/steamrelay-helper \
         "mods/Lush Foliage/mod.ini" "mods/Lush Foliage/trees.lft" lua-examples/pla_research.lua \
         cheats/AEE8F150DDA1B5A8.txt cheats/CREDITS.txt \
         licenses/LICENSE.txt licenses/SDL3-LICENSE.txt licenses/zig-LICENSE.txt licenses/Python-LICENSE.txt; do
    [ -e "$R/$f" ] || { echo "contents: MISSING $f"; fail=1; }
done
bad=$(find "$R" \( -iname 'lasgame*' -o -iname '*.nso' -o -iname '*.nca' -o -iname '*.nsp' -o -iname '*.xci' -o -iname 'prod.keys' \
      -o -iname 'title.keys' -o -name exefs -o -name romfs \) -print)
[ -z "$bad" ] || { echo "contents: game files: $bad"; fail=1; }
for f in "$R/lasrecomp" "$R"/lib/* "$R"/plugins/*; do
    if objdump -T "$f" 2>/dev/null | grep -qE 'GLIBC_2\.(4[2-9]|[5-9][0-9])'; then echo "contents: $(basename "$f") needs glibc > 2.41"; fail=1; fi
done
"$R/python/python" -I -c 'import lz4.block, capstone, unicorn' || { echo "contents: bundled Python can't import its modules"; fail=1; }
[ $fail = 0 ] && echo "contents: ok ($(du -h "$IMG" | cut -f1), $(ls "$R/lib" | wc -l) libraries)"
exit $fail
