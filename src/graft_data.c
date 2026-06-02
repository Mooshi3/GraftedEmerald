#include "global.h"
#include "pokemon.h"
#include "graft_data.h"
#include "move.h"
#include "constants/species.h"
#include "palette.h"
#include "constants/rgb.h"
#include "battle_main.h"
#include "event_data.h"
#include "wild_encounter.h"
#include "global.fieldmap.h"
#include "move.h"

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
u8 GetBadgeCount(void)
{
    u8 count = 0;
    u32 i;
    for (i = FLAG_BADGE01_GET; i < FLAG_BADGE01_GET + NUM_BADGES; i++)
    {
        if (FlagGet(i))
            count++;
    }
    return count;
}
u8 GetStatGraftCap(void)
{
    u8 badges = GetBadgeCount();
    if (badges >= 5)
        return 255; // uncapped
    return 1 + badges;
}
u8 CountStatGraftsOnBoxMon(const struct BoxPokemon *boxMon)
{
    u8 count = 0;
    u8 i;
    for (i = 0; i < 6; i++)
    {
        if (boxMon->graftStatOverrides[i] != 0)
            count++;
    }
    return count;
}
bool8 CanApplyStatGraft(const struct BoxPokemon *boxMon, u8 statIndex)
{
    // Overwriting an existing graft slot is always free
    if (statIndex < 6 && boxMon->graftStatOverrides[statIndex] != 0)
        return TRUE;
    // New slot: check against cap
    return CountStatGraftsOnBoxMon(boxMon) < GetStatGraftCap();
}

// ============================================================
// Pointer-based graft setters (work on any Pokemon pointer)
// ============================================================
void SetGraftedStatOnMon(struct Pokemon *mon, u8 statIndex, u16 statValue)
{
    if (statIndex >= 6)
        return;
    mon->box.graftStatOverrides[statIndex] = (u8)statValue;
    CalculateMonStats(mon);
    mon->box.graftFlags |= GRAFT_FLAG_STAT;
}

void SetGraftedTypeOnMon(struct Pokemon *mon, u8 typeIndex, u8 type)
{
    if (typeIndex >= 2)
        return;
    mon->box.graftTypes[typeIndex] = type;
    mon->box.graftFlags |= GRAFT_FLAG_TYPE;
}

void SetGraftedAbilityOnMon(struct Pokemon *mon, u16 ability)
{
    mon->box.graftAbility = ability;
    mon->box.graftFlags |= GRAFT_FLAG_ABILITY;
}

void SetGraftedNatureOnMon(struct Pokemon *mon, u8 natureId)
{
    // Nature grafts not applied to enemy mons (per design)
    (void)mon;
    (void)natureId;
}

// ============================================================
// Enemy trainer graft system
// ============================================================
#define MAX_GRAFT_POOL 64
#define GRAFT_ROLL_ATTEMPTS 40

static u16 sGraftDonorPool[MAX_GRAFT_POOL];
static u8 sGraftDonorCount;

static void AddSpeciesToPool(u16 species)
{
    if (species == SPECIES_NONE)
        return;
    for (u8 i = 0; i < sGraftDonorCount; i++)
        if (sGraftDonorPool[i] == species)
            return; // dedupe
    if (sGraftDonorCount < MAX_GRAFT_POOL)
        sGraftDonorPool[sGraftDonorCount++] = species;
}

