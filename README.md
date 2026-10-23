# Freeciv 3DS

[Freeciv](https://www.freeciv.org/), the open-source Civilization-style strategy game, as a Nintendo 3DS
homebrew app with a handheld interface built for the two screens and the stylus.

The real Freeciv engine (server, rulesets, map generator and AI) is cross-compiled for the 3DS and runs
in-process. A native `citro2d` client draws the game and talks to the engine directly, so there is no
network client. Everything ships as a single `freeciv3ds.3dsx`.

This is a fork of [xPsycho999/freeciv3ds](https://github.com/xPsycho999/freeciv3ds), which got the
engine running on the 3DS. This fork rebuilds the interface, adds sound, music and stereo 3D, and
supports the Old 3DS.

## Features

- **DS-style two-screen layout**, modelled on *Civilization Revolution* and *Anno* for the DS:
  - **Bottom screen (touch): the map.** Drag to scroll and zoom in five steps. Tap your unit to
    command it and open its orders. Tap a neighbouring square to move, or tap a farther square twice
    to send the unit along a route. Tapping a stack or a city with units gives a picker.
  - **Top screen (information only):**
    - status bar with turn, gold, government and tax rates;
    - minimap with a frame showing where the bottom screen is looking;
    - research progress;
    - a card for whatever you selected: a unit (portrait, attack/defense/firepower, HP, moves,
      activity, home city) or a tile (terrain, improvements, resource, city, units).
- **Unit orders menu** lists only what the unit can do right now: Build City, Join City, Build Road,
  Irrigate, Mine, Fortify, Sentry, Explore, Cancel Orders, Wait, Done, and Disband (which asks first).
  After you give orders, the next unit that needs them is selected automatically.
- **Map labels**: city name plates with size, owner shields, health bars for wounded units,
  activity badges (Fortified, Sentry, Working, Exploring, Goto), and dimmed units that are done.
- **Cities**: yields, growth and production bars, and the units inside (lead one out with Y). Change
  production, keep a build queue, and buy.
- **Civilization**: research (list and tech tree), tax rates, government and revolutions, and
  diplomacy with the AI.
- **New Game setup**: map size, number of rivals and AI skill.
- **Sound and music**: Freeciv's sound set (unit moves, combat, cities, technology) plus a rotation of
  CC0 medieval music. Volumes are in the Menu.
- **Stereo 3D** on the top screen, driven by the 3D slider: panels float gently in front of the screen.
- **Crisp text**: the UI font is pre-rendered at exact pixel sizes, not scaled.
- Save and load to the SD card, plus autosaves. Suspending with HOME is safe.

## Controls

| | |
|---|---|
| Stylus on the map | Tap a unit: command it and show its orders. Tap a neighbouring square: move. Tap a farther square twice: route. Drag: scroll. |
| Menu / Next / − + / End Turn | Bottom-row buttons (Menu: Research, Tax Rates, Government, Diplomacy, Save, Music, Effects) |
| D-Pad | Move the selected unit (or scroll if none is selected) |
| Circle Pad | Scroll |
| A | Selected unit's orders, or the next unit |
| X | Next unit |
| B | Back, close, or cancel a route |
| L / R | Research / Tax rates |
| ZL / ZR | Zoom out / in (New 3DS) |
| START | End turn |
| SELECT + START | Quit |
| In a city | Tap an item to build it now (or to queue it), L/R to pick a unit, Y to lead it out |

## Old 3DS and New 3DS

The same `.3dsx` runs on both:

- **New 3DS:** 804 MHz mode on, and the engine on its own CPU core.
- **Old 3DS (64 MB, 268 MHz):** New Game defaults to a Small map with 2 rivals, and is capped at a
  Medium map with 4 rivals so turns stay quick and memory holds.

Sound needs a DSP firmware dump (`/3ds/dspfirm.cdc`, made by the DSP1 homebrew). Without one, the game
runs silently.

There is no DS version. The DS has 4 MB of RAM, far less than the Freeciv engine needs.

## Installing

Copy `freeciv3ds.3dsx` to `/3ds/` on the SD card and start it from the Homebrew Launcher. Saves go to
`/3ds/freeciv/saves/` and settings to `/3ds/freeciv/settings.ini`. CI builds every push; the latest
build is on the `builds` branch.

## Building

GitHub Actions does all of this (`.github/workflows/build.yml`). To build locally you need
[devkitPro](https://devkitpro.org/) with `3ds-dev`, `3ds-curl`, `3ds-mbedtls` and `3ds-zlib`, plus the
usual autotools.

```sh
scripts/build_freeciv_core.sh   # fetch upstream Freeciv at the pinned commit, patch it, build the
                                # engine as static ARM11 libraries, copy rulesets into romfs
cd 3ds_port && make             # build the client -> 3ds_port.3dsx
```

`tools/make_font.py` (needs Pillow) regenerates the font atlas, and `tools/make_audio.sh` (needs
ffmpeg) regenerates the audio. Both outputs are committed, so a normal build needs neither tool.

## How it works

- **The engine runs as a cooperative coroutine.** It runs only when the UI loop lets it and parks at
  one yield point, so the UI reads game state only while the engine is parked. That makes reads
  race-free and suspend safe.
- **One read path, one write path.** The UI snapshots engine state into plain structs, and every
  action goes through a small command ring into Freeciv's own validated handlers. Freeciv's game
  logic is unpatched; `port-patches/freeciv-core.patch` only adds build-system and headless glue.

| Path | Contents |
|---|---|
| `3ds_port/source/` | The client: `main.c` (input and screens), `render.c` (drawing), `game_loop.c` (engine glue), `fc3ds_server.c` (engine coroutine), `font.c`, `audio.c` |
| `3ds_port/gfx/` | Sprites cut from Freeciv's `trident` tileset, plus the font atlas |
| `3ds_port/romfs/` | Server scripts and audio (rulesets are added by the build) |
| `port-patches/` | Changes applied to upstream Freeciv |
| `scripts/`, `tools/` | Engine build, font and audio generators |

## Credits and licences

- **Freeciv** engine, rulesets, `trident` tileset and sound set: the Freeciv project, GPL-2.0-or-later.
- **Original 3DS port**: [xPsycho999](https://github.com/xPsycho999/freeciv3ds), GPL-2.0-or-later.
- **Menu music** "Dawning Fanfare": Cullam Bruce-Lockhart (Freeciv `stdmusic`).
- **Game music** "Exploration", "Harvest Season", "Market Day" and "The Bard's Tale": RandomMind,
  [opengameart.org](https://opengameart.org/users/randommind), CC0.
- **Inter** font: Rasmus Andersson, SIL Open Font License 1.1 (`licenses/Inter-LICENSE.txt`).

This program is free software under the GNU General Public License, version 2 or later; see `LICENSE`.
