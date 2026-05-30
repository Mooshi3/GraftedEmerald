#include "global.h"
#include "pokemon.h"
#include "graft_data.h"
#include "move.h"
#include "constants/species.h"
#include "palette.h"
#include "constants/rgb.h"
#include "battle_main.h"

// Get grafted type for a party mon. Returns GRAFT_TYPE_NONE if not grafted.
u8 GetGraftedType(u8 partySlot, u8 typeIndex)
{
    if (partySlot >= PARTY_SIZE || typeIndex > 1)
        return GRAFT_TYPE_NONE;
    u8 val = gPlayerParty[partySlot].box.graftTypes[typeIndex];
    if (val == GRAFT_TYPE_NONE)
        return GRAFT_TYPE_NONE;
    return val;
}

// Set grafted type for a party mon
void SetGraftedType(u8 partySlot, u8 typeIndex, u8 type)
{
    if (partySlot >= PARTY_SIZE || typeIndex > 1)
        return;
    gPlayerParty[partySlot].box.graftTypes[typeIndex] = type;
    gPlayerParty[partySlot].box.graftFlags |= GRAFT_FLAG_TYPE;
}

// Get grafted stat for a party mon
u16 GetGraftedStat(u8 partySlot, u8 statIndex)
{
    if (partySlot >= PARTY_SIZE || statIndex >= 6)
        return GRAFT_STAT_NONE;
    u8 val = gPlayerParty[partySlot].box.graftStatOverrides[statIndex];
    if (val == 0) // 0 = no override
        return GRAFT_STAT_NONE;
    return val;
}

// Set grafted stat for a party mon
void SetGraftedStat(u8 partySlot, u8 statIndex, u16 statValue)
{
    if (partySlot >= PARTY_SIZE || statIndex >= 6)
        return;
    gPlayerParty[partySlot].box.graftStatOverrides[statIndex] = (u8)statValue;
    CalculateMonStats(&gPlayerParty[partySlot]);
    gPlayerParty[partySlot].box.graftFlags |= GRAFT_FLAG_STAT;
}

// Get the effective type for a mon
u8 GetMonEffectiveType(u16 species, u8 partySlot, u8 typeIndex)
{
    u8 grafted = GetGraftedType(partySlot, typeIndex);
    if (grafted != GRAFT_TYPE_NONE)
        return grafted;
    return gSpeciesInfo[species].types[typeIndex];
}

// Get effective type from a struct Pokemon pointer (checks grafts first)
u8 GetEffectiveTypeFromMon(struct Pokemon *mon, u8 typeIndex)
{
    u16 species = GetMonData(mon, MON_DATA_SPECIES);
    if (typeIndex > 1)
        return GetSpeciesType(species, typeIndex);
    u8 grafted = mon->box.graftTypes[typeIndex];
    if (grafted != GRAFT_TYPE_NONE)
        return grafted;
    return GetSpeciesType(species, typeIndex);
}

// Get effective type from a struct BoxPokemon pointer (checks grafts first)
u8 GetEffectiveTypeFromBoxMon(struct BoxPokemon *boxMon, u8 typeIndex)
{
    u16 species = GetMonData((struct Pokemon *)boxMon, MON_DATA_SPECIES);
    if (typeIndex > 1)
        return GetSpeciesType(species, typeIndex);
    u8 grafted = boxMon->graftTypes[typeIndex];
    if (grafted != GRAFT_TYPE_NONE)
        return grafted;
    return GetSpeciesType(species, typeIndex);
}


// Get grafted ability for a party mon
u16 GetGraftedAbility(u8 partySlot)
{
    if (partySlot >= PARTY_SIZE)
        return GRAFT_ABILITY_NONE;
    u16 val = gPlayerParty[partySlot].box.graftAbility;
    if (val == GRAFT_ABILITY_NONE)
        return GRAFT_ABILITY_NONE;
    return val;
}

// Set grafted ability for a party mon
void SetGraftedAbility(u8 partySlot, u16 ability)
{
    if (partySlot >= PARTY_SIZE)
        return;
    gPlayerParty[partySlot].box.graftAbility = ability;
    gPlayerParty[partySlot].box.graftFlags |= GRAFT_FLAG_ABILITY;
}

