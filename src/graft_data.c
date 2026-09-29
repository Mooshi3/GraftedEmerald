#include "global.h"
#include "pokemon.h"
#include "graft_data.h"
#include "move.h"
#include "constants/species.h"
#include "constants/abilities.h"
#include "test_runner.h"
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
    SetGraftedAbilityOnMon(&gPlayerParty[partySlot], ability);
}

static bool32 IsLevelUpMoveOf(u16 species, u16 move)
{
    const struct LevelUpMove *learnset = GetSpeciesLevelUpLearnset(species);
    u32 i;
    for (i = 0; learnset != NULL && learnset[i].move != LEVEL_UP_MOVE_END; i++)
    {
        if (learnset[i].move == move)
            return TRUE;
    }
    return FALSE;
}

// Record a grafted move in the mon's relearner history, so it can be relearned after it's
// forgotten. Duplicates are skipped. The history holds MAX_MON_MOVES entries; when it's full,
// an entry the species can relearn by level-up anyway is replaced first, otherwise the oldest.
void RecordGraftMove(struct BoxPokemon *boxMon, u16 move)
{
    u32 i, evict = 0;
    u16 species;

    if (move == MOVE_NONE)
        return;
    boxMon->graftFlags |= GRAFT_FLAG_MOVE;
    for (i = 0; i < MAX_MON_MOVES; i++)
    {
        if (boxMon->graftMoves[i] == move)
            return;
        if (boxMon->graftMoves[i] == MOVE_NONE)
        {
            boxMon->graftMoves[i] = move;
            return;
        }
    }
    species = GetBoxMonData(boxMon, MON_DATA_SPECIES);
    for (i = 0; i < MAX_MON_MOVES; i++)
    {
        if (IsLevelUpMoveOf(species, boxMon->graftMoves[i]))
        {
            evict = i;
            break;
        }
    }
    // Keep oldest-to-newest order: close the gap, append the new move at the end.
    for (i = evict; i < MAX_MON_MOVES - 1; i++)
        boxMon->graftMoves[i] = boxMon->graftMoves[i + 1];
    boxMon->graftMoves[MAX_MON_MOVES - 1] = move;
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
// Shedinja, and anything grafted with Wonder Guard, is a hollow husk: max HP is always 1.
// The drawback travels with the ability, the same way Shedinja fusions work in other games.
bool8 IsGraftHusk(const struct BoxPokemon *boxMon)
{
    if (boxMon->graftAbility == ABILITY_WONDER_GUARD)
        return TRUE;
    return GetBoxMonData((struct BoxPokemon *)boxMon, MON_DATA_SPECIES) == SPECIES_SHEDINJA;
}

static const u8 sText_HuskHPGraft[] = _("It won't have any effect.");
static const u8 sText_StatGraftLimit[] = _("Stat graft limit reached!\nNeed more badges.");

// Returns the message explaining why this stat graft is refused, or NULL if it is allowed.
const u8 *GetStatGraftRejectReason(const struct BoxPokemon *boxMon, u8 statIndex)
{
    if (statIndex == STAT_HP && IsGraftHusk(boxMon))
        return sText_HuskHPGraft;
    // Overwriting an existing graft slot is always free
    if (statIndex < 6 && boxMon->graftStatOverrides[statIndex] != 0)
        return NULL;
    // New slot: check against cap
    if (CountStatGraftsOnBoxMon(boxMon) >= GetStatGraftCap())
        return sText_StatGraftLimit;
    return NULL;
}

bool8 CanApplyStatGraft(const struct BoxPokemon *boxMon, u8 statIndex)
{
    return GetStatGraftRejectReason(boxMon, statIndex) == NULL;
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
    CalculateMonStats(mon); // Wonder Guard grafts change max HP
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
            const u8 counts[] = { LAND_WILD_COUNT, WATER_WILD_COUNT, ROCK_WILD_COUNT, FISH_WILD_COUNT };
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
                if (s == STAT_HP && IsGraftHusk(&mon->box))
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
            RecordGraftMove(&mon->box, move);
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
#if TESTING
    // Battle tests expect trainer parties exactly as written; random grafts break them.
    if (gTestRunnerEnabled)
        return;
#endif
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
#define GRAFT_OUTLINE_LUM 4   // entries dimmer than this keep original color
#define GRAFT_SAT_THRESHOLD 3 // RGB555 (max-min) below this = gray, no usable hue
#define GRAFT_PAL_MAX 16      // GBA sprite palette size
#define GRAFT_HUE_GAP_MIN 85 // minimum second-gap for dual-cluster; below = single hue

// Luminance-adaptive neutral threshold:
// Dark pixels: even slight tint counts (threshold ~0.04 of max delta)
// Bright pixels: need clearly colored (threshold ~0.19 of max delta)
// Linear ramp: threshold = 1 + (lum_norm * 6), in 5-bit delta units
// Matches Python: 0.04 + t * 0.15 where t = (lum-4)/(31-4)

// Weighted luminance: 0.30 R + 0.59 G + 0.11 B. Inputs 0..31, output 0..31.
static u8 GraftLuminance(s16 r, s16 g, s16 b)
{
    return (u8)((77 * r + 151 * g + 28 * b) >> 8);
}

// Integer hue on a 0..1535 wheel. Returns FALSE if too desaturated.
static bool8 GraftHue(s16 r, s16 g, s16 b, u16 *outHue)
{
    s16 maxc = (r > g) ? (r > b ? r : b) : (g > b ? g : b);
    s16 minc = (r < g) ? (r < b ? r : b) : (g < b ? g : b);
    s16 delta = maxc - minc;
    s32 h;

    if (delta < GRAFT_SAT_THRESHOLD)
        return FALSE;

    if (maxc == r)
        h = ((g - b) * 256) / delta;
    else if (maxc == g)
        h = 512 + ((b - r) * 256) / delta;
    else
        h = 1024 + ((r - g) * 256) / delta;

    if (h < 0)
        h += 1536;
    if (h >= 1536)
        h -= 1536;

    *outHue = (u16)h;
    return TRUE;
}

// Luminance-adaptive neutral check. Returns TRUE if the pixel is "neutral"
// (gray/white/black) and should NOT be recolored.
// Dark pixels: low threshold (subtle tints are real color)
// Bright pixels: high threshold (skip eye whites, pure grays)
static bool8 GraftIsNeutral(s16 r5, s16 g5, s16 b5)
{
    s16 maxc = (r5 > g5) ? (r5 > b5 ? r5 : b5) : (g5 > b5 ? g5 : b5);
    s16 minc = (r5 < g5) ? (r5 < b5 ? r5 : b5) : (g5 < b5 ? g5 : b5);
    s16 delta = maxc - minc;
    u8 lum = GraftLuminance(r5, g5, b5);
    u8 threshold;

    // t = (lum - OUTLINE_LUM) / (31 - OUTLINE_LUM), 0..1
    // threshold = 1 + (t * 6), integer 5-bit delta threshold
    // Maps to Python: 0.04 + t * 0.15 scaled to delta units
    if (lum <= GRAFT_OUTLINE_LUM)
        return (delta < 1);

    {
        // (lum - 4) * 6 / 27, clamped
        s16 t256 = ((s16)(lum - GRAFT_OUTLINE_LUM) * 256) / (31 - GRAFT_OUTLINE_LUM);
        // threshold in delta units: 1 + (t256 * 6) / 256
        threshold = (u8)(1 + ((t256 * 6) >> 8));
    }

    return (delta < threshold);
}

// Anchored luminance-preserving shade interpolation.
// Dark pixels -> dark shade (7/16 of type color)
// Mid pixels -> pure type color (full saturation guaranteed)
// Bright pixels -> light shade (type + 9/16 blend toward white)
// 
// Two-segment: dark->base at t=0..0.5, base->light at t=0.5..1.0
// This ensures the type color itself appears at the midpoint luminance,
// preventing desaturation that a simple dark-to-light lerp would cause.
static u16 GraftShadeContinuous(const struct PlttData *type, u8 lum, u8 minLum, u8 maxLum)
{
    s16 tr = type->r, tg = type->g, tb = type->b;
    s16 dr, dg, db;  // dark shade
    s16 lr, lg, lb;  // light shade
    s16 r, g, b;
    u16 t256;

    // t = (lum - minLum) / (maxLum - minLum), as 0..256 fixed point
    if (maxLum <= minLum)
        t256 = 128;
    else
        t256 = (u16)(((s16)(lum - minLum) * 256) / (maxLum - minLum));

    // Dark shade: type * 7/16
    dr = (tr * 7) >> 4;
    dg = (tg * 7) >> 4;
    db = (tb * 7) >> 4;

    // Light shade: type + (31 - type) * 9/16
    lr = tr + (((31 - tr) * 9) >> 4);
    lg = tg + (((31 - tg) * 9) >> 4);
    lb = tb + (((31 - tb) * 9) >> 4);

    if (t256 <= 128)
    {
        // Dark to base: lerp dr->tr with t mapped to [0, 256]
        u16 subT = t256 * 2;  // 0..256
        r = dr + (((tr - dr) * subT) >> 8);
        g = dg + (((tg - dg) * subT) >> 8);
        b = db + (((tb - db) * subT) >> 8);
    }
    else
    {
        // Base to light: lerp tr->lr with t mapped to [0, 256]
        u16 subT = (t256 - 128) * 2;  // 0..256
        r = tr + (((lr - tr) * subT) >> 8);
        g = tg + (((lg - tg) * subT) >> 8);
        b = tb + (((lb - tb) * subT) >> 8);
    }

    // Clamp to 5-bit range
    if (r < 0) r = 0; if (r > 31) r = 31;
    if (g < 0) g = 0; if (g > 31) g = 31;
    if (b < 0) b = 0; if (b > 31) b = 31;

    return RGB(r, g, b);
}

void ApplyGraftTypePaletteSwap(u16 palOffset, u16 numEntries, u32 type1Color, u32 type2Color, bool8 isDualType)
{
    struct PlttData *c1 = (struct PlttData *)&type1Color;
    struct PlttData *c2 = (struct PlttData *)&type2Color;
    u16 hue[GRAFT_PAL_MAX];
    u8 cluster[GRAFT_PAL_MAX]; // 0 -> type1, 1 -> type2
    u8 valid[GRAFT_PAL_MAX];   // entry indices that carry a usable hue
    u8 numValid = 0;
    u16 i;

    // Global luminance min/max across all colored slots
    u8 globalMinLum = 31, globalMaxLum = 0;

    if (numEntries > GRAFT_PAL_MAX)
        numEntries = GRAFT_PAL_MAX;

    // ---- Pass 1: classify entries, collect hues of colored (non-outline) ones
    for (i = 0; i < numEntries; i++)
    {
        struct PlttData *src = (struct PlttData *)&gPlttBufferUnfaded[palOffset + i];
        s16 r = src->r, g = src->g, b = src->b;
        u8 lum;

        cluster[i] = 0; // default type1
        if (i == 0) continue; // transparent slot
        lum = GraftLuminance(r, g, b);
        if (lum < GRAFT_OUTLINE_LUM) continue; // outline / near-black

        // Luminance-adaptive neutral check
        if (GraftIsNeutral(r, g, b)) continue;

        // Track global luminance range
        if (lum < globalMinLum) globalMinLum = lum;
        if (lum > globalMaxLum) globalMaxLum = lum;

        if (GraftHue(r, g, b, &hue[i]))
            valid[numValid++] = (u8)i;
    }

    // ---- Pass 2: for dual types with 2+ hues, split into two clusters by hue.
    if (isDualType && numValid >= 2)
    {
        u8 j, k;
        u16 g1size = 0, g2size = 0;
        u8 g1 = 0, g2 = 0;
        u8 lo, hi, sizeB, sizeA;
        bool8 bigIsB;

        // insertion sort valid[] by hue
        for (j = 1; j < numValid; j++)
        {
            u8 key = valid[j];
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
            u16 hCur = hue[valid[k]];
            u16 hNext = hue[valid[(k + 1) % numValid]];
            u16 gap = (k + 1 == numValid) ? (1536 - hCur + hNext) : (hNext - hCur);
            if (gap > g1size)
            {
                g2size = g1size; g2 = g1;
                g1size = gap; g1 = k;
            }
            else if (gap > g2size)
            {
                g2size = gap; g2 = k;
            }
        }

        // Check if second gap is big enough for dual clustering
        if (g2size < GRAFT_HUE_GAP_MIN)
        {
            // ---- Luminance fallback for single-hue sprites ----
            // Split by brightness: top 40% brightest = type2 accent, rest = type1 body
            u8 lums[GRAFT_PAL_MAX];
            u8 lumOrder[GRAFT_PAL_MAX];
            u8 nAccent;
            u8 count0, count1;

            for (k = 0; k < numValid; k++)
            {
                lums[k] = GraftLuminance(
                    ((struct PlttData *)&gPlttBufferUnfaded[palOffset + valid[k]])->r,
                    ((struct PlttData *)&gPlttBufferUnfaded[palOffset + valid[k]])->g,
                    ((struct PlttData *)&gPlttBufferUnfaded[palOffset + valid[k]])->b
                );
                lumOrder[k] = k;
            }

            // Sort by luminance (insertion sort, small N)
            for (j = 1; j < numValid; j++)
            {
                u8 keyIdx = lumOrder[j];
                s16 m = (s16)j - 1;
                while (m >= 0 && lums[lumOrder[m]] > lums[keyIdx])
                {
                    lumOrder[m + 1] = lumOrder[m];
                    m--;
                }
                lumOrder[m + 1] = keyIdx;
            }

            // Top 40% = accent (type2), rest = body (type1)
            nAccent = (numValid * 4) / 10;
            if (nAccent < 1) nAccent = 1;

            for (k = 0; k < numValid; k++)
            {
                u8 srcIdx = lumOrder[k];
                cluster[valid[srcIdx]] = (k >= (numValid - nAccent)) ? 1 : 0;
            }

            // If type2 group is larger, swap so type1 is the bigger group
            count0 = 0; count1 = 0;
            for (k = 0; k < numValid; k++)
            {
                if (cluster[valid[k]] == 0) count0++;
                else count1++;
            }
            if (count1 > count0)
            {
                for (k = 0; k < numValid; k++)
                    cluster[valid[k]] = 1 - cluster[valid[k]];
            }
        }
        else
        {
            // ---- Gap-based hue split ----
            lo = (g1 < g2) ? g1 : g2;
            hi = (g1 < g2) ? g2 : g1;
            sizeB = hi - lo;
            sizeA = numValid - sizeB;
            bigIsB = (sizeB > sizeA);

            for (k = 0; k < numValid; k++)
            {
                bool8 inB = (k > lo && k <= hi);
                if (inB)
                    cluster[valid[k]] = bigIsB ? 0 : 1;
                else
                    cluster[valid[k]] = bigIsB ? 1 : 0;
            }

            // ---- Rebalance: if split is too imbalanced, move border slots ----
            {
                u8 count0 = 0, count1 = 0;
                u8 smallCount, minSize, need;
                u8 smallCluster, bigCluster;
                u16 avgSmallHue;
                s32 bestDist, bestSlot;

                for (k = 0; k < numValid; k++)
                {
                    if (cluster[valid[k]] == 0) count0++;
                    else count1++;
                }

                smallCount = (count0 < count1) ? count0 : count1;
                minSize = (numValid * 3) / 10; // at least 30%
                if (minSize < 2) minSize = 2;

                if (smallCount < minSize)
                {
                    smallCluster = (count0 < count1) ? 0 : 1;
                    bigCluster = 1 - smallCluster;

                    // Average hue of small group
                    avgSmallHue = 0;
                    {
                        u8 sc = 0;
                        for (k = 0; k < numValid; k++)
                        {
                            if (cluster[valid[k]] == smallCluster)
                            {
                                avgSmallHue += hue[valid[k]];
                                sc++;
                            }
                        }
                        if (sc > 0) avgSmallHue /= sc;
                    }

                    need = minSize - smallCount;

                    // Move border slots from big group closest to small group centroid
                    while (need > 0)
                    {
                        bestSlot = -1;
                        bestDist = 9999;
                        for (k = 0; k < numValid; k++)
                        {
                            u16 hd;
                            if (cluster[valid[k]] != bigCluster) continue;
                            // Circular hue distance
                            hd = abs((s16)hue[valid[k]] - (s16)avgSmallHue);
                            if (hd > 768) hd = 1536 - hd;
                            if ((s16)hd < bestDist)
                            {
                                bestDist = (s16)hd;
                                bestSlot = k;
                            }
                        }
                        if (bestSlot < 0) break;
                        cluster[valid[bestSlot]] = smallCluster;
                        need--;
                    }
                }
            }
        }
    }

    // ---- Pass 3: write the swapped palette using continuous shading ----
    for (i = 0; i < numEntries; i++)
    {
        struct PlttData *src = (struct PlttData *)&gPlttBufferUnfaded[palOffset + i];
        s16 r = src->r, g = src->g, b = src->b;
        u8 lum;

        if (i == 0) continue; // keep transparent

        lum = GraftLuminance(r, g, b);
        if (lum < GRAFT_OUTLINE_LUM)
        {
            gPlttBufferFaded[palOffset + i] = gPlttBufferUnfaded[palOffset + i];
            continue;
        }

        // Skip neutral (gray/white) pixels
        if (GraftIsNeutral(r, g, b))
        {
            gPlttBufferFaded[palOffset + i] = gPlttBufferUnfaded[palOffset + i];
            continue;
        }

        {
            const struct PlttData *tc = (cluster[i] == 0) ? c1 : c2;
            gPlttBufferFaded[palOffset + i] = GraftShadeContinuous(tc, lum, globalMinLum, globalMaxLum);
        }
    }
}

// ============================================================
// Breeding
// ============================================================
#define GRAFT_INHERIT_CHANCE 25 // % per graft when no parent holds a Graft Knot

// Picks which parent (if any) passes one graft. hasA/hasB: whether each parent has this graft.
// A parent holding a Graft Knot always passes its grafts; if both hold one, a random one of them
// does. Otherwise a random parent that has the graft passes it with GRAFT_INHERIT_CHANCE.
static struct BoxPokemon *PickGraftDonor(struct BoxPokemon *parentA, struct BoxPokemon *parentB, bool32 hasA, bool32 hasB)
{
    bool32 knotA = hasA && GetBoxMonData(parentA, MON_DATA_HELD_ITEM) == ITEM_GRAFT_KNOT;
    bool32 knotB = hasB && GetBoxMonData(parentB, MON_DATA_HELD_ITEM) == ITEM_GRAFT_KNOT;

    if (knotA || knotB)
    {
        if (knotA && knotB)
            return (Random() & 1) ? parentA : parentB;
        return knotA ? parentA : parentB;
    }
    if (!(hasA || hasB) || Random() % 100 >= GRAFT_INHERIT_CHANCE)
        return NULL;
    if (hasA && hasB)
        return (Random() & 1) ? parentA : parentB;
    return hasA ? parentA : parentB;
}

// Copy grafts from the Day Care parents onto a new egg. Nature grafts are not inherited:
// they work like mints, and the Everstone already handles nature.
void InheritGrafts(struct BoxPokemon *egg, struct BoxPokemon *parentA, struct BoxPokemon *parentB)
{
    struct BoxPokemon *donor;
    u32 i, j, count = 0;

    for (i = 0; i < 2; i++)
    {
        donor = PickGraftDonor(parentA, parentB, parentA->graftTypes[i] != GRAFT_TYPE_NONE, parentB->graftTypes[i] != GRAFT_TYPE_NONE);
        if (donor != NULL)
        {
            egg->graftTypes[i] = donor->graftTypes[i];
            egg->graftFlags |= GRAFT_FLAG_TYPE;
        }
    }
    donor = PickGraftDonor(parentA, parentB, parentA->graftAbility != GRAFT_ABILITY_NONE, parentB->graftAbility != GRAFT_ABILITY_NONE);
    if (donor != NULL)
    {
        egg->graftAbility = donor->graftAbility;
        egg->graftFlags |= GRAFT_FLAG_ABILITY;
    }
    for (i = 0; i < NUM_STATS; i++)
    {
        donor = PickGraftDonor(parentA, parentB, parentA->graftStatOverrides[i] != 0, parentB->graftStatOverrides[i] != 0);
        if (donor != NULL)
        {
            egg->graftStatOverrides[i] = donor->graftStatOverrides[i];
            egg->graftFlags |= GRAFT_FLAG_STAT;
        }
    }
    // Grafted moves (relearner history): each move is its own graft.
    for (i = 0; i < 2 * MAX_MON_MOVES && count < MAX_MON_MOVES; i++)
    {
        struct BoxPokemon *owner = (i < MAX_MON_MOVES) ? parentA : parentB;
        struct BoxPokemon *other = (i < MAX_MON_MOVES) ? parentB : parentA;
        u16 move = owner->graftMoves[i % MAX_MON_MOVES];
        bool32 dup = FALSE;
        if (move == MOVE_NONE)
            continue;
        for (j = 0; j < count; j++)
            dup |= (egg->graftMoves[j] == move);
        if (dup || PickGraftDonor(owner, other, TRUE, FALSE) == NULL)
            continue;
        egg->graftMoves[count++] = move;
        egg->graftFlags |= GRAFT_FLAG_MOVE;
    }
}

// Copy every graft field (used when an egg hatches into a freshly created mon).
void CopyGrafts(struct BoxPokemon *dest, const struct BoxPokemon *src)
{
    dest->graftTypes[0] = src->graftTypes[0];
    dest->graftTypes[1] = src->graftTypes[1];
    dest->graftAbility = src->graftAbility;
    dest->graftFlags = src->graftFlags;
    memcpy(dest->graftStatOverrides, src->graftStatOverrides, sizeof(dest->graftStatOverrides));
    memcpy(dest->graftMoves, src->graftMoves, sizeof(dest->graftMoves));
}

// Overwrite a battler's types and ability with its grafts, if it has any.
// Call after gBattleMons[battler] has been filled with the species defaults.
// Works for both sides: the grafts live on the party mon's BoxPokemon.
void ApplyBattlerGraftOverrides(u32 battler)
{
    const struct BoxPokemon *boxMon = &GetBattlerMon(battler)->box;

    if (boxMon->graftTypes[0] != GRAFT_TYPE_NONE)
        gBattleMons[battler].types[0] = boxMon->graftTypes[0];
    if (boxMon->graftTypes[1] != GRAFT_TYPE_NONE)
        gBattleMons[battler].types[1] = boxMon->graftTypes[1];
    if (boxMon->graftAbility != GRAFT_ABILITY_NONE)
        gBattleMons[battler].ability = boxMon->graftAbility;
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
