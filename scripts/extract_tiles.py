# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 xPsycho999 (original Freeciv 3DS port)
# Copyright (C) 2026 the freeciv-3ds authors
import os
from PIL import Image

# Base terrain tiles: native 30x30, drawn to fill a whole grid cell. Never resize these.
TERRAIN_TAGS = {
    "t.l0.grassland1": "grassland",
}

MATCH_TERRAINS = ["plains", "desert", "arctic", "tundra", "swamp", "jungle", "hills", "forest", "mountains"]
for terrain in MATCH_TERRAINS:
    for idx in range(16):
        n, e, s, w = (idx >> 0) & 1, (idx >> 1) & 1, (idx >> 2) & 1, (idx >> 3) & 1
        tag = f"t.l0.{terrain}_n{n}e{e}s{s}w{w}"
        TERRAIN_TAGS[tag] = f"{terrain}_n{n}e{e}s{s}w{w}"


# Coast corner cells: native 15x15, drawn four-to-a-tile as quadrant overlays.
COAST_TAGS = {}
for letter in ['u', 'd', 'l', 'r']:
    for i in range(8):
        bits = f"{i//4}{(i//2)%2}{i%2}"
        tag = f"t.l0.coast_cell_{letter}{bits}"
        COAST_TAGS[tag] = f"coast_cell_{letter}{bits}"

# Whole-tile cardinal-matched coast frame sprite (layer1): the main coastline
# border/curve, native 30x30. Bit order in the tag is fixed N,E,S,W (freeciv's
# cardinal_index_str order); each digit is 1 if that neighbor is also water,
# 0 if it's land. (trident also ships a layer2 pass with opaque white "sea
# foam" highlights, but it reads as a solid white block at this tile size -
# skipped, see render.c.)
COAST_WHOLE_TAGS = {}
for idx in range(16):
    n, e, s, w = (idx >> 0) & 1, (idx >> 1) & 1, (idx >> 2) & 1, (idx >> 3) & 1
    tag = f"t.l1.coast_n{n}e{e}s{s}w{w}"
    COAST_WHOLE_TAGS[tag] = f"coast_l1_n{n}e{e}s{s}w{w}"

ROAD_TAGS = {}
ROAD_TAGS["road.road_isolated:0"] = "road_isolated"
ROAD_TAGS["road.rail_isolated:0"] = "rail_isolated"
ROAD_TAGS["road.maglev_isolated:0"] = "maglev_isolated"
for idx in range(1, 16):
    n, e, s, w = (idx >> 0) & 1, (idx >> 1) & 1, (idx >> 2) & 1, (idx >> 3) & 1
    ROAD_TAGS[f"road.road_c_n{n}e{e}s{s}w{w}:0"] = f"road_n{n}e{e}s{s}w{w}"
    ROAD_TAGS[f"road.rail_c_n{n}e{e}s{s}w{w}:0"] = f"rail_n{n}e{e}s{s}w{w}"
    ROAD_TAGS[f"road.maglev_c_n{n}e{e}s{s}w{w}:0"] = f"maglev_n{n}e{e}s{s}w{w}"

RIVER_TAGS = {}
for idx in range(16):
    n, e, s, w = (idx >> 0) & 1, (idx >> 1) & 1, (idx >> 2) & 1, (idx >> 3) & 1
    RIVER_TAGS[f"road.river_s_n{n}e{e}s{s}w{w}:0"] = f"river_n{n}e{e}s{s}w{w}"

RESOURCE_TAGS = {
    "ts.spice:0": "ts_spice",
    "ts.furs:0": "ts_furs",
    "ts.peat:0": "ts_peat",
    "ts.arctic_ivory:0": "ts_arctic_ivory",
    "ts.fruit:0": "ts_fruit",
    "ts.iron:0": "ts_iron",
    "ts.whales:0": "ts_whales",
    "ts.wheat:0": "ts_wheat",
    "ts.pheasant:0": "ts_pheasant",
    "ts.buffalo:0": "ts_buffalo",
    "ts.silk:0": "ts_silk",
    "ts.wine:0": "ts_wine",
    "ts.seals:0": "ts_seals",
    "ts.oasis:0": "ts_oasis",
    "ts.forest_game:0": "ts_forest_game",
    "ts.grassland_resources:0": "ts_grassland_resources",
    "ts.coal:0": "ts_coal",
    "ts.gems:0": "ts_gems",
    "ts.gold:0": "ts_gold",
    "ts.fish:0": "ts_fish",
    "ts.horses:0": "ts_horses",
    "ts.river_resources:0": "ts_river_resources",
    "ts.oil:0": "ts_oil",
    "ts.tundra_game:0": "ts_tundra_game",
    "ts.aluminum:0": "ts_aluminum",
    "ts.uranium:0": "ts_uranium",
    "ts.saltpeter:0": "ts_saltpeter",
    "ts.elephant:0": "ts_elephant",
}