static void AddMapEncountersToPool(u8 mapGroup, u8 mapNum)
{
    for (u32 i = 0; gWildMonHeaders[i].mapGroup != 0xFF; i++)
    {
        if (gWildMonHeaders[i].mapGroup != mapGroup || gWildMonHeaders[i].mapNum != mapNum)
            continue;
        // Check all time-of-day encounter types
        for (u32 tod = 0; tod < TIMES_OF_DAY_COUNT; tod++)
        {
            const struct WildEncounterTypes *enc = &gWildMonHeaders[i].encounterTypes[tod];
            const struct WildPokemonInfo *infos[] = {
                enc->landMonsInfo, enc->waterMonsInfo,
                enc->rockSmashMonsInfo, enc->fishingMonsInfo
            };
            const u8 counts[] = { LAND_WILD_COUNT, WATER_WILD_COUNT, ROCK_WILD_COUNT, 12 }; // fishing varies, 12 is safe upper bound
            for (u32 t = 0; t < 4; t++)
            {
                if (infos[t] == NULL)
                    continue;
                for (u32 s = 0; s < counts[t]; s++)
                {
                    if (infos[t]->wildPokemon[s].species != SPECIES_NONE)
                        AddSpeciesToPool(infos[t]->wildPokemon[s].species);
                }
            }
        }
        return;
    }
}

// Walk one level of connections from current map
static void AddConnectedMapEncounters(void)
{
    if (gMapHeader.connections == NULL)
        return;
    for (s32 i = 0; i < gMapHeader.connections->count; i++)
    {
        AddMapEncountersToPool(
            gMapHeader.connections->connections[i].mapGroup,
            gMapHeader.connections->connections[i].mapNum
        );
    }
}

// Manual fallback for disconnected indoor maps
static const struct {
    u8 group, num, srcGroup, srcNum;
} sGraftFallbacks[] = {
    // Gyms - pull from nearby route
    { MAP_GROUP(MAP_PETALBURG_CITY_GYM), MAP_NUM(MAP_PETALBURG_CITY_GYM), MAP_GROUP(MAP_ROUTE102), MAP_NUM(MAP_ROUTE102) },
    { MAP_GROUP(MAP_RUSTBORO_CITY_GYM), MAP_NUM(MAP_RUSTBORO_CITY_GYM), MAP_GROUP(MAP_ROUTE116), MAP_NUM(MAP_ROUTE116) },
    { MAP_GROUP(MAP_DEWFORD_TOWN_GYM), MAP_NUM(MAP_DEWFORD_TOWN_GYM), MAP_GROUP(MAP_ROUTE106), MAP_NUM(MAP_ROUTE106) },
    { MAP_GROUP(MAP_MAUVILLE_CITY_GYM), MAP_NUM(MAP_MAUVILLE_CITY_GYM), MAP_GROUP(MAP_ROUTE110), MAP_NUM(MAP_ROUTE110) },
    { MAP_GROUP(MAP_LAVARIDGE_TOWN_GYM_1F), MAP_NUM(MAP_LAVARIDGE_TOWN_GYM_1F), MAP_GROUP(MAP_ROUTE112), MAP_NUM(MAP_ROUTE112) },
    { MAP_GROUP(MAP_FORTREE_CITY_GYM), MAP_NUM(MAP_FORTREE_CITY_GYM), MAP_GROUP(MAP_ROUTE119), MAP_NUM(MAP_ROUTE119) },
    { MAP_GROUP(MAP_MOSSDEEP_CITY_GYM), MAP_NUM(MAP_MOSSDEEP_CITY_GYM), MAP_GROUP(MAP_ROUTE124), MAP_NUM(MAP_ROUTE124) },
    { MAP_GROUP(MAP_SOOTOPOLIS_CITY_GYM_1F), MAP_NUM(MAP_SOOTOPOLIS_CITY_GYM_1F), MAP_GROUP(MAP_ROUTE126), MAP_NUM(MAP_ROUTE126) },
    // Hideouts - pull from nearest route
    { MAP_GROUP(MAP_AQUA_HIDEOUT_1F), MAP_NUM(MAP_AQUA_HIDEOUT_1F), MAP_GROUP(MAP_ROUTE124), MAP_NUM(MAP_ROUTE124) },
    { MAP_GROUP(MAP_MAGMA_HIDEOUT_1F), MAP_NUM(MAP_MAGMA_HIDEOUT_1F), MAP_GROUP(MAP_ROUTE112), MAP_NUM(MAP_ROUTE112) },
    // Special interiors
    { MAP_GROUP(MAP_CAVE_OF_ORIGIN_1F), MAP_NUM(MAP_CAVE_OF_ORIGIN_1F), MAP_GROUP(MAP_ROUTE126), MAP_NUM(MAP_ROUTE126) },
    { MAP_GROUP(MAP_SEAFLOOR_CAVERN_ENTRANCE), MAP_NUM(MAP_SEAFLOOR_CAVERN_ENTRANCE), MAP_GROUP(MAP_ROUTE128), MAP_NUM(MAP_ROUTE128) },
};

