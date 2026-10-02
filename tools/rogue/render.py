#!/usr/bin/env python3
"""Render Emerald map layouts and tileset metatiles to PNG, for designing Rogue rooms without the emulator.

  render.py layout LAYOUT_ID out.png          # a layout from data/layouts/layouts.json
  render.py sheet PRIMARY SECONDARY out.png   # every metatile, labeled with its id (e.g. general cave)
  render.py grid PRIMARY SECONDARY in.txt out.png  # a grid of hex metatile ids (one row per line)
"""
import json, os, struct, sys
from PIL import Image, ImageDraw

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
NUM_TILES_PRIMARY = 512
NUM_METATILES_PRIMARY = 512
NUM_PALS_PRIMARY = 6

def tileset_dir(name):
    for kind in ('primary', 'secondary'):
        d = os.path.join(ROOT, 'data/tilesets', kind, name)
        if os.path.isdir(d):
            return d
    raise SystemExit('no tileset ' + name)

def tileset_name(symbol):
    # gTileset_General -> general, gTileset_PokemonCenter -> pokemon_center
    s = symbol.replace('gTileset_', '')
    out = ''
    for i, c in enumerate(s):
        if c.isupper() and i and not s[i - 1].isupper():
            out += '_'
        out += c.lower()
    return out

def load_pal(path):
    lines = open(path).read().split('\n')
    return [tuple(int(x) for x in l.split()) for l in lines[3:19]]

class Tilesets:
    def __init__(self, primary, secondary):
        pd, sd = tileset_dir(primary), tileset_dir(secondary)
        self.tiles = [Image.open(os.path.join(pd, 'tiles.png')), Image.open(os.path.join(sd, 'tiles.png'))]
        self.pals = []
        for i in range(16):
            src = pd if i < NUM_PALS_PRIMARY else sd
            p = os.path.join(src, 'palettes', '%02d.pal' % i)
            self.pals.append(load_pal(p) if os.path.exists(p) else [(255, 0, 255)] * 16)
        self.meta = [open(os.path.join(pd, 'metatiles.bin'), 'rb').read(), open(os.path.join(sd, 'metatiles.bin'), 'rb').read()]
        self.cache = {}

    def tile(self, index, pal, hflip, vflip):
        img = self.tiles[0] if index < NUM_TILES_PRIMARY else self.tiles[1]
        index = index if index < NUM_TILES_PRIMARY else index - NUM_TILES_PRIMARY
        w = img.width // 8
        x, y = (index % w) * 8, (index // w) * 8
        if y >= img.height:
            return Image.new('RGBA', (8, 8), (0, 0, 0, 0))
        t = img.crop((x, y, x + 8, y + 8))
        out = Image.new('RGBA', (8, 8))
        px = t.load(); op = out.load()
        for j in range(8):
            for i in range(8):
                c = px[i, j] & 15
                op[i, j] = (0, 0, 0, 0) if c == 0 else self.pals[pal][c] + (255,)
        if hflip: out = out.transpose(Image.FLIP_LEFT_RIGHT)
        if vflip: out = out.transpose(Image.FLIP_TOP_BOTTOM)
        return out

    def metatile(self, mid):
        if mid in self.cache:
            return self.cache[mid]
        data = self.meta[0] if mid < NUM_METATILES_PRIMARY else self.meta[1]
        i = mid if mid < NUM_METATILES_PRIMARY else mid - NUM_METATILES_PRIMARY
        img = Image.new('RGBA', (16, 16), self.pals[0][0] + (255,))
        if (i + 1) * 16 <= len(data):
            entries = struct.unpack('<8H', data[i * 16:(i + 1) * 16])
            for layer in range(2):
                for k in range(4):
                    e = entries[layer * 4 + k]
                    t = self.tile(e & 0x3FF, e >> 12, (e >> 10) & 1, (e >> 11) & 1)
                    img.alpha_composite(t, ((k % 2) * 8, (k // 2) * 8))
        self.cache[mid] = img
        return img

def render_grid(ts, rows, scale=2):
    h, w = len(rows), max(len(r) for r in rows)
    img = Image.new('RGBA', (w * 16, h * 16), (0, 0, 0, 255))
    for y, r in enumerate(rows):
        for x, v in enumerate(r):
            img.alpha_composite(ts.metatile(v & 0x3FF), (x * 16, y * 16))
    return img.resize((w * 16 * scale, h * 16 * scale), Image.NEAREST)

def main():
    mode = sys.argv[1]
    if mode == 'layout':
        L = {l['id']: l for l in json.load(open(os.path.join(ROOT, 'data/layouts/layouts.json')))['layouts'] if 'id' in l}
        l = L[sys.argv[2]]
        d = open(os.path.join(ROOT, l['blockdata_filepath']), 'rb').read()
        v = struct.unpack('<%dH' % (len(d) // 2), d)
        rows = [v[y * l['width']:(y + 1) * l['width']] for y in range(l['height'])]
        ts = Tilesets(tileset_name(l['primary_tileset']), tileset_name(l['secondary_tileset']))
        render_grid(ts, rows).save(sys.argv[3])
    elif mode == 'sheet':
        ts = Tilesets(sys.argv[2], sys.argv[3])
        ids = list(range(0, 512)) + list(range(512, 1024))
        cols = 16
        img = Image.new('RGBA', (cols * 34, (len(ids) // cols) * 40), (40, 40, 40, 255))
        d = ImageDraw.Draw(img)
        for n, mid in enumerate(ids):
            x, y = (n % cols) * 34, (n // cols) * 40
            img.alpha_composite(ts.metatile(mid).resize((32, 32), Image.NEAREST), (x + 1, y + 8))
            d.text((x + 2, y - 1), '%03x' % mid, fill=(255, 255, 0, 255))
        img.save(sys.argv[4])
    elif mode == 'grid':
        ts = Tilesets(sys.argv[2], sys.argv[3])
        rows = [[int(t, 16) for t in l.split()] for l in open(sys.argv[4]) if l.strip()]
        render_grid(ts, rows).save(sys.argv[5])

if __name__ == '__main__':
    main()
