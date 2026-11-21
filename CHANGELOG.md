# Changelog

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
