# PL:A Recomp AppImage — design

Date: 2026-11-10. Scope: Pokémon Legends: Arceus only (upstream branch `LegendsA`). Z-A comes later, as its own project.

## Goal

One self-contained `PLA-Recomp-<version>-x86_64.AppImage` that a Linux player downloads and runs. In its launcher they
pick their **base game**, their **1.1.1 update** and their **prod.keys**, press **Recompile**, and the game code is
generated and compiled on their PC — then **Play**. Every feature, plugin and mod of the upstream Arceus recomp is in it.

Success criteria:

1. The AppImage runs on Debian 13 / Ubuntu 25.04 / Fedora 42 / Arch and newer (x86-64, AVX2, Vulkan 1.3) with
   nothing installed beyond a GPU driver and FUSE (or `--appimage-extract-and-run`).
2. Nothing from the game is in the AppImage or in this repository (no code, no assets, no keys).
3. From a clean data folder, the three pickers → Recompile → Play reaches the game, using only the AppImage's tools.
4. All bundled plugins and mods appear in the Mods page, **off** by default, and work when switched on.

## What the user decided

- Arceus only; Z-A later.
- Pick base game file, update file and prod.keys in the GUI launcher, then recompile.
- Bundled mods and plugins are **off** until the user turns them on (as upstream).
- **Wide compatibility**: built in a Debian 13 container (glibc 2.41), not on the glibc 2.44 host.

Assumptions (stated to the user, not objected to): zig 0.17 is the bundled compiler; an extracted-dump option stays as
a secondary button; the project lives in `PLA/appimage-build/`.

## Upstream, pinned

`https://gitlab.com/mr.wifi1143/LZA-Recomp`, branch `LegendsA`, commit `bc830dd` (v1.2.0-rc1). Fetched at build time,
never vendored. Our changes are patch files applied on top (`patches/`), so moving to a newer upstream is re-pinning
and re-applying.

## What goes in the AppImage (the inventory)

Built into the runtime (`lasrecomp`), no switch needed: 30/60/120/144 fps with the game at the right speed, draw
distance / shadows / shadow colour / round bloom / RCAS sharpening / SMAA / render scale to 4K, keyboard + mouse with
remapping, Xbox / PlayStation / keyboard button prompts, the game's Settings screen rows, PKHeX saves + PKHeX Link
(LiveHeX, port 6000), `.pa8` import / export, Link Trades on this PC and LAN, texture packs, several mods in a load order.

Shipped, off until switched on in the Mods page:

