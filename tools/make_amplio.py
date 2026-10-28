#!/usr/bin/env python3
"""Compile Freeciv's Amplio2 tileset into 3DS atlases + a C table.

Reads the tileset straight from a Freeciv source tree (data/amplio2*), does
every crop/mask the Freeciv client does at load time (corner cells cut from
cell groups, blend quadrants cut through the dither mask), scales everything
to half size (a 96x48 tile becomes 48x24) and writes:

  3ds_port/gfx/iso/*.png             one PNG per sprite
  3ds_port/gfx/iso_terrain.t3s       atlas 0: terrain, water, fog, extras
  3ds_port/gfx/iso_units.t3s         atlas 1: units, cities, bases, select
  3ds_port/source/iso_data.h         sprite indices and terrain drawing rules

Sprite references in iso_data.h are (sheet << 12) | index.

usage: make_amplio.py <freeciv-source>/data
"""
import os, re, sys
from PIL import Image

DATA = sys.argv[1] if len(sys.argv) > 1 else '../freeciv/data'
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT_GFX = os.path.join(ROOT, '3ds_port', 'gfx')
OUT_PNG = os.path.join(OUT_GFX, 'iso')
OUT_H = os.path.join(ROOT, '3ds_port', 'source', 'iso_data.h')
S = 0.5           # output scale
W, H = 96, 48     # Amplio2 tile size

# ---------------------------------------------------------------- spec files
def parse_spec(path):
    txt = open(path, encoding='utf-8').read()
    gfx = re.search(r'gfx\s*=\s*"([^"]+)"', txt).group(1)
    img = Image.open(os.path.join(DATA, gfx + '.png')).convert('RGBA')
    out = {}
    for sec in re.finditer(r'\[(grid_[a-z_0-9]+)\](.*?)(?=\n\[|\Z)', txt, re.S):
        body = sec.group(2)
        def num(k, d=0):
            m = re.search(r'^\s*%s\s*=\s*(-?\d+)' % k, body, re.M)
            return int(m.group(1)) if m else d
        x0, y0, dx, dy, pb = num('x_top_left'), num('y_top_left'), num('dx'), num('dy'), num('pixel_border')
        m = re.search(r'tiles\s*=\s*\{(.*?)\n\s*\}', body, re.S)
        if not m:
            continue
        for line in m.group(1).split('\n'):
            line = line.split(';')[0].strip()
            mm = re.match(r'(\d+)\s*,\s*(\d+)\s*,(.*)', line)
            if not mm:
                continue
            r, c = int(mm.group(1)), int(mm.group(2))
            tags = re.findall(r'"([^"]+)"', mm.group(3))
            x, y = x0 + c * (dx + pb), y0 + r * (dy + pb)
            for t in tags:
                out.setdefault(t, (img, x, y, dx, dy))
    return out

tilespec = open(os.path.join(DATA, 'amplio2.tilespec'), encoding='utf-8').read()
spec_files = re.findall(r'"(amplio2/[a-z_0-9]+\.spec)"', tilespec)
TAGS = {}
for f in spec_files:
    for k, v in parse_spec(os.path.join(DATA, f)).items():
        TAGS.setdefault(k, v)

def raw(tag):
    if tag not in TAGS and (tag + ':0') in TAGS:
        tag = tag + ':0'
    if tag not in TAGS:
        return None
    img, x, y, w, h = TAGS[tag]
    return img.crop((x, y, x + w, y + h))

def mask(src, m, mx, my):
    """Freeciv mask_surface(): alpha *= mask alpha read at (mx, my)."""
    a = src.split()[3]
    ma = m.crop((mx, my, mx + src.width, my + src.height)).split()[3]
    from PIL import ImageChops
    src = src.copy()
    src.putalpha(ImageChops.multiply(a, ma))
    return src

def half(im):
    w, h = max(1, round(im.width * S)), max(1, round(im.height * S))
    return im.convert('RGBa').resize((w, h), Image.LANCZOS).convert('RGBA')

