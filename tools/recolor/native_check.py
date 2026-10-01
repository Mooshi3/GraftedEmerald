"""Checks src/graft_recolor.c against recolor_v3.py: builds the C for the host and compares every sprite.

usage: native_check.py [COMBOS_PER_MON]   (exits 1 on any difference)
"""
import ctypes, os, random, subprocess, sys, tempfile
import sheet, recolor_v3

ROOT = sheet.ROOT
so = os.path.join(tempfile.gettempdir(), 'graft_recolor_native.so')
subprocess.run(['gcc', '-O1', '-shared', '-fPIC', '-DGRAFT_RECOLOR_NATIVE', '-o', so,
                os.path.join(ROOT, 'src/graft_recolor.c')], check=True)
lib = ctypes.CDLL(so)
U16x16, U8x2, U16x2 = ctypes.c_uint16 * 16, ctypes.c_uint8 * 2, ctypes.c_uint16 * 2
TYPE_IDS = {t: i + 1 for i, t in enumerate(sheet.TYPES)}


def rgb(c):
    return c[0] | (c[1] << 5) | (c[2] << 10)


def to_gfx(im):
    """the frame as 4bpp tiles, like the game's decompressed sprite"""
    px = im.load()
    out = bytearray(2048)
    for y in range(64):
        for x in range(64):
            off = ((y >> 3) * 8 + (x >> 3)) * 32 + (y & 7) * 4 + ((x & 7) >> 1)
            out[off] |= (px[x, y] & 15) << ((x & 1) * 4)
    return bytes(out)


def main():
    per = int(sys.argv[1]) if len(sys.argv) > 1 else 6
    colors = sheet.type_colors()
    spt = sheet.species_types()
    rnd = random.Random(1)
    mons = [m for m in sorted(os.listdir(os.path.join(ROOT, 'graphics/pokemon')))
            if m in spt and os.path.exists(os.path.join(ROOT, 'graphics/pokemon', m, 'normal.pal'))]
    bad = checked = 0
    for m in mons:
        try:
            im, pal, (counts, edges) = sheet.load(m)
        except Exception:
            continue
        pal = (pal + [(0, 0, 0)] * 16)[:16]
        gfx = to_gfx(im)
        c_counts, c_edges = U16x16(), U16x16()
        lib.GraftCountSpritePixels(gfx, c_counts, c_edges)
        if list(c_counts) != counts or list(c_edges) != edges:
            print('pixel counts differ:', m)
            bad += 1
            continue
        sp = spt[m]
        for _ in range(per):
            ef = [rnd.choice(sheet.TYPES), rnd.choice(sheet.TYPES)]
            if rnd.random() < 0.6:
                ef[rnd.randrange(2)] = sp[rnd.randrange(2)]
            py = recolor_v3.recolor_types(pal, sp, tuple(ef), colors, (counts, edges))
            dst = U16x16()
            lib.GraftRecolorPalette(U16x16(*map(rgb, pal)), dst, c_counts, c_edges,
                                    U8x2(*(TYPE_IDS[t] for t in sp)), U8x2(*(TYPE_IDS[t] for t in ef)),
                                    U16x2(*(rgb(colors[t]) for t in sp)), U16x2(*(rgb(colors[t]) for t in ef)))
            checked += 1
            if list(dst) != [rgb(c) for c in py]:
                bad += 1
                if bad <= 10:
                    print('differs:', m, sp, ef)
    print(f'{checked} recolors checked, {bad} differ')
    sys.exit(1 if bad else 0)


if __name__ == '__main__':
    main()
