#!/bin/bash
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
TILESET="$SCRIPT_DIR/../freeciv/data/trident/tiles.png"
DEST="$SCRIPT_DIR/gfx"

magick "$TILESET" -crop 30x30+60+0 "$DEST/grassland.png"
magick "$TILESET" -crop 30x30+120+0 "$DEST/hills.png"
magick "$TILESET" -crop 30x30+240+0 "$DEST/forest.png"
magick "$TILESET" -crop 30x30+360+0 "$DEST/mountains.png"
magick "$TILESET" -crop 30x30+450+30 "$DEST/desert.png"
magick "$TILESET" -crop 30x30+450+180 "$DEST/arctic.png"
magick "$TILESET" -crop 30x30+450+180 "$DEST/tundra.png"
magick "$TILESET" -crop 30x30+450+90 "$DEST/jungle.png"
magick "$TILESET" -crop 30x30+450+120 "$DEST/plains.png"
magick "$TILESET" -crop 30x30+450+150 "$DEST/swamp.png"
magick "$TILESET" -crop 30x30+450+300 "$DEST/ocean.png"

echo "Cropping done!"
