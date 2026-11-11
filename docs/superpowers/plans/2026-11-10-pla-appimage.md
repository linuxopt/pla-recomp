# PL:A Recomp AppImage Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** A self-contained `PLA-Recomp-1.2.0-rc1-x86_64.AppImage` of the upstream Arceus recomp, built in Debian 13, whose launcher takes a base game file, an update file and prod.keys and recompiles the game code with bundled tools.

**Architecture:** Upstream (`LegendsA` @ `bc830dd`) is fetched at build time and patched (`patches/*.patch`). A Debian 13 podman image builds the runtime and plugins; `assemble.sh` lays out an AppDir modelled on upstream's Windows zip (`tools/windows/make_zip.sh`), with portable Python, trimmed zig 0.17.0 and bundled libs; `appimagetool` packs it.

**Tech Stack:** bash, podman, Debian 13, clang/lld, make, C/C++ (upstream runtime, Dear ImGui, SDL 3.4), python-build-standalone 3.12, zig 0.17.0, appimagetool.

**Spec:** `docs/superpowers/specs/2026-11-10-pla-appimage-design.md` (read it first).

## Global Constraints

- Arceus only. Upstream `https://gitlab.com/mr.wifi1143/LZA-Recomp.git`, branch `LegendsA`, commit `bc830dd`.
- Only supported game: program id `0x01001F5010DFA000`, update 1.1.1, main build id `aee8f150dda1b5a8`.
- Nothing from the game in the repo, the AppImage or `patches/` (no `lasgame*`, `.nso`, `.nca`, `.nsp`, `.xci`, `prod.keys`, `title.keys`, `exefs`, `romfs`).
- Built in Debian 13 (glibc 2.41). Never link against the host's glibc 2.44.
- Plugins and mods ship **off** (upstream default: opt-in plugins in `plugins_on`, mods in `mod`, both empty).
- zig 0.17.0 is the game-code compiler at run time; PGO profile is `data/pgo/pla.clang21.profdata.zst`.
- Data root stays upstream's Linux one: `$XDG_DATA_HOME/plarecomp`, default `~/.local/share/plarecomp`.
- Every download sha256-pinned in `pins.env`; each download unpacked in its own empty folder.
- New launcher strings come in English + Portuguese pairs via `T(en, pt)`, as the rest of `launcher_lg.mm`.
- Shell scripts: `#!/bin/bash` + `set -euo pipefail`, run from the repo root.
- The game dump at `/run/media/x/84635e6c-e26f-4a02-ab62-ae3b3583c551/PLA` is read-only input: tests never write to it.
- Z-A work never goes in this repo or `PLA/`.

## Review Focus

