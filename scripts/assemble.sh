#!/bin/bash
# assemble.sh: in the build container (./build.sh assemble), after build-runtime.sh. work/AppDir laid out the way the
# runtime looks for its shipped files (next to the executable: runtime/paths.c rt_resource, runtime/gamebuild.c
# find_tools), modelled on upstream's Windows zip (tools/windows/make_zip.sh), then packed by appimagetool into
# dist/PLA-Recomp-<version>-x86_64.AppImage. No game files, no game code: the launcher builds that on the player's PC.
set -euo pipefail
cd "$(dirname "$0")/.."
die() { echo "assemble: $*" >&2; exit 1; }
S=work/src
B=$S/build
[ -x "$B/lasrecomp" ] || die "no $B/lasrecomp: run ./build.sh runtime first"
VERSION=$(sed -n 's/^#define RT_APP_VERSION "\(.*\)".*/\1/p' "$S/runtime/paths.h")
A=work/AppDir
rm -rf "$A" && mkdir -p "$A"/{lib,gen,gamelib-src,plugins,mods,lua-examples,licenses}

# ---- the runtime and the libraries it links that a desktop doesn't reliably have in a compatible version. Left to
# the system: glibc, the GPU stack (Vulkan loader, GL / EGL, DRM), the display (X11, xcb, Wayland, xkbcommon), sound
# servers, D-Bus / udev / systemd and the C++ runtime (Debian 13's libstdc++ / libgcc_s are older than every target's).
cp "$B/lasrecomp" "$A/"
exclude='^(linux-vdso|ld-linux|libc\.so|libm\.so|libmvec|libdl\.so|libpthread|librt\.so|libresolv|libutil|libvulkan|libGL|libEGL|libGLX|libGLdispatch|libOpenGL|libdrm|libgbm|libX|libxcb|libwayland|libxkbcommon|libasound|libpulse|libpipewire|libdbus|libudev|libsystemd|libgcc_s|libstdc\+\+|libdecor)'
ldd "$A/lasrecomp" | grep -q "not found" && die "lasrecomp has unresolved libraries: $(ldd "$A/lasrecomp" | grep 'not found')"
ldd "$A/lasrecomp" | awk '/=>/ && $3 ~ /^\// {print $1, $3}' | while read -r so path; do
    echo "$so" | grep -Eq "$exclude" && continue
    cp -L "$path" "$A/lib/$so"