EXTRA_TAGS = {
    "tx.village:0": "tx_village",
    "tx.mine:0": "tx_mine",
    "tx.irrigation:0": "tx_irrigation",
    "tx.farmland:0": "tx_farmland",
    "tx.pollution:0": "tx_pollution",
    "tx.fallout:0": "tx_fallout",
    "tx.oil_mine:0": "tx_oil_mine",
    "tx.oil_rig:0": "tx_oil_rig",
    "tx.nets:0": "tx_nets",
}

DARKNESS_TAGS = {}
for idx in range(1, 16):
    n, e, s, w = (idx >> 0) & 1, (idx >> 1) & 1, (idx >> 2) & 1, (idx >> 3) & 1
    DARKNESS_TAGS[f"tx.darkness_n{n}e{e}s{s}w{w}"] = f"tx_darkness_n{n}e{e}s{s}w{w}"
DARKNESS_TAGS["tx.fog"] = "tx_fog"

GRID_TAGS = {
    "grid.borders.n": "grid_borders_n",
    "grid.borders.s": "grid_borders_s",
    "grid.borders.e": "grid_borders_e",
    "grid.borders.w": "grid_borders_w",
}

TARGET_TAGS = {
    **TERRAIN_TAGS,
    **COAST_TAGS,
    **COAST_WHOLE_TAGS,
    **ROAD_TAGS,
    **RIVER_TAGS,
    **RESOURCE_TAGS,
    **EXTRA_TAGS,
    **DARKNESS_TAGS,
    **GRID_TAGS
}

def parse_spec(spec_file):
    tiles = {}
    with open(spec_file, 'r') as f:
        lines = f.readlines()
    
    in_tiles = False
    current_dx = 30
    current_dy = 30
    current_x_top = 0
    current_y_top = 0

    current_row = 0
    current_col = 0

    for line in lines:
        if ';' in line:
            line = line.split(';')[0]
        line = line.strip()
        if not line:
            continue
            
        if line.startswith('x_top_left'):
            current_x_top = int(line.split('=')[1].strip())
        elif line.startswith('y_top_left'):
            current_y_top = int(line.split('=')[1].strip())
        elif line.startswith('dx'):
            current_dx = int(line.split('=')[1].strip())
        elif line.startswith('dy'):
            current_dy = int(line.split('=')[1].strip())
            
        if line.replace(' ', '').startswith('tiles={'):
            in_tiles = True
            continue
        if in_tiles and line.startswith('}'):
            in_tiles = False
            continue
        
        if in_tiles:
            parts = [p.strip().strip('"').strip(',') for p in line.split('"')]
            if len(parts) >= 2:
                tag = parts[1]
                coords = parts[0].split(',')
                if len(coords) >= 2 and coords[0].strip() and coords[1].strip():
                    try:
                        current_row = int(coords[0].strip())
                        current_col = int(coords[1].strip())
                    except ValueError:
                        pass
                if tag in TARGET_TAGS:
                    tiles[tag] = {
                        "row": current_row,
                        "col": current_col,
                        "dx": current_dx,
                        "dy": current_dy,
                        "x_top": current_x_top,
                        "y_top": current_y_top
                    }
                elif 'coast' in tag:
                    print(f"Found tag {tag} but not in TARGET_TAGS")
    return tiles

def get_all_unit_tags():
    tags = {}
    with open('../freeciv/data/trident/units.spec', 'r') as f:
        for line in f:
            if 'u.' in line and '_Idle:0' in line:
                parts = [p.strip().strip('"').strip(',') for p in line.split('"')]
                if len(parts) >= 2 and parts[1].startswith('u.') and parts[1].endswith('_Idle:0'):
                    tag = parts[1]
                    name = 'unit_' + tag[2:-7]
                    tags[tag] = name
    return tags

# Entity sprites (units / cities) kept at native 30x30 so they sit on top of tiles.
# Maps output sprite name -> (spec file, tag).
ENTITY_TAGS = {
    "city": ("../freeciv/data/trident/cities.spec", "city.classical_city_0"),
}

BG_DIR = '../3ds_port/gfx'

