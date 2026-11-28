# Changelog

## 2026-11-27

- Saving no longer says "Save failed" after a save that was written (it happened when the same turn was saved twice). The save before it is kept as .bak.
- START menu in the house order: RESUME, MANUAL, SAVE GAME, SETTINGS, EXIT GAME. The Menu button on the touch screen opens the game's panels (Research, Tax Rates, ...) and has START Menu... at the end.
- Left hand can be changed during a game (START, SETTINGS) and in the manual (Left hand: off / on).
- EXIT GAME asks YES / NO like the other ports and saves first; a failed exit save asks EXIT ANYWAY / NO. It also asks before the first turn.
- SELECT asks once before ending a turn while units still wait, like the End Turn button.
- An order from the bar is named on the map ("Settlers: Build City"). If no city can be built, the map says why. Picking research says "Now researching ...".
- D-pad and A work in the Load Game list and in the research, government, diplomacy, messages and Encyclopedia lists (gold ring).
- Load Game rows start with Saved or Autosave; years read BCE / CE as in the game. Title menu labels are centred in their rows.
- The two button rows at the bottom of the touch screen are taller and clear of the screen edge.
- settings.ini is written through a temporary file.

## 2026-11-21

- Turn autosaves now go into five rotating slots (freeciv-auto-1 to freeciv-auto-5) instead of one new file per turn. The last five turns are always kept, and the saves folder no longer grows and slows the Load list. Your older autosaves are left alone.
- "Save failed" now shows above combat news, so you cannot miss it.
- Cut-off fixed in long unit and city names in pop-ups.
- The debug shortcut that started a quick game is no longer in the release build.

## 2026-11-20

- HOME > Close saves the game in play, as Save & Exit does.
- The log file is small and buffered; the engine's detailed log and timing lines need an empty /3ds/freeciv/debug.txt.
- SELECT is End Turn and START is the menu everywhere (hints and README agree).
- The title screen shows the build version.

## 2026-11-19

- Quitting waits for the engine thread instead of tearing it down.
- Combat news appears as a toast on the map.
- Manual: drag the stylus to scroll, with a little momentum; section buttons act on a tap.
- 3D: panels and text stay at screen depth; only the minimap has depth.