done
patchelf --set-rpath '$ORIGIN/lib' "$A/lasrecomp"
for l in "$A"/lib/*; do chmod 644 "$l"; patchelf --set-rpath '$ORIGIN' "$l"; done
echo "assemble: $(ls "$A/lib" | wc -l) libraries bundled"

# ---- plugins (Legends Lua, the Steam relay + its helper): libc / libm only, so no RPATH (AppRun copies them into the
# data root, where the runtime loads plugins from)
cp "$B"/plugins/liblegendslua.so "$B"/plugins/libsteamrelay.so "$B"/plugins/steamrelay-helper "$A/plugins/"
for p in "$A"/plugins/*; do
    bad=$(ldd "$p" | awk '{print $1}' | grep -Ev '^(linux-vdso|libc\.so|libm\.so|libdl\.so|libpthread|/lib64/ld-linux)' || true)
    [ -z "$bad" ] || die "plugin $(basename "$p") links more than libc: $bad"
done

# ---- the game code generator: portable CPython + lz4 / capstone / unicorn, and upstream's generator scripts
tar -xf cache/python/*.tar.gz -C "$A"   # (-> $A/python)
PY=$A/python/bin/python3
PYV=$("$PY" -c 'import sys; print("%d.%d" % sys.version_info[:2])')
SP=$A/python/lib/python$PYV/site-packages
"$PY" -m pip install --quiet --no-index --no-deps --no-compile --disable-pip-version-check --target "$SP" \
    cache/wheel_lz4/*.whl cache/wheel_capstone/*.whl cache/wheel_unicorn/*.whl
rm -rf "$A/python/lib/python$PYV"/{test,idlelib,tkinter,turtledemo,ensurepip,lib2to3,pydoc_data} "$A"/python/lib/{tcl,tk,itcl,thread}* \
    "$A/python/share" "$A/python/include" "$SP"/unicorn/include "$SP"/capstone/include "$SP"/pip "$SP"/pip-*.dist-info
find "$SP" -name '*.a' -delete
ln -s bin/python3 "$A/python/python"   # (the runtime starts python/python gen/gen.py)
cp "$S"/tools/{gen,lifter,analyze,nso,passes,patches,simd_c}.py "$A/gen/"
# compiled ahead of time: the AppImage is read-only, so Python couldn't cache them on the player's PC
"$PY" -m compileall -q "$A/gen" "$SP" >/dev/null
"$PY" -c "import sys; sys.path[:0] = ['$A/gen']; import lz4.block, capstone, unicorn; import gen" ||
    die "the bundled Python can't import the generator's modules"

# ---- the compiler: zig 0.17.0, trimmed to what compiling C for x86-64 Linux (glibc) needs
tar -xf cache/zig/*.tar.xz -C work/ && rm -rf "$A/zig" && mv work/zig-x86_64-linux-0.17.0 "$A/zig"
Z=$A/zig/lib
for d in "$Z"/libc/include/*; do
    case "$(basename "$d")" in generic-glibc|x86-linux-any|x86-linux-gnu|any-linux-any) ;; *) rm -rf "$d" ;; esac
done
for d in "$Z"/libc/*; do case "$(basename "$d")" in include|glibc) ;; *) rm -rf "$d" ;; esac; done
rm -rf "$Z"/{libcxx,libcxxabi,libtsan,libunwind,build-web,docs,init,lldb,compiler} "$A/zig/doc" "$A/zig/README.md"

# ---- what the game code is compiled against (runtime/gamebuild.c), and the optimiser's profile
cp "$S"/runtime/{cpu.h,simd.h,simd_fp.h,gamelib.h} "$S"/tools/gamelib_entry.c "$S"/runtime/hle/hle_symbols.txt "$A/gamelib-src/"
cp "$B/pgo.profdata" "$A/gamelib-src/pgo.profdata"
[ -f "$S/data/shader_manifest.txt" ] && cp "$S/data/shader_manifest.txt" "$A/"

# ---- shipped extras, all off until turned on in the Mods page
cp -R "$S"/contrib/mods/. "$A/mods/"
cp "$S/contrib/legendslua/research/pla_research.lua" "$A/lua-examples/"

# ---- the AppImage's own files
cp appimage/AppRun "$A/AppRun" && chmod 755 "$A/AppRun"
cp appimage/plarecomp.desktop appimage/README.txt "$A/"
cp "$S/docs/icon/icon_1024.png" "$A/icon.png"
cp "$S/docs/icon/icon_1024.png" "$A/plarecomp.png"
ln -s plarecomp.png "$A/.DirIcon"
cp "$S/third_party/fonts/Baloo2/Baloo2-SemiBold.ttf" "$A/"

# ---- licenses: this project's, upstream's notices, every bundled component's
L=$A/licenses
cp "$S/LICENSE" "$L/LICENSE.txt"
cp "$S/LICENSE-LASRecomp" "$L/LICENSE-LASRecomp.txt"
cp "$S/THIRD-PARTY-NOTICES.md" "$L/THIRD-PARTY-NOTICES.txt"
cp "$S"/third_party/licenses/* "$L/"
cp "$S/third_party/imgui/LICENSE.txt" "$L/imgui-LICENSE.txt"
cp "$S/third_party/vma/LICENSE.txt" "$L/VulkanMemoryAllocator-LICENSE.txt"
cp "$S/third_party/smaa/LICENSE.txt" "$L/SMAA-LICENSE.txt"
cp "$S/third_party/fonts/Baloo2/OFL.txt" "$L/Baloo2-OFL.txt"
cp "$A/zig/LICENSE" "$L/zig-LICENSE.txt"
cp "$(ls "$A"/python/lib/python"$PYV"/LICENSE.txt 2>/dev/null || ls "$A"/python/LICENSE* | head -1)" "$L/Python-LICENSE.txt"
for w in lz4 capstone unicorn; do
    f=$(find "$SP"/"$w"-*.dist-info -iname 'LICENSE*' -o -iname 'COPYING*' 2>/dev/null | grep -i "$w" | head -1 || true)
    [ -n "$f" ] && [ -f "$f" ] && cp "$f" "$L/python-$w-$(basename "$f").txt"
done
cp /usr/local/share/licenses/SDL3/LICENSE.txt "$L/SDL3-LICENSE.txt"
for l in "$A"/lib/*; do  # (each bundled Debian library's copyright file)
    pkg=$(dpkg -S "$(ldd "$A/lasrecomp" | awk -v so="$(basename "$l")" '$1 == so {print $3}')" 2>/dev/null | head -1 | cut -d: -f1 || true)
    [ -n "$pkg" ] && [ -f "/usr/share/doc/$pkg/copyright" ] && cp "/usr/share/doc/$pkg/copyright" "$L/debian-$pkg-copyright.txt"
done

# ---- nothing from the game (as upstream's make_zip.sh checks)
bad=$(find "$A" \( -iname 'lasgame*' -o -iname '*.nso' -o -iname '*.nca' -o -iname '*.nsp' -o -iname '*.xci' -o -iname 'prod.keys' \
      -o -iname 'title.keys' -o -name exefs -o -name romfs \) -print)
[ -z "$bad" ] || die "game files in the AppDir: $bad"

# ---- built against glibc 2.41 at most (the oldest target distros)
for f in "$A/lasrecomp" "$A"/lib/* "$A"/plugins/*; do
    objdump -T "$f" 2>/dev/null | grep -qE 'GLIBC_2\.(4[2-9]|[5-9][0-9])' && die "$(basename "$f") needs a glibc newer than 2.41"
done

# ---- pack
mkdir -p dist
OUT=dist/PLA-Recomp-$VERSION-x86_64.AppImage
rm -f "$OUT"
ARCH=x86_64 cache/appimagetool/appimagetool-x86_64.AppImage --appimage-extract-and-run --no-appstream \
    --runtime-file cache/runtime/runtime-x86_64 "$A" "$OUT" > work/appimagetool.log 2>&1 ||
    { tail -20 work/appimagetool.log; die "appimagetool failed (work/appimagetool.log)"; }
chmod 755 "$OUT"
echo "assemble: $OUT ($(du -h "$OUT" | cut -f1); AppDir $(du -sh "$A" | cut -f1))"
