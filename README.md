# PL:A Recomp AppImage

Builds a self-contained Linux AppImage of [LZ&A Recomp](https://gitlab.com/mr.wifi1143/LZA-Recomp)'s Pokémon Legends:
Arceus port (branch `LegendsA`, v1.2.0-rc1, commit `bc830dd`): the static recompilation's runtime, the tools that
recompile the game code on the player's PC (a portable Python with the generator, and zig), every plugin and mod
upstream ships, and a launcher where the player picks their **base game**, their **1.1.1 update** and their
**prod.keys**, then presses **Recompile**.

**No game files, game code or keys are in this repository or the AppImage**, and none ever will be. The player brings
their own dump. The build checks the AppImage for anything from the game and fails if it finds something.

Pokémon Z-A is a separate project, in its own folder.

## Using the AppImage

```sh
chmod +x PLA-Recomp-1.2.0-rc1-x86_64.AppImage
./PLA-Recomp-1.2.0-rc1-x86_64.AppImage        # no FUSE: add --appimage-extract-and-run
```

1. **Game Files**: pick the base game (`.nsp` / `.xci`), the 1.1.1 update (`.nsp`) and `prod.keys` (dumped from your
   own Switch with Lockpick_RCM; found by itself if it's in `~/.switch` or an emulator's key folder). Anything other
   than Legends: Arceus 1.1.1 is refused before your earlier setup is touched. An already-extracted dump (`exefs/` +
   `romfs/`) works too: **Use an extracted dump instead...**.
2. **Recompile**: the game code is generated and compiled for your PC once (about 8 minutes on a 24-thread Ryzen;
   longer on smaller CPUs; ~25 GB free during setup).
3. **Play.** After an AppImage update whose generator changed, the launcher rebuilds the game code by itself
   (the game files stay).

Data (game files set up, saves, settings, mods, plugins, logs): `~/.local/share/plarecomp` (or
`$XDG_DATA_HOME/plarecomp`, or `$LARECOMP_DATA_ROOT`). The log is `lasrecomp.log` there.

Requirements: x86-64 with AVX2, a Vulkan 1.3 GPU driver, an X11 or Wayland desktop. Runs on Debian 13, Ubuntu 25.04,
Fedora 42, Arch and newer (built against glibc 2.41).

### What's in it

Built into the runtime: 30 / 60 / 120 / 144 fps at the game's real speed, draw distance, shadows, round bloom, RCAS
sharpening, SMAA, render scale up to 4K, keyboard and mouse with remapping, Xbox / PlayStation / keyboard button
prompts, PKHeX saves and PKHeX Link (LiveHeX, port 6000), `.pa8` import / export, Link Trades on this PC and the LAN,
texture packs, several mods in a load order.

Shipped **off**, turned on in the launcher's Mods page:

| | |
|---|---|
| **Lush Foliage** | 3x the grass, flowers and small plants, ~19,000 more trees (built from your own game files on first use). Needs a decent PC. |
| **Legends Lua** | Lua 5.3 scripting for mod makers: scripts in `<data>/lua/`; an example in `<data>/lua-examples/`. Only run scripts you trust. |
| **Steam relay** | Link Trades with Steam friends anywhere (Steam must be running). |

Added by this AppImage (launcher, Options and Mods pages):

| | |
|---|---|
| **Cheats** (Mods > Cheats) | Ships with the GBAtemp cheat database's 1.1.1 file (41 cheats: infinite money, merits and health, max items, shiny Pokémon, always level 100, 100 % catch rate, infinite PP, six IVs 31, easy research tasks...), credited in `cheats/CREDITS.txt`; *Import cheats...* takes another `AEE8F150DDA1B5A8.txt` or a cheat database's `.zip`. Tick each one on or off (all off at first); they run from the game's next start. Cheats that change values run live. Cheats that change the game's code are compiled into the game code as switchable variants (once per cheat file, about as long as the first setup: the launcher does it by itself, or *Rebuild the game code with these cheats*), then toggle like the others; one whose new instructions can't be translated stays greyed out. Back up your save first. |
| **Field of view** (Options > Graphics) | 40-100 degrees (the game: 50). The field camera widens; aiming and close-ups keep their zoom. Legends: Arceus 1.1.1 only. |
| **Render scale / Upscaler** (Options > Graphics) | With Resolution on Automatic: render at 100 / 75 / 50 / 25 % of the window, or Dynamic (lowered to hold the frame rate); AMD FSR 1 or bilinear brings it up to the window. |

## Building it

Needs podman (rootless). Everything else happens in a Debian 13 container, so the AppImage runs on older distros than
the build machine.

```sh
./build.sh            # image, fetch, runtime, assemble -> dist/PLA-Recomp-<version>-x86_64.AppImage
```

Steps on their own: `./build.sh image | fetch | runtime | assemble | shell`.

- `pins.env`: every download (upstream commit, zig 0.17.0, python-build-standalone 3.12, the lz4 / capstone / unicorn
  wheels, appimagetool, the AppImage runtime) with its sha256. `scripts/fetch.sh` refuses a mismatch;
  `./build.sh fetch --record` fills an empty hash. appimagetool and the runtime are "continuous" releases: when they
  change upstream, re-pin them on purpose.
- `container/Containerfile`: Debian 13 + build dependencies + SDL 3.4 from its release tarball (upstream needs 3.4).
- `scripts/build-runtime.sh`: a fresh upstream checkout, `patches/*.patch` applied with `git am`, then
  `make all legendslua steamrelay`.
- `scripts/assemble.sh`: the AppDir (the runtime finds its tools next to its executable), bundled libraries, the
  "nothing from the game" and glibc checks, appimagetool.

Podman on a kernel without the `overlay` / `tun` modules (some CachyOS kernels): install `fuse-overlayfs` and set
`mount_program = "/usr/bin/fuse-overlayfs"` under `[storage.options.overlay]` in `~/.config/containers/storage.conf`;
the build uses host networking.

### The patches

| | |
|---|---|
| `0001-nxpkg-open-explicit-files` | the package reader opens exactly the picked files with exactly the picked prod.keys |
| `0002-gamesetup-files-and-version` | setup from the three files; the 1.1.1 check; the picked paths saved in `larecomp.ini` |
| `0003-launcher-three-pickers` | the Game Files page: base game / update / prod.keys rows, Recompile, errors under their row |
| `0004-rebuild-stale-game-code` | the game code is rebuilt by itself when the game files are set up but the code is missing or outdated |
| `0005-launcher-minimize-crash` | the launcher no longer crashes when minimized on the home page (upstream divides by a zero-width banner) |
| `0006-cheats` | Atmosphère cheat files (a `<build id>.txt` or a cheat database's `.zip`) in a Cheats tab on the Mods page; the ones ticked run every frame in Atmosphère's cheat VM. Cheats that rewrite the game's code can't work in a recompilation and are greyed out |
| `0007-launcher-upscaling-options` | Render scale and Upscaler on the Graphics tab (both already in the renderer and `larecomp.ini`) |
| `0008-field-of-view` | the Field of view setting: a hook on the camera update (`f_700193b65c`) widens the frame's render camera (`[[x0+0x3d0]+0x78]+0x1ec`) in tangent space |
| `0009-helper-exit-logging` | the log says how a helper process ended (exit code or signal); `LARECOMP_PROC_LOG=1` traces every helper started, waited for and ended |
| `0010-code-cheats` | cheats that rewrite instructions: every fixed instruction write in the cheat file is compiled in next to the original (`gen.py --code-variants`, the randomizer's `RANDO_CODE` path; `rando_word` picks the word on), and the cheat VM's writes into main's code switch those variants; `lasgame.so.variants` says which list the game code has, and a different one rebuilds it |

Moving to a newer upstream: change `PIN_UPSTREAM_COMMIT` in `pins.env` and rebuild. A patch that no longer applies
stops the build with its name: refresh it in `work/src` (edit, `git commit`, `git format-patch -1`).

### Tests

| | |
|---|---|
| `tests/run_unit.sh` | C unit tests of the patched reader, setup and settings, the cheats (parser, VM, import, on / off) and the field of view (`./build.sh shell tests/run_unit.sh`) |
| `tests/zig_shared.sh` | zig links a game-library-shaped `.so` against the runtime (`ZIG_DIR=` for the AppDir's trimmed zig) |
| `tests/contents.sh` | the AppImage holds everything, nothing from the game, nothing needing glibc > 2.41 |
| `tests/smoke.sh setup\|stale\|interrupt\|rebuild [dump]` | the AppImage end to end from an extracted dump, bundled tools only (no compiler or Python on PATH) |
| `tests/compat.sh` | the AppImage on a stock Debian 13 with a desktop's libraries: everything resolves, the launcher starts |
| `tests/gui_pages.sh` | screenshots of the launcher's Game Files, Mods, Cheats, Options and Graphics pages on a virtual display (in a container) |
| `tests/gui_minimize.sh` | the launcher survives being minimized (needs an X11 desktop and xdotool) |

Never commit `cache/`, `work/` or `dist/` (gitignored): `work/` holds the recompiled game code of test runs.

`cheats/` holds the bundled cheat file and its credits (`CREDITS.txt`: the GBAtemp thread and its contributors). The thread
gives no license for the codes: ask its maintainer before distributing them widely.

## Credits and licenses

LZ&A Recomp by MrWifi, built on letsgo-recomp (raulzrrs) and LAS Recomp (Peiikai), MIT licensed. Every bundled
component's license is in the AppImage's `licenses/`. Pokémon Legends: Arceus is © Nintendo / Creatures Inc. /
GAME FREAK inc. / The Pokémon Company. This project is not affiliated with or endorsed by them.