// Apply a move graft to a party mon
void ApplyMoveGraft(u8 partySlot, u16 moveId)
{
    struct Pokemon *mon = &gPlayerParty[partySlot];
    u8 emptySlot = MAX_MON_MOVES;
    u8 i;

    // Find an empty move slot
    for (i = 0; i < MAX_MON_MOVES; i++)
    {
        if (GetMonData(mon, MON_DATA_MOVE1 + i) == MOVE_NONE)
        {
            emptySlot = i;
            break;
        }
    }

    // If all 4 moves are known, overwrite the last slot
    // TODO: Show move deletion dialog instead of silent overwrite
    if (emptySlot >= MAX_MON_MOVES)
        emptySlot = MAX_MON_MOVES - 1;

    // Teach the move
    SetMonData(mon, MON_DATA_MOVE1 + emptySlot, &moveId);
    {
        u8 pp = GetMovePP(moveId);
        SetMonData(mon, MON_DATA_PP1 + emptySlot, &pp);
    }

    // Record graft move in history for move relearner
    for (i = 0; i < 4; i++)
    {
        if (gPlayerParty[partySlot].box.graftMoves[i] == MOVE_NONE)
        {
            gPlayerParty[partySlot].box.graftMoves[i] = moveId;
            break;
        }
    }

    mon->box.graftFlags |= GRAFT_FLAG_MOVE;
}

// Clear all graft data for a party slot
void ClearGraftData(u8 partySlot)
{
    if (partySlot >= PARTY_SIZE)
        return;
    gPlayerParty[partySlot].box.graftTypes[0] = GRAFT_TYPE_NONE;
    gPlayerParty[partySlot].box.graftTypes[1] = GRAFT_TYPE_NONE;
    gPlayerParty[partySlot].box.graftAbility = GRAFT_ABILITY_NONE;
    gPlayerParty[partySlot].box.graftFlags = 0;
}

// Set grafted nature for a party mon
void SetGraftedNature(u8 partySlot, u8 natureId)
{
    if (partySlot >= PARTY_SIZE)
        return;
    SetMonData(&gPlayerParty[partySlot], MON_DATA_HIDDEN_NATURE, &natureId);
    CalculateMonStats(&gPlayerParty[partySlot]);
    gPlayerParty[partySlot].box.graftFlags |= GRAFT_FLAG_NATURE;
}

// Initialize all graft data (called on new game)
void InitGraftData(void)
{
    for (u8 i = 0; i < PARTY_SIZE; i++)
        ClearGraftData(i);
}


// Direct BoxPokemon accessors (for summary screen viewing box mons)
u8 GetGraftedTypeFromBoxMon(const struct BoxPokemon *boxMon, u8 typeIndex)
{
    if (typeIndex > 1)
        return GRAFT_TYPE_NONE;
    u8 val = boxMon->graftTypes[typeIndex];
    if (val == GRAFT_TYPE_NONE)
        return GRAFT_TYPE_NONE;
    return val;
}

u16 GetGraftedAbilityFromBoxMon(const struct BoxPokemon *boxMon)
{
    u16 val = boxMon->graftAbility;
    if (val == GRAFT_ABILITY_NONE)
        return GRAFT_ABILITY_NONE;
    return val;
}

// Direct BoxPokemon accessor for stat grafts (for summary screen)
u16 GetGraftedStatFromBoxMon(const struct BoxPokemon *boxMon, u8 statIndex)
{
    if (statIndex >= 6)
        return GRAFT_STAT_NONE;
    u8 val = boxMon->graftStatOverrides[statIndex];
    if (val == 0)
        return GRAFT_STAT_NONE;
    return val;
}

// Blend sprite palette for grafted type tinting.
// Light entries blend toward type1Color, dark entries toward type2Color.
// This creates a natural split-tint where highlights show one type
// color and shadows show the other.
// ============================================================
// Grafted-type palette SWAP (replaces BlendGraftTypePalette)
// ------------------------------------------------------------
// Instead of tinting (multiply toward a color), this rebuilds each
// palette entry as a shade of the grafted type's color, preserving the
// sprite's shading structure like a shiny palette does.
//
// Dual-type mons: the sprite's own colors are split into two groups by
// HUE (not RGB distance, not luminance), so naturally-different color
// families — e.g. Torchic's orange body vs. yellow feathers — land in
// the two type colors. The larger group maps to type1 (the "body"),
// the smaller to type2.
//
// Outlines / near-black entries are left untouched.
//
// No floating point (GBA software-emulates it slowly); everything is
// integer math over RGB555 (each channel 0..31). Runs once on sprite
// load, ~16 entries, effectively free.
//
// Drop-in: call this at the same site BlendGraftTypePalette was called.
// Pass the RGB555 color you already associate with each type (the
// gTypesInfo color Elora referenced). For a single-type graft, pass
// isDualType = FALSE (type2Color is then ignored).
// ============================================================

#include "global.h"
#include "graft_data.h"
#include "palette.h"
#include "constants/rgb.h"

