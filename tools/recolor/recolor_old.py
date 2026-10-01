"""Port of the previous in-game recolor (ApplyGraftTypePaletteSwap, replaced by src/graft_recolor.c), kept for A/B sheets.

Palettes are lists of 16 (r5, g5, b5) tuples (GBA 5-bit channels). Integer math matches the C.
"""

OUTLINE_LUM = 4
SAT_THRESHOLD = 3
HUE_GAP_MIN = 85


def lum5(r, g, b):
    return (77 * r + 151 * g + 28 * b) >> 8


def hue5(r, g, b):
    mx, mn = max(r, g, b), min(r, g, b)
    d = mx - mn
    if d < SAT_THRESHOLD:
        return None
    if mx == r:
        h = int((g - b) * 256 / d)
    elif mx == g:
        h = 512 + int((b - r) * 256 / d)
    else:
        h = 1024 + int((r - g) * 256 / d)
    return h % 1536


def is_neutral(r, g, b):
    d = max(r, g, b) - min(r, g, b)
    l = lum5(r, g, b)
    if l <= OUTLINE_LUM:
        return d < 1
    t256 = (l - OUTLINE_LUM) * 256 // (31 - OUTLINE_LUM)
    return d < 1 + ((t256 * 6) >> 8)


def shade_continuous(t, l, mn, mx):
    tr, tg, tb = t
    t256 = 128 if mx <= mn else (l - mn) * 256 // (mx - mn)
    dr, dg, db = (tr * 7) >> 4, (tg * 7) >> 4, (tb * 7) >> 4
    lr, lg, lb = tr + (((31 - tr) * 9) >> 4), tg + (((31 - tg) * 9) >> 4), tb + (((31 - tb) * 9) >> 4)
    if t256 <= 128:
        s = t256 * 2
        out = (dr + (((tr - dr) * s) >> 8), dg + (((tg - dg) * s) >> 8), db + (((tb - db) * s) >> 8))
    else:
        s = (t256 - 128) * 2
        out = (tr + (((lr - tr) * s) >> 8), tg + (((lg - tg) * s) >> 8), tb + (((lb - tb) * s) >> 8))
    return tuple(max(0, min(31, c)) for c in out)


def recolor(pal, c1, c2, dual):
    n = min(16, len(pal))
    hue = [0] * n
    cluster = [0] * n
    valid = []
    gmin, gmax = 31, 0
    for i in range(1, n):
        r, g, b = pal[i]
        l = lum5(r, g, b)
        if l < OUTLINE_LUM or is_neutral(r, g, b):
            continue
        gmin, gmax = min(gmin, l), max(gmax, l)
        h = hue5(r, g, b)
        if h is not None:
            hue[i] = h
            valid.append(i)
    if dual and len(valid) >= 2:
        valid.sort(key=lambda i: hue[i])  # stable like the insertion sort
        nv = len(valid)
        g1s = g2s = g1 = g2 = 0
        for k in range(nv):
            hc, hn = hue[valid[k]], hue[valid[(k + 1) % nv]]
            gap = 1536 - hc + hn if k + 1 == nv else hn - hc
            if gap > g1s:
                g2s, g2, g1s, g1 = g1s, g1, gap, k
            elif gap > g2s:
                g2s, g2 = gap, k
        if g2s < HUE_GAP_MIN:
            order = sorted(range(nv), key=lambda k: lum5(*pal[valid[k]]))
            na = max(1, nv * 4 // 10)
            for pos, k in enumerate(order):
                cluster[valid[k]] = 1 if pos >= nv - na else 0
            if sum(cluster[v] for v in valid) * 2 > nv:
                for v in valid:
                    cluster[v] = 1 - cluster[v]
        else:
            lo, hi = min(g1, g2), max(g1, g2)
            big_b = (hi - lo) > nv - (hi - lo)
            for k in range(nv):
                inb = lo < k <= hi
                cluster[valid[k]] = (0 if big_b else 1) if inb else (1 if big_b else 0)
            c1n = sum(cluster[v] for v in valid)
            c0n = nv - c1n
            small = min(c0n, c1n)
            minsize = max(2, nv * 3 // 10)
            if small < minsize:
                sc = 0 if c0n < c1n else 1
                hs = [hue[v] for v in valid if cluster[v] == sc]
                avg = sum(hs) // len(hs) if hs else 0
                for _ in range(minsize - small):
                    best, bd = None, 9999
                    for v in valid:
                        if cluster[v] == sc:
                            continue
                        hd = abs(hue[v] - avg)
                        if hd > 768:
                            hd = 1536 - hd
                        if hd < bd:
                            bd, best = hd, v
                    if best is None:
                        break
                    cluster[best] = sc
    out = list(pal)
    for i in range(1, n):
        r, g, b = pal[i]
        l = lum5(r, g, b)
        if l < OUTLINE_LUM or is_neutral(r, g, b):
            continue
        out[i] = shade_continuous(c1 if cluster[i] == 0 else c2, l, gmin, gmax)
    return out