# ---------------------------------------------------------------- atlases
sheets = [[], []]          # PNG names per sheet
def add(im, sheet=0):
    if im is None:
        return -1
    im = half(im)
    if im.getbbox() is None:   # fully transparent: nothing to draw
        return -1
    n = len(sheets[sheet])
    name = 'iso/%s%04d.png' % ('t' if sheet == 0 else 'u', n)
    im.save(os.path.join(OUT_GFX, name))
    sheets[sheet].append(name)
    return (sheet << 12) | n

os.makedirs(OUT_PNG, exist_ok=True)
for f in os.listdir(OUT_PNG):
    os.remove(os.path.join(OUT_PNG, f))

MASK_TILE = raw('mask.tile')
DITHER = raw('t.dither_tile')

# ---------------------------------------------------------------- terrain
def ini_sections(txt):
    secs = {}
    for m in re.finditer(r'^\[([a-z_0-9]+)\]\s*\n(.*?)(?=^\[|\Z)', txt, re.S | re.M):
        d = {}
        for line in m.group(2).split('\n'):
            line = line.split(';')[0]
            mm = re.match(r'\s*([a-z_0-9]+)\s*=\s*(.*)', line)
            if mm:
                d[mm.group(1)] = mm.group(2).strip()
        secs[m.group(1)] = d
    return secs

secs = ini_sections(tilespec)
layer_types = []
for l in range(3):
    layer_types.append(re.findall(r'"([^"]+)"', secs['layer%d' % l]['match_types']))

def qlist(v):
    return re.findall(r'"([^"]+)"', v or '')

