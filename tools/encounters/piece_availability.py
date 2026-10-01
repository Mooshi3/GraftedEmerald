#!/usr/bin/env python3
"""When each notable graft piece first becomes harvestable from wild Pokémon, by badge count.

Reads src/data/wild_encounters.json (the National Dex tables, gWildMonHeaders) and the species data. Wild mons
give their non-hidden abilities only. The area -> badge table below is approximate (when an area is first
reachable in a normal playthrough), and the method sets a floor: Surf 5, Rock Smash 3 (Dynamo Badge... see
FLOOR), Old Rod 1, Good Rod 3, Super Rod 7, Dive 7.
usage: tools/encounters/piece_availability.py OUT.md
"""
import glob, json, os, re, sys

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..'))

# First reachable, in badges (9 = postgame). Prefix match on the map constant, longest prefix wins.
AREAS = {
    'MAP_ROUTE101': 0, 'MAP_ROUTE102': 0, 'MAP_ROUTE103': 0, 'MAP_ROUTE104': 0, 'MAP_PETALBURG': 0,
    'MAP_ROUTE116': 0, 'MAP_RUSTURF_TUNNEL': 0,
    'MAP_DEWFORD': 1, 'MAP_GRANITE_CAVE': 1, 'MAP_ROUTE105': 1, 'MAP_ROUTE106': 1, 'MAP_ROUTE107': 1,
    'MAP_ROUTE108': 1, 'MAP_ROUTE109': 1, 'MAP_SLATEPORT': 1, 'MAP_ROUTE110': 1,
    'MAP_ROUTE117': 2, 'MAP_ROUTE118': 2, 'MAP_ROUTE112': 2, 'MAP_ROUTE113': 2, 'MAP_ROUTE114': 2,
    'MAP_ROUTE115': 2, 'MAP_METEOR_FALLS_1F': 2, 'MAP_MAUVILLE': 2,
    'MAP_MT_CHIMNEY': 3, 'MAP_JAGGED_PASS': 3, 'MAP_FIERY_PATH': 3,
    'MAP_ROUTE111': 4, 'MAP_MIRAGE_TOWER': 4,
    'MAP_ROUTE119': 5, 'MAP_ROUTE120': 5, 'MAP_NEW_MAUVILLE': 5, 'MAP_ALTERING_CAVE': 5, 'MAP_ABANDONED_SHIP': 5,
    'MAP_ROUTE121': 6, 'MAP_ROUTE122': 6, 'MAP_ROUTE123': 6, 'MAP_MT_PYRE': 6, 'MAP_LILYCOVE': 6,
    'MAP_SAFARI_ZONE': 6, 'MAP_ROUTE124': 6, 'MAP_ROUTE125': 6, 'MAP_MOSSDEEP': 6, 'MAP_SHOAL_CAVE': 6,
    'MAP_AQUA_HIDEOUT': 6, 'MAP_MAGMA_HIDEOUT': 6, 'MAP_ROUTE126': 6, 'MAP_ROUTE127': 6,
    'MAP_ROUTE128': 7, 'MAP_ROUTE129': 7, 'MAP_ROUTE130': 7, 'MAP_ROUTE131': 7, 'MAP_ROUTE132': 6,
    'MAP_ROUTE133': 6, 'MAP_ROUTE134': 6, 'MAP_PACIFIDLOG': 6, 'MAP_SEAFLOOR_CAVERN': 7, 'MAP_UNDERWATER': 7,
    'MAP_SOOTOPOLIS': 7, 'MAP_CAVE_OF_ORIGIN': 7, 'MAP_SKY_PILLAR': 7,
    'MAP_VICTORY_ROAD': 8, 'MAP_EVER_GRANDE': 8,
    'MAP_METEOR_FALLS_B1F': 8, 'MAP_METEOR_FALLS_STEVENS_CAVE': 9, 'MAP_ARTISAN_CAVE': 9, 'MAP_DESERT_UNDERPASS': 9,
}
FLOOR = {'land_mons': 0, 'water_mons': 5, 'rock_smash_mons': 3, 'hidden_mons': 0}
RODS = [(0, 1), (2, 3), (5, 7)]  # fishing slot ranges: old rod slots 0-1, good rod 2-4, super rod 5-9
NOTABLE = ['HUGE_POWER', 'PURE_POWER', 'SPEED_BOOST', 'WONDER_GUARD', 'SHADOW_TAG', 'ARENA_TRAP', 'MOODY',
           'PROTEAN', 'LIBERO', 'DRIZZLE', 'DROUGHT', 'SAND_STREAM', 'SNOW_WARNING', 'MAGIC_GUARD', 'SIMPLE',
           'CONTRARY', 'ADAPTABILITY', 'TOUGH_CLAWS', 'SHEER_FORCE', 'TECHNICIAN', 'SKILL_LINK', 'TINTED_LENS',
           'INTIMIDATE', 'LEVITATE', 'UNAWARE', 'REGENERATOR', 'MAGIC_BOUNCE', 'PRANKSTER', 'SERENE_GRACE',
           'GUTS', 'POISON_HEAL', 'MULTISCALE', 'FUR_COAT', 'WATER_BUBBLE', 'ICE_SCALES', 'IMPOSTER',
           'GORILLA_TACTICS', 'DOWNLOAD', 'SHARPNESS', 'STRONG_JAW', 'IRON_FIST', 'TECHNICIAN', 'SWIFT_SWIM',
           'CHLOROPHYLL', 'SAND_RUSH', 'SLUSH_RUSH', 'UNBURDEN', 'STURDY', 'DISGUISE', 'GOOD_AS_GOLD',
           'PURIFYING_SALT', 'THICK_FAT', 'FLUFFY', 'BEAST_BOOST', 'SOUL_HEART', 'NO_GUARD', 'COMPOUND_EYES']
