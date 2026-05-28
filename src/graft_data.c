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
void BlendGraftTypePalette(u16 palOffset, u16 numEntries, u32 type1Color, u32 type2Color, u8 coeff)
{
    u16 i;
    struct PlttData *c1 = (struct PlttData *)&type1Color;
    struct PlttData *c2 = (struct PlttData *)&type2Color;

    for (i = 0; i < numEntries; i++)
    {
        u16 index = i + palOffset;
        struct PlttData *src = (struct PlttData *)&gPlttBufferUnfaded[index];
        s16 r = src->r;
        s16 g = src->g;
        s16 b = src->b;
        u16 brightness;
        s16 targetR, targetG, targetB;

        // Skip transparent entry (index 0)
        if (i == 0)
            continue;

        // Measure brightness (0 to ~93 in RGB555)
        brightness = r + g + b;

        // Distinct two-color tinting:
        // bright pixels (highlights) → tint toward type1 ONLY
        // dark pixels (shadows) → tint toward type2 ONLY
        // mid-range → smooth crossfade between the two
        // This keeps the two type colors visually distinct instead of muddy blend
        if (brightness >= 60)
        {
            // Bright: pure type1 tint
            targetR = c1->r;
            targetG = c1->g;
            targetB = c1->b;
        }
        else if (brightness >= 30)
        {
            // Mid: crossfade from type1 to type2
            // 60 brightness = 100% type1, 30 brightness = 0% type1 (100% type2)
            u8 type1Fraction = (brightness - 30) * 255 / 30;
            targetR = ((c1->r * type1Fraction) + (c2->r * (255 - type1Fraction))) >> 8;
            targetG = ((c1->g * type1Fraction) + (c2->g * (255 - type1Fraction))) >> 8;
            targetB = ((c1->b * type1Fraction) + (c2->b * (255 - type1Fraction))) >> 8;
        }
        else
        {
            // Dark: pure type2 tint
            targetR = c2->r;
            targetG = c2->g;
            targetB = c2->b;
        }

        // Blend original pixel toward target color
        gPlttBufferFaded[index] = RGB(
            r + ((targetR - r) * coeff >> 4),
            g + ((targetG - g) * coeff >> 4),
            b + ((targetB - b) * coeff >> 4)
        );
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