| Item | Upstream source | In the AppImage |
|---|---|---|
| Legends Lua (Lua 5.3 scripting plugin) | `contrib/legendslua/` | `plugins/liblegendslua.so` |
| Legends Lua example script | `contrib/legendslua/research/pla_research.lua` | `lua-examples/pla_research.lua` (copied by the user into `<data>/lua/`) |
| Steam relay (Link Trades with Steam friends) | `contrib/steamrelay/` | `plugins/libsteamrelay.so`, `plugins/steamrelay-helper` |
| Lush Foliage (3x plants, +19k trees built from the player's files) | `contrib/mods/Lush Foliage/` | `mods/Lush Foliage/` (seeded into `<data>/mods/` on first start) |

Also: the PGO profile (`data/pgo/pla.clang21.profdata.zst` → `gamelib-src/pgo.profdata`; zig 0.17 is clang 22, the
clang21 family per the Makefile), `data/shader_manifest.txt` if upstream has it, icon, Baloo2 font, every license.

## AppImage layout

The runtime finds shipped files next to its executable (`rt_resource`, `runtime/paths.c`), so the AppDir root is the
executable's folder:

```
AppDir/
  AppRun                     sh: sets LD_LIBRARY_PATH=$APPDIR/lib, execs $APPDIR/lasrecomp "$@"
  lasrecomp                  the runtime (RPATH $ORIGIN/lib as well)
  lib/                       SDL3, libavformat/avcodec/avutil/swscale (+ their deps), libshaderc_shared, libstdc++ if newer
  python/bin/python          portable CPython 3.12 (python-build-standalone), lz4 / capstone / unicorn in site-packages
  gen/                       gen.py lifter.py analyze.py nso.py passes.py patches.py simd_c.py
  zig/                       zig 0.17.0 x86_64-linux, trimmed to what compiling C for x86-64 Linux (glibc) needs
  gamelib-src/               cpu.h simd.h simd_fp.h gamelib.h gamelib_entry.c hle_symbols.txt pgo.profdata
  plugins/  mods/  lua-examples/
  icon.png  Baloo2-SemiBold.ttf  plarecomp.desktop  plarecomp.png  licenses/  README.txt
```

Not bundled (from the system, on purpose): glibc, libvulkan + GPU drivers, libGL / EGL, X11 / Wayland client libs,
PipeWire / PulseAudio (SDL loads them at run time), FUSE.

The runtime's generator lookup is `python/python` + `gen/gen.py`; on Linux the portable Python lives at
`python/bin/python3`, so the AppDir gets a `python/python` symlink to it. The runtime runs `python gen/gen.py` (no
`-I`); that is safe because `gen/` holds only the seven upstream scripts we copy, so nothing can shadow a module.

Data (writable) stays where upstream already puts it on Linux: `$XDG_DATA_HOME/plarecomp` (`~/.local/share/plarecomp`):
`extracted/` (exefs, romfs.bin, `lasgame.so`), `save/`, `cache/`, `mods/`, `plugins/` state, `larecomp.ini`, logs.

## Launcher change: three pickers

Today the Game Files page has one **Select game folder...** (`runtime/ui/launcher_lg.mm`), and prod.keys is searched
for in that folder and the emulators' folders (`keys_find`, `runtime/nxpkg.c`).

New Game Files page (not yet set up):

```
Base game        [ /path/Pokemon Legends Arceus.nsp        ] [Browse...]
Update (1.1.1)   [ /path/Pokemon Legends Arceus v1.1.1.nsp ] [Browse...]
prod.keys        [ /home/me/.switch/prod.keys              ] [Browse...]
                 [                Recompile                ]   (enabled when all three are set)
Use an extracted dump instead...  (small link: the old folder picker, for exefs + romfs dumps)
```

- File filters: base `*.nsp *.xci`; update `*.nsp`; keys `prod.keys keys.txt *.keys`.
- prod.keys is pre-filled with the first key file `keys_find` would have found (~/.switch, emulators), so most users
  only browse twice. `title.keys` next to the picked prod.keys is still read.
- The three paths are saved in `larecomp.ini` (`setup_base`, `setup_update`, `setup_keys`) so a failed or interrupted
  recompile keeps them.
- Errors show under the picker they're about: keys missing / too old → prod.keys; compressed / not Arceus / unreadable
  → base or update; wrong version → update ("This is v1.x.x; the recomp needs update 1.1.1").
- When set up: "Pokémon Legends: Arceus — ready to play" and **Change...** opens the same three pickers.

Code changes (as patches against upstream):

1. `runtime/nxpkg.{h,c}`: `nxpkg_open_files(const char *const *pkgs, int n, const char *keys, titles, err, why, wn)`:
   loads the keys from that exact file (+ `title.keys` beside it), then `add_package` / scans each given file only.
   `nxpkg_open` and `nxpkg_open_files` share one internal function (no duplicated logic). `keys_default(out)` exposes
   the search for the pre-fill.
2. `runtime/gamesetup.{h,c}`: `setup_start_files(base, update, keys)` — the existing `from_packages` path with the
   explicit files; after writing the exefs, the main's build id must be Arceus 1.1.1 (`aee8f150dda1b5a8`, the one
   `tools/patches.py` has addresses for), else the new `SETUP_FAIL_GAME_VERSION`. The folder path (`setup_start`) is
   unchanged and still used by the extracted-dump link.
3. `runtime/settings.{h,c}`: the three saved paths.
4. `runtime/ui/launcher_lg.mm`: the page above (English and Portuguese strings, like the rest of it).

## Build pipeline

