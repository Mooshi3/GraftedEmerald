// Graft type recolor: both of a grafted mon's types show on its sprite.
//
// 1. Palette colors are split into groups: up to two hue groups (a weighted 2-means on hue), the grays, and
//    the whites when they cover much of the sprite. Outlines stay: near-black colors, grays whose pixels mostly
//    touch transparency, and small whites (eye whites, shine).
// 2. Each of the species' types is matched to the group that looks most like it. A type with no group of its
//    own takes the next biggest group, else the shadows of the main one (highlights would read as spots).
//    Whites only ever stand for a whitish type (Normal, Steel, Rock, Ghost, Dark).
// 3. A region is repainted in its type's color, keeping each color's brightness, when its type was replaced or
//    when its colors don't look like the type (a kept type's colors stay only if they're recognizable).
//
// tools/recolor/ has the reference (recolor_v3.py) and contact sheets; keep the two in step.
// The core works on plain arrays so tools/recolor/native_check.py can compile it on the host.

#ifdef GRAFT_RECOLOR_NATIVE
#include <stdint.h>
#include <stddef.h>
typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef int16_t s16;
typedef int8_t s8;
typedef int32_t s32;
typedef int bool32;
#define TRUE 1
#define FALSE 0
#else
#include "global.h"
#include "graft_data.h"
#include "palette.h"
#include "pokemon.h"
#endif

#define PAL_SIZE        16
#define SPRITE_DIM      64
#define OUTLINE_LUM     4   // darker than this always stays (outlines, pupils)
#define WHITE_LUM       27  // near-white: stays unless whites cover much of the sprite
#define WHITE_DELTA     6
#define WHITE_BODY      4   // of 16: whites covering this share of the colored + white pixels are a body color
#define GRAY_DELTA      3   // max - min channel below this: a gray with no usable hue
#define OUTLINE_EDGE    5   // of 16: a gray whose pixels touch transparency this often is an outline
#define HUE_SPLIT       96  // of 1536: two hue groups this far apart are separate color groups
#define MIN_OWNER       2   // of 16: a group must cover this share of the pixels to stand for a type
#define SECOND_MIN      2   // of 16: a second group must cover this share of the main group's pixels
#define SPLIT           6   // of 16: share of the main group's pixels (its shadows) a type without a group takes
#define LUM_PULL        6   // of 16: how far a region's mean brightness moves toward the type color's
#define REC_DIST        120 // a kept type's region looks like it within this hue/saturation distance...
#define REC_LUM         9   // ...and this brightness difference
#define NEUTRAL_CHROMA  40  // a type color this close to gray can be shown by whites
#define MAX_GROUPS      4

#define R5(c) ((c) & 31)
#define G5(c) (((c) >> 5) & 31)
#define B5(c) (((c) >> 10) & 31)
#define RGB5(r, g, b) ((u16)((r) | ((g) << 5) | ((b) << 10)))

static s32 Lum(s32 r, s32 g, s32 b)
{
    return (77 * r + 151 * g + 28 * b) >> 8;
}

static s32 ColorLum(u16 c)
{
    return Lum(R5(c), G5(c), B5(c));
}

// Hue on a 0..1535 wheel, or -1 for a gray
static s32 Hue(u16 c)
{
    s32 r = R5(c), g = G5(c), b = B5(c);
    s32 mx = r > g ? (r > b ? r : b) : (g > b ? g : b);
    s32 mn = r < g ? (r < b ? r : b) : (g < b ? g : b);
    s32 d = mx - mn, h;
    if (d < GRAY_DELTA)
        return -1;
    if (mx == r)
        h = (g - b) * 256 / d;
    else if (mx == g)
        h = 512 + (b - r) * 256 / d;
    else
        h = 1024 + (r - g) * 256 / d;
    return ((h % 1536) + 1536) % 1536;
}

static s32 HueDist(s32 a, s32 b)
{
    s32 d = a > b ? a - b : b - a;
    d %= 1536;
    return d < 1536 - d ? d : 1536 - d;
}