1. Paths with spaces, brackets and non-ASCII (`Pokémon Legends Arceus [01001F5010DFA000][v0].nsp`, an AppImage in `~/My Games/`) must work in the pickers, the setup and the AppImage start. → Task 3 test `files_spaces`, Task 8 smoke runs from a path with a space.
2. Base and update picked the wrong way round, or the same file twice. The package reader classifies by its metadata, so swapping should work, and the same file twice should give "only the update / not the base game". → Task 3 test `files_order`.
3. A setup interrupted mid-recompile (window closed, crash): the next start keeps the three paths and **Recompile** works again. → Task 4 test `settings_roundtrip` + Task 8 smoke step "kill and restart".
4. No prod.keys anywhere: the keys field stays empty, Recompile is disabled, and nothing crashes. → Task 3 test `default_none`.
5. Data root left by an upstream source install (an older `lasgame.so` with another id): the AppImage rebuilds instead of loading it (upstream's `.id` check). → Task 8 smoke step "stale lasgame".

---

## File structure (this repo, `PLA/appimage-build/`)

```
build.sh                    host entry: podman image, then the in-container build, -> dist/
pins.env                    URL + sha256 of every download
container/Containerfile     Debian 13 + build deps + SDL 3.4 from source
scripts/fetch.sh            downloads pins.env into cache/, checks sha256 (--record fills empty hashes)
scripts/build-runtime.sh    upstream checkout in work/src, patches applied, make all legendslua steamrelay
scripts/assemble.sh         work/AppDir laid out, libs bundled, game-files check, appimagetool
appimage/AppRun  appimage/plarecomp.desktop
patches/0001-nxpkg-open-explicit-files.patch
patches/0002-gamesetup-files-and-version.patch
patches/0003-launcher-three-pickers.patch
tests/zig_shared.sh         Task 2: zig links a game-library-shaped .so
tests/nxpkg_files_test.c    Task 3 tests, built against upstream's nxpkg.c
tests/run_unit.sh           builds + runs the C tests in the container
tests/smoke.sh              Task 8: the AppImage end to end
tests/compat.sh             Task 9: the AppImage in a stock Debian 13 container
```

`work/`, `cache/`, `dist/` are gitignored. Patches are made by editing `work/src` (a git checkout), committing there,
and `git format-patch -1 -o ../../patches/` with the patch's file name.

---

### Task 1: Container image and pinned downloads

**Files:** Create `build.sh`, `pins.env`, `container/Containerfile`, `scripts/fetch.sh`.

**Interfaces:**
- Produces: image `localhost/pla-appimage-build:trixie`; `cache/<name>` per pin; `build.sh [fetch|runtime|assemble|all]` (default `all`) runs each step as `podman run --rm -v "$PWD":/w:Z -w /w localhost/pla-appimage-build:trixie scripts/<step>.sh`.
- `pins.env` lines: `PIN_<NAME>_URL=…`, `PIN_<NAME>_SHA256=…` for `UPSTREAM` (a `git archive`-free clone: fetch.sh clones and checks out `bc830dd`, no sha), `ZIG` (`zig-x86_64-linux-0.17.0.tar.xz`), `PYTHON` (python-build-standalone cpython 3.12 `x86_64-unknown-linux-gnu-install_only_stripped.tar.gz`), `WHEEL_LZ4`, `WHEEL_CAPSTONE`, `WHEEL_UNICORN` (manylinux x86_64, cp312 or abi3), `APPIMAGETOOL` (`appimagetool-x86_64.AppImage`, continuous release; pin the current file's hash), `RUNTIME` (AppImage type-2 `runtime-x86_64` from `AppImage/type2-runtime` releases).

- [ ] **Step 1:** Install podman on the host (`sudo pacman -S --needed podman`; the user types the password). Verify: `podman run --rm docker.io/library/debian:trixie cat /etc/debian_version` prints `13.*`.
- [ ] **Step 2:** Write `container/Containerfile`: `FROM docker.io/library/debian:trixie`; apt: `build-essential clang lld make pkg-config git curl ca-certificates xz-utils zstd file squashfs-tools patchelf python3 python3-venv cmake ninja-build libavcodec-dev libavformat-dev libavutil-dev libswscale-dev libshaderc-dev libvulkan-dev libwayland-dev libxkbcommon-dev libx11-dev libxext-dev libxrandr-dev libxcursor-dev libxi-dev libxss-dev libpipewire-0.3-dev libpulse-dev libasound2-dev libdbus-1-dev libudev-dev libdecor-0-dev libegl-dev`; then SDL `release-3.4.x` (latest 3.4 tag at build time, pinned in the Containerfile by tag) built with CMake into `/usr/local`, `-DSDL_STATIC=OFF`.
- [ ] **Step 3:** Verify the image: `podman build -t localhost/pla-appimage-build:trixie container/` then `podman run --rm localhost/pla-appimage-build:trixie sh -c 'ldd --version | head -1; pkg-config --modversion sdl3'`. Expected: `2.41` and `3.4.*`.
- [ ] **Step 4:** Write `scripts/fetch.sh`: for each pin, download to `cache/dl.<name>/` then move to `cache/<name>`; skip if present and the hash matches; fail with `fetch: <name> sha256 mismatch` otherwise; with `--record`, write the computed hash into an empty `PIN_*_SHA256=` in `pins.env`. Upstream: `git clone https://gitlab.com/mr.wifi1143/LZA-Recomp.git cache/upstream` then `git -C cache/upstream checkout --detach bc830dd`.
- [ ] **Step 5:** Run `./build.sh fetch` once with `--record` (fills hashes), then again without. Expected: the second run prints `fetch: all pins ok` and downloads nothing.
- [ ] **Step 6:** Commit `build.sh pins.env container/ scripts/fetch.sh` — "Build container (Debian 13 + SDL 3.4) and pinned downloads".

### Task 2: zig can build a Linux game library (risk first)

**Files:** Create `tests/zig_shared.sh`.

**Interfaces:**
- Consumes: `cache/zig/` (Task 1), the image.
- Produces: the zig flags the runtime will use — confirmed, or a recorded fallback.

- [ ] **Step 1: Write the test.** In a scratch dir in the container: `host.c` (exports `int rt_hello(int)` and `dlopen`s `./g.so`, calls its `int game_entry(void)`, prints the result), built with `clang -rdynamic host.c -ldl -o host`; `g.c` calls `rt_hello(41)` and returns it +0, compiled by the bundled zig exactly as `runtime/gamebuild.c` will: `zig cc -O2 -fno-strict-aliasing -fdenormal-fp-math=dynamic -fvisibility=hidden -mcpu=x86_64_v3 -fPIC -fprofile-instr-use=<decompressed pla.clang21.profdata> -Wno-profile-instr-unprofiled -Wno-profile-instr-out-of-date -Wno-backend-plugin -c g.c`, then `zig cc -shared -o g.so g.o`, with `ZIG_GLOBAL_CACHE_DIR`/`ZIG_LOCAL_CACHE_DIR` in the scratch dir. Mark `game_entry` with `__attribute__((visibility("default")))`. Assert `./host` prints `42`.
- [ ] **Step 2:** Run `podman run … tests/zig_shared.sh`. Expected: `zig_shared: ok (42)`. If the link fails on undefined `rt_hello`, re-run with `-Wl,--allow-shlib-undefined -Wl,-z,undefs`; if that passes, Task 4's patch also makes `GAMELIB_SHARED` for zig-on-Linux include those flags (record it in the plan's Task 4 step 3 note). If the PGO flag is rejected, record and drop PGO for zig (`LARECOMP_NO_PGO` semantics: ship without `pgo.profdata`).
- [ ] **Step 3:** Commit `tests/zig_shared.sh` — "Check zig builds a game-library-shaped .so against an -rdynamic host".

### Task 3: Package reader opens exact files (patch 0001)

**Files:** Modify `work/src/runtime/nxpkg.h`, `work/src/runtime/nxpkg.c` (`keys_find` ~l.289, `nxpkg_open` ~l.917). Create `tests/nxpkg_files_test.c`, `tests/run_unit.sh`, `patches/0001-nxpkg-open-explicit-files.patch`.

**Interfaces:**
- Produces (in `nxpkg.h`):
  - `NxPkg *nxpkg_open_files(const char *const *pkgs, int npkgs, const char *keys, const uint64_t *titles, int *err, char *why, size_t why_size);` — keys from exactly `keys` (+ `title.keys` in its folder); only the listed package files are read (`add_package`, no folder scan). Same error codes and same result object as `nxpkg_open`.
  - `bool nxpkg_keys_default(char *out, size_t n);` — the first usable key file `keys_find` would pick with no package folder (data root, `keys/`, `~/.switch`, emulators); false = none.
- `nxpkg_open` and `nxpkg_open_files` share one static function that takes the already-loaded `Keys` and a "add packages" step; no duplicated post-scan logic.

- [ ] **Step 1: Write the failing tests** in `tests/nxpkg_files_test.c` (compiled with `../work/src/runtime/nxpkg.c` and a stub `LOG`), each building its fixtures in a `mkdtemp` folder whose name contains a space and `é`:
  - `no_keys`: keys path does not exist → returns NULL, `err == NXPKG_NO_KEYS`.
  - `no_game`: keys file `header_key = ` + 64 hex zeros, one package `a b.nsp` of 64 zero bytes → NULL, `err == NXPKG_NO_GAME`.
  - `files_spaces`: same as `no_game` but the package file is named `Pokémon Legends Arceus [01001F5010DFA000][v0].nsp` → `err == NXPKG_NO_GAME` (it was opened, not "no packages found": `why` doesn't contain `no packages`).
  - `files_order`: `{update, base}` and `{base, update}` with the same two zero files → identical `err` and `why`.
  - `default_none`: `HOME` and `XDG_DATA_HOME` set to the empty temp folder, cwd there → `nxpkg_keys_default` returns false.
  - `default_switch`: `$HOME/.switch/prod.keys` written with the header key → returns true, `out` ends with `/.switch/prod.keys`.
- [ ] **Step 2:** `tests/run_unit.sh` builds with `clang -I work/src/runtime -I work/src/third_party` and runs it. Run in the container; expected: compile error (functions not declared).
- [ ] **Step 3:** Implement the two functions in `nxpkg.c` as described in Interfaces.
- [ ] **Step 4:** Run `tests/run_unit.sh`. Expected: `6/6 passed`.
- [ ] **Step 5:** In `work/src`, commit, `git format-patch -1` → `patches/0001-nxpkg-open-explicit-files.patch`. Commit `patches/0001* tests/nxpkg_files_test.c tests/run_unit.sh` here — "nxpkg: open the picked package files with the picked prod.keys".

### Task 4: Setup from three files, version check, saved paths (patch 0002)

**Files:** Modify `work/src/runtime/gamesetup.{h,c}`, `work/src/runtime/settings.{h,c}`, and (only if Task 2 required it) `work/src/runtime/gamebuild.c`. Extend `tests/nxpkg_files_test.c` → rename to `tests/unit_test.c`. Create `patches/0002-gamesetup-files-and-version.patch`.

**Interfaces:**
- Consumes: `nxpkg_open_files` (Task 3).
- Produces:
  - `void setup_start_files(const char *base, const char *update, const char *keys);` in `gamesetup.h` — the `from_packages` flow on those files, then `build_code()`, on the setup thread. `setup_start(folder)` unchanged.
  - `SETUP_FAIL_GAME_VERSION` appended **after** `SETUP_FAIL_UNPACK` in the enum (the launcher indexes its message tables by `st - SETUP_FAIL_NOT_FOUND`).
  - `bool game_is_pla_111(const char *exefs);` — main's NSO module id at 0x40, first 8 bytes == `ae e8 f1 50 dd a1 b5 a8` and program id == `TITLE_PLA`.
  - `Settings` gains `char setup_base[1024], setup_update[1024], setup_keys[1024];` read/written as `setup_base = …` lines in `larecomp.ini` (may be empty, like `mod`).

- [ ] **Step 1: Write the failing tests** in `tests/unit_test.c`:
  - `pla111_yes`: an `exefs/` with a `main` whose bytes 0x40..0x47 are the build id above and a `main.npdm` carrying `META` + `ACI0` with program id `0x01001F5010DFA000` (construct the 0x80-byte META header with the ACI0 offset at 0x70) → `game_is_pla_111` true.
  - `pla111_other_build`: same with build id byte 0 changed → false.
  - `settings_roundtrip`: `settings_defaults`, set the three paths to values with spaces and `é`, `settings_save`, zero the struct, `settings_load` → the three strings equal what was saved. (Run in a temp cwd: settings use `larecomp.ini` in the data root = cwd.)
- [ ] **Step 2:** Run `tests/run_unit.sh`. Expected: fails to compile (`game_is_pla_111`, `setup_base` unknown).
- [ ] **Step 3:** Implement. In `from_packages`, after a supported exefs is found and **before** `rt_remove_game_files(ROOT)`, reject with `SETUP_FAIL_GAME_VERSION` if `!game_is_pla_111(UNPACK_TMP "/exefs")` (the old game files stay). Apply the same check in the extracted-folder path before copying. `setup_start_files` stores the three paths in statics and reuses the thread with a mode flag. If Task 2 needed extra link flags, add them to the zig link args in `gamebuild.c` under `#ifndef _WIN32`.
- [ ] **Step 4:** Run `tests/run_unit.sh`. Expected: `9/9 passed`.
- [ ] **Step 5:** format-patch → `patches/0002-gamesetup-files-and-version.patch` (applies on top of 0001). Commit here — "Setup from base/update/keys files, 1.1.1 check, paths saved".

### Task 5: Launcher Game Files page with three pickers (patch 0003)

**Files:** Modify `work/src/runtime/ui/launcher_lg.mm` (`game_files_section` ~l.432–500, pick callbacks ~l.352–364, main loop `take(g_pick_game…)` ~l.1478). Create `patches/0003-launcher-three-pickers.patch`.

**Interfaces:**
- Consumes: `setup_start_files`, `SETUP_FAIL_GAME_VERSION`, `g_settings.setup_*` (Task 4), `nxpkg_keys_default` (Task 3).
- Produces: the page in the spec's "Launcher change" section.

- [ ] **Step 1:** Add `g_pick_base`, `g_pick_update`, `g_pick_keys` buffers + `SDLCALL` callbacks in the existing `g_pick_lock` pattern; in the main loop, `take()` each into `g_settings.setup_*` and `settings_save()`.
- [ ] **Step 2:** Rewrite the not-ready branch of `game_files_section`: three rows (label, read-only text of the path or "Not selected", **Browse...** → `SDL_ShowOpenFileDialog` with filters base `nsp;xci`, update `nsp`, keys `keys;txt` + "All files" `*`), **Recompile** (`ImGui::BeginDisabled` unless all three paths exist on disk) → `setup_start_files(...)`, and a `SmallButton` "Use an extracted dump instead..." → the existing `SDL_ShowOpenFolderDialog(picked_game…)`. On first draw, if `setup_keys` is empty, fill it from `nxpkg_keys_default`. **Change...** on the ready branch shows the same rows instead of the folder dialog.
- [ ] **Step 3:** Error placement: append to the `en`/`pt`/`en_h`/`pt_h` tables the entry for `SETUP_FAIL_GAME_VERSION`: "That update isn't 1.1.1." / "Pick the 1.1.1 update of Pokémon Legends: Arceus (the recomp is made for that version only)." (+ Portuguese). Draw the red message under the row it concerns: KEYS / KEYS_OLD → prod.keys row; GAME_VERSION / NO_BASE → update row; PACKAGE / VERSION / UNPACK / NOT_FOUND → base row; COPY / BUILD / TOOLS → under Recompile.
- [ ] **Step 4:** Build check in the container: `make -C work/src runtime-objs SHADERC=-lshaderc` succeeds with no new warnings in `launcher_lg.mm`.
- [ ] **Step 5:** format-patch → `patches/0003-launcher-three-pickers.patch`. Commit here — "Launcher: base game, update and prod.keys pickers".
  (Visual check happens in Task 8 with the real AppImage.)

### Task 6: Runtime and plugins built in the container

**Files:** Create `scripts/build-runtime.sh`.

**Interfaces:**
- Consumes: `cache/upstream` (Task 1), `patches/*.patch` (Tasks 3–5).
- Produces: `work/src/build/lasrecomp`, `work/src/build/plugins/{liblegendslua.so,libsteamrelay.so,steamrelay-helper}`, `work/src/build/pgo.profdata` (decompressed clang21 profile, for assemble).

- [ ] **Step 1:** Script: fresh `work/src` = `git clone cache/upstream work/src && git -C work/src checkout --detach bc830dd`; `git -C work/src am` each patch in name order (`build-runtime: patch <name> does not apply` on failure); pick `SHADERC` as `install.sh` does; `make -C work/src -j"$(nproc)" all legendslua steamrelay SHADERC=…`; `zstd -d data/pgo/pla.clang21.profdata.zst -o build/pgo.profdata`.
- [ ] **Step 2:** Run `./build.sh runtime`. Expected: the four binaries exist; `ldd work/src/build/lasrecomp` (in the container) resolves `libSDL3.so.0` to `/usr/local/lib`; `objdump -T` of `lasrecomp` lists `setup_start_files`.
- [ ] **Step 3:** Commit `scripts/build-runtime.sh` — "Build the patched runtime and plugins in Debian 13".

### Task 7: AppDir and AppImage

**Files:** Create `scripts/assemble.sh`, `appimage/AppRun`, `appimage/plarecomp.desktop`.

**Interfaces:**
- Consumes: Task 6 outputs, `cache/{zig,python,wheel_*,appimagetool}`.
- Produces: `dist/PLA-Recomp-1.2.0-rc1-x86_64.AppImage` with the spec's AppImage layout.

- [ ] **Step 1:** `AppRun`: `HERE=$(dirname "$(readlink -f "$0")")`; `export LD_LIBRARY_PATH="$HERE/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"`; then **seed the data root** (upstream reads `plugins/` and `mods/` only from the data root, `runtime/plugin.c:170`, `runtime/mods.c:54`): `D=${LARECOMP_DATA_ROOT:-${XDG_DATA_HOME:-$HOME/.local/share}/plarecomp}` (XDG only if absolute, as `paths.c`); `mkdir -p "$D/plugins" "$D/mods" "$D/lua-examples"`; each `$HERE/plugins/*` copied over when missing or different (`cmp -s`), so an updated AppImage updates its plugins; each `$HERE/mods/<name>` copied only if `$D/mods/<name>` doesn't exist (Lush Foliage builds its trees into its own folder: never overwritten); `lua-examples/*` copied when missing. Then `exec "$HERE/lasrecomp" "$@"`. Seeding never touches `larecomp.ini`, so everything stays off. `.desktop`: `Name=PL:A Recomp`, `Exec=lasrecomp`, `Icon=plarecomp`, `Categories=Game;`, `Terminal=false`.
- [ ] **Step 2:** `assemble.sh` copies, into a fresh `work/AppDir`, exactly the spec's layout list. Specifics the spec doesn't fix:
  - libs: walk `ldd` of `lasrecomp` and the three plugins recursively; copy every resolved lib **except** names matching `ld-linux|libc\.so|libm\.so|libdl|libpthread|librt|libresolv|libvulkan|libGL|libEGL|libGLX|libGLdispatch|libdrm|libgbm|libX|libxcb|libwayland|libxkbcommon|libasound|libpulse|libpipewire|libdbus|libudev|libsystemd|libgcc_s|libstdc\+\+`; then `patchelf --set-rpath '$ORIGIN/lib'` on `lasrecomp`. (libstdc++ is excluded because Debian 13's is older than every target distro's.)
  - python: unpack `cache/python` → `python/`; `python/bin/python3 -m pip install --no-index --no-deps --target python/lib/python3.12/site-packages cache/wheel_*`; `ln -s bin/python3 python/python`; delete `python/lib/python3.12/{test,idlelib,tkinter,ensurepip,lib2to3}` and `__pycache__` dirs.
  - zig: unpack → `zig/`; delete `doc/`, `lib/libc/include/*` not matching `generic-glibc|x86_64-linux-gnu|x86-linux-any|any-linux-any|generic-musl` (keep these), `lib/libc/{musl,wasi,mingw,darwin,freebsd*,netbsd*,openbsd*}` ; keep `lib/compiler_rt*`, `lib/libc/glibc`, `lib/std`, `lib/include`.
  - mods/plugins/lua: `contrib/mods/.` → `mods/`; Task 6 plugins → `plugins/`; `contrib/legendslua/research/pla_research.lua` → `lua-examples/`.
  - Licenses: as upstream `make_zip.sh` plus `zig/LICENSE`, `python/LICENSE*`, each wheel's license, `SDL3` (zlib) from the SDL source.
  - The "nothing from the game" check from upstream `make_zip.sh` over `work/AppDir`; any hit fails the build.
  - Plugins have no RPATH (they're copied into the data root by AppRun and link only libc / libm / libdl: assert with `ldd`).
  - `ARCH=x86_64 cache/appimagetool --appimage-extract-and-run --runtime-file cache/runtime work/AppDir dist/PLA-Recomp-1.2.0-rc1-x86_64.AppImage` (the type-2 runtime pinned too, so packing needs no network).
- [ ] **Step 3:** Run `./build.sh assemble`. Expected: the AppImage exists; `--appimage-extract` of it lists `lasrecomp zig/zig python/python gen/gen.py gamelib-src/pgo.profdata plugins/liblegendslua.so mods/Lush Foliage/mod.ini`; `objdump -T` of every bundled ELF shows no `GLIBC_2.4[2-9]` symbol versions (built against 2.41).
- [ ] **Step 4:** Commit `scripts/assemble.sh appimage/` — "Assemble the AppDir and pack the AppImage".

### Task 8: End-to-end smoke test on this PC

**Files:** Create `tests/smoke.sh`.

**Interfaces:**
- Consumes: the AppImage (Task 7), the extracted dump `PLA/` (read-only).

- [ ] **Step 1:** Write `tests/smoke.sh`: copies the AppImage into `$SCRATCH/My Games/` (space in the path); `LARECOMP_DATA_ROOT=$SCRATCH/data`, the AppImage started with `env -i HOME=… DISPLAY=… WAYLAND_DISPLAY=… XDG_RUNTIME_DIR=… PATH=/nonexistent` (no clang, no python reachable; the script itself calls tools by absolute path); `LARECOMP_SETUP_DIR=<PLA dump>`; runs the AppImage in the background with its log to `$SCRATCH/run1.log`; waits (Monitor, until `[setup] ready` or `[setup] failed`, timeout 60 min).
  Asserts: log has `[gamebuild] generator …/python/python …/gen/gen.py, compiler …/zig/zig cc` (bundled tools, from the mount path); `[setup] ready`; `$SCRATCH/data/extracted/lasgame.so` and `.id` exist; AppRun seeded `$SCRATCH/data/plugins/{liblegendslua.so,libsteamrelay.so,steamrelay-helper}`, `$SCRATCH/data/mods/Lush Foliage/mod.ini`, `$SCRATCH/data/lua-examples/pla_research.lua`; `larecomp.ini` has empty `mod =` and `plugins_on =`.
- [ ] **Step 2:** Run it. Expected: all asserts pass. Record the recompile time.
- [ ] **Step 3: Interrupted setup:** start a second clean data root, kill the AppImage 60 s into `Recompiling...`, start it again without `LARECOMP_SETUP_DIR`. Expected: no crash; Game Files shows the pickers (not "ready"), and a setup started again reaches `[setup] ready`.
- [ ] **Step 4: Stale lasgame:** in the data root from Step 2, overwrite `extracted/lasgame.so.id` with `0000000000000000`, start. Expected: the log shows the game code rebuilt (or the launcher offers to rebuild), not a load of the stale library.
- [ ] **Step 5: GUI checks** (by eye, screenshots into `$SCRATCH`): Game Files page shows the three rows with prod.keys pre-filled when `~/.switch/prod.keys` exists; Recompile disabled with any row empty; Mods page lists Legends Lua, Steam relay and Lush Foliage, all unticked; Play boots the game past the Nintendo logo to the title screen.
- [ ] **Step 6:** Commit `tests/smoke.sh` — "Smoke test: clean data root, bundled tools only, interrupted and stale setups".

### Task 9: Compatibility in a stock Debian 13

**Files:** Create `tests/compat.sh`.

- [ ] **Step 1:** Script: `podman run --rm` stock `debian:trixie` (no build deps), install only `mesa-vulkan-drivers libgl1 libegl1 libwayland-client0 libx11-6 libxkbcommon0 libasound2 libpulse0 xvfb`, mount the AppImage, run `xvfb-run ./PLA-Recomp-*.AppImage --appimage-extract-and-run` with `LARECOMP_DATA_ROOT=/tmp/d`, `VK_ICD_FILENAMES` pointing at lavapipe; kill after 20 s.
- [ ] **Step 2:** Run. Expected: log contains `[paths] data: /tmp/d` and the launcher's first frame line (`[launcher] window`), no `error while loading shared libraries`, no `GLIBC_2.` "not found".
- [ ] **Step 3:** Commit `tests/compat.sh` — "Compat test: the AppImage starts on a stock Debian 13".

### Task 10: README

**Files:** Create `README.md`.

- [ ] **Step 1:** Write: what it is; requirements (x86-64 AVX2, Vulkan 1.3, ~25 GB free during setup, FUSE or `--appimage-extract-and-run`); using it (three pickers → Recompile → Play; where data lives; turning on mods/plugins and what each does, Lush Foliage's "needs a decent PC"); building it (`./build.sh`, needs podman); updating upstream (change the commit in `fetch.sh`/Global Constraints, rebuild; a failed `git am` means a patch needs refreshing); the "no game files" rule; credits and licenses pointing to upstream.
- [ ] **Step 2:** Commit `README.md` — "README: using and building the PL:A Recomp AppImage".
