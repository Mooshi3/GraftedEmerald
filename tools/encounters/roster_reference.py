#!/usr/bin/env python3
"""Trainer roster reference: for each Gym Leader, Elite Four member and the Champion, their current
(vanilla) team and the evolution lines of their type that live in the wild (encounter sheet) in areas
reachable by then. Writes an HTML page for the roster redesign. Story stages are approximate.

usage: python3 tools/encounters/roster_reference.py OUT.html
"""
import collections, glob, html, json, os, re, sys

sys.path.insert(0, os.path.dirname(__file__))
import sheet_to_json as s2j  # noqa: E402  (reuses the sheet reader's species-name resolver)
import openpyxl  # noqa: E402

ROOT = s2j.ROOT

# Area -> the first stage it's reachable at (0 = before the first gym, 8 = after the 8th badge).
# Surfing (Water rows) needs Surf, from Norman's badge, so it counts from stage 5 at the earliest.
STAGES = [
    ['ROUTE 101', 'ROUTE 102', 'ROUTE 103', 'ROUTE 104', 'PETALBURG WOODS', 'PETALBURG CITY'],
    ['ROUTE 116', 'RUSTURF TUNNEL', 'DEWFORD TOWN', 'ROUTE 106', 'ROUTE 107', 'ROUTE 105',
     'GRANITE CAVE 1F', 'GRANITE CAVE B1F', 'GRANITE CAVE B2F'],
    ['ROUTE 109', 'SLATEPORT CITY', 'ROUTE 110'],
    ['ROUTE 117', 'ROUTE 111', 'ROUTE 112', 'FIERY PATH', 'ROUTE 113', 'ROUTE 114', 'ROUTE 115',
     'METEOR FALLS', 'METEOR FALLS 1F', 'JAGGED PASS', 'MT. CHIMNEY', 'NEW MAUVILLE'],
    ['ROUTE 118', 'MIRAGE TOWER', 'ROUTE 108', 'ABANDONED SHIP'],
    ['ROUTE 119', 'ROUTE 120'],
    ['ROUTE 121', 'ROUTE 122', 'ROUTE 123', 'SAFARI ZONE', 'SAFARI ZONE SW', 'SAFARI ZONE NW', 'SAFARI ZONE SE',
     'LILYCOVE CITY', 'MT. PYRE EXTERIOR', 'MT. PYRE INTERIOR', 'MAGMA HIDEOUT', 'AQUA HIDEOUT',
     'ROUTE 124', 'ROUTE 125', 'MOSSDEEP CITY', 'SHOAL CAVE'],
    ['ROUTE 126', 'ROUTE 127', 'ROUTE 128', 'UNDERWATER 124', 'UNDERWATER 126', 'SEAFLOOR CAVERN',
     'SOOTOPOLIS CITY', 'CAVE OF ORIGIN', 'SKY PILLAR 1F', 'SKY PILLAR 3F', 'SKY PILLAR 5F'],
    ['ROUTE 129', 'ROUTE 130', 'ROUTE 131', 'ROUTE 132', 'ROUTE 133', 'ROUTE 134', 'PACIFIDLOG TOWN',
     'EVER GRANDE CITY', 'VICTORY ROAD 1F', 'VICTORY ROAD B1F', 'VICTORY ROAD B2F', 'METEOR FALLS B1F'],
]
POSTGAME = {'ARTISAN CAVE', 'ALTERING CAVE', 'METEOR FALLS STEVENS CAVE', 'DESERT UNDERPASS'}
# Water rows: the time column names the method. Surf comes with Norman's badge; the rods are handed
# out in Dewford (Old), on Route 118 (Good) and in Mossdeep (Super).
WATER_STAGE = {'Surfing': 5, 'Old Rod Fishing': 1, 'Good Rod Fishing': 3, 'Super Rod Fishing': 6, 'Fishing': 1}