// Distance in hue and saturation (brightness ignored)
static s32 ChromaDist(s32 r1, s32 g1, s32 b1, u16 c2)
{
    s32 a = (r1 - g1) - (R5(c2) - G5(c2));
    s32 b = ((r1 + g1) / 2 - b1) - ((R5(c2) + G5(c2)) / 2 - B5(c2));
    return a * a + b * b;
}

static bool32 IsNeutralColor(u16 c)
{
    s32 a = R5(c) - G5(c), b = (R5(c) + G5(c)) / 2 - B5(c);
    return a * a + b * b <= NEUTRAL_CHROMA;
}

struct RecolorGroups
{
    u8 count;
    s8 white; // index of the whites' group, or -1
    u8 size[MAX_GROUPS];
    u8 members[MAX_GROUPS][PAL_SIZE];
    u32 weight[MAX_GROUPS];
};

static void MeanColor(const u16 *pal, const u8 *members, u32 n, s32 *r, s32 *g, s32 *b)
{
    s32 sr = 0, sg = 0, sb = 0;
    for (u32 k = 0; k < n; k++)
    {
        sr += R5(pal[members[k]]);
        sg += G5(pal[members[k]]);
        sb += B5(pal[members[k]]);
    }
    *r = sr / (s32)n;
    *g = sg / (s32)n;
    *b = sb / (s32)n;
}

// For matching a group to a type: hue and saturation, plus a little brightness
static s32 MatchDist(const u16 *pal, const u8 *members, u32 n, u16 color)
{
    s32 r, g, b, dl;
    MeanColor(pal, members, n, &r, &g, &b);
    dl = Lum(r, g, b) - ColorLum(color);
    return ChromaDist(r, g, b, color) + dl * dl / 4;
}

static bool32 Recognizable(const u16 *pal, const u8 *members, u32 n, u16 color)
{
    s32 r, g, b, dl;
    MeanColor(pal, members, n, &r, &g, &b);
    dl = Lum(r, g, b) - ColorLum(color);
    return ChromaDist(r, g, b, color) <= REC_DIST && dl <= REC_LUM && dl >= -REC_LUM;
}

static void AddGroup(struct RecolorGroups *g, const u8 *members, u32 n, const u16 *counts)
{
    u32 j = g->count++;
    g->size[j] = n;
    g->weight[j] = 0;
    for (u32 k = 0; k < n; k++)
    {
        g->members[j][k] = members[k];
        g->weight[j] += counts[members[k]];
    }
}

// Weighted circular mean of the hues of members, unrolled around the first
static s32 HueMean(const s32 *hue, const u8 *members, u32 n, const u16 *counts)
{
    s32 h0 = hue[members[0]], acc = 0, tot = 0;
    for (u32 k = 0; k < n; k++)
    {
        s32 d = ((hue[members[k]] - h0 + 768) % 1536 + 1536) % 1536 - 768;
        acc += d * counts[members[k]];
        tot += counts[members[k]];
    }
    if (tot < 1)
        tot = 1;
    return (h0 + acc / tot + 1536) % 1536;
}