static void ApplyGraftPoolFallback(void)
{
    u8 curGroup = gSaveBlock1Ptr->location.mapGroup;
    u8 curNum = gSaveBlock1Ptr->location.mapNum;
    for (u32 i = 0; i < ARRAY_COUNT(sGraftFallbacks); i++)
    {
        if (sGraftFallbacks[i].group == curGroup && sGraftFallbacks[i].num == curNum)
        {
            AddMapEncountersToPool(sGraftFallbacks[i].srcGroup, sGraftFallbacks[i].srcNum);
            return;
        }
    }
}

void BuildNearbyGraftPool(void)
{
    sGraftDonorCount = 0;
    // Tier 1: current map
    AddMapEncountersToPool(gSaveBlock1Ptr->location.mapGroup, gSaveBlock1Ptr->location.mapNum);
    // Tier 2: connected maps
    AddConnectedMapEncounters();
    // Tier 3: manual fallback
    if (sGraftDonorCount == 0)
        ApplyGraftPoolFallback();
}

// ============================================================
// Graft roll logic for enemy trainers
// ============================================================

// Graft kind constants
enum {
    GK_STAT,
    GK_MOVE,
    GK_ABILITY,
    GK_TYPE,
    GK_COUNT
};

static u8 GetSpeciesBaseStatGraft(u16 species, u8 statIndex)
{
    switch (statIndex)
    {
    case 0: return gSpeciesInfo[species].baseHP;
    case 1: return gSpeciesInfo[species].baseAttack;
    case 2: return gSpeciesInfo[species].baseDefense;
    case 3: return gSpeciesInfo[species].baseSpeed;
    case 4: return gSpeciesInfo[species].baseSpAttack;
    case 5: return gSpeciesInfo[species].baseSpDefense;
    }
    return 0;
}

static u8 GetEffectiveBaseStat(struct Pokemon *mon, u8 statIndex)
{
    u8 ov = mon->box.graftStatOverrides[statIndex];
    if (ov != 0)
        return ov;
    return GetSpeciesBaseStatGraft(GetMonData(mon, MON_DATA_SPECIES), statIndex);
}

// Move value: raw power for attacks, 50 baseline for status moves
static u16 MoveGraftValue(u16 move)
{
    u16 power = gMovesInfo[move].power;
    return (power == 0) ? 50 : power;
}

static bool8 IsAttackingMove(u16 move)
{
    return gMovesInfo[move].power > 0;
}