terrains = []
for name, d in secs.items():
    if not name.startswith('tile_'):
        continue
    tag = qlist(d['tag'])[0]
    nl = int(d.get('num_layers', '1'))
    layers = []
    for l in range(nl):
        mt = qlist(d.get('layer%d_match_type' % l))
        mw = qlist(d.get('layer%d_match_with' % l))
        idx = [layer_types[l].index(mt[0])] if mt else []
        idx += [layer_types[l].index(x) for x in mw]
        st = d.get('layer%d_sprite_type' % l, '"whole"').strip('"')
        corner = st == 'corner'
        if len(idx) <= 1: style = 0
        elif len(idx) == 2: style = 1 if idx[0] == idx[1] else 2
        else: style = 3
        tall = d.get('layer%d_is_tall' % l, 'FALSE').upper() == 'TRUE'
        oy = int(d.get('layer%d_offset_y' % l, '0'))
        L = dict(corner=corner, style=style, idx=idx, tall=tall, oy=oy, base=[], match=[-1] * 16,
                 cells=[-1] * (4 * 27))
        if not corner:
            if style == 0:
                i = 1
                while True:
                    im = raw('t.l%d.%s%d' % (l, tag, i))
                    if im is None: break
                    if tall: L['oy'] = oy + (H - im.height)
                    L['base'].append(add(im)); i += 1
            elif style == 1:
                for i in range(16):
                    t = 't.l%d.%s_n%de%ds%dw%d' % (l, tag, i & 1, (i >> 1) & 1, (i >> 2) & 1, (i >> 3) & 1)
                    im = raw(t)
                    if im is not None and tall: L['oy'] = oy + (H - im.height)
                    L['match'][i] = add(im)
        else:
            count = len(idx)
            letters = 'udrl'
            n = 4 if style == 0 else 4 * 8 if style in (1, 2) else 4 * count ** 3
            for i in range(n):
                dr, value = i % 4, i // 4
                im = None
                if style == 0:
                    im = raw('t.l%d.%s_cell_%s' % (l, tag, letters[dr]))
                elif style == 2:
                    lt = layer_types[l]
                    t = 't.l%d.%s_cell_%s_%s_%s_%s' % (l, tag, letters[dr],
                        lt[idx[value & 1]][0], lt[idx[(value >> 1) & 1]][0], lt[idx[(value >> 2) & 1]][0])
                    im = raw(t)
                elif style == 1:
                    im = raw('t.l%d.%s_cell_%s%d%d%d' % (l, tag, letters[dr], value & 1, (value >> 1) & 1, (value >> 2) & 1))
                else:
                    this = idx[0]
                    v1 = idx[value % count]; value //= count
                    v2 = idx[value % count]; value //= count
                    v3 = idx[value % count]
                    if dr == 0: s_, w_, n_, e_ = this, v1, v2, v3
                    elif dr == 2: w_, n_, e_, s_ = this, v1, v2, v3
                    elif dr == 1: n_, e_, s_, w_ = this, v1, v2, v3
                    else: e_, s_, w_, n_ = this, v1, v2, v3
                    lt = layer_types[l]
                    cg = raw('t.l%d.cellgroup_%s_%s_%s_%s' % (l, lt[n_][0], lt[e_][0], lt[s_][0], lt[w_][0]))
                    if cg is not None:
                        xs = [W // 4, W // 4, 0, W // 2]; ys = [H // 2, 0, H // 4, H // 4]
                        xo = [0, 0, -W // 2, W // 2]; yo = [H // 2, -H // 2, 0, 0]
                        x, y = xs[dr], ys[dr]
                        im = mask(cg.crop((x, y, x + W // 2, y + H // 2)), MASK_TILE, x - xo[dr], y - yo[dr])
                L['cells'][i] = add(im)
        layers.append(L)
    blending = int(d.get('blend_layer', '0'))
    blender = raw('t.blend.%s' % tag)
    if blender is None and blending > 0:
        blender = raw('t.l%d.%s1' % (blending - 1, tag))
    blend = [-1] * 4
    if blender is not None:
        offs = [(W // 2, 0), (0, H // 2), (W // 2, H // 2), (0, 0)]
        for dr in range(4):
            x, y = offs[dr]
            blend[dr] = add(mask(blender.crop((x, y, x + W // 2, y + H // 2)), DITHER, x, y))
    terrains.append(dict(tag=tag, layers=layers, blending=blending if blender is not None else 0, blend=blend))

# ---------------------------------------------------------------- fog, extras
fog = []
for i in range(81):
    k, s = i, ''
    for _ in range(4):
        s += '_' + 'ufk'[k % 3]; k //= 3
    fog.append(add(raw('t.fog' + s)))

def card(i):
    return 'n%de%ds%dw%d' % (i & 1, (i >> 1) & 1, (i >> 2) & 1, (i >> 3) & 1)

river = [add(raw('road.river_s_' + card(i))) for i in range(16)]
outlet = [add(raw('road.river_outlet_' + d)) for d in 'nesw']
DIRS8 = ['nw', 'n', 'ne', 'w', 'e', 'sw', 's', 'se']    # freeciv direction8 order
roads = {}
for r in ('road', 'rail', 'maglev'):
    roads[r] = [add(raw('road.%s_%s' % (r, d))) for d in DIRS8] + [add(raw('road.%s_isolated' % r))]
single = {}
for t in ('tx.irrigation', 'tx.farmland', 'tx.mine', 'tx.oil_mine', 'tx.pollution', 'tx.fallout',
          'tx.village', 'tx.oil_rig', 'tx.nets'):
    single[t] = add(raw(t))
resources = []
for t in sorted(k for k in TAGS if k.startswith('ts.')):
    resources.append((t.split(':')[0], add(raw(t))))

# ---------------------------------------------------------------- units, cities, bases
units = []
for t in sorted(k for k in TAGS if re.match(r'u\.[a-z_]+_Idle:0$', k)):
    nm = t[2:-len('_Idle:0')]
    im = raw(t)
    units.append((nm, add(im, 1)))
styles = sorted({re.match(r'city\.([a-z]+)_city_\d', k).group(1) for k in TAGS if re.match(r'city\.[a-z]+_city_\d', k)})
cities = []
for st in styles:
    c = [add(raw('city.%s_city_%d' % (st, i)), 1) for i in range(5)]
    wl = [add(raw('city.%s_wall_%d' % (st, i)), 1) for i in range(5)]
    oc = add(raw('city.%s_occupied_0' % st), 1)
    cities.append((st, c, wl, oc))
bases = {}
for b in ('fortress', 'airbase', 'outpost', 'buoy', 'airstrip'):
    bases[b] = (add(raw('base.%s_bg' % b) or raw('base.%s_mg' % b), 1), add(raw('base.%s_fg' % b), 1))
select = [add(raw('unit.select:%d' % i), 1) for i in range(4)]
hp = [add(raw('unit.hp_%d' % (i * 10)), 1) for i in range(11)]

# ---------------------------------------------------------------- write
for n, (sh, nm) in enumerate(((0, 'iso_terrain'), (1, 'iso_units'))):
    with open(os.path.join(OUT_GFX, nm + '.t3s'), 'w') as f:
        f.write('--atlas -f rgba8888 -z auto\n')
        for p in sheets[sh]:
            f.write(p + '\n')

def arr(a):
    return '{' + ','.join(str(x) for x in a) + '}'

with open(OUT_H, 'w') as f:
    f.write('// Generated by tools/make_amplio.py from Freeciv\'s Amplio2 tileset - do not edit.\n')
    f.write('// Sprite refs: (sheet << 12) | index, -1 = none. Sizes are half of Amplio2 (tile 48x24).\n')
    f.write('#pragma once\n#define ISO_W 48\n#define ISO_H 24\n\n')
    f.write('typedef struct { signed char corner, style, nidx, tall; signed char idx[3]; signed char oy;\n'
            '                 signed char nbase; short base[4]; short match[16]; short cells[108]; } IsoLayer;\n')
    f.write('typedef struct { const char *tag; signed char nlayers, blending; short blend[4]; IsoLayer layer[3]; } IsoTerrain;\n\n')
    f.write('#define ISO_NTERRAINS %d\nstatic const IsoTerrain iso_terrains[ISO_NTERRAINS] = {\n' % len(terrains))
    for t in terrains:
        ls = []
        for L in t['layers']:
            idx = (L['idx'] + [-1, -1, -1])[:3]
            base = (L['base'] + [-1] * 4)[:4]
            ls.append('{%d,%d,%d,%d,%s,%d,%d,%s,%s,%s}' % (L['corner'], L['style'], len(L['idx']), L['tall'],
                      arr(idx), round(L['oy'] * S), len(L['base']), arr(base), arr(L['match']), arr(L['cells'])))
        f.write('  {"%s",%d,%d,%s,{%s}},\n' % (t['tag'], len(t['layers']), t['blending'], arr(t['blend']), ','.join(ls)))
    f.write('};\n\n')
    f.write('static const short iso_fog[81] = %s;\n' % arr(fog))
    f.write('static const short iso_river[16] = %s;\n' % arr(river))
    f.write('static const short iso_outlet[4] = %s; // n e s w\n' % arr(outlet))
    for r, a in roads.items():
        f.write('static const short iso_%s[9] = %s; // direction8 order, then isolated\n' % (r, arr(a)))
    for t, v in single.items():
        f.write('#define ISO_%s %d\n' % (t.replace('tx.', 'TX_').upper(), v))
    f.write('\n#define ISO_NRES %d\nstatic const struct { const char *tag; short spr; } iso_res[ISO_NRES] = {\n' % len(resources))
    for t, v in resources:
        f.write('  {"%s",%d},\n' % (t, v))
    f.write('};\n#define ISO_NUNITS %d\nstatic const struct { const char *name; short spr; } iso_units[ISO_NUNITS] = {\n' % len(units))
    for nm, v in units:
        f.write('  {"%s",%d},\n' % (nm, v))
    f.write('};\n#define ISO_NSTYLES %d\nstatic const struct { const char *style; short city[5], wall[5], occupied; } iso_cities[ISO_NSTYLES] = {\n' % len(cities))
    for st, c, wl, oc in cities:
        f.write('  {"%s",%s,%s,%d},\n' % (st, arr(c), arr(wl), oc))
    f.write('};\n')
    for b, (bg, fg) in bases.items():
        f.write('#define ISO_BASE_%s_BG %d\n#define ISO_BASE_%s_FG %d\n' % (b.upper(), bg, b.upper(), fg))
    f.write('static const short iso_select[4] = %s;\n' % arr(select))
    f.write('static const short iso_hp[11] = %s; // 0%%..100%%\n' % arr(hp))

print('sheet 0: %d sprites, sheet 1: %d sprites, %d terrains' % (len(sheets[0]), len(sheets[1]), len(terrains)))
