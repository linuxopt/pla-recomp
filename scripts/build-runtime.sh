#!/bin/bash
# build-runtime.sh: in the build container (./build.sh runtime). A fresh upstream checkout at the pinned commit in
# work/src, patches/*.patch applied in name order, then the runtime (build/lasrecomp) and the plugins (Legends Lua,
# the Steam relay and its helper) built, and the optimiser's profile decompressed for the AppImage.
set -euo pipefail
cd "$(dirname "$0")/.."
. ./pins.env
die() { echo "build-runtime: $*" >&2; exit 1; }
[ -d cache/upstream/.git ] || die "no cache/upstream: run ./build.sh fetch first"

rm -rf work/src
mkdir -p work
git clone --quiet cache/upstream work/src
git -C work/src checkout --quiet --detach "$PIN_UPSTREAM_COMMIT"
export GIT_COMMITTER_NAME="PLA AppImage build" GIT_COMMITTER_EMAIL="build@localhost"
for p in patches/*.patch; do
    git -C work/src am --quiet --keep-cr "$PWD/$p" || { git -C work/src am --abort || true; die "patch $(basename "$p") does not apply"; }
    echo "build-runtime: applied $(basename "$p")"
done

# DEPS=/usr/local: SDL 3.4 built into the image there (container/Containerfile).
# shaderc: Debian ships libshaderc.so (Arch / Fedora: only libshaderc_shared.so) - as upstream's install.sh picks it
shaderc=-lshaderc
for d in /usr/lib /usr/lib64 /usr/lib/x86_64-linux-gnu; do
    [ -e "$d/libshaderc.so" ] && break
    [ -e "$d/libshaderc_shared.so" ] && { shaderc=-lshaderc_shared; break; }
done
log=work/build-runtime.log
echo "build-runtime: make all legendslua steamrelay ($(nproc) jobs, log $log)"
if ! make -C work/src -j"$(nproc)" all legendslua steamrelay SHADERC=$shaderc DEPS=/usr/local > "$log" 2>&1; then
    grep -E "error" "$log" | head -20
    die "the build failed ($log)"
fi
zstd -q -d -f work/src/data/pgo/pla.clang21.profdata.zst -o work/src/build/pgo.profdata
for f in build/lasrecomp build/plugins/liblegendslua.so build/plugins/libsteamrelay.so build/plugins/steamrelay-helper \
         build/pgo.profdata; do
    [ -s "work/src/$f" ] || die "missing work/src/$f"
done
echo "build-runtime: ok ($(git -C work/src log --oneline | head -n $(ls patches/*.patch | wc -l) | wc -l) patches on $PIN_UPSTREAM_COMMIT)"