// Pick slot to replace, or 0xFF to reject
static u8 PickMoveReplaceSlot(struct Pokemon *mon, u16 newMove)
{
    u8 worstAttackingSlot = 0xFF;
    u16 worstAttackingVal = 0xFFFF;
    u8 worstStatusSlot = 0xFF;
    u16 worstStatusVal = 0xFFFF;
    u8 attackingCount = 0;

    for (u8 i = 0; i < MAX_MON_MOVES; i++)
    {
        u16 cur = GetMonData(mon, MON_DATA_MOVE1 + i, NULL);
        if (cur == MOVE_NONE)
            return i; // free slot
        if (cur == newMove)
            return 0xFF; // already knows it

        if (IsAttackingMove(cur))
        {
            attackingCount++;
            u16 v = MoveGraftValue(cur);
            if (v < worstAttackingVal)
            {
                worstAttackingVal = v;
                worstAttackingSlot = i;
            }
        }
        else
        {
            u16 v = MoveGraftValue(cur);
            if (v < worstStatusVal)
            {
                worstStatusVal = v;
                worstStatusSlot = i;
            }
        }
    }

    if (IsAttackingMove(newMove))
    {
        // Only replace an attacking move with lower power
        // But never replace the last attacking move
        if (worstAttackingSlot == 0xFF)
            return 0xFF; // no attacking moves to replace? shouldn't happen but skip
        if (attackingCount <= 1)
            return 0xFF; // don't remove the only attacking move
        return (MoveGraftValue(newMove) > worstAttackingVal) ? worstAttackingSlot : 0xFF;
    }
    else
    {
        // Status move: replace another status move
        if (worstStatusSlot == 0xFF)
            return 0xFF; // no status moves to replace, skip
        return (MoveGraftValue(newMove) > worstStatusVal) ? worstStatusSlot : 0xFF;
    }
}

static u16 PickDonorMove(u16 donor, u8 recipientLevel)
{
    const struct LevelUpMove *ls = gSpeciesInfo[donor].levelUpLearnset;
    u16 moves[64];
    u8 n = 0;
    if (ls == NULL)
        return MOVE_NONE;
    for (u8 i = 0; ls[i].move != LEVEL_UP_MOVE_END && n < 64; i++)
    {
        if (ls[i].level <= recipientLevel) // only moves the donor could know at this level
            moves[n++] = ls[i].move;
    }
    return n ? moves[Random() % n] : MOVE_NONE;
}

static void ApplyEnemyGrafts(struct Pokemon *mon, u8 graftCount)
{
    u16 species = GetMonData(mon, MON_DATA_SPECIES);
    bool8 statUsed[6] = { FALSE };
    bool8 abilityUsed = FALSE;
    bool8 typeSlotUsed[2] = { FALSE };
    u8 applied = 0, attempts = 0;

    while (applied < graftCount && attempts < GRAFT_ROLL_ATTEMPTS)
    {
        attempts++;
        if (sGraftDonorCount == 0)
            return;

        u16 donor = sGraftDonorPool[Random() % sGraftDonorCount];
        u8 kind = Random() % GK_COUNT;

        if (kind == GK_STAT)
        {
            // Find stats the donor improves that aren't already grafted
            u8 candidates[6];
            u8 n = 0;
            for (u8 s = 0; s < 6; s++)
            {
                if (statUsed[s])
                    continue;
                u8 donorStat = GetSpeciesBaseStatGraft(donor, s);
                if (donorStat > GetEffectiveBaseStat(mon, s))
                    candidates[n++] = s;
            }
            if (n == 0)
                continue;
            u8 stat = candidates[Random() % n];
            SetGraftedStatOnMon(mon, stat, GetSpeciesBaseStatGraft(donor, stat));
            statUsed[stat] = TRUE;
            applied++;
        }
        else if (kind == GK_MOVE)
        {
            u16 move = PickDonorMove(donor, GetMonData(mon, MON_DATA_LEVEL));
            if (move == MOVE_NONE || move == MOVE_NONE)
                continue;
            u8 slot = PickMoveReplaceSlot(mon, move);
            if (slot == 0xFF)
                continue;
            SetMonMoveSlot(mon, move, slot);
            mon->box.graftFlags |= GRAFT_FLAG_MOVE;
            // Record in graftMoves for relearner
            for (u8 g = 0; g < 4; g++)
            {
                if (mon->box.graftMoves[g] == MOVE_NONE)
                {
                    mon->box.graftMoves[g] = move;
                    break;
                }
            }
            applied++;
        }
        else if (kind == GK_ABILITY)
        {
            if (abilityUsed)
                continue;
            u16 curAbility = mon->box.graftAbility;
            if (curAbility == ABILITY_NONE)
            {
                u8 abilityNum = GetMonData(mon, MON_DATA_ABILITY_NUM);
                u16 species = GetMonData(mon, MON_DATA_SPECIES);
                curAbility = gSpeciesInfo[species].abilities[abilityNum];
            }
            u16 picks[2];
            u8 n = 0;
            // Only use ability slots 0-1 (no hidden abilities for enemies)
            for (u8 i = 0; i < 2; i++)
            {
                u16 a = gSpeciesInfo[donor].abilities[i];
                if (a != ABILITY_NONE && a != curAbility)
                    picks[n++] = a;
            }
            if (n == 0)
                continue;
            SetGraftedAbilityOnMon(mon, picks[Random() % n]);
            abilityUsed = TRUE;
            applied++;
        }
        else // GK_TYPE
        {
            u8 type1 = gSpeciesInfo[species].types[0];
            u8 type2 = gSpeciesInfo[species].types[1];
            // Check grafted types too
            if (mon->box.graftTypes[0] != GRAFT_TYPE_NONE)
                type1 = mon->box.graftTypes[0];
            if (mon->box.graftTypes[1] != GRAFT_TYPE_NONE)
                type2 = mon->box.graftTypes[1];

            u8 picks[2];
            u8 n = 0;
            for (u8 i = 0; i < 2; i++)
            {
                u8 t = gSpeciesInfo[donor].types[i];
                if (t != type1 && t != type2 && t != TYPE_MYSTERY)
                    picks[n++] = t;
            }
            if (n == 0)
                continue;
            u8 newType = picks[Random() % n];
            // Pick slot: if mono-type, add as type2; else random slot
            u8 slot;
            if (type1 == type2)
                slot = 1;
            else if (typeSlotUsed[0] && typeSlotUsed[1])
                continue;
            else if (typeSlotUsed[0])
                slot = 1;
            else if (typeSlotUsed[1])
                slot = 0;
            else
                slot = Random() & 1;

            if (typeSlotUsed[slot])
                continue;
            SetGraftedTypeOnMon(mon, slot, newType);
            typeSlotUsed[slot] = TRUE;
            applied++;
        }
    }
}

