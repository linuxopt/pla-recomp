# PL:A Recomp — Pokémon Legends: Arceus for Linux, as an AppImage

A native Linux PC port of **Pokémon Legends: Arceus** (update 1.1.1), packaged as a single AppImage. It's built from
[LZ&A Recomp](https://gitlab.com/mr.wifi1143/LZA-Recomp) by MrWifi, a **static recompilation**: the game's own code is
translated and compiled for your PC, from **your own** game files, on your PC. Not an emulator.

> **No game files, game code or keys are in this repository or the AppImage**, and none ever will be. You need your own
> dump of the game from your own Switch. Pokémon Legends: Arceus is © Nintendo / Creatures Inc. / GAME FREAK inc. /
> The Pokémon Company. This project is not affiliated with or endorsed by them.

What this AppImage adds on top of upstream: everything bundled (no compiler, Python or libraries to install), a
launcher where you pick your **base game**, **update** and **prod.keys** and press **Recompile**, a **Cheats** tab
(with code cheats working in a recompiled game), a **Field of view** slider, **Render scale / Upscaler** options, and
fixes (see [The patches](#the-patches)).

---

## Download

Get **`PLA-Recomp-1.2.0-rc1-x86_64.AppImage`** from the
[latest release](https://github.com/linuxopt/pla-recomp/releases/latest) (156 MB). The release page also has its
SHA-256 checksum.

## What you need

**A PC:**

- 64-bit x86 CPU with **AVX2** (Intel 2013 / Haswell or newer, AMD Zen or newer)
- A GPU with a **Vulkan 1.3** driver (AMD / Intel: Mesa; NVIDIA: the proprietary driver)
- A Linux desktop (X11 or Wayland) from about 2025 or newer: **Debian 13, Ubuntu 25.04, Fedora 42, Arch / CachyOS /
  Manjaro** or anything newer (built against glibc 2.41)
- About **25 GB free** while the game is set up (about 8 GB afterwards)

**From your own Switch:**

| File | What it is | How to get it |
|---|---|---|
| Base game | `.nsp` or `.xci` of Pokémon Legends: Arceus | dumped from your Switch (e.g. with nxdumptool) |
| Update **1.1.1** | `.nsp` of the 1.1.1 update | dumped the same way — **only 1.1.1 works** |
| `prod.keys` | your console's keys | dumped with Lockpick_RCM |

Compressed dumps (`.nsz` / `.xcz`) aren't read: decompress them first (`nsz -D file.nsz`). If you already use an
emulator, the launcher finds `prod.keys` by itself in `~/.switch` or the usual Eden / Yuzu / Ryujinx key folders.

## Install

```bash
chmod +x PLA-Recomp-1.2.0-rc1-x86_64.AppImage
```

```bash
./PLA-Recomp-1.2.0-rc1-x86_64.AppImage
```

That's all — it can live anywhere (a folder with spaces is fine). If your system has no FUSE (the AppImage says
"FUSE" / "fusermount" when it won't start), either install it (`sudo apt install libfuse2`, `sudo dnf install fuse`,
`sudo pacman -S fuse2`) or run it with `--appimage-extract-and-run`. To get a menu entry, use a tool such as Gear Lever
or AppImageLauncher, or make a `.desktop` file pointing at the AppImage.

## First start: set up the game

1. Click **Set up game** (or **Game Files**).
2. **Browse...** next to each row and pick your **base game**, your **1.1.1 update** and your **prod.keys**.
   (prod.keys is often filled in already.)
3. Click **Recompile**. The game's code is read out of your dumps, translated and compiled for your PC. This happens
   **once**: about 6–8 minutes on a fast 16–24-thread CPU, longer on smaller ones. **Leave the window open** until it
   finishes — closing it stops the build (it starts again next time).
4. Click **Play**. The first start also prepares the shaders once (a progress bar); short stutters the first time a
   new effect appears are normal.

Already have extracted files (`exefs/` with `main`, `main.npdm`… and `romfs/` with `bin/`, the 1.1.1 update applied)?
Use **Use an extracted dump instead...** on the same page.

If something is wrong, the message appears under the row it's about — e.g. "That update isn't 1.1.1" under Update,
or "Your keys can't open those game files" under prod.keys. Your earlier setup is never touched by a failed one.

## Playing

- **Controllers**: Xbox, PlayStation, Switch Pro and others, with button prompts to match (Options > Game).
- **Keyboard and mouse**: WASD, mouse camera (click to take the mouse, Esc to let it go), mouse in the menus.
  Remap everything in **Controls**.
- **Settings** are in the launcher (**Options**) and in the game (Menu > Help > Settings).
- **Saves** are ordinary Switch save files, so PKHeX opens them; **PKHeX Link** (Settings) lets PKHeX's LiveHeX
  connect to the running game.
- Hold **Shift** while starting to see the launcher again if you ticked "Skip the launcher".

### Options worth knowing

| Where | Option | |
|---|---|---|
| Options > Display | Resolution, Fullscreen, SMAA, Frame rate | 30, 60, 72, 120, 144 or Unlimited |
| Options > Graphics | **Render scale** | with Resolution on Automatic: 100 / 75 / 50 / 25 % of the window, or Dynamic (lowered to keep the frame rate) |
| Options > Graphics | **Upscaler** | AMD FSR 1 (sharp) or bilinear, when rendering below the window's size |
| Options > Graphics | **Field of view** | 40–100° (the game: 50°); aiming and close-ups keep their zoom |
| Options > Graphics | Sharpening, Shadows, Shadow color, Draw distance, Shader loading | |

Upstream marked frame rates above 60 as experimental. This build offers them as ordinary choices: in testing, 144 fps
played fine through story scenes. A cutscene crash at 144 fps (the game reading an object that was already gone) is
fixed. If a scripted scene ever stalls, set 60 or 30 fps in the game's Settings and try the scene again.

## Cheats

**Mods > Cheats.** The AppImage ships the
[GBAtemp Pokémon Legends: Arceus cheat database](https://gbatemp.net/threads/pokemon-legends-arceus-cheat-database.606215/)'s
1.1.1 file — 41 cheats (infinite money, merits and health, max items and Poké Balls, shiny Pokémon, always level 100,
100 % catch rate, infinite PP, six IVs at 31, easy research tasks…), credited in [`cheats/CREDITS.txt`](cheats/CREDITS.txt).

- All cheats start **off**. Tick the ones you want; they apply from the game's next start.
- Cheats that **rewrite the game's code** are compiled into the game code. The bundled ones already are, from your first
  setup. For a cheat file you import, those cheats show "(rebuild the game code first)" until you click
  **Rebuild the game code with these cheats** (once per cheat file, about as long as the first setup). After that every
  cheat switches on and off freely. Nothing is rebuilt unless you ask.
- **Import cheats...** takes another Atmosphère cheat file for 1.1.1 (`AEE8F150DDA1B5A8.txt`) or a cheat database's
  `.zip` (the file for this version is taken out of it). Your own import is never replaced by the bundled file, and if
  you delete the bundled file it stays deleted.
- Cheat databases come with "off" twins ("Inf Health (off)", "(OFF) …", "… Off"). You don't need them here: to turn a
  cheat off, untick it. The launcher greys the twins out and says which cheat to untick instead.
- **Back up your save first** (`~/.local/share/plarecomp/save/`). Cheats change the game's memory directly.

## Mods and plugins

**Mods** in the launcher. Shipped, all **off** until you tick them:

| | |
|---|---|
| **Lush Foliage** | 3× the grass, flowers and small plants, ~19,000 more trees (built from your own game files the first time). Needs a decent PC. |
| **Legends Lua** | Lua scripting for mod makers: scripts in `~/.local/share/plarecomp/lua/`, an example in `lua-examples/`. Only run scripts you trust. |
| **Steam relay** | Link Trades with Steam friends anywhere (Steam must be running). |

**Add a mod...** adds a mod you downloaded (a `romfs/` replacement, a texture pack, or a plugin `.so`); several mods can
be on at once, in a load order. **Open the mods folder** shows where they live, if you'd rather copy one in by hand.

## Where your files are

Everything the game writes — the set-up game files, saves, settings, mods, plugins, cheats and logs — is in
**`~/.local/share/plarecomp`** (or `$XDG_DATA_HOME/plarecomp`; `LARECOMP_DATA_ROOT=<folder>` picks another). The
AppImage itself is never written to. To remove everything, delete that folder and the AppImage.

## Updating

Download the new AppImage and run it instead of the old one. Your setup, saves, settings and cheats stay. If the new
version translates the game code differently, the launcher rebuilds the game code by itself on its first start (a few
minutes, as at setup).

## Troubleshooting

The log is **`~/.local/share/plarecomp/lasrecomp.log`** (or the terminal, when you start the AppImage from one).

| Problem | What to do |
|---|---|
| "No keys found" / "Your keys can't open those game files" | Dump `prod.keys` again with Lockpick_RCM on your Switch's current system version, and pick it. |
| "That update isn't 1.1.1" | Only update 1.1.1 is supported: dump the 1.1.1 update. |
| "Those are compressed game files" | Decompress `.nsz` / `.xcz` first: `nsz -D <file>`. |
| "The game code didn't build" | Check there are ~25 GB free; the log says what failed. |
| The log says `stopped: PL:A Recomp was closed during the build` | The window was closed (or the app got a quit signal) while building — just start it again. |
| It won't start, mentions FUSE | Install FUSE (see Install), or use `--appimage-extract-and-run`. |
| "needs a CPU with AVX2" / a Vulkan error | The PC doesn't meet the requirements, or the GPU driver lacks Vulkan 1.3 — install your distribution's current GPU driver (`vulkaninfo` shows the version). |
| A scripted scene stalls above 60 fps | Set 60 or 30 fps in the game's Settings and try the scene again. |

---

## Building it yourself

Needs **podman** (rootless). Everything else happens in a Debian 13 container, so the AppImage runs on distros older
than the build machine.

```bash
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
  bundled cheats, the "nothing from the game" and glibc checks, appimagetool.

Podman on a kernel without the `overlay` / `tun` modules (some CachyOS kernels): install `fuse-overlayfs` and set
`mount_program = "/usr/bin/fuse-overlayfs"` under `[storage.options.overlay]` in `~/.config/containers/storage.conf`;
the build uses host networking.

### The patches

Applied in order on top of upstream `LegendsA` at `bc830dd` (v1.2.0-rc1):

| | |
|---|---|
| `0001-nxpkg-open-explicit-files` | the package reader opens exactly the picked files with exactly the picked prod.keys |
| `0002-gamesetup-files-and-version` | setup from the three files; the 1.1.1 check; the picked paths saved in `larecomp.ini` |
| `0003-launcher-three-pickers` | the Game Files page: base game / update / prod.keys rows, Recompile, errors under their row |
| `0004-rebuild-stale-game-code` | the game code is rebuilt by itself when the game files are set up but the code is missing or outdated |
| `0005-launcher-minimize-crash` | the launcher no longer crashes when minimized on the home page (upstream divides by a zero-width banner) |
| `0006-cheats` | Atmosphère cheat files in a Cheats tab on the Mods page; the ones ticked run every frame in a dmnt-compatible cheat VM |
| `0007-launcher-upscaling-options` | Render scale and Upscaler on the Graphics tab (both already in the renderer and `larecomp.ini`) |
| `0008-field-of-view` | the Field of view setting: a hook on the camera update (`f_700193b65c`) widens the frame's render camera (`[[x0+0x3d0]+0x78]+0x1ec`) in tangent space |
| `0009-helper-exit-logging` | the log says how a helper process ended (exit code or signal); `LARECOMP_PROC_LOG=1` traces every helper started, waited for and ended |
| `0010-code-cheats` | cheats that rewrite instructions: every fixed instruction write in the cheat file is compiled in next to the original (`gen.py --code-variants`, the randomizer's `RANDO_CODE` path; `rando_word` picks the word on), and the cheat VM's writes into main's code switch those variants; `lasgame.so.variants` says which list the game code has, and a different one rebuilds it |
| `0011-gamebuild-closed-during-build` | closing the launcher mid-build is logged as that, not as a generator failure |
| `0012-code-cheats-registers-caves` | code cheats written through a not-yet-set offset register, and code caves (new code past main's code, e.g. a master code's hook function, compiled as a function of its own) |
| `0013-code-cheats-rebuild-opt-in` | a new cheat file no longer rebuilds the game code by itself: the Cheats tab offers it; the self-rebuild requires 1.1.1 |
| `0014-cheats-master-code-checked` | the master code runs only when all its code (and cave) is compiled in, like any cheat |
| `0015-code-cave-data` | a cave's code is what control flow reaches from the cheats' branches; the rest is the cave's data, written to memory (Inf Health) |
| `0016-gen-variant-branch-targets` | a code variant's branch target becomes a function start (else the game code wouldn't load) |
| `0017-setup-part-files` | the game data images are written as `.part` and renamed when complete: a setup cut short isn't taken as set up |
| `0018-cheats-off-pairs` | a database's "off" cheats (`X (off)`, `(OFF) X`, `X Off`) are paired with their cheat by name, shown as not needed and left out (ticked with their partner, they undid it) |
| `0019-high-frame-rates-not-experimental` | 72 / 120 / 144 / Unlimited fps are ordinary Frame rate choices: no Experimental settings box, kept as saved |
| `0020-cutscene-expired-weak-reference` | a cutscene that reached an object already gone no longer crashes (signal 11 at 0x118, seen at 144 fps): the game's own "nothing here" result is given instead |

Moving to a newer upstream: change `PIN_UPSTREAM_COMMIT` in `pins.env` and rebuild. A patch that no longer applies
stops the build with its name: refresh it in `work/src` (edit, `git commit`, `git format-patch -1`).

### Tests

| | |
|---|---|
| `tests/run_unit.sh` | C unit tests of the patched reader, setup and settings, the cheats (parser, VM, import, on / off, code variants, caves), the field of view, high frame rates kept and the cutscene reference check (`./build.sh shell tests/run_unit.sh`) |
| `tests/apprun_test.sh` | AppRun in a fake AppDir: the data folder is pinned for the runtime, shipped files seeded, bundled cheats seeded once |
| `tests/gen_variants_test.py` | the generator's code-cheat variants and caves with the real lifter (`work/AppDir/python/bin/python3 -I tests/gen_variants_test.py work/src/tools`) |
| `tests/zig_shared.sh` | zig links a game-library-shaped `.so` against the runtime (`ZIG_DIR=` for the AppDir's trimmed zig) |
| `tests/contents.sh` | the AppImage holds everything, nothing from the game, nothing needing glibc > 2.41 |
| `tests/smoke.sh setup\|play\|stale\|interrupt\|unpackkill\|rebuild\|quit [dump]` | the AppImage end to end from an extracted dump, bundled tools only (no compiler or Python on PATH), no window (SDL offscreen) |
| `tests/compat.sh` | the AppImage on a stock Debian 13 with a desktop's libraries: everything resolves, the launcher starts |
| `tests/gui_pages.sh` | screenshots of the launcher's Game Files, Mods, Cheats, Options and Graphics pages on a virtual display (in a container) |
| `tests/gui_minimize.sh` | the launcher survives being minimized (needs an X11 desktop and xdotool) |

Never commit `cache/`, `work/` or `dist/` (gitignored): `work/` holds the recompiled game code of test runs.

`cheats/` holds the bundled cheat file and its credits (`CREDITS.txt`: the GBAtemp thread and its contributors). The
thread gives no license for the codes: ask its maintainer before distributing them widely.

## Credits and licenses

[LZ&A Recomp](https://gitlab.com/mr.wifi1143/LZA-Recomp) by MrWifi, built on letsgo-recomp (raulzrrs) and LAS Recomp
(Peiikai), MIT licensed. Cheats: the GBAtemp thread's maintainer OblivionReign and its contributors (see
`cheats/CREDITS.txt`). Every bundled component's license is in the AppImage's `licenses/` folder.