// ---- Tunables ----------------------------------------------------------
// Luminance bands are biased toward the midtones, where most sprite pixels
// live, so the "base" and "light" shades get the resolution instead of
// wasting two of four shades on near-empty highlight/shadow extremes.
#define GRAFT_OUTLINE_LUM    4   // entries dimmer than this keep their original color (outlines)
#define GRAFT_BAND_DARK_MAX  12  // lum <= this  -> dark shade
#define GRAFT_BAND_BASE_MAX  20  // lum <= this  -> base shade
#define GRAFT_BAND_LIGHT_MAX 26  // lum <= this  -> light shade; above -> highlight
#define GRAFT_SAT_THRESHOLD  3   // RGB555 (max-min) below this = gray, no usable hue
#define GRAFT_PAL_MAX        16  // GBA sprite palette size

// Weighted luminance: 0.30 R + 0.59 G + 0.11 B. Inputs 0..31, output 0..31.
// (77 + 151 + 28 = 256, so >> 8 rescales cleanly.) A flat (r+g+b)/3 average
// misbands blue-heavy sprites — exactly the kind you'd hit testing on a
// Water-type like Mudkip — because green reads far brighter than blue.
static u8 GraftLuminance(s16 r, s16 g, s16 b)
{
    return (u8)((77 * r + 151 * g + 28 * b) >> 8);
}

// Integer hue on a 0..1535 wheel (256 units per sextant).
// Returns FALSE when the color is too desaturated to carry a hue
// (gray / white / black), so callers can route grays sensibly.
static bool8 GraftHue(s16 r, s16 g, s16 b, u16 *outHue)
{
    s16 maxc = (r > g) ? (r > b ? r : b) : (g > b ? g : b);
    s16 minc = (r < g) ? (r < b ? r : b) : (g < b ? g : b);
    s16 delta = maxc - minc;
    s32 h;

    if (delta < GRAFT_SAT_THRESHOLD)
        return FALSE;

    if (maxc == r)
        h = ((g - b) * 256) / delta;          // red sextant (may be negative)
    else if (maxc == g)
        h = 512 + ((b - r) * 256) / delta;    // green sextant
    else
        h = 1024 + ((r - g) * 256) / delta;   // blue sextant

    if (h < 0)
        h += 1536;
    if (h >= 1536)
        h -= 1536;

    *outHue = (u16)h;
    return TRUE;
}

// Luminance -> shade band. 0 = dark, 1 = base, 2 = light, 3 = highlight.
static u8 GraftBandFromLum(u8 lum)
{
    if (lum <= GRAFT_BAND_DARK_MAX)  return 0;
    if (lum <= GRAFT_BAND_BASE_MAX)  return 1;
    if (lum <= GRAFT_BAND_LIGHT_MAX) return 2;
    return 3;
}

// Build the shade of a type color for a given band.
// dark  ~ 44% toward black; light ~ 56% toward white; highlight ~ 81%.
static u16 GraftShade(const struct PlttData *type, u8 band)
{
    s16 tr = type->r, tg = type->g, tb = type->b;
    s16 r, g, b;

    switch (band)
    {
    case 0: // dark
        r = (tr * 7) >> 4;  g = (tg * 7) >> 4;  b = (tb * 7) >> 4;
        break;
    case 2: // light
        r = tr + (((31 - tr) * 9)  >> 4);
        g = tg + (((31 - tg) * 9)  >> 4);
        b = tb + (((31 - tb) * 9)  >> 4);
        break;
    case 3: // highlight
        r = tr + (((31 - tr) * 13) >> 4);
        g = tg + (((31 - tg) * 13) >> 4);
        b = tb + (((31 - tb) * 13) >> 4);
        break;
    default: // 1 = base
        r = tr;  g = tg;  b = tb;
        break;
    }
    return RGB(r, g, b);
}

