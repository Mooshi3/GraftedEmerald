"""Graft recolor reference (src/graft_recolor.c follows this): both resulting types must show.

Each effective type gets a region of the sprite: the color group that shows the species' type in that slot,
or for a type with no group of its own, the next color group, else part of the main group (the shadows for a
darker type, the highlights for a lighter one). A region is repainted in its type's color when that type was
replaced, or when its colors don't look like the type (a kept type stays only if it's recognizable).

recolor_types(pal, species_types, eff_types, colors, (counts, edges)) -> pal
  pal: 16 (r5, g5, b5); types are names; colors: name -> (r5, g5, b5); counts/edges: per palette color, the
  pixels in the sprite's first frame and how many of them touch transparency
"""
from recolor_old import lum5, hue5

OUTLINE_LUM = 4    # darker than this always stays
WHITE_LUM = 27     # near-white: stays unless whites cover much of the sprite
WHITE_DELTA = 6
WHITE_BODY = 4     # of 16: whites covering this share of the colored + white pixels are a body color
GRAY_DELTA = 3     # max - min channel below this: a gray with no usable hue
OUTLINE_EDGE = 5   # of 16: a gray whose pixels touch transparency this often is an outline
HUE_SPLIT = 96     # of 1536: two hue groups this far apart are separate color groups
MIN_OWNER = 2      # of 16: a group must cover this share of the pixels to stand for a type
SECOND_MIN = 2     # of 16: a second group must cover this share of the main group's pixels to take a type
SPLIT = 6          # of 16: share of the main group's pixels (its shadows) a type without a group takes
DARK_MON_LUM = 12  # a main group darker than this gives its highlights instead (dark shadows can't show a color)
LUM_PULL = 6       # of 16: how far a region's mean brightness moves toward the type color's
REC_DIST = 120     # a kept type's region looks like it within this hue/saturation distance
REC_LUM = 9        # ...and this brightness difference


def cdiv(a, b):
    """C integer division (rounds toward zero)"""
    q = abs(a) // abs(b)
    return q if (a >= 0) == (b > 0) else -q


