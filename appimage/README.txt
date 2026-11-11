PL:A Recomp - AppImage

A native Linux port of Pokémon Legends: Arceus (update 1.1.1) by static recompilation: the game's code is translated
and compiled on your PC from your own game files. No game files, game code or keys are in this AppImage.

Using it
  1. Make it executable and start it:  chmod +x PLA-Recomp-*.AppImage && ./PLA-Recomp-*.AppImage
     (no FUSE on your system: add --appimage-extract-and-run)
  2. Game Files: pick your base game (.nsp / .xci), its 1.1.1 update (.nsp) and your prod.keys, all dumped from your
     own Switch. Press Recompile: the game code is built for this PC once (several minutes; ~25 GB free needed).
  3. Play.

Your data (game files set up, saves, settings, mods, plugins, logs): ~/.local/share/plarecomp
(or $XDG_DATA_HOME/plarecomp, or $LARECOMP_DATA_ROOT).

Shipped, OFF until you turn them on in the Mods page:
  Lush Foliage   3x grass, flowers and small plants and ~19,000 more trees (built from your own files on first use).
                 Needs a decent PC.
  Legends Lua    Lua scripting for mod makers: scripts in <data>/lua/ (an example is in <data>/lua-examples/).
                 Only run scripts you trust.
  Steam relay    Link Trades with Steam friends anywhere (Steam must be running).

Requirements: x86-64 CPU with AVX2, a GPU with a Vulkan 1.3 driver, a desktop with X11 or Wayland.

Built from LZ&A Recomp (https://gitlab.com/mr.wifi1143/LZA-Recomp, branch LegendsA). MIT licensed; the bundled
components keep their own licenses (licenses/). Pokémon Legends: Arceus is © Nintendo / Creatures Inc. / GAME FREAK
inc. / The Pokémon Company; this project is not affiliated with or endorsed by them.