void ApplyGraftTypePaletteSwap(u16 palOffset, u16 numEntries, u32 type1Color, u32 type2Color, bool8 isDualType)
{
    struct PlttData *c1 = (struct PlttData *)&type1Color;
    struct PlttData *c2 = (struct PlttData *)&type2Color;

    u16 hue[GRAFT_PAL_MAX];
    u8  cluster[GRAFT_PAL_MAX];     // 0 -> type1, 1 -> type2
    u8  valid[GRAFT_PAL_MAX];       // entry indices that carry a usable hue
    u8  numValid = 0;
    u16 i;

    if (numEntries > GRAFT_PAL_MAX)
        numEntries = GRAFT_PAL_MAX;

    // ---- Pass 1: classify entries, collect hues of colored (non-outline) ones
    for (i = 0; i < numEntries; i++)
    {
        struct PlttData *src = (struct PlttData *)&gPlttBufferUnfaded[palOffset + i];
        s16 r = src->r, g = src->g, b = src->b;

        cluster[i] = 0; // default everything to type1 (grays, single-type, fallbacks)

        if (i == 0)
            continue;                                   // transparent slot
        if (GraftLuminance(r, g, b) < GRAFT_OUTLINE_LUM)
            continue;                                   // outline / near-black
        if (GraftHue(r, g, b, &hue[i]))
            valid[numValid++] = (u8)i;
    }

    // ---- Pass 2: for dual types with 2+ hues, split into two clusters by hue.
    // Method: sort hues, find the TWO largest circular gaps; the two arcs they
    // bound are the clusters. Deterministic, no iteration, no convergence pain.
    if (isDualType && numValid >= 2)
    {
        u8 j, k;
        u16 g1size = 0, g2size = 0;   // magnitudes of the two largest gaps
        u8  g1 = 0, g2 = 0;           // their positions (gap follows valid[pos])
        u8  lo, hi, sizeB, sizeA;
        bool8 bigIsB;

        // insertion sort valid[] by hue (<= 16 elements)
        for (j = 1; j < numValid; j++)
        {
            u8  key = valid[j];
            s16 m = (s16)j - 1;
            while (m >= 0 && hue[valid[m]] > hue[key])
            {
                valid[m + 1] = valid[m];
                m--;
            }
            valid[m + 1] = key;
        }

        // two largest circular gaps
        for (k = 0; k < numValid; k++)
        {
            u16 hCur  = hue[valid[k]];
            u16 hNext = hue[valid[(k + 1) % numValid]];
            u16 gap   = (k + 1 == numValid) ? (1536 - hCur + hNext) : (hNext - hCur);

            if (gap > g1size)      { g2size = g1size; g2 = g1; g1size = gap; g1 = k; }
            else if (gap > g2size) { g2size = gap;            g2 = k; }
        }

        lo = (g1 < g2) ? g1 : g2;
        hi = (g1 < g2) ? g2 : g1;
        sizeB = hi - lo;                 // arc (lo+1 .. hi)
        sizeA = numValid - sizeB;        // the rest, wrapping around
        bigIsB = (sizeB > sizeA);        // larger group -> type1 ("body")

        for (k = 0; k < numValid; k++)
        {
            bool8 inB = (k > lo && k <= hi);
            if (inB)
                cluster[valid[k]] = bigIsB ? 0 : 1;
            else
                cluster[valid[k]] = bigIsB ? 1 : 0;
        }
    }

    // ---- Pass 3: write the swapped palette
    for (i = 0; i < numEntries; i++)
    {
        struct PlttData *src = (struct PlttData *)&gPlttBufferUnfaded[palOffset + i];
        s16 r = src->r, g = src->g, b = src->b;
        u8 lum;

        if (i == 0)
            continue;                                          // keep transparent

        lum = GraftLuminance(r, g, b);
        if (lum < GRAFT_OUTLINE_LUM)
        {
            gPlttBufferFaded[palOffset + i] = gPlttBufferUnfaded[palOffset + i]; // outline stays
            continue;
        }

        {
            const struct PlttData *tc = (cluster[i] == 0) ? c1 : c2;
            gPlttBufferFaded[palOffset + i] = GraftShade(tc, GraftBandFromLum(lum));
        }
    }
}


// Check if a player battler has any grafted types that differ from
// the species' natural types. Returns the effective types and which
// ones are grafted (differ from species default).
bool8 GetBattlerGraftedTypeInfo(u8 battler, u8 *effectiveType1, u8 *effectiveType2)
{
    u8 partySlot;
    u16 species;
    u8 speciesType1, speciesType2;
    u8 gType1, gType2;

    if (GetBattlerSide(battler) != B_SIDE_PLAYER)
        return FALSE;

    partySlot = gBattlerPartyIndexes[battler];
    if (partySlot >= PARTY_SIZE)
        return FALSE;

    species = GetMonData(&gPlayerParty[partySlot], MON_DATA_SPECIES);
    speciesType1 = gSpeciesInfo[species].types[0];
    speciesType2 = gSpeciesInfo[species].types[1];

    // Get effective types (grafted override if set, species default if not)
    gType1 = GetGraftedType(partySlot, 0);
    gType2 = GetGraftedType(partySlot, 1);
    *effectiveType1 = (gType1 != GRAFT_TYPE_NONE) ? gType1 : speciesType1;
    *effectiveType2 = (gType2 != GRAFT_TYPE_NONE) ? gType2 : speciesType2;

    // Check if either type differs from species default
    return (*effectiveType1 != speciesType1 || *effectiveType2 != speciesType2);
}
