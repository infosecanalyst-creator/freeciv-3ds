# Freeciv 3DS

[Freeciv](https://www.freeciv.org/), the open-source empire-building strategy game, as a Nintendo 3DS
homebrew app with a handheld interface built for the two screens and the stylus.

> Not affiliated with or endorsed by the Freeciv project, Firaxis, Take-Two or Nintendo. Civilization is a trademark of Take-Two Interactive.

The real Freeciv engine (server, rulesets, map generator and AI) is cross-compiled for the 3DS and runs
in-process. A native `citro2d` client draws the game and talks to the engine directly, so there is no
network client. Everything ships as a single `freeciv3ds.3dsx`.

This is a fork of [xPsycho999/freeciv3ds](https://github.com/xPsycho999/freeciv3ds), which got the
engine running on the 3DS. This fork rebuilds the interface, adds sound, music and stereo 3D, and
supports the Old 3DS.

## Features

- **Two-screen layout**:
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
- **Turn report**: what happened while the AI moved (battles, cities growing or starving, technologies,
  first contact, treaties) on the top screen at the start of each turn, most important first. The full
  log is under Menu > Messages; tap a message to jump to where it happened.
- **Combat odds and routes**: tapping an enemy next to your unit shows the chance to win (and both units'
  HP) before you commit. Marking a route draws the path and how many turns it takes.
- **Caravans, diplomats and more**: next to a foreign city a unit is offered everything it can do there
  (embassy, investigate, steal tech, sabotage, incite revolt, trade route, sell goods) besides Attack. In
  a city: Help Wonder, Disband for shields, Upgrade and Make Home City. Also Pillage.
- **Encyclopedia** (Menu): every unit, building, wonder, technology and government, with its picture,
  stats, what it leads to, and the ruleset's help text.
- **Map labels**: city name plates with size, owner shields, health bars for wounded units,
  activity badges (Fortified, Sentry, Working, Exploring, Goto), and dimmed units that are done.
- **Cities**: yields, growth and production bars, and the units inside (lead one out with Y). Three
  tabs: **Build** (change production, keep a queue, buy), **Tiles** (the city's land as a grid with
  food/shields/trade per tile; tap to put a citizen to work or free one, switch specialists) and
  **Buildings** (sell one per turn).
- **Civilization**: research (list and tech tree), tax rates, government and revolutions, and
  diplomacy with the AI.
- **New Game setup**: rules (Classic or Civ2Civ3), map size, land (islands, continents or pangaea),
  number of rivals, AI skill and barbarians. Your choices are remembered.
- **End of game**: victory or defeat screen with your score.
- **Sound and music**: Freeciv's sound set (unit moves, combat, cities, technology) plus a rotation of
  CC0 medieval music. Volumes are in the Menu.
- **Stereo 3D** on the top screen, driven by the 3D slider: panels float gently in front of the screen.
- **Crisp text**: the UI font is pre-rendered at exact pixel sizes, not scaled.
- Save and load to the SD card, plus autosaves. Suspending with HOME is safe.

## Manual

The full instruction manual is built into the game: Manual on the title screen, or Menu, then Manual, in a game. Its text is [3ds_port/romfs/manual.txt](3ds_port/romfs/manual.txt); a `manual.txt` in `sdmc:/3ds/freeciv/` replaces it.

## Controls

| | |
|---|---|
| Stylus on the map | Tap a unit: command it and show its orders. Tap a neighbouring square: move. Tap a farther square twice: route. Drag: scroll. |
| Menu / Next / − + / End Turn | Bottom-row buttons (Menu: Research, Tax Rates, Government, Diplomacy, Save, Music, Effects) |
| D-Pad | Move the selected unit (or scroll if none is selected) |
| Circle Pad | Scroll |
| A | Selected unit's orders, or the next unit (also puts the turn report away) |
| X | Next unit |
| B | Back, close, or cancel a route |
| L / R | Research / Tax rates |
| ZL | Swap screens (3D and touch off while swapped) |
| ZR | Zoom in (wraps back to the widest view); the on-screen + and - also work |
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

- **CIA** (shows on the HOME menu with its own banner): install `freeciv3ds.cia` with FBI.
- **3dsx**: copy `freeciv3ds.3dsx` to `/3ds/` and start it from the Homebrew Launcher.
- **Over Wi-Fi**: in the Homebrew Launcher press Y, then `python3 tools/3dslink.py <3ds-ip> freeciv3ds.3dsx`.

Saves go to `/3ds/freeciv/saves/` and settings to `/3ds/freeciv/settings.ini`. CI builds every push; the
latest `.3dsx` and `.cia` are on the `builds` branch, and pushing a `v*` tag publishes a GitHub Release.

## Building

GitHub Actions does all of this (`.github/workflows/build.yml`). To build locally you need
[devkitPro](https://devkitpro.org/) with `3ds-dev`, `3ds-curl`, `3ds-mbedtls` and `3ds-zlib`, plus the
usual autotools.

```sh
scripts/build_freeciv_core.sh   # fetch upstream Freeciv at the pinned commit, patch it, build the
                                # engine as static ARM11 libraries, copy rulesets into romfs
cd 3ds_port && make             # build the client -> 3ds_port.3dsx
```

`tools/make_font.py` (needs Pillow) regenerates the font atlas, `tools/make_pedia_icons.py` the
encyclopedia icons, and `tools/make_audio.sh` (needs
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

- **Freeciv** engine, rulesets, `trident` and `Amplio2` tilesets, encyclopedia icons and the `stdsounds`
  sound set (artists listed in Freeciv's `data/stdsounds.soundspec`): the Freeciv project, GPL-2.0-or-later.
  The HOME-menu banner jingle is Freeciv's `stdsounds/fanfarehappy.ogg`, shortened.
- **Original 3DS port**: [xPsycho999](https://github.com/xPsycho999/freeciv3ds), GPL-2.0-or-later.
- **Menu music** "Dawning Fanfare" by Cullam Bruce-Lockhart: shipped with Freeciv in `data/stdmusic`
  (Freeciv patch #4753), distributed under Freeciv's licence, GPL-2.0-or-later.
- **Game music** by RandomMind, released under CC0 1.0 (public domain dedication) on OpenGameArt:
  [Exploration](https://opengameart.org/content/medieval-exploration),
  [Harvest Season](https://opengameart.org/content/medieval-harvest-season),
  [Market Day](https://opengameart.org/content/medieval-market-day) and
  [The Bard's Tale](https://opengameart.org/content/medieval-the-bards-tale).
- **UI click**: synthesized by `tools/make_click.py` (part of this project, GPL-2.0-or-later).
- **Inter** font: Rasmus Andersson, SIL Open Font License 1.1 (`licenses/Inter-LICENSE.txt`).
- **Cinzel** font (title logo): The Cinzel Project Authors, SIL Open Font License 1.1 (`licenses/Cinzel-OFL.txt`).

This program is free software under the GNU General Public License, version 2 or (at your option) any
later version (GPL-2.0-or-later); see `LICENSE`. Freeciv files changed by `port-patches/freeciv-core.patch`
are marked "Modified for freeciv-3ds" when the build applies the patch.