static void FindGroups(const u16 *pal, const u16 *counts, const u16 *edges, struct RecolorGroups *g)
{
    u8 colored[PAL_SIZE], grays[PAL_SIZE], whites[PAL_SIZE], ga[PAL_SIZE], gb[PAL_SIZE];
    u32 nc = 0, ng = 0, nw = 0, na = 0, nb = 0, wsum = 0, rest = 0;
    s32 hue[PAL_SIZE];

    g->count = 0;
    g->white = -1;
    for (u32 i = 1; i < PAL_SIZE; i++)
    {
        u16 c = pal[i];
        s32 r = R5(c), gr = G5(c), b = B5(c);
        s32 mx = r > gr ? (r > b ? r : b) : (gr > b ? gr : b);
        s32 mn = r < gr ? (r < b ? r : b) : (gr < b ? gr : b);
        s32 l = Lum(r, gr, b);
        if (l < OUTLINE_LUM || counts[i] == 0)
            continue;
        if (l >= WHITE_LUM && mx - mn < WHITE_DELTA)
        {
            whites[nw++] = i;
            continue;
        }
        hue[i] = Hue(c);
        if (hue[i] < 0)
        {
            if (edges[i] * 16 < counts[i] * OUTLINE_EDGE)
                grays[ng++] = i;
        }
        else
        {
            colored[nc++] = i;
        }
    }

    if (nc != 0)
    {
        // 2-means on hue, seeded with the two hues farthest apart
        s32 best = -1, ca = 0, cb = 0;
        for (u32 x = 0; x < nc; x++)
        {
            for (u32 y = 0; y < nc; y++)
            {
                s32 d = HueDist(hue[colored[x]], hue[colored[y]]);
                if (d > best)
                {
                    best = d;
                    ca = hue[colored[x]];
                    cb = hue[colored[y]];
                }
            }
        }
        for (u32 it = 0; it < 4; it++)
        {
            na = nb = 0;
            for (u32 k = 0; k < nc; k++)
            {
                s32 h = hue[colored[k]];
                if (HueDist(h, ca) <= HueDist(h, cb))
                    ga[na++] = colored[k];
                else
                    gb[nb++] = colored[k];
            }
            if (nb == 0)
                break;
            ca = HueMean(hue, ga, na, counts);
            cb = HueMean(hue, gb, nb, counts);
        }
        if (nb != 0 && HueDist(ca, cb) >= HUE_SPLIT)
        {
            u32 wa = 0, wb = 0;
            for (u32 k = 0; k < na; k++)
                wa += counts[ga[k]];
            for (u32 k = 0; k < nb; k++)
                wb += counts[gb[k]];
            // Bigger group first (the first one on ties)
            if (wb > wa)
            {
                AddGroup(g, gb, nb, counts);
                AddGroup(g, ga, na, counts);
            }
            else
            {
                AddGroup(g, ga, na, counts);
                AddGroup(g, gb, nb, counts);
            }
        }
        else
        {
            AddGroup(g, colored, nc, counts);
        }
    }
    if (ng != 0)
        AddGroup(g, grays, ng, counts);
    for (u32 k = 0; k < nw; k++)
        wsum += counts[whites[k]];
    for (u32 j = 0; j < g->count; j++)
        rest += g->weight[j];
    if (nw != 0 && wsum * 16 >= (wsum + rest) * WHITE_BODY)
    {
        g->white = g->count;
        AddGroup(g, whites, nw, counts);
    }
}

// The type color at brightness l
static u16 Shade(u16 t, s32 l)
{
    s32 r = R5(t), g = G5(t), b = B5(t);
    s32 lt = Lum(r, g, b);
    if (lt < 1)
        lt = 1;
    if (l <= lt)
        return RGB5(r * l / lt, g * l / lt, b * l / lt);
    if (lt > 30)
        lt = 30;
    return RGB5(r + (31 - r) * (l - lt) / (31 - lt), g + (31 - g) * (l - lt) / (31 - lt), b + (31 - b) * (l - lt) / (31 - lt));
}

static void Paint(const u16 *src, u16 *dst, const u8 *members, u32 n, u16 color)
{
    s32 sum = 0, shift;
    if (n == 0)
        return;
    for (u32 k = 0; k < n; k++)
        sum += ColorLum(src[members[k]]);
    shift = (ColorLum(color) - sum / (s32)n) * LUM_PULL / 16;
    for (u32 k = 0; k < n; k++)
    {
        s32 l = ColorLum(src[members[k]]) + shift;
        if (l < OUTLINE_LUM)
            l = OUTLINE_LUM;
        if (l > 30)
            l = 30;
        dst[members[k]] = Shade(color, l);
    }
}

