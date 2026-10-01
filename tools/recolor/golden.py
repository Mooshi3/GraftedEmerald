"""Pins recolor results Jacob approved: `golden.py save FILE` records them, `golden.py check FILE` lists what changed.

The cases are the round-2 A/B sheets' rows (mon:type1/type2), minus the ones still being tuned.
"""
import json, sys
import sheet, recolor_v3

def cases(items):
    for it in items:
        mon, combos = it.split(':')
        for c in combos.split(','):
            yield mon, tuple(c.split('/'))

def run(items):
    colors, spt, out = sheet.type_colors(), sheet.species_types(), {}
    for mon, eff in cases(items):
        im, pal, ce = sheet.load(mon)
        out[f'{mon}:{eff[0]}/{eff[1]}'] = recolor_v3.recolor_types(pal, spt[mon], eff, colors, ce)
    return out

if __name__ == '__main__':
    mode, path = sys.argv[1], sys.argv[2]
    if mode == 'save':
        items = sys.argv[3].split()
        json.dump({'items': items, 'results': run(items)}, open(path, 'w'), indent=0)
    else:
        g = json.load(open(path))
        now = run(g['items'])
        bad = [k for k in g['results'] if [list(c) for c in now[k]] != g['results'][k]]
        print(f'{len(g["results"])} pinned, {len(bad)} changed' + (': ' + ' '.join(bad) if bad else ''))
        sys.exit(1 if bad else 0)
