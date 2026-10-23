# Freeciv 3DS (fork)

A fork of [xPsycho999/freeciv3ds](https://github.com/xPsycho999/freeciv3ds) (GPL-2.0-or-later) working toward a
fuller 3DS client: the map on the top screen with zoom, touch controls on the bottom screen, sound and
music, readable text and stereo 3D. CI builds `freeciv3ds.3dsx` on every push (see `.github/workflows`).

---

A New Nintendo 3DS homebrew port of [Freeciv](https://www.freeciv.org/), the open-source
Civilization-style strategy game.

The Freeciv **server core** — the full game engine, rulesets, map generator and AI, with no desktop
client — is cross-compiled to the 3DS's ARM11 processor. A custom `libctru`/`citro2d` client renders
the game across the console's two screens and reads the engine's state **in-process** (there is no
network client): the whole thing runs as a single ~10 MB `.3dsx` homebrew app. It runs on the
[Azahar](https://azahar-emu.org/) emulator and on real 3DS hardware with custom firmware (developed and
tested on a New 3DS — see [Hardware](#hardware)).

> **Status:** playable end-to-end. You can start or load a game, explore, found and manage cities,
> research tech, change government, conduct diplomacy with the AI, and play turns through to a
> decision — all on the handheld.

## Features

- **Real, playable game loop** — start menu (New Game / Load Game), a generated overhead map with one
  human civ and several AI opponents, and turn advancement (**A** = End Turn) with the AI taking its
  turns, founding cities and waging war.
- **Two-screen UI** — the bottom screen renders the map (`citro2d`, 60 FPS): terrain with
  cardinal-matched tiling, per-type unit and city sprites, coastlines, fog-of-war, owner-tinted
  borders, and road/rail/river/resource/hut/pollution overlays. The top screen is a HUD featuring a **real-time Minimap**, along with turn, selected tile, government, and control info.
- **Unit control** — D-Pad / Circle-Pad pan; touch selects a tile 1:1; touch your own unit to arm it,
  D-Pad moves it (8 directions, corner = diagonal), moving into an enemy attacks/conquers. **Multi-turn
  goto**: arm a unit, tap **GOTO**, tap a destination — the engine's own pathfinder routes it and
  continues across turns until it arrives.
- **City & civilization management** — tap a city for its production / buy / worklist / garrison
  screen; a touch action bar opens Research (with a tech-tree view), tax rates, Government (with
  revolutions), and Diplomacy (propose / accept / cancel treaties with the AI).
- **Save / Load** on demand to the SD card, plus autosaves, with a newest-first load picker.
- **HOME suspend/resume safe** — the app parks the engine before the OS freezes it.
- **Dev quick-game preset** — hold **L+R** while tapping New Game for a tiny, fast-starting test world
  (small map, one AI nearby, tech pre-granted).

## How it works

Two design invariants keep the in-process client and the full Freeciv engine coexisting safely on a
single console:

- **Cooperative-coroutine server.** The engine runs as one gated coroutine that keeps its own stack
  across yields, runs only when the main loop ticks it, and parks at a single yield point. Exactly one
  of {UI thread, engine} is runnable at a time — this is what makes HOME/suspend safe and every read of
  game state race-free.
- **One read path, one write path.** The UI reads engine state only while the engine is parked
  (snapshotting into plain structs); all UI actions go through a small command ring drained on the
  engine thread into Freeciv's own self-validating handlers. The port adds features this way with **no
  patches to Freeciv's game logic** (only build-system and headless-server glue; see `port-patches/`).

## Repository layout

| Path | Contents |
|---|---|
| `3ds_port/` | The custom 3DS client — source (`source/`), Makefile, sprite art (`gfx/`), and server config (`romfs/*.serv`). **This is the port.** |
| `port-patches/` | What's applied to upstream Freeciv: `freeciv-core.patch` (build / headless glue), the ARM configure script, and the CMake toolchain file. |
| `scripts/`, `3ds_port/crop.sh` | Asset pipeline that slices the sprite art out of Freeciv's GPL `trident` tileset. |

The upstream `freeciv/` source clone and all build artifacts are not tracked (see `.gitignore`).

## Building

Prerequisites: the [devkitPro](https://devkitpro.org/) 3DS toolchain (`devkitARM`, `libctru`,
`citro2d`) installed at `/opt/devkitpro`, plus ImageMagick (for the art pipeline) and, optionally,
`azahar` to run it.

1. **Freeciv core** — clone upstream Freeciv into `freeciv/`, apply `port-patches/freeciv-core.patch`,
   then configure and build it for ARM (server-only, static, `--disable-icu --disable-nls`) using
   `port-patches/run_configure_3ds.sh`. This produces the static libraries the client links against.
2. **Client**
   ```sh
   export DEVKITPRO=/opt/devkitpro DEVKITARM=/opt/devkitpro/devkitARM
   export PATH=$PATH:/opt/devkitpro/tools/bin:/opt/devkitpro/devkitARM/bin
   cd 3ds_port && make
   ```
   Output: `3ds_port/3ds_port.3dsx`.
3. **ROMFS data** — populate `3ds_port/romfs/data/` with the Freeciv **server** ruleset data (from
   Freeciv's `data/` dir). Only server rulesets are needed, not client tilesets.

## Running

- **Emulator:** `azahar -w 3ds_port/3ds_port.3dsx`
- **Hardware:** copy the `.3dsx` to `sdmc:/3ds/` and launch it from the Homebrew Launcher (see
  [Hardware](#hardware) for which consoles are supported).

## Hardware

This is unsigned homebrew, so it needs a console running **custom firmware** (e.g.
[Luma3DS](https://github.com/LumaTeam/Luma3DS)); it launches from the Homebrew Launcher. There is no
strict New-3DS-only dependency in the code, though it takes advantage of New 3DS features — specifically a CPU clock boost
(`osSetSpeedupEnable`) and **Core Affinity** (pinning the Freeciv engine thread to the spare ARM11 core) — which gracefully fall back to default behavior on older hardware.

- **New 3DS / New 3DS XL / New 2DS XL** — developed and tested here. The faster ARM11 (with the clock
  boost and Core Affinity) and the larger 256 MB RAM comfortably run the engine, AI, and a full-size map without stalling the renderer.
- **Old 3DS / 3DS XL / 2DS** — expected to work (nothing in the port is New-3DS-only), but **currently
  untested**. Without the clock boost, world generation and AI turns will be noticeably slower, and the
  smaller 128 MB RAM may require a smaller map — the L+R "dev" quick-game preset (tiny map) is a good
  starting point there.

## Controls

| Input | Action |
|---|---|
| Touch | Select a tile / your unit; tap a city to open it; drive all menus |
| D-Pad / Circle Pad | Move the armed unit / pan the camera |
| A | End Turn |
| B / X / Y | Fortify-Sentry / context action (found city, build road…) / disband |
| L / R | Research / tax-rate panels |
| SELECT | Re-center on your units |
| Action bar (touch) | Research, Rates, Government, Diplomacy, Save, Goto |
| START | Quit to the Homebrew Launcher |

## License

Freeciv is licensed under the GNU General Public License, version 2 or later. This port links the
Freeciv engine and uses art derived from Freeciv's `trident` tileset, so it is distributed under the
**GPL-2.0-or-later** as well (see `3ds_port/LICENSE`).

## Acknowledgements

- The [Freeciv](https://www.freeciv.org/) project and its contributors — the engine, rulesets, AI, and
  the `trident` tileset this port's art is sliced from.
- [devkitPro](https://devkitpro.org/) — `devkitARM`, `libctru`, and `citro2d`.