# (trainer id, display name, type, stage when fought)
BOSSES = [
    ('TRAINER_ROXANNE_1', 'Roxanne', 'ROCK', 0),
    ('TRAINER_BRAWLY_1', 'Brawly', 'FIGHTING', 1),
    ('TRAINER_WATTSON_1', 'Wattson', 'ELECTRIC', 2),
    ('TRAINER_FLANNERY_1', 'Flannery', 'FIRE', 3),
    ('TRAINER_NORMAN_1', 'Norman', 'NORMAL', 4),
    ('TRAINER_WINONA_1', 'Winona', 'FLYING', 5),
    ('TRAINER_TATE_AND_LIZA_1', 'Tate & Liza', 'PSYCHIC', 6),
    ('TRAINER_JUAN_1', 'Juan', 'WATER', 7),
    ('TRAINER_SIDNEY', 'Sidney', 'DARK', 8),
    ('TRAINER_PHOEBE', 'Phoebe', 'GHOST', 8),
    ('TRAINER_GLACIA', 'Glacia', 'ICE', 8),
    ('TRAINER_DRAKE', 'Drake', 'DRAGON', 8),
    ('TRAINER_WALLACE', 'Wallace', 'WATER', 8),
]
TYPE_COLORS = {
    'NORMAL': '#9a9a7a', 'FIRE': '#e0702c', 'WATER': '#4f7fe0', 'ELECTRIC': '#d9b21c', 'GRASS': '#5aa83c',
    'ICE': '#6cc4c4', 'FIGHTING': '#b8302a', 'POISON': '#9a3f9a', 'GROUND': '#c9a04a', 'FLYING': '#8f7fe8',
    'PSYCHIC': '#e9507f', 'BUG': '#94a62a', 'ROCK': '#a8922f', 'GHOST': '#6a5490', 'DRAGON': '#6a3ef0',
    'DARK': '#6a5444', 'STEEL': '#9e9eba', 'FAIRY': '#d985a0',
}


def load_species_data():
    """name, types and evolution targets per species id, from the species_info files."""
    data = {}
    for path in sorted(glob.glob(os.path.join(ROOT, 'src/data/pokemon/species_info/gen_*_families.h'))):
        text = open(path, encoding='utf-8').read()
        for m in re.finditer(r'\[(SPECIES_[A-Z0-9_]+)\]\s*=\s*\{(.*?)\n    \},', text, re.S):
            body = m.group(2)
            name = re.search(r'\.speciesName\s*=\s*_\("([^"]*)"\)', body)
            types = re.search(r'\.types\s*=\s*MON_TYPES\(([^)]*)\)', body)
            evos = re.findall(r'\{EVO_[A-Z_]+,[^,]*,\s*(SPECIES_[A-Z0-9_]+)', body)
            data[m.group(1)] = dict(
                name=name.group(1) if name else m.group(1)[8:].title(),
                types=[t.strip()[5:] for t in types.group(1).split(',')] if types else [],
                evos=evos)
    return data


def evolution_line(sid, data):
    """sid and everything it evolves into, in order (branches included)."""
    line, todo = [], [sid]
    while todo:
        s = todo.pop(0)
        if s in line or s not in data:
            continue
        line.append(s)
        todo.extend(data[s]['evos'])
    return line


def read_sheet_rows():
    wb = openpyxl.load_workbook(s2j.SHEET, read_only=True)
    ws = wb.worksheets[0]
    rows = []
    for row in list(ws.iter_rows(values_only=True))[1:]:
        if not row[0]:
            continue
        area, zone, time = row[0], row[1] or '', row[2] or ''
        for name in row[3:]:
            if name:
                rows.append((area, zone, time, s2j.resolve(str(name))))
    return rows


def vanilla_species():
    data = json.load(open(os.path.join(ROOT, 'src/data/wild_encounters.json'), encoding='utf-8'))
    out = set()
    for g in data['wild_encounter_groups']:
        if g['label'] != 'gWildMonHeadersVanilla':
            continue
        for e in g['encounters']:
            for k, v in e.items():
                if k.endswith('_mons'):
                    out.update(m['species'] for m in v['mons'])
    return out


def read_teams():
    text = open(os.path.join(ROOT, 'src/data/trainers.party'), encoding='utf-8').read()
    teams = {}
    for block in re.split(r'^=== ', text, flags=re.M)[1:]:
        head, _, body = block.partition(' ===')
        mons = []
        for chunk in re.split(r'\n\s*\n', body):
            lines = [l.strip() for l in chunk.strip().splitlines() if l.strip()]
            if not lines or ':' in lines[0] or lines[0].startswith('-'):
                continue
            lvl = next((l.split(':')[1].strip() for l in lines if l.startswith('Level:')), '?')
            mons.append((lines[0].split(' @ ')[0], lvl))
        teams[head.strip()] = mons
    return teams