// Splits a group by brightness: part gets its darkest colors, covering about SPLIT/16 of its pixels
static void SplitGroup(const u16 *pal, const u16 *counts, const u8 *members, u32 n, u8 *rest, u32 *nRest, u8 *part, u32 *nPart)
{
    u8 order[PAL_SIZE];
    u32 total = 0, acc = 0, k;
    s32 target;

    // Darkest first (stable)
    for (k = 0; k < n; k++)
    {
        s32 j = k;
        while (j > 0 && ColorLum(pal[order[j - 1]]) > ColorLum(pal[members[k]]))
        {
            order[j] = order[j - 1];
            j--;
        }
        order[j] = members[k];
        total += counts[members[k]];
    }
    target = total * SPLIT;
    *nPart = 0;
    for (k = 0; k < n; k++)
    {
        s32 with = (s32)(acc + counts[order[k]]) * 16 - target;
        s32 without = (s32)acc * 16 - target;
        if (*nPart != 0 && (with < 0 ? -with : with) >= (without < 0 ? -without : without))
            break;
        part[(*nPart)++] = order[k];
        acc += counts[order[k]];
    }
    if (*nPart == n)
        (*nPart)--;
    *nRest = 0;
    for (k = *nPart; k < n; k++)
        rest[(*nRest)++] = order[k];
}

// The group (among those big enough) that looks most like a type color; whites only for a whitish type
static u32 FindOwner(const u16 *pal, const struct RecolorGroups *g, const bool32 *big, u16 color)
{
    s32 bestDist = 0x7FFFFFFF, owner = -1;
    for (u32 j = 0; j < g->count; j++)
    {
        if (!big[j] || ((s32)j == g->white && !IsNeutralColor(color)))
            continue;
        s32 d = MatchDist(pal, g->members[j], g->size[j], color);
        if (d < bestDist)
        {
            bestDist = d;
            owner = j;
        }
    }
    return owner < 0 ? 0 : owner;
}

void GraftRecolorPalette(const u16 *src, u16 *dst, const u16 *counts, const u16 *edges,
                         const u8 *speciesTypes, const u8 *effTypes, const u16 *speciesColors, const u16 *effColors)
{
    struct RecolorGroups g;
    bool32 changed[2], big[MAX_GROUPS];
    u8 sp[2], ef[2];
    u16 spColor[2], efColor[2];
    u32 owner[2], total = 0, j, k;
    const u8 *region[2];
    u32 regionSize[2];

    for (k = 0; k < PAL_SIZE; k++)
        dst[k] = src[k];
    for (k = 0; k < 2; k++)
    {
        sp[k] = speciesTypes[k];
        ef[k] = effTypes[k];
        spColor[k] = speciesColors[k];
        efColor[k] = effColors[k];
        changed[k] = (ef[k] != sp[k]);
    }
    if (!changed[0] && !changed[1])
        return;
    FindGroups(src, counts, edges, &g);
    if (g.count == 0)
        return;

    for (j = 0; j < g.count; j++)
        total += g.weight[j];
    for (j = 0; j < g.count; j++)
        big[j] = (g.weight[j] * 16 >= total * MIN_OWNER);

    // Which group shows each species type
    owner[0] = FindOwner(src, &g, big, spColor[0]);
    owner[1] = FindOwner(src, &g, big, spColor[1]);
    if (sp[0] != sp[1] && owner[0] == owner[1])
    {
        // Both types point at the same group: it shows the closer one; the other gets what's left
        const u8 *m = g.members[owner[0]];
        if (MatchDist(src, m, g.size[owner[0]], spColor[1]) < MatchDist(src, m, g.size[owner[0]], spColor[0]))
        {
            u8 t;
            u16 c;
            bool32 b;
            t = sp[0]; sp[0] = sp[1]; sp[1] = t;
            t = ef[0]; ef[0] = ef[1]; ef[1] = t;
            c = efColor[0]; efColor[0] = efColor[1]; efColor[1] = c;
            b = changed[0]; changed[0] = changed[1]; changed[1] = b;
        }
    }
    region[0] = g.members[owner[0]];
    regionSize[0] = g.size[owner[0]];
    region[1] = NULL;
    regionSize[1] = 0;
    if (sp[0] != sp[1] && owner[0] != owner[1])
    {
        region[1] = g.members[owner[1]];
        regionSize[1] = g.size[owner[1]];
    }
    else
    {
        // The next biggest group big enough to see (whites only for a whitish type)
        s32 other = -1;
        for (j = 0; j < g.count; j++)
        {
            if (j == owner[0] || ((s32)j == g.white && !IsNeutralColor(efColor[1])))
                continue;
            if (other < 0 || g.weight[j] > g.weight[other])
                other = j;
        }
        if (other >= 0 && g.weight[other] * 16 >= g.weight[owner[0]] * SECOND_MIN)
        {
            region[1] = g.members[other];
            regionSize[1] = g.size[other];
        }
    }

    if (ef[0] == ef[1])
    {
        // One type in both slots: all of it
        for (j = 0; j < g.count; j++)
            Paint(src, dst, g.members[j], g.size[j], efColor[0]);
        return;
    }
    if (region[1] == NULL)
    {
        // No group for the second type: it takes the main group's shadows (new either way, so painted)
        u8 rest[PAL_SIZE], part[PAL_SIZE];
        u32 nRest, nPart;
        SplitGroup(src, counts, region[0], regionSize[0], rest, &nRest, part, &nPart);
        Paint(src, dst, part, nPart, efColor[1]);
        if (changed[0] || !Recognizable(src, rest, nRest, efColor[0]))
            Paint(src, dst, rest, nRest, efColor[0]);
        return;
    }
    for (k = 0; k < 2; k++)
    {
        if (changed[k] || !Recognizable(src, region[k], regionSize[k], efColor[k]))
            Paint(src, dst, region[k], regionSize[k], efColor[k]);
    }
}