def crop_transparent(img, t, target_size=None):
    """Crop the tile at spec entry t, preserving its source alpha, optional resize.

    Trident's PNGs (tiles/units/cities) already carry a correct alpha channel,
    so we just keep it as-is. An earlier version keyed out every pixel matching
    the tile's top-left corner color, treating it as a magenta-style background
    key — but these sheets have no such background. That keying was destructive
    for flat-color fills: an open-ocean coast_cell is ~66% one solid deep-blue
    (== its top-left pixel), so the keying punched transparent holes through the
    water and the black clear color showed through, giving the "black netting"
    ocean. Tiles that are legitimately fully opaque (water fill, land base
    tiles) must stay fully opaque. So: no color-keying, ever — just trust the
    source alpha.
    """
    x = t["x_top"] + t["col"] * t["dx"]
    y = t["y_top"] + t["row"] * t["dy"]
    cropped = img.crop((x, y, x + t["dx"], y + t["dy"]))
    cropped = cropped.convert("RGBA")
    if target_size and (t["dx"], t["dy"]) != target_size:
        cropped = cropped.resize(target_size, Image.Resampling.LANCZOS)
    return cropped

def extract_set(spec_path, img_path, wanted, target_size, t3s_lines):
    """Extract the wanted {tag: name} from one spec/png pair. Returns list of names written."""
    tiles = parse_spec_any(spec_path, set(wanted.keys()))
    img = Image.open(img_path)
    written = []
    for tag, name in wanted.items():
        if tag not in tiles:
            print(f"Warning: Tag {tag} not found in {spec_path}.")
            continue
        sprite = crop_transparent(img, tiles[tag], target_size)
        out_name = f"{name}.png"
        sprite.save(os.path.join(BG_DIR, out_name))
        t3s_lines.append(out_name)
        written.append(name)
        print(f"Extracted {name} from {tag}")
    return written

def parse_spec_any(spec_file, wanted_tags):
    """Like parse_spec but for an arbitrary set of tags (handles ':hotspot' suffixes)."""
    global TARGET_TAGS
    saved = TARGET_TAGS
    TARGET_TAGS = {t: t for t in wanted_tags}
    try:
        return parse_spec(spec_file)
    finally:
        TARGET_TAGS = saved

def extract_tiles():
    os.makedirs(BG_DIR, exist_ok=True)

    # Terrain tiles are native 30x30 (fill a whole grid cell) and coast cells are
    # native 15x15 (quadrant overlays) — extract each at its own native size, no resize.
    if '--terrain' in os.sys.argv:
        extract_set('../freeciv/data/trident/tiles.spec',
                    '../freeciv/data/trident/tiles.png',
                    TERRAIN_TAGS, None, [])
        extract_set('../freeciv/data/trident/tiles.spec',
                    '../freeciv/data/trident/tiles.png',
                    COAST_TAGS, None, [])
        extract_set('../freeciv/data/trident/tiles.spec',
                    '../freeciv/data/trident/tiles.png',
                    COAST_WHOLE_TAGS, None, [])
        extract_set('../freeciv/data/trident/tiles.spec',
                    '../freeciv/data/trident/tiles.png',
                    RIVER_TAGS, None, [])
        extract_set('../freeciv/data/trident/tiles.spec',
                    '../freeciv/data/trident/tiles.png',
                    RESOURCE_TAGS, None, [])
        extract_set('../freeciv/data/trident/tiles.spec',
                    '../freeciv/data/trident/tiles.png',
                    EXTRA_TAGS, None, [])
        extract_set('../freeciv/data/trident/roads.spec',
                    '../freeciv/data/trident/roads.png',
                    ROAD_TAGS, None, [])
        extract_set('../freeciv/data/trident/tiles.spec',
                    '../freeciv/data/trident/tiles.png',
                    DARKNESS_TAGS, None, [])
        extract_set('../freeciv/data/trident/grid.spec',
                    '../freeciv/data/trident/grid.png',
                    GRID_TAGS, None, [])

    # Entities (units/cities) at native size, grouped by their spec file.
    for name, (spec, tag) in ENTITY_TAGS.items():
        img_path = spec.replace('.spec', '.png')
        extract_set(spec, img_path, {tag: name}, None, [])

    unit_tags = get_all_unit_tags()
    extract_set('../freeciv/data/trident/units.spec', '../freeciv/data/trident/units.png', unit_tags, None, [])

    # The cities.spec is missing, only units.spec and tiles.spec are present in trident.
    try:
        extract_set('../freeciv/data/trident/cities.spec', '../freeciv/data/trident/cities.png', {'city.classical_city_0': 'city'}, None, [])
    except FileNotFoundError:
        pass

    # Rebuild sprites.t3s from every PNG currently in the gfx dir (deterministic order).
    pngs = sorted(f for f in os.listdir(BG_DIR) if f.endswith('.png'))
    with open(os.path.join(BG_DIR, 'sprites.t3s'), 'w') as f:
        f.write("--atlas -f rgba8888 -z auto\n")
        f.write("\n".join(pngs) + "\n")
    print(f"Generated sprites.t3s with {len(pngs)} sprites")

if __name__ == "__main__":
    extract_tiles()