def chroma(c):
    r, g, b = c
    return (r - g, (r + g) // 2 - b)


def dist(c1, c2):
    a1, b1 = chroma(c1)
    a2, b2 = chroma(c2)
    return (a1 - a2) ** 2 + (b1 - b2) ** 2


def match_dist(c1, c2):
    """for matching a group to a type: hue/saturation, plus a little brightness"""
    dl = lum5(*c1) - lum5(*c2)
    return dist(c1, c2) + dl * dl // 4


def hdist(a, b):
    d = abs(a - b) % 1536
    return min(d, 1536 - d)


def hmean(hs):
    h0 = hs[0][0]
    tot = sum(w for h, w in hs)
    acc = sum(((h - h0 + 768) % 1536 - 768) * w for h, w in hs)
    return (h0 + cdiv(acc, max(1, tot)) + 1536) % 1536


def groups_of(pal, counts, edges):
    """-> (groups, white): groups are lists of palette indices, biggest hue group first: up to 2 hue groups,
    the grays, then the whites if they're a body color; white is the whites' index or -1"""
    colored, grays, whites = [], [], []
    for i in range(1, 16):
        r, g, b = pal[i]
        l = lum5(r, g, b)
        d = max(r, g, b) - min(r, g, b)
        if l < OUTLINE_LUM or counts[i] == 0:
            continue
        if l >= WHITE_LUM and d < WHITE_DELTA:
            whites.append(i)
            continue
        h = hue5(r, g, b)
        if h is None:
            if edges[i] * 16 < counts[i] * OUTLINE_EDGE:
                grays.append(i)
        else:
            colored.append((h, i))
    out = []
    if colored:
        a, b = max(((x, y) for x in colored for y in colored), key=lambda p: hdist(p[0][0], p[1][0]))
        ca, cb = a[0], b[0]
        ga, gb = colored, []
        for _ in range(4):
            ga = [(h, i) for h, i in colored if hdist(h, ca) <= hdist(h, cb)]
            gb = [(h, i) for h, i in colored if hdist(h, ca) > hdist(h, cb)]
            if not gb:
                break
            ca = hmean([(h, counts[i]) for h, i in ga])
            cb = hmean([(h, counts[i]) for h, i in gb])
        if gb and hdist(ca, cb) >= HUE_SPLIT:
            out += sorted(([i for h, i in ga], [i for h, i in gb]), key=lambda g: sum(counts[i] for i in g), reverse=True)
        else:
            out.append([i for h, i in colored])
    if grays:
        out.append(grays)
    wsum = sum(counts[i] for i in whites)
    if whites and wsum * 16 >= (wsum + sum(counts[i] for g in out for i in g)) * WHITE_BODY:
        out.append(whites)
        return out, len(out) - 1
    return out, -1


def mean(pal, g):
    return tuple(sum(pal[i][c] for i in g) // len(g) for c in range(3))


def shade(t, l):
    lt = max(1, lum5(*t))
    if l <= lt:
        return tuple(c * l // lt for c in t)
    return tuple(c + (31 - c) * (l - lt) // max(1, 31 - lt) for c in t)


def paint(pal, out, g, t):
    if not g:
        return
    lt = lum5(*t)
    ls = [lum5(*pal[i]) for i in g]
    shift = cdiv((lt - sum(ls) // len(ls)) * LUM_PULL, 16)
    for i, l in zip(g, ls):
        out[i] = shade(t, max(OUTLINE_LUM, min(30, l + shift)))


def split(pal, g, counts, bright=False):
    """-> (rest, part): part is the darkest (or brightest) colors covering about SPLIT/16 of the pixels
    (shadows run along edges and read as shading; highlights are local and read as spots)"""
    g = sorted(g, key=lambda i: lum5(*pal[i]), reverse=bright)
    target = sum(counts[i] for i in g) * SPLIT
    part, acc = [], 0
    for i in g:
        if part and abs((acc + counts[i]) * 16 - target) >= abs(acc * 16 - target):
            break
        part.append(i)
        acc += counts[i]
    if len(part) == len(g):
        part = part[:-1]
    return [i for i in g if i not in part], part


NEUTRAL_CHROMA = 40  # a type color this close to gray (Normal, Steel, Rock, Ghost, Dark) can be shown by whites


def is_neutral(t):
    a, b = chroma(t)
    return a * a + b * b <= NEUTRAL_CHROMA


def recognizable(pal, g, t):
    m = mean(pal, g)
    return dist(m, t) <= REC_DIST and abs(lum5(*m) - lum5(*t)) <= REC_LUM


def recolor_types(pal, species, eff, colors, ce=None):
    counts, edges = ce or ([1] * 16, [0] * 16)
    pal = (list(pal) + [(0, 0, 0)] * 16)[:16]
    out = list(pal)
    species, eff = list(species), list(eff)
    changed = [eff[k] != species[k] for k in range(2)]
    if not any(changed):
        return out
    groups, white = groups_of(pal, counts, edges)
    if not groups:
        return out
    w = [sum(counts[i] for i in g) for g in groups]
    col = lambda t: colors[t]
    big = [j for j in range(len(groups)) if w[j] * 16 >= sum(w) * MIN_OWNER]

    def owner_of(t):
        cands = [j for j in big if j != white or is_neutral(col(t))] or [0]
        return min(cands, key=lambda j: match_dist(mean(pal, groups[j]), col(t)))

    owner = [owner_of(species[0]), owner_of(species[1])]
    if species[0] != species[1] and owner[0] == owner[1]:
        # both types point at the same group: it shows the closer one; the other gets what's left
        m = mean(pal, groups[owner[0]])
        if match_dist(m, col(species[1])) < match_dist(m, col(species[0])):
            species.reverse(), eff.reverse(), changed.reverse()
    regions = [groups[owner[0]], None]
    if species[0] != species[1] and owner[0] != owner[1]:
        regions[1] = groups[owner[1]]
    else:
        # the next biggest group big enough to see (whites only for a whitish type)
        others = sorted((j for j in range(len(groups)) if j != owner[0] and (j != white or is_neutral(col(eff[1])))),
                        key=lambda j: -w[j])
        if others and w[others[0]] * 16 >= w[owner[0]] * SECOND_MIN:
            regions[1] = groups[others[0]]
    if eff[0] == eff[1]:
        # one type in both slots: all of it
        for g in groups:
            paint(pal, out, g, col(eff[0]))
        return out
    if regions[1] is None:
        main = regions[0]
        # The shadows, unless they're too dark to show a color, or the highlights would leave the rest
        # looking like the kept type (a hued group only: Absol's dark face stays, its pale fur takes the type)
        bright = lum5(*mean(pal, main)) < DARK_MON_LUM
        regions = list(split(pal, main, counts, bright))
        if not bright and not changed[0] and not recognizable(pal, regions[0], col(eff[0])) \
                and all(hue5(*pal[i]) is not None for i in main):
            alt = list(split(pal, main, counts, True))
            if recognizable(pal, alt[0], col(eff[0])):
                regions = alt
        # the part is new either way: it shows eff[1]; the rest shows eff[0]
        paint(pal, out, regions[1], col(eff[1]))
        if changed[0] or not recognizable(pal, regions[0], col(eff[0])):
            paint(pal, out, regions[0], col(eff[0]))
        return out
    for k in range(2):
        if changed[k] or not recognizable(pal, regions[k], col(eff[k])):
            paint(pal, out, regions[k], col(eff[k]))
    return out