// Pixel counts per palette color in a 4bpp 64x64 frame, and how many of them touch transparency
void GraftCountSpritePixels(const u8 *gfx, u16 *counts, u16 *edges)
{
    for (u32 i = 0; i < PAL_SIZE; i++)
        counts[i] = edges[i] = 0;
    for (s32 y = 0; y < SPRITE_DIM; y++)
    {
        for (s32 x = 0; x < SPRITE_DIM; x++)
        {
#define PIXEL(px, py) ((gfx[(((py) >> 3) * 8 + ((px) >> 3)) * 32 + ((py) & 7) * 4 + (((px) & 7) >> 1)] >> ((((px) & 1)) * 4)) & 15)
            u32 i = PIXEL(x, y);
            counts[i]++;
            if (i != 0
             && (x == 0 || x == SPRITE_DIM - 1 || y == 0 || y == SPRITE_DIM - 1
              || PIXEL(x - 1, y) == 0 || PIXEL(x + 1, y) == 0 || PIXEL(x, y - 1) == 0 || PIXEL(x, y + 1) == 0))
                edges[i]++;
#undef PIXEL
        }
    }
}

#ifndef GRAFT_RECOLOR_NATIVE
void ApplyGraftTypePalette(u32 palOffset, struct Pokemon *mon, const u8 *gfx)
{
    u16 species = GetMonData(mon, MON_DATA_SPECIES);
    bool32 isShiny = GetMonData(mon, MON_DATA_IS_SHINY);
    u16 counts[PAL_SIZE], edges[PAL_SIZE], out[PAL_SIZE], spColor[2], efColor[2];
    u8 sp[2], ef[2];

    if (mon->box.graftTypes[0] == GRAFT_TYPE_NONE && mon->box.graftTypes[1] == GRAFT_TYPE_NONE)
        return;
    for (u32 k = 0; k < 2; k++)
    {
        sp[k] = GetSpeciesType(species, k);
        ef[k] = mon->box.graftTypes[k] != GRAFT_TYPE_NONE ? mon->box.graftTypes[k] : sp[k];
        spColor[k] = gTypesInfo[sp[k]].graftPaletteRGBValue;
        efColor[k] = isShiny ? gTypesInfo[ef[k]].shinyGraftPaletteRGBValue : gTypesInfo[ef[k]].graftPaletteRGBValue;
    }
    if (gfx != NULL)
    {
        GraftCountSpritePixels(gfx, counts, edges);
    }
    else
    {
        for (u32 k = 0; k < PAL_SIZE; k++)
        {
            counts[k] = 1;
            edges[k] = 0;
        }
    }
    GraftRecolorPalette(&gPlttBufferUnfaded[palOffset], out, counts, edges, sp, ef, spColor, efColor);
    CpuCopy16(out, &gPlttBufferUnfaded[palOffset], sizeof(out));
    CpuCopy16(out, &gPlttBufferFaded[palOffset], sizeof(out));
}
#endif
