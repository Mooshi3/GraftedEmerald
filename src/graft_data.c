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
    u16 existingMove;
    u8 emptySlot = MAX_MON_MOVES;
    for (u8 i = 0; i < MAX_MON_MOVES; i++)
    {
        existingMove = GetMonData(mon, MON_DATA_MOVE1 + i);
        if (existingMove == MOVE_NONE)
        {
            emptySlot = i;
            break;
            // Record graft move in history for move relearner
    {
        u8 i;
        for (i = 0; i < 4; i++)
        {
            if (gPlayerParty[partySlot].box.graftMoves[i] == MOVE_NONE)
            {
                gPlayerParty[partySlot].box.graftMoves[i] = moveId;
                break;
            }
        }
    }
}
        // Record graft move in history for move relearner
    {
        u8 i;
        for (i = 0; i < 4; i++)
        {
            if (gPlayerParty[partySlot].box.graftMoves[i] == MOVE_NONE)
            {
                gPlayerParty[partySlot].box.graftMoves[i] = moveId;
                break;
            }
        }
    }
}
    if (emptySlot >= MAX_MON_MOVES)
        emptySlot = MAX_MON_MOVES - 1;
    SetMonData(mon, MON_DATA_MOVE1 + emptySlot, &moveId);
    u8 pp = GetMovePP(moveId);
    SetMonData(mon, MON_DATA_PP1 + emptySlot, &pp);
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
        u16 type1Weight, type2Weight, totalWeight;
        s16 targetR, targetG, targetB;

        // Skip transparent entry (index 0)
        if (i == 0)
            continue;

        // Measure brightness (0 to ~93 in RGB555)
        brightness = r + g + b;

        // Weight type colors by brightness:
        // bright pixels → more type1 color (highlights)
        // dark pixels → more type2 color (shadows)
        // This creates a natural-looking split tint
        if (brightness >= 40)
        {
            type1Weight = 3;
            type2Weight = 1;
        }
        else if (brightness >= 20)
        {
            type1Weight = 2;
            type2Weight = 2;
        }
        else
        {
            type1Weight = 1;
            type2Weight = 3;
        }

        totalWeight = type1Weight + type2Weight;

        // Calculate blended target color
        targetR = ((c1->r * type1Weight) + (c2->r * type2Weight)) / totalWeight;
        targetG = ((c1->g * type1Weight) + (c2->g * type2Weight)) / totalWeight;
        targetB = ((c1->b * type1Weight) + (c2->b * type2Weight)) / totalWeight;

        // Blend original toward target
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
