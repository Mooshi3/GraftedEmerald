"""Candidate graft recolor: the parts of the sprite that show a replaced type take the new type's color;
the rest keeps the original design. All math is integer 5-bit, so it ports to C as is.

recolor_types(pal, species_types, eff_types, colors) -> pal
  pal: 16 (r5, g5, b5); species_types/eff_types: (type1, type2) names; colors: name -> (r5, g5, b5)
"""
from recolor_old import lum5, hue5

OUTLINE_LUM = 4
WHITE_LUM = 27     # near-whites (eyes, shine) stay
GRAY_DELTA = 3     # max-min below this is gray
LUM_PULL = 6       # of 16: how far a group's mean brightness moves toward the type color's


def chroma(c):
    r, g, b = c
    return (r - g, (r + g) // 2 - b)


def dist(c1, c2):
    a1, b1 = chroma(c1)
    a2, b2 = chroma(c2)
    return (a1 - a2) ** 2 + (b1 - b2) ** 2


HUE_SPLIT = 200     # of 1536: two hue groups this far apart are separate color groups
MIN_OWNER = 2       # of 16: a group must cover this share of the colored pixels to stand for a type


def cdiv(a, b):
    """C integer division (rounds toward zero)"""
    q = abs(a) // abs(b)
    return q if (a >= 0) == (b > 0) else -q


def hdist(a, b):
    d = abs(a - b) % 1536
    return min(d, 1536 - d)


def hmean(hs):
    """weighted circular mean of (hue, weight) pairs, by unrolling around the first"""
    h0 = hs[0][0]
    tot = sum(w for h, w in hs)
    acc = sum(((h - h0 + 768) % 1536 - 768) * w for h, w in hs)
    return (h0 + cdiv(acc, max(1, tot)) + 1536) % 1536


OUTLINE_EDGE = 5    # of 16: a gray whose pixels touch transparency this often is an outline


def groups_of(pal, counts, edges):
    """-> list of groups (lists of palette indices): up to 2 hue groups, then the grays"""
    colored, grays = [], []
    for i in range(1, min(16, len(pal))):
        r, g, b = pal[i]
        l = lum5(r, g, b)
        d = max(r, g, b) - min(r, g, b)
        if l < OUTLINE_LUM or (l >= WHITE_LUM and d < 6) or counts[i] == 0:
            continue
        h = hue5(r, g, b)
        if d < GRAY_DELTA or h is None:
            if edges[i] * 16 < counts[i] * OUTLINE_EDGE:
                grays.append(i)
        else:
            colored.append((h, i))
    out = []
    if colored:
        # 2-means on hue, seeded with the two hues farthest apart
        a, b = max(((x, y) for x in colored for y in colored), key=lambda p: hdist(p[0][0], p[1][0]))
        ca, cb = a[0], b[0]
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
    return out


def mean(pal, g):
    return tuple(sum(pal[i][c] for i in g) // len(g) for c in range(3))


def shade(t, l):
    """the type color at luminance l"""
    lt = max(1, lum5(*t))
    if l <= lt:
        return tuple(c * l // lt for c in t)
    return tuple(c + (31 - c) * (l - lt) // max(1, 31 - lt) for c in t)


def paint(pal, out, g, t):
    lt = lum5(*t)
    ls = [lum5(*pal[i]) for i in g]
    shift = cdiv((lt - sum(ls) // len(ls)) * LUM_PULL, 16)
    for i, l in zip(g, ls):
        out[i] = shade(t, max(OUTLINE_LUM, min(30, l + shift)))


SECOND_MIN = 4      # of 16: a second color group must cover this share of the main group's pixels
SPLIT = 6           # of 16: share of the main group's pixels (brightest first) that takes the second type
SPLIT_DARK = False  # True: the darkest share takes it instead


def split_main(pal, g, counts):
    g = sorted(g, key=lambda i: lum5(*pal[i]), reverse=not SPLIT_DARK)
    total = sum(counts[i] for i in g)
    part, acc = [], 0
    for i in g:
        target = total * SPLIT
        if part and abs((acc + counts[i]) * 16 - target) >= abs(acc * 16 - target):
            break
        part.append(i)
        acc += counts[i]
    if len(part) == len(g):
        part = part[:-1]
    return [i for i in g if i not in part], part


def recolor_types(pal, species, eff, colors, counts=None):
    counts, edges = counts or ([1] * 16, [0] * 16)
    out = list(pal)
    changed = [eff[k] != species[k] for k in range(2)]
    if not any(changed):
        return out
    groups = groups_of(pal, counts, edges)
    if not groups:
        return out
    total = sum(counts[i] for g in groups for i in g)
    big = [j for j in range(len(groups)) if sum(counts[i] for i in groups[j]) * 16 >= total * MIN_OWNER] or [0]
    mono = species[0] == species[1]
    # Which group shows each species type: the closest in hue/saturation
    owner = [min(big, key=lambda j: dist(mean(pal, groups[j]), colors[species[k]])) for k in range(2)]
    if not mono and owner[0] == owner[1]:
        # both types point at the same group: it shows the closer one; the other gets what's left
        m = mean(pal, groups[owner[0]])
        if dist(m, colors[species[1]]) < dist(m, colors[species[0]]):
            species, eff, changed = species[::-1], eff[::-1], changed[::-1]
    if mono or owner[0] == owner[1]:
        w = lambda j: sum(counts[i] for i in groups[j])
        others = sorted((j for j in range(len(groups)) if j != owner[0]), key=w, reverse=True)
        # second type: the biggest other group if it's big enough, else part of the main one
        owner[1] = others[0] if others and w(others[0]) * 16 >= w(owner[0]) * SECOND_MIN else None
    targets = []  # (group indices, type)
    for k in range(2):
        if not changed[k]:
            continue
        if owner[k] is not None:
            targets.append((groups[owner[k]], eff[k]))
        else:
            body, part = split_main(pal, groups[owner[0]], counts)
            if changed[0]:
                targets = [(body, eff[0])]
            targets.append((part, eff[k]))
    for g, t in targets:
        if g:
            paint(pal, out, g, colors[t])
    return out
