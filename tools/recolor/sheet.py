"""Contact sheets of graft recolors on real sprites.

usage: sheet.py OUT.png ALGO "mon:type1/type2 ..." [--shiny]
  ALGO: a module in this folder with recolor(pal, c1, c2, dual) (pal and colors in 5-bit RGB)
  Each item renders original | recolor. type1/type2 are the effective types after the graft.
  "mon:a/b,c/d" renders several in one row; "mon:all" renders the mon with each of the 18 types grafted into its second slot.
"""
import importlib, os, re, sys
from PIL import Image, ImageDraw

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..'))
TYPES = ['normal', 'fighting', 'flying', 'poison', 'ground', 'rock', 'bug', 'ghost', 'steel',
         'fire', 'water', 'grass', 'electric', 'psychic', 'ice', 'dragon', 'dark', 'fairy']


def type_colors(shiny=False):
    src = open(os.path.join(ROOT, 'src/data/types_info.h')).read()
    key = 'shinyGraftPaletteRGBValue' if shiny else 'graftPaletteRGBValue'
    out = {}
    for m in re.finditer(r'\[TYPE_(\w+)\]\s*=\s*\{(.*?)\n    \}', src, re.S):
        c = re.search(r'\.' + key + r'\s*=\s*RGB\((\d+),\s*(\d+),\s*(\d+)\)', m.group(2))
        if c:
            out[m.group(1).lower()] = tuple(int(x) for x in c.groups())
    return out


def species_types():
    """species folder -> (type1, type2) from the species info headers"""
    out = {}
    import glob
    for f in glob.glob(os.path.join(ROOT, 'src/data/pokemon/species_info/gen_*_families.h')):
        txt = open(f).read()
        macros = {}
        for d in re.finditer(r'#define (\w+)\s+\(?[^\n]*?TYPE_(\w+)', txt):
            macros.setdefault(d.group(1), d.group(2))  # first one: the updated types
        tok = lambda x: macros.get(x, x[5:] if x.startswith('TYPE_') else x).lower()
        for m in re.finditer(r'\[SPECIES_(\w+)\]\s*=\s*\{(.*?)\n    \},', txt, re.S):
            t = re.search(r'\.types = MON_TYPES\((\w+)(?:,\s*(\w+))?\)', m.group(2))
            if t:
                a = tok(t.group(1))
                out[m.group(1).lower()] = (a, tok(t.group(2) or t.group(1)))
    return out


def load(mon, shiny=False):
    d = os.path.join(ROOT, 'graphics/pokemon', mon)
    pal_lines = [l for l in open(os.path.join(d, 'shiny.pal' if shiny else 'normal.pal')).read().splitlines() if l.strip()][3:19]
    pal = [tuple(int(x) >> 3 for x in l.split()) for l in pal_lines]
    for f in ('anim_front.png', 'front.png'):
        if os.path.exists(os.path.join(d, f)):
            im = Image.open(os.path.join(d, f))
            break
    im = im.crop((0, 0, 64, 64))
    counts = [0] * 16
    edges = [0] * 16  # pixels next to transparency (or the frame's edge): where outlines sit
    px = im.load()
    for y in range(64):
        for x in range(64):
            i = px[x, y] & 15
            counts[i] += 1
            if i and any(not (0 <= x + dx < 64 and 0 <= y + dy < 64) or (px[x + dx, y + dy] & 15) == 0
                         for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1))):
                edges[i] += 1
    return im, pal, (counts, edges)


def render(im, pal5):
    im = im.copy()
    flat = []
    for r, g, b in pal5:
        flat += [(r << 3) | (r >> 2), (g << 3) | (g >> 2), (b << 3) | (b >> 2)]
    im.putpalette(flat + [0] * (768 - len(flat)))
    rgb = im.convert('RGB')
    bg = Image.new('RGB', (64, 64), (236, 236, 228))
    mask = Image.new('L', im.size)
    mask.putdata([0 if i == 0 else 255 for i in im.get_flattened_data()])
    bg.paste(rgb, (0, 0), mask)
    return bg


def main():
    out, algo, items = sys.argv[1], sys.argv[2], sys.argv[3].split()
    shiny = '--shiny' in sys.argv
    sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
    algos = [importlib.import_module(a) for a in algo.split(',')]
    colors = type_colors(shiny)
    spt = species_types()
    rows = []
    for it in items:
        mon, types = it.split(':')
        im, pal, counts = load(mon, shiny)
        if types == 'all':
            t1 = spt.get(mon, ('normal', 'normal'))[0]
            combos = [(t1, t) for t in TYPES if t != t1]
        else:
            combos = [tuple(t.split('/')) for t in types.split(',')]
        cells = [(render(im, pal), mon)]
        for t1, t2 in combos:
            for a in algos:
                if hasattr(a, 'recolor_types'):
                    p = a.recolor_types(pal, spt.get(mon, (t1, t1)), (t1, t2), colors, counts)
                else:
                    p = a.recolor(pal, colors[t1], colors[t2], t1 != t2)
                cells.append((render(im, p), f'{t1[:4]}/{t2[:4]}' if t1 != t2 else t1[:8]))
        rows.append(cells)
    S = 1 if any(':all' in it for it in items) else 2
    cw, ch = 64 * S, 64 * S + 12
    W = max(len(r) for r in rows) * cw
    sheet = Image.new('RGB', (W, len(rows) * ch), (40, 40, 44))
    d = ImageDraw.Draw(sheet)
    for y, r in enumerate(rows):
        for x, (img, label) in enumerate(r):
            sheet.paste(img.resize((64 * S, 64 * S), Image.NEAREST), (x * cw, y * ch + 12))
            d.text((x * cw + 2, y * ch), label, fill=(230, 230, 230))
    sheet.save(out)


if __name__ == '__main__':
    main()
