#!/bin/bash
# fetch.sh [--record]: every pin in pins.env into cache/<name>, sha256-checked. Each download lands in its own empty
# folder first (untrusted until checked). --record writes the hash of a pin whose PIN_*_SHA256 is empty.
set -euo pipefail
cd "$(dirname "$0")/.."
record=false
[ "${1:-}" = --record ] && record=true
. ./pins.env
mkdir -p cache

die() { echo "fetch: $*" >&2; exit 1; }

# upstream: a git checkout at the pinned commit
if [ ! -d cache/upstream/.git ]; then
    rm -rf cache/dl.upstream
    git clone --quiet "$PIN_UPSTREAM_URL" cache/dl.upstream
    mv cache/dl.upstream cache/upstream
fi
git -C cache/upstream cat-file -e "$PIN_UPSTREAM_COMMIT^{commit}" 2>/dev/null || git -C cache/upstream fetch --quiet origin
git -C cache/upstream rev-parse --verify --quiet "$PIN_UPSTREAM_COMMIT^{commit}" >/dev/null ||
    die "upstream: commit $PIN_UPSTREAM_COMMIT not found"

fetched=0
for name in ZIG PYTHON WHEEL_LZ4 WHEEL_CAPSTONE WHEEL_UNICORN APPIMAGETOOL RUNTIME; do
    url_var=PIN_${name}_URL sha_var=PIN_${name}_SHA256
    url=${!url_var} want=${!sha_var}
    lower=$(echo "$name" | tr 'A-Z' 'a-z')
    file=$(basename "${url//%2B/+}")
    dest=cache/$lower/$file
    if [ -f "$dest" ] && [ -n "$want" ] && [ "$(sha256sum "$dest" | cut -d' ' -f1)" = "$want" ]; then continue; fi
    rm -rf "cache/dl.$lower" && mkdir -p "cache/dl.$lower"
    curl -fsSL --retry 3 -o "cache/dl.$lower/$file" "$url" || die "$lower: download failed ($url)"
    got=$(sha256sum "cache/dl.$lower/$file" | cut -d' ' -f1)
    if [ -z "$want" ]; then
        $record || die "$lower: no sha256 pinned (run with --record)"
        sed -i "s|^$sha_var=.*|$sha_var=$got|" pins.env
        echo "fetch: $lower recorded $got"
    elif [ "$got" != "$want" ]; then
        die "$lower sha256 mismatch (got $got, pinned $want)"
    fi
    rm -rf "cache/$lower" && mv "cache/dl.$lower" "cache/$lower"
    fetched=$((fetched + 1))
done
echo "fetch: all pins ok ($fetched downloaded)"