def area_stage(area, zone, time):
    if area in POSTGAME:
        return 99
    stage = next((i for i, areas in enumerate(STAGES) if area in areas), 99)
    if zone == 'Water':
        stage = max(stage, WATER_STAGE.get(time, 5))
    return stage


def main():
    out_path = sys.argv[1]
    data = load_species_data()
    rows = read_sheet_rows()
    vanilla = vanilla_species()
    teams = read_teams()

    # Wild base species -> (earliest stage, areas)
    found = collections.defaultdict(lambda: [99, set()])
    for area, zone, time, sid in rows:
        if sid not in data:
            continue
        st = area_stage(area, zone, time)
        f = found[sid]
        f[0] = min(f[0], st)
        f[1].add(area.title().replace("Mt. ", "Mt. "))

    sections = []
    for tid, name, typ, stage in BOSSES:
        team = teams.get(tid, [])
        lines, seen = [], set()
        for sid, (st, areas) in sorted(found.items(), key=lambda kv: kv[1][0]):
            if st > stage:
                continue
            line = evolution_line(sid, data)
            key = tuple(line)
            if key in seen or not any(typ in data[s]['types'] for s in line):
                continue
            # skip lines whose base is reachable as a later member of a line we already listed
            if any(set(line) <= set(k) for k in seen):
                continue
            seen.add(key)
            new = not any(s in vanilla for s in line)
            lines.append(dict(line=line, stage=st, areas=sorted(areas), new=new))
        sections.append(dict(tid=tid, name=name, typ=typ, stage=stage, team=team, lines=lines))

    write_html(out_path, sections, data)
    print(f'wrote {out_path}: ' + ', '.join(f"{s['name']} {len(s['lines'])}" for s in sections))