Host entry point `./build.sh` (needs podman; runs rootless). Everything else happens in the container.

1. `container/Containerfile`: Debian 13 (trixie) + clang, lld, make, pkg-config, git, curl, xz, zstd, file,
   squashfs-tools, patchelf, FFmpeg / shaderc / Vulkan dev packages, Python 3. **SDL 3.4.x built from source** in the
   image if Debian's libsdl3-dev is older than upstream needs (upstream calls `SDL_LoadPNG`, which is SDL 3.4).
2. `scripts/fetch.sh`: downloads and **sha256-checks** each pinned input into `cache/`: upstream at `bc830dd`, zig
   0.17.0 x86_64-linux, python-build-standalone 3.12 install_only, the lz4 / capstone / unicorn manylinux cp312 wheels,
   appimagetool + its type-2 runtime. Each download in its own empty folder, as untrusted data.
3. `scripts/build-runtime.sh`: applies `patches/*.patch` (fails loudly if one doesn't apply), then
   `make all legendslua steamrelay` with the Debian flags (`SHADERC=-lshaderc_shared` or `-lshaderc` as found).
4. `scripts/assemble.sh`: lays out the AppDir as above; copies shared libs found with `ldd` minus an exclude list
   (glibc, vulkan, GL, X11, wayland, drm, asound, pulse, pipewire...); sets RPATH; installs the wheels into the portable
   Python; trims zig; decompresses the PGO profile; seeds licenses; runs the "nothing from the game" check from upstream's
   `make_zip.sh` (no `lasgame*`, `.nso`, `.nca`, `.nsp`, `.xci`, keys, `exefs`, `romfs`).
5. `appimagetool` → `dist/PLA-Recomp-1.2.0-rc1-x86_64.AppImage`.

## Error handling

- Build: each script is `set -euo pipefail`; a patch that no longer applies, a checksum mismatch or a missing lib stops
  the build with the step's name.
- Runtime: upstream's existing messages (tools missing, disk space, build failure → `lasrecomp.log`), plus the
  per-picker messages above. The AppRun prints where the log is.

## Testing

1. **Patch unit check**: the runtime builds with the patches; `nxpkg_open_files` with a missing keys path returns
   `NXPKG_NO_KEYS`; with a keys file but no package returns `NXPKG_NO_GAME` (small C test linked against nxpkg.o).
2. **AppImage smoke test** (`tests/smoke.sh`), with `LARECOMP_DATA_ROOT` set to an empty scratch folder:
   - the AppImage starts and shows the launcher;
   - setup from the local extracted 1.1.1 dump (`LARECOMP_SETUP_DIR`, upstream's hook) recompiles **with the
     bundled python + zig only** (PATH stripped of clang/python), reaches `setup_ready()`; the log shows the bundled
     tool paths;
   - Play boots the game to its title screen (screenshot);
   - the Mods page lists Legends Lua, Steam relay and Lush Foliage, all off; Lush Foliage switched on builds its trees.
3. **Compatibility**: the AppImage runs inside a stock Debian 13 container (with the host's GPU passed through, or at
   least to the launcher with lavapipe).
4. **Three pickers**: exercised in the GUI; the full package path (nsp + update + keys) needs the user's `.nsp` dumps —
   the local dump is extracted, so that end-to-end run is done when the user provides them (or by the user).

## Risks

- zig cc building the Linux game library: upstream only ships zig for Windows; Linux uses clang in dev mode. `-shared`
  with the runtime's symbols left undefined must link (lld allows it by default). Checked first, before the rest.
- zig trimming too much (it builds compiler_rt / libc stubs on first use): trim conservatively, verified by the smoke
  test from a clean zig cache.
- Debian 13's FFmpeg (7.1) vs upstream's development FFmpeg (9.0): upstream's install.sh supports Debian 13, so it
  should build; if not, FFmpeg is built from source in the container like SDL.
- Size: roughly 250–350 MB (zig and unicorn dominate). Acceptable; noted in the README.

## Out of scope

Z-A; a Windows build (upstream has one); shipping any game data; changes to the recompiler itself.