STATS = ['baseHP', 'baseAttack', 'baseDefense', 'baseSpeed', 'baseSpAttack', 'baseSpDefense']
STAT_NAMES = ['HP', 'Atk', 'Def', 'Spe', 'SpA', 'SpD']


CAPS = [15, 19, 24, 29, 31, 33, 42, 46, 58, 100]  # level cap by badges (src/caps.c), then postgame


def evolutions():
    """species -> [(level, evolved species)] for level-up evolutions (EVO_LEVEL and its variants)"""
    out = {}
    for f in glob.glob(os.path.join(ROOT, 'src/data/pokemon/species_info/gen_*_families.h')):
        txt = open(f).read()
        for m in re.finditer(r'\[(SPECIES_\w+)\]\s*=\s*\{(.*?)\n    \},', txt, re.S):
            ev = re.search(r'\.evolutions = EVOLUTION\((.*?)\),?\n', m.group(2), re.S)
            if ev:
                text = ev.group(1)
                for kind, lvl, target, rest in re.findall(r'\{(EVO_LEVEL\w*),\s*(\d+),\s*(SPECIES_\w+)(.*?)\}(?=\s*[,)])', text, re.S):
                    if int(lvl) > 0 and 'CONDITIONS' not in rest:  # plain level-up (not a move, item or time)
                        out.setdefault(m.group(1), []).append((int(lvl), target))
                # Shedinja-style: a second mon appears when the first evolves
                for evo, extra in re.findall(r'\{EVO_SPLIT_FROM_EVO,\s*(SPECIES_\w+),\s*(SPECIES_\w+)', text):
                    lv = [l for l, t in out.get(m.group(1), []) if t == evo]
                    if lv:
                        out[m.group(1)].append((lv[0], extra))
    return out


def area_badges(m):
    best = None
    for p, b in AREAS.items():
        if m.startswith(p) and (best is None or len(p) > len(best[0])):
            best = (p, b)
    return None if best is None else best[1]


def species_data():
    out = {}
    for f in glob.glob(os.path.join(ROOT, 'src/data/pokemon/species_info/gen_*_families.h')):
        txt = open(f).read()
        for m in re.finditer(r'\[(SPECIES_\w+)\]\s*=\s*\{(.*?)\n    \},', txt, re.S):
            body = m.group(2)
            ab = re.search(r'\.abilities = \{ ?(ABILITY_\w+), ?(ABILITY_\w+), ?(ABILITY_\w+) ?\}', body)
            st = {s: re.search(r'\.' + s + r'\s*=\s*(\d+)', body) for s in STATS}
            if not ab or not all(st.values()):
                continue
            out[m.group(1)] = ([a[8:] for a in ab.groups()[:2] if a != 'ABILITY_NONE'],
                               [int(st[s].group(1)) for s in STATS])
    return out