def write_html(path, sections, data):
    def chip(t):
        return f'<span class="chip" style="--t:{TYPE_COLORS.get(t, "#888")}">{t.title()}</span>'

    def mon(s, typ):
        d = data[s]
        cls = 'mon has' if typ in d['types'] else 'mon'
        return f'<span class="{cls}">{html.escape(d["name"])}</span>'

    stage_names = ['before Roxanne', 'after 1 badge', 'after 2', 'after 3', 'after 4',
                   'after 5 (Surf)', 'after 6', 'after 7', 'after 8']
    nav = ''.join(f'<a href="#{s["tid"].lower()}">{html.escape(s["name"])}</a>' for s in sections)
    body = []
    for s in sections:
        team = ', '.join(f'{html.escape(n)} <span class="lv">Lv{l}</span>' for n, l in s['team']) or '(none found)'
        rows = []
        for ln in s['lines']:
            chain = '<span class="arrow">→</span>'.join(mon(x, s['typ']) for x in ln['line'])
            badge = '<span class="new">new</span>' if ln['new'] else '<span class="old">Emerald</span>'
            where = html.escape(', '.join(ln['areas'][:4]) + (' …' if len(ln['areas']) > 4 else ''))
            rows.append(f'<tr><td>{badge}</td><td class="chain">{chain}</td>'
                        f'<td class="stage">{stage_names[min(ln["stage"], 8)]}</td><td class="where">{where}</td></tr>')
        newcount = sum(1 for ln in s['lines'] if ln['new'])
        body.append(f'''
<section id="{s['tid'].lower()}">
  <header><h2>{html.escape(s['name'])}</h2>{chip(s['typ'])}<span class="when">fought {stage_names[min(s['stage'], 8)]}</span></header>
  <p class="team"><span class="label">Current team</span> {team}</p>
  <p class="count">{len(s['lines'])} {s['typ'].title()} lines catchable by then, {newcount} of them new to Emerald</p>
  <div class="scroll"><table>
    <thead><tr><th></th><th>Evolution line <small>(bold: has the type)</small></th><th>First reachable</th><th>Where</th></tr></thead>
    <tbody>{''.join(rows)}</tbody>
  </table></div>
</section>''')

    page = f'''<title>Grafted Emerald Rosters</title>
<link rel="stylesheet" href="https://fonts.googleapis.com/css2?family=Chakra+Petch:wght@600&family=IBM+Plex+Sans:wght@400;600&display=swap">
<style>
/* Layout: one reading column; a jump bar of bosses, then one section per boss with its team and a table of candidate lines */
:root {{
  --bg: #f3f5f2; --panel: #ffffff; --fg: #1d2620; --muted: #5d6a61; --line: #d8ded9; --accent: #1f7a4d; --new: #1f7a4d; --old: #8a948d;
  --display: "Chakra Petch", "Trebuchet MS", sans-serif; --body: "IBM Plex Sans", system-ui, sans-serif;
}}
@media (prefers-color-scheme: dark) {{ :root:not([data-theme="light"]) {{
  --bg: #121815; --panel: #19211d; --fg: #e3ebe5; --muted: #98a69c; --line: #2c3731; --accent: #5fcf95; --new: #5fcf95; --old: #7d8a81; color-scheme: dark }} }}
:root[data-theme="dark"] {{
  --bg: #121815; --panel: #19211d; --fg: #e3ebe5; --muted: #98a69c; --line: #2c3731; --accent: #5fcf95; --new: #5fcf95; --old: #7d8a81; color-scheme: dark }}
body {{ background: var(--bg); color: var(--fg); font: 15px/1.55 var(--body); }}
.wrap {{ max-width: 980px; margin: 0 auto; padding-inline: 16px; padding-block: 28px 64px; display: grid; gap: 28px; }}
h1 {{ font: 600 2rem/1.1 var(--display); margin: 0; text-wrap: balance; letter-spacing: .01em; }}
.intro p {{ max-width: 68ch; color: var(--muted); margin: .5rem 0 0; }}
nav {{ display: flex; flex-wrap: wrap; gap: 6px 14px; font-size: .9rem; }}
nav a {{ color: var(--accent); text-decoration: none; }} nav a:hover, nav a:focus-visible {{ text-decoration: underline; }}
section {{ background: var(--panel); border: 1px solid var(--line); border-radius: 6px; padding: 18px; display: grid; gap: 10px; min-width: 0; }}
section header {{ display: flex; flex-wrap: wrap; align-items: baseline; gap: 10px; }}
h2 {{ font: 600 1.35rem/1.2 var(--display); margin: 0; }}
.chip {{ font-size: .75rem; font-weight: 600; letter-spacing: .06em; text-transform: uppercase; padding: 2px 8px; border-radius: 3px; color: #fff; background: var(--t); }}
.when, .count {{ color: var(--muted); font-size: .9rem; margin: 0; }}
.team {{ margin: 0; }} .label {{ font-size: .72rem; letter-spacing: .08em; text-transform: uppercase; color: var(--muted); margin-right: 6px; }}
.lv {{ color: var(--muted); font-variant-numeric: tabular-nums; font-size: .85rem; }}
.scroll {{ overflow-x: auto; }}
table {{ width: 100%; border-collapse: collapse; font-size: .9rem; }}
th {{ text-align: left; font-weight: 600; font-size: .75rem; letter-spacing: .06em; text-transform: uppercase; color: var(--muted); border-bottom: 1px solid var(--line); padding: 6px 8px; }}
th small {{ text-transform: none; letter-spacing: 0; font-weight: 400; }}
td {{ border-bottom: 1px solid var(--line); padding: 6px 8px; vertical-align: top; }}
.mon.has {{ font-weight: 600; }} .arrow {{ color: var(--muted); margin: 0 4px; }}
.new {{ color: var(--new); font-weight: 600; font-size: .75rem; text-transform: uppercase; letter-spacing: .06em; }}
.old {{ color: var(--old); font-size: .75rem; text-transform: uppercase; letter-spacing: .06em; }}
.stage, .where {{ color: var(--muted); white-space: nowrap; }} .where {{ white-space: normal; min-width: 12ch; }}
</style>
<div class="wrap">
  <div class="intro">
    <h1>Grafted Emerald Rosters</h1>
    <p>Each boss's current team is still Emerald's. Under it is every evolution line of the boss's type that the encounter sheet puts in the wild somewhere the player can reach by that fight. "New" lines never appeared in Emerald's own wild tables. Story stages are approximate: surfing counts from Norman's badge, and the Old, Good and Super Rods from 1, 3 and 6 badges. Generated by tools/encounters/roster_reference.py.</p>
  </div>
  <nav>{nav}</nav>
  {''.join(body)}
</div>
'''
    open(path, 'w', encoding='utf-8').write(page)


if __name__ == '__main__':
    main()
