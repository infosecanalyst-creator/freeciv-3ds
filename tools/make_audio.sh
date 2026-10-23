#!/bin/sh
# Rebuilds 3ds_port/romfs/audio from its sources (outputs are committed).
#   tools/make_audio.sh <freeciv-source-dir> <dir-with-the-CC0-mp3s>
# Sound effects: Freeciv data/stdsounds (GPL-2+, see its soundspec for authors).
# Menu music: Freeciv data/stdmusic "Dawning Fanfare" by Cullam Bruce-Lockhart.
# Game music (CC0, RandomMind, opengameart.org): Exploration, Harvest Season,
#   Market Day (loop), The Bard's Tale.
set -e
FC=$1; MP3=$2; OUT=$(dirname "$0")/../3ds_port/romfs/audio
mkdir -p "$OUT/sfx" "$OUT/music"
conv() { ffmpeg -v error -y -i "$1" -ac 1 -ar 22050 -c:a adpcm_ima_wav "$2"; }
for f in "$FC"/data/stdsounds/*.ogg; do conv "$f" "$OUT/sfx/$(basename "$f" .ogg).wav"; done
conv "$FC/data/stdmusic/CullamBruce-Lockhart--Dawning_Fanfare.ogg" "$OUT/music/menu.wav"
conv "$MP3/Exploration_0.mp3"       "$OUT/music/exploration.wav"
conv "$MP3/harvestseason_2.mp3"     "$OUT/music/harvest_season.wav"
conv "$MP3/Loop_Market_Day_0.mp3"   "$OUT/music/market_day.wav"
conv "$MP3/The_Bards_Tale.mp3"      "$OUT/music/bards_tale.wav"