void ApplyTrainerGrafts(struct Pokemon *party, u8 count)
{
    u8 badges = GetBadgeCount();
    u8 graftCount = 2 + badges; // 2 base + 1 per badge
    BuildNearbyGraftPool();
    if (sGraftDonorCount == 0)
        return;
    for (u8 i = 0; i < count; i++)
        ApplyEnemyGrafts(&party[i], graftCount);
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
    struct Pokemon *mon;

    if (GetBattlerSide(battler) == B_SIDE_PLAYER)
    {
        partySlot = gBattlerPartyIndexes[battler];
        if (partySlot >= PARTY_SIZE)
            return FALSE;
        mon = &gPlayerParty[partySlot];
    }
    else if (GetBattlerSide(battler) == B_SIDE_OPPONENT)
    {
        partySlot = gBattlerPartyIndexes[battler];
        if (partySlot >= PARTY_SIZE)
            return FALSE;
        mon = &gEnemyParty[partySlot];
    }
    else
    {
        return FALSE;
    }

    species = GetMonData(mon, MON_DATA_SPECIES);
    speciesType1 = gSpeciesInfo[species].types[0];
    speciesType2 = gSpeciesInfo[species].types[1];

    // Get effective types (grafted override if set, species default if not)
    gType1 = mon->box.graftTypes[0];
    gType2 = mon->box.graftTypes[1];
    *effectiveType1 = (gType1 != GRAFT_TYPE_NONE) ? gType1 : speciesType1;
    *effectiveType2 = (gType2 != GRAFT_TYPE_NONE) ? gType2 : speciesType2;

    // Check if either type differs from species default
    return (*effectiveType1 != speciesType1 || *effectiveType2 != speciesType2);
}