def main(path):
    data = species_data()
    enc = json.load(open(os.path.join(ROOT, 'src/data/wild_encounters.json')))['wild_encounter_groups'][0]['encounters']
    first = {}  # species -> (badges, where)
    for e in enc:
        b = area_badges(e['map'])
        if b is None:
            continue
        for method, floor in list(FLOOR.items()) + [('fishing_mons', None)]:
            if method not in e:
                continue
            for slot, mon in enumerate(e[method]['mons']):
                if floor is None:
                    rod = next((k for k, (lo, hi) in enumerate(RODS) if lo <= slot <= hi), 2)
                    req = max(b, (1, 3, 7)[rod])
                    how = ('Old', 'Good', 'Super')[rod] + ' Rod'
                else:
                    req = max(b, floor)
                    how = {'land_mons': 'grass', 'water_mons': 'surf', 'rock_smash_mons': 'Rock Smash',
                           'hidden_mons': 'hidden'}[method]
                sp = mon['species']
                where = f"{e['map'][4:].replace('_', ' ').title()} ({how})"
                if sp not in first or req < first[sp][0]:
                    first[sp] = (req, where)
    # Level-up evolutions within each badge count's level cap: the Splicer breaks down evolved mons too
    evo = evolutions()
    changed = True
    while changed:
        changed = False
        for sp, (b, where) in list(first.items()):
            for lvl, target in evo.get(sp, []):
                # an evolved mon is broken down with the Splicer, from 3 badges
                t = max(3, next(k for k in range(10) if CAPS[k] >= lvl and k >= b))
                if target not in first or t < first[target][0]:
                    first[target] = (t, f"evolve {sp[8:].replace('_', ' ').title()} at Lv{lvl} ({where.split(' (')[0]})")
                    changed = True
    lines = ['# Graft piece availability by badge', '',
             'When each piece can first be harvested from a wild Pokémon (Patch/Graft/Splice Ball, or the Splicer on '
             "a catch), by badges, including level-up evolutions within that badge count's level cap (the Splicer "
             "breaks down evolved mons too; stone, trade and friendship evolutions aren't counted). Areas are dated "
             'by when they are first reachable (approximate); Surf needs 5 badges, Rock Smash 3, Old/Good/Super Rod '
             '1/3/7, Dive 7. Hidden abilities never drop from wild mons.', '']
    lines += ['## Notable abilities', '', '| Ability | Badges | Earliest source |', '|---|---|---|']
    abil = {}
    for sp, (b, where) in first.items():
        if sp not in data:
            continue
        for a in data[sp][0]:
            if a not in abil or b < abil[a][0]:
                abil[a] = (b, f"{sp[8:].replace('_', ' ').title()}, {where}")
    for a in sorted(set(NOTABLE), key=lambda a: (abil.get(a, (99,))[0], a)):
        b, src = abil.get(a, (None, 'not in the wild tables'))
        lines.append(f"| {a.replace('_', ' ').title()} | {'-' if b is None else ('postgame' if b == 9 else b)} | {src} |")
    lines += ['', '## Highest base stat available as a piece, by badges', '',
              '| Badges | ' + ' | '.join(STAT_NAMES) + ' |', '|---|' + '---|' * 6]
    for tier in range(0, 10):
        row = []
        for s in range(6):
            best = max(((data[sp][1][s], sp) for sp, (b, w) in first.items() if sp in data and b <= tier), default=None)
            row.append('-' if best is None else f"{best[0]} ({best[1][8:].replace('_', ' ').title()})")
        lines.append(f"| {'postgame' if tier == 9 else tier} | " + ' | '.join(row) + ' |')
    open(path, 'w').write('\n'.join(lines) + '\n')
    print('\n'.join(lines))


if __name__ == '__main__':
    main(sys.argv[1])
