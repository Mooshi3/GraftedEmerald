#include "global.h"
#include "constants/opponents.h"
#include "pokemon.h"
#include "graft_data.h"
#include "overworld.h"
#include "party_menu.h"
#include "graft_menu.h"
#include "graft_settings.h"
#include "move.h"
#include "constants/species.h"
#include "constants/abilities.h"
#include "test_runner.h"
#include "palette.h"
#include "data.h"
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
    u8 val = gParties[B_TRAINER_PLAYER][partySlot].box.graftTypes[typeIndex];
    if (val == GRAFT_TYPE_NONE)
        return GRAFT_TYPE_NONE;
    return val;
}

// Set grafted type for a party mon
void SetGraftedType(u8 partySlot, u8 typeIndex, u8 type)
{
    if (partySlot >= PARTY_SIZE || typeIndex > 1)
        return;
    gParties[B_TRAINER_PLAYER][partySlot].box.graftTypes[typeIndex] = type;
    gParties[B_TRAINER_PLAYER][partySlot].box.graftFlags |= GRAFT_FLAG_TYPE;
}

// Get grafted stat for a party mon
u16 GetGraftedStat(u8 partySlot, u8 statIndex)
{
    if (partySlot >= PARTY_SIZE || statIndex >= 6)
        return GRAFT_STAT_NONE;
    u8 val = gParties[B_TRAINER_PLAYER][partySlot].box.graftStatOverrides[statIndex];
    if (val == 0) // 0 = no override
        return GRAFT_STAT_NONE;
    return val;
}

// Set grafted stat for a party mon
void SetGraftedStat(u8 partySlot, u8 statIndex, u16 statValue)
{
    if (partySlot >= PARTY_SIZE || statIndex >= 6)
        return;
    gParties[B_TRAINER_PLAYER][partySlot].box.graftStatOverrides[statIndex] = (u8)statValue;
    CalculateMonStats(&gParties[B_TRAINER_PLAYER][partySlot]);
    gParties[B_TRAINER_PLAYER][partySlot].box.graftFlags |= GRAFT_FLAG_STAT;
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
    u16 val = gParties[B_TRAINER_PLAYER][partySlot].box.graftAbility;
    if (val == GRAFT_ABILITY_NONE)
        return GRAFT_ABILITY_NONE;
    return val;
}

// Set grafted ability for a party mon
void SetGraftedAbility(u8 partySlot, u16 ability)
{
    if (partySlot >= PARTY_SIZE)
        return;
    SetGraftedAbilityOnMon(&gParties[B_TRAINER_PLAYER][partySlot], ability);
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
    gParties[B_TRAINER_PLAYER][partySlot].box.graftTypes[0] = GRAFT_TYPE_NONE;
    gParties[B_TRAINER_PLAYER][partySlot].box.graftTypes[1] = GRAFT_TYPE_NONE;
    gParties[B_TRAINER_PLAYER][partySlot].box.graftAbility = GRAFT_ABILITY_NONE;
    gParties[B_TRAINER_PLAYER][partySlot].box.graftFlags = 0;
}

// Set grafted nature for a party mon
void SetGraftedNature(u8 partySlot, u8 natureId)
{
    if (partySlot >= PARTY_SIZE)
        return;
    SetMonData(&gParties[B_TRAINER_PLAYER][partySlot], MON_DATA_HIDDEN_NATURE, &natureId);
    CalculateMonStats(&gParties[B_TRAINER_PLAYER][partySlot]);
    gParties[B_TRAINER_PLAYER][partySlot].box.graftFlags |= GRAFT_FLAG_NATURE;
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
    return HasShedinjaHPHandling(GetBoxMonData((struct BoxPokemon *)boxMon, MON_DATA_SPECIES));
}

static const u8 sText_HuskHPGraft[] = _("It won't have any effect.");
static const u8 sText_StatGraftLimit[] = _("Stat graft limit reached!\nNeed more badges.");
static const u8 sText_FallenTarget[] = _("Fallen Pokémon can't take grafts.");
static const u8 sText_GraftBudgetFull[] = _("It can't take any more grafts.");
static const u8 sText_GateType[] = _("Type grafts need a badge.");
static const u8 sText_GateStat[] = _("Stat grafts need 2 badges.");
static const u8 sText_GateAbility[] = _("Ability grafts need 3 badges.");

// Does this mon have anything the Ungrafter would remove?
bool32 BoxMonHasGrafts(const struct BoxPokemon *boxMon)
{
    u32 i;
    if (boxMon->graftFlags & ~GRAFT_FLAG_FALLEN)
        return TRUE;
    if (boxMon->graftTypes[0] != GRAFT_TYPE_NONE || boxMon->graftTypes[1] != GRAFT_TYPE_NONE
     || boxMon->graftAbility != GRAFT_ABILITY_NONE)
        return TRUE;
    for (i = 0; i < NUM_STATS; i++)
    {
        if (boxMon->graftStatOverrides[i] != 0)
            return TRUE;
    }
    for (i = 0; i < MAX_MON_MOVES; i++)
    {
        if (boxMon->graftMoves[i] != MOVE_NONE)
            return TRUE;
    }
    return FALSE;
}

// Ungrafter: removes every graft and the grafted-move relearner history. Moves the mon
// currently knows stay. A grafted nature goes back to the one from its personality.
// Keeps the fallen mark and the harvest area, which are challenge state, not grafts.
void UngraftMon(struct Pokemon *mon)
{
    u32 i;
    if (mon->box.graftFlags & GRAFT_FLAG_NATURE)
    {
        u8 nature = GetNatureFromPersonality(GetMonData(mon, MON_DATA_PERSONALITY));
        SetMonData(mon, MON_DATA_HIDDEN_NATURE, &nature);
    }
    mon->box.graftTypes[0] = GRAFT_TYPE_NONE;
    mon->box.graftTypes[1] = GRAFT_TYPE_NONE;
    mon->box.graftAbility = GRAFT_ABILITY_NONE;
    for (i = 0; i < NUM_STATS; i++)
        mon->box.graftStatOverrides[i] = 0;
    for (i = 0; i < MAX_MON_MOVES; i++)
        mon->box.graftMoves[i] = MOVE_NONE;
    mon->box.graftFlags &= GRAFT_FLAG_FALLEN;
    CalculateMonStats(mon); // stat grafts and the Wonder Guard husk
}

// Graft Budget/Strict: how many grafts a mon holds. Moves don't count; both type slots count as one.
u32 CountBudgetGraftsOnBoxMon(const struct BoxPokemon *boxMon)
{
    u32 count = CountStatGraftsOnBoxMon(boxMon);
    if (boxMon->graftTypes[0] != GRAFT_TYPE_NONE || boxMon->graftTypes[1] != GRAFT_TYPE_NONE)
        count++;
    if (boxMon->graftAbility != GRAFT_ABILITY_NONE)
        count++;
    if (boxMon->graftFlags & GRAFT_FLAG_NATURE)
        count++;
    return count;
}

u32 GetGraftBudget(void)
{
    switch (GetGraftSetting(GRAFT_SETTING_GRAFT_LIMIT))
    {
    case GRAFT_LIMIT_BUDGET:
        return 2 + GetBadgeCount() / 2;
    case GRAFT_LIMIT_STRICT:
        return 3;
    default:
        return 0xFF;
    }
}

// The graft cap that applies to a kind of graft under the Graft Limit setting, for counters: FALSE when none
// does (Free; Standard for anything but stats, or from 5 badges; moves never count). graftMenuType
// GRAFT_MENU_COUNT asks for the mon's general cap (Standard: its stat grafts).
bool32 GetGraftLimitCount(const struct BoxPokemon *boxMon, u8 graftMenuType, u32 *used, u32 *cap)
{
    switch (GetGraftSetting(GRAFT_SETTING_GRAFT_LIMIT))
    {
    case GRAFT_LIMIT_STANDARD:
        if ((graftMenuType != GRAFT_MENU_STAT && graftMenuType != GRAFT_MENU_COUNT) || GetStatGraftCap() == 255)
            return FALSE;
        *used = CountStatGraftsOnBoxMon(boxMon);
        *cap = GetStatGraftCap();
        return TRUE;
    case GRAFT_LIMIT_BUDGET:
    case GRAFT_LIMIT_STRICT:
        if (graftMenuType == GRAFT_MENU_MOVE)
            return FALSE;
        *used = CountBudgetGraftsOnBoxMon(boxMon);
        *cap = GetGraftBudget();
        return TRUE;
    default:
        return FALSE;
    }
}

// Would this graft take a new budget slot, or replace one the mon already has?
static bool32 GraftTakesNewSlot(const struct BoxPokemon *boxMon, u8 graftMenuType, u8 statIndex)
{
    switch (graftMenuType)
    {
    case GRAFT_MENU_TYPE:
        return boxMon->graftTypes[0] == GRAFT_TYPE_NONE && boxMon->graftTypes[1] == GRAFT_TYPE_NONE;
    case GRAFT_MENU_ABILITY:
        return boxMon->graftAbility == GRAFT_ABILITY_NONE;
    case GRAFT_MENU_NATURE:
        return !(boxMon->graftFlags & GRAFT_FLAG_NATURE);
    case GRAFT_MENU_STAT:
        return statIndex >= NUM_STATS || boxMon->graftStatOverrides[statIndex] == 0;
    default: // moves never count
        return FALSE;
    }
}

// Returns the message explaining why this graft can't go on this mon, or NULL if it can.
// Every graft target check (Graft Ball, Splicer) goes through here.
const u8 *GetGraftRejectReason(const struct BoxPokemon *boxMon, u8 graftMenuType, u8 statIndex)
{
    u32 badges = GetBadgeCount();

    if (boxMon->graftFlags & GRAFT_FLAG_FALLEN)
        return sText_FallenTarget;
    if (graftMenuType == GRAFT_MENU_STAT && statIndex == STAT_HP && IsGraftHusk(boxMon))
        return sText_HuskHPGraft;

    if (GetGraftSetting(GRAFT_SETTING_GRAFT_GATES))
    {
        if (graftMenuType == GRAFT_MENU_TYPE && badges < 1)
            return sText_GateType;
        if (graftMenuType == GRAFT_MENU_STAT && badges < 2)
            return sText_GateStat;
        if (graftMenuType == GRAFT_MENU_ABILITY && badges < 3)
            return sText_GateAbility;
    }

    if (!GraftTakesNewSlot(boxMon, graftMenuType, statIndex))
        return NULL; // replacing an existing graft is always allowed

    switch (GetGraftSetting(GRAFT_SETTING_GRAFT_LIMIT))
    {
    case GRAFT_LIMIT_STANDARD:
        if (graftMenuType == GRAFT_MENU_STAT && CountStatGraftsOnBoxMon(boxMon) >= GetStatGraftCap())
            return sText_StatGraftLimit;
        break;
    case GRAFT_LIMIT_BUDGET:
    case GRAFT_LIMIT_STRICT:
        if (CountBudgetGraftsOnBoxMon(boxMon) >= GetGraftBudget())
            return sText_GraftBudgetFull;
        break;
    }
    return NULL;
}

// Is this graft kind usable at all right now (gates only)? Used to order Patch/Graft Ball offers.
bool32 IsGraftKindGated(u8 graftMenuType)
{
    u32 badges = GetBadgeCount();
    if (!GetGraftSetting(GRAFT_SETTING_GRAFT_GATES))
        return FALSE;
    return (graftMenuType == GRAFT_MENU_TYPE && badges < 1)
        || (graftMenuType == GRAFT_MENU_STAT && badges < 2)
        || (graftMenuType == GRAFT_MENU_ABILITY && badges < 3);
}

const u8 *GetStatGraftRejectReason(const struct BoxPokemon *boxMon, u8 statIndex)
{
    return GetGraftRejectReason(boxMon, GRAFT_MENU_STAT, statIndex);
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
            const u8 counts[] = { NUM_LAND_MONS_ENCOUNTER_SLOTS, NUM_WATER_MONS_ENCOUNTER_SLOTS, NUM_ROCK_SMASH_MONS_ENCOUNTER_SLOTS, NUM_FISHING_MONS_ENCOUNTER_SLOTS };
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

// Walk one level of connections from a map
static void AddConnectedMapEncounters(const struct MapHeader *header)
{
    if (header->connections == NULL)
        return;
    for (s32 i = 0; i < header->connections->count; i++)
    {
        AddMapEncountersToPool(
            header->connections->connections[i].mapGroup,
            header->connections->connections[i].mapNum
        );
    }
}

// Every map in the same map section (the other floors of a cave or hideout)
static void AddMapsecEncountersToPool(u32 mapsec)
{
    for (u32 i = 0; gWildMonHeaders[i].mapGroup != 0xFF; i++)
    {
        const struct MapHeader *header = Overworld_GetMapHeaderByGroupAndId(gWildMonHeaders[i].mapGroup, gWildMonHeaders[i].mapNum);
        if (header->regionMapSectionId == mapsec)
            AddMapEncountersToPool(gWildMonHeaders[i].mapGroup, gWildMonHeaders[i].mapNum);
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
    // The League: Victory Road
    { MAP_GROUP(MAP_EVER_GRANDE_CITY_SIDNEYS_ROOM), MAP_NUM(MAP_EVER_GRANDE_CITY_SIDNEYS_ROOM), MAP_GROUP(MAP_VICTORY_ROAD_B1F), MAP_NUM(MAP_VICTORY_ROAD_B1F) },
    { MAP_GROUP(MAP_EVER_GRANDE_CITY_PHOEBES_ROOM), MAP_NUM(MAP_EVER_GRANDE_CITY_PHOEBES_ROOM), MAP_GROUP(MAP_VICTORY_ROAD_B1F), MAP_NUM(MAP_VICTORY_ROAD_B1F) },
    { MAP_GROUP(MAP_EVER_GRANDE_CITY_GLACIAS_ROOM), MAP_NUM(MAP_EVER_GRANDE_CITY_GLACIAS_ROOM), MAP_GROUP(MAP_VICTORY_ROAD_B1F), MAP_NUM(MAP_VICTORY_ROAD_B1F) },
    { MAP_GROUP(MAP_EVER_GRANDE_CITY_DRAKES_ROOM), MAP_NUM(MAP_EVER_GRANDE_CITY_DRAKES_ROOM), MAP_GROUP(MAP_VICTORY_ROAD_B1F), MAP_NUM(MAP_VICTORY_ROAD_B1F) },
    { MAP_GROUP(MAP_EVER_GRANDE_CITY_CHAMPIONS_ROOM), MAP_NUM(MAP_EVER_GRANDE_CITY_CHAMPIONS_ROOM), MAP_GROUP(MAP_VICTORY_ROAD_B1F), MAP_NUM(MAP_VICTORY_ROAD_B1F) },
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

u32 GetGraftDonorPoolSize(void)
{
    return sGraftDonorCount;
}

void BuildNearbyGraftPool(void)
{
    sGraftDonorCount = 0;
    // Tier 1: current map
    AddMapEncountersToPool(gSaveBlock1Ptr->location.mapGroup, gSaveBlock1Ptr->location.mapNum);
    // Tier 2: connected maps
    AddConnectedMapEncounters(&gMapHeader);
    // Tier 3: manual fallback (gyms, the League, ...)
    if (sGraftDonorCount == 0)
        ApplyGraftPoolFallback();
    // Tier 4: the rest of this map section (other floors of a cave or hideout)
    if (sGraftDonorCount == 0)
        AddMapsecEncountersToPool(gMapHeader.regionMapSectionId);
    // Tier 5: the outdoor map the player came in from (the Escape Rope destination) and its neighbors
    if (sGraftDonorCount == 0 && gSaveBlock1Ptr->escapeWarp.mapGroup != MAP_GROUP(MAP_UNDEFINED))
    {
        AddMapEncountersToPool(gSaveBlock1Ptr->escapeWarp.mapGroup, gSaveBlock1Ptr->escapeWarp.mapNum);
        AddConnectedMapEncounters(Overworld_GetMapHeaderByGroupAndId(gSaveBlock1Ptr->escapeWarp.mapGroup, gSaveBlock1Ptr->escapeWarp.mapNum));
    }
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

// keepType: a boss's specialty type, which type grafts never replace (TYPE_NONE for anyone else),
// or KEEP_BOTH_TYPES for a boss's ace, whose wrong type plus grafted specialty is the point
#define KEEP_BOTH_TYPES 0xFE
static void ApplyEnemyGrafts(struct Pokemon *mon, u8 graftCount, bool32 forceAbility, u8 keepType)
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
        if (forceAbility && !abilityUsed)
            kind = GK_ABILITY; // Enemy Grafts: More gives bosses' aces an ability first

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
            if (keepType == KEEP_BOTH_TYPES)
                continue;
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
            if (keepType != TYPE_NONE && type1 == keepType)
                slot = 1; // a boss's mons keep their specialty type
            else if (keepType != TYPE_NONE && type2 == keepType)
                slot = 0;
            else if (type1 == type2)
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

// Wild Grafts setting: some wild Pokémon come with grafts rolled like a trainer's, from the same nearby pool.
void TryApplyWildGrafts(struct Pokemon *mon)
{
    u32 chance;
#if TESTING
    if (gTestRunnerEnabled)
        return;
#endif
    switch (GetGraftSetting(GRAFT_SETTING_WILD_GRAFTS))
    {
    case GRAFT_WILD_GRAFTS_RARE:
        chance = 5;
        break;
    case GRAFT_WILD_GRAFTS_COMMON:
        chance = 20;
        break;
    default:
        return;
    }
    // Not before the Pokédex: keeps Birch's rescue battle plain
    if (!FlagGet(FLAG_SYS_POKEDEX_GET) || Random() % 100 >= chance)
        return;
    BuildNearbyGraftPool();
    ApplyEnemyGrafts(mon, 1 + GetBadgeCount() / 2, FALSE, TYPE_NONE);
}

// The first rival battle on Route 103 is a tutorial fight; it stays ungrafted.
static bool32 IsUngraftedTrainer(u16 trainerNum)
{
    switch (trainerNum)
    {
    case TRAINER_BRENDAN_ROUTE_103_MUDKIP:
    case TRAINER_BRENDAN_ROUTE_103_TREECKO:
    case TRAINER_BRENDAN_ROUTE_103_TORCHIC:
    case TRAINER_MAY_ROUTE_103_MUDKIP:
    case TRAINER_MAY_ROUTE_103_TREECKO:
    case TRAINER_MAY_ROUTE_103_TORCHIC:
        return TRUE;
    default:
        return FALSE;
    }
}

struct BossAce;
static const struct BossAce *GetBossAce(u16 trainerNum);

static bool32 IsBossTrainer(u16 trainerNum)
{
    if (GetBossAce(trainerNum) != NULL || trainerNum == TRAINER_WALLACE)
        return TRUE; // Emerald's Leaders (rematches too), Elite Four and Champion
    switch (GetTrainerClassFromId(trainerNum))
    {
    case TRAINER_CLASS_LEADER:
    case TRAINER_CLASS_ELITE_FOUR:
    case TRAINER_CLASS_CHAMPION:
        return TRUE;
    default:
        return FALSE;
    }
}

u32 GetEnemyGraftCount(void)
{
    u32 badges = GetBadgeCount();
    switch (GetGraftSetting(GRAFT_SETTING_ENEMY_GRAFTS))
    {
    case GRAFT_ENEMY_GRAFTS_FEWER:
        return 1 + badges / 2;
    case GRAFT_ENEMY_GRAFTS_MORE:
        return 3 + badges;
    default:
        return 2 + badges;
    }
}

static void RollTrainerGrafts(struct Pokemon *party, u8 count, u16 trainerNum);

void ApplyTrainerGrafts(struct Pokemon *party, u8 count, u16 trainerNum)
{
#if TESTING
    // Battle tests expect trainer parties exactly as written; random grafts break them.
    if (gTestRunnerEnabled)
        return;
#endif
    RollTrainerGrafts(party, count, trainerNum);
}

#if TESTING
// For the graft tests: the same roll, without the test-runner skip
void Test_RollTrainerGrafts(struct Pokemon *party, u8 count, u16 trainerNum)
{
    RollTrainerGrafts(party, count, trainerNum);
}
#endif

// ============================================================
// Boss specialty types and "Tera" aces (like Scarlet/Violet's Gym Leaders): every Leader and
// Elite Four member brings an extra ace of the wrong type, grafted with their specialty type and a
// specialty-type attack that scales with level. Their own mons never lose the specialty type.
// ============================================================

// The type most of the party shares (a boss's specialty), or TYPE_NONE for an empty party
static u8 GetPartySpecialtyType(struct Pokemon *party, u8 count)
{
    u8 tally[NUMBER_OF_MON_TYPES] = {0};
    u32 i, best = TYPE_NONE;
    for (i = 0; i < count && i < PARTY_SIZE; i++)
    {
        u32 species = GetMonData(&party[i], MON_DATA_SPECIES);
        u32 t1 = GetSpeciesType(species, 0), t2 = GetSpeciesType(species, 1);
        if (species == SPECIES_NONE)
            continue;
        tally[t1]++;
        if (t2 != t1)
            tally[t2]++;
    }
    for (i = 0; i < NUMBER_OF_MON_TYPES; i++)
    {
        if (tally[i] > tally[best])
            best = i;
    }
    return tally[best] != 0 ? best : TYPE_NONE;
}

struct BossAce
{
    u16 species;
    u16 lateSpecies; // for an evolution the level rule below can't do (friendship, stones)
    u8 lateLevel;
};

// Leaders in gym order (first fights 265-272, rematches 770-801 in blocks of 4), then the Elite Four
static const struct BossAce sBossAces[] =
{
    { SPECIES_SANDYGAST },                          // Roxanne (Rock): a sandcastle
    { SPECIES_BUNEARY, SPECIES_LOPUNNY, 30 },       // Brawly (Fighting): a kickboxer
    { SPECIES_KLINK },                              // Wattson (Electric): gears
    { SPECIES_SANDILE },                            // Flannery (Fire): desert heat
    { SPECIES_SNUBBULL },                           // Norman (Normal)
    { SPECIES_TRAPINCH },                           // Winona (Flying): Vibrava/Flygon's wings
    { SPECIES_CLEFFA, SPECIES_CLEFABLE, 35 },       // Tate & Liza (Psychic): the moon
    { SPECIES_RALTS },                              // Juan (Water): grace
    { SPECIES_CROAGUNK },                           // Sidney (Dark)
    { SPECIES_GOTHITA },                            // Phoebe (Ghost)
    { SPECIES_VULPIX, SPECIES_NINETALES, 30 },      // Glacia (Ice): a nod to Alolan Ninetales
    { SPECIES_CHARMANDER },                         // Drake (Dragon): a nod to Mega Charizard X
};

static const struct BossAce *GetBossAce(u16 trainerNum)
{
    if (trainerNum >= TRAINER_ROXANNE_1 && trainerNum <= TRAINER_JUAN_1)
        return &sBossAces[trainerNum - TRAINER_ROXANNE_1];
    if (trainerNum >= TRAINER_ROXANNE_2 && trainerNum <= TRAINER_JUAN_5)
        return &sBossAces[(trainerNum - TRAINER_ROXANNE_2) / 4];
    if (trainerNum >= TRAINER_SIDNEY && trainerNum <= TRAINER_DRAKE)
        return &sBossAces[8 + trainerNum - TRAINER_SIDNEY];
    return NULL;
}

// The form a level-appropriate trainer would have: follows level evolutions, plus the table's own
static u16 GetBossAceSpecies(const struct BossAce *ace, u32 level)
{
    u16 species = ace->species;
    bool32 evolved = TRUE;
    if (ace->lateSpecies != SPECIES_NONE && level >= ace->lateLevel)
        species = ace->lateSpecies;
    while (evolved)
    {
        const struct Evolution *evos = GetSpeciesEvolutions(species);
        evolved = FALSE;
        for (u32 i = 0; evos != NULL && evos[i].method != EVOLUTIONS_END; i++)
        {
            if (evos[i].method == EVO_LEVEL && evos[i].param <= level && IsSpeciesEnabled(evos[i].targetSpecies))
            {
                species = evos[i].targetSpecies;
                evolved = TRUE;
                break;
            }
        }
    }
    return species;
}

// A specialty-type attack by tier (below Lv25, below Lv42, from Lv42) and category
static const u16 sBossAceMoves[NUMBER_OF_MON_TYPES][2][3] =
{
    [TYPE_ROCK]     = { { MOVE_ROCK_TOMB,     MOVE_ROCK_SLIDE,   MOVE_STONE_EDGE },   { MOVE_ANCIENT_POWER, MOVE_POWER_GEM,    MOVE_POWER_GEM } },
    [TYPE_FIGHTING] = { { MOVE_KARATE_CHOP,   MOVE_BRICK_BREAK,  MOVE_CLOSE_COMBAT }, { MOVE_VACUUM_WAVE,   MOVE_AURA_SPHERE,  MOVE_FOCUS_BLAST } },
    [TYPE_ELECTRIC] = { { MOVE_SPARK,         MOVE_THUNDER_PUNCH, MOVE_WILD_CHARGE }, { MOVE_SHOCK_WAVE,    MOVE_THUNDERBOLT,  MOVE_THUNDER } },
    [TYPE_FIRE]     = { { MOVE_FLAME_WHEEL,   MOVE_FIRE_PUNCH,   MOVE_FLARE_BLITZ },  { MOVE_INCINERATE,    MOVE_FLAMETHROWER, MOVE_FIRE_BLAST } },
    [TYPE_NORMAL]   = { { MOVE_HEADBUTT,      MOVE_BODY_SLAM,    MOVE_DOUBLE_EDGE },  { MOVE_SWIFT,         MOVE_HYPER_VOICE,  MOVE_BOOMBURST } },
    [TYPE_FLYING]   = { { MOVE_WING_ATTACK,   MOVE_DRILL_PECK,   MOVE_BRAVE_BIRD },   { MOVE_AIR_CUTTER,    MOVE_AIR_SLASH,    MOVE_HURRICANE } },
    [TYPE_PSYCHIC]  = { { MOVE_PSYCHO_CUT,    MOVE_ZEN_HEADBUTT, MOVE_PSYCHIC_FANGS }, { MOVE_PSYBEAM,      MOVE_PSYSHOCK,     MOVE_PSYCHIC } },
    [TYPE_WATER]    = { { MOVE_AQUA_JET,      MOVE_WATERFALL,    MOVE_LIQUIDATION },  { MOVE_WATER_PULSE,   MOVE_SURF,         MOVE_HYDRO_PUMP } },
    [TYPE_DARK]     = { { MOVE_BITE,          MOVE_CRUNCH,       MOVE_CRUNCH },       { MOVE_SNARL,         MOVE_DARK_PULSE,   MOVE_DARK_PULSE } },
    [TYPE_GHOST]    = { { MOVE_SHADOW_PUNCH,  MOVE_SHADOW_CLAW,  MOVE_POLTERGEIST },  { MOVE_HEX,           MOVE_SHADOW_BALL,  MOVE_SHADOW_BALL } },
    [TYPE_ICE]      = { { MOVE_ICE_FANG,      MOVE_ICE_PUNCH,    MOVE_ICICLE_CRASH }, { MOVE_ICY_WIND,      MOVE_ICE_BEAM,     MOVE_BLIZZARD } },
    [TYPE_DRAGON]   = { { MOVE_DRAGON_TAIL,   MOVE_DRAGON_CLAW,  MOVE_OUTRAGE },      { MOVE_DRAGON_BREATH, MOVE_DRAGON_PULSE, MOVE_DRACO_METEOR } },
    [TYPE_GRASS]    = { { MOVE_RAZOR_LEAF,    MOVE_SEED_BOMB,    MOVE_POWER_WHIP },   { MOVE_MEGA_DRAIN,    MOVE_ENERGY_BALL,  MOVE_LEAF_STORM } },
    [TYPE_GROUND]   = { { MOVE_BULLDOZE,      MOVE_DIG,          MOVE_EARTHQUAKE },   { MOVE_MUD_SHOT,      MOVE_EARTH_POWER,  MOVE_EARTH_POWER } },
    [TYPE_POISON]   = { { MOVE_POISON_FANG,   MOVE_POISON_JAB,   MOVE_GUNK_SHOT },    { MOVE_ACID,          MOVE_SLUDGE_BOMB,  MOVE_SLUDGE_BOMB } },
    [TYPE_BUG]      = { { MOVE_BUG_BITE,      MOVE_X_SCISSOR,    MOVE_MEGAHORN },     { MOVE_STRUGGLE_BUG,  MOVE_BUG_BUZZ,     MOVE_BUG_BUZZ } },
    [TYPE_STEEL]    = { { MOVE_METAL_CLAW,    MOVE_IRON_HEAD,    MOVE_IRON_HEAD },    { MOVE_MIRROR_SHOT,   MOVE_FLASH_CANNON, MOVE_FLASH_CANNON } },
    [TYPE_FAIRY]    = { { MOVE_PLAY_ROUGH,    MOVE_PLAY_ROUGH,   MOVE_PLAY_ROUGH },   { MOVE_DRAINING_KISS, MOVE_DAZZLING_GLEAM, MOVE_MOONBLAST } },
};

// Adds the boss's "Tera" ace after the party (same level as their strongest mon). Returns the new count.
static u8 TryAddBossAce(struct Pokemon *party, u8 count, u16 trainerNum, u8 specialty)
{
    const struct BossAce *ace = GetBossAce(trainerNum);
    struct Pokemon *mon;
    u32 i, level = 1, species, move, tier, physical, worst = 0, worstPower = 0xFFFF;

    if (ace == NULL || specialty == TYPE_NONE || count >= PARTY_SIZE)
        return count;
    for (i = 0; i < count; i++)
    {
        if (GetMonData(&party[i], MON_DATA_LEVEL) > level)
            level = GetMonData(&party[i], MON_DATA_LEVEL);
    }
    species = GetBossAceSpecies(ace, level);
    mon = &party[count];
    CreateMonWithIVs(mon, species, level, 0, OTID_STRUCT_RANDOM_NO_SHINY, 24); // IVs like the Leaders' aces
    GiveMonInitialMoveset(mon);
    {
        // Like a mon built from trainer data (trainer_util.c): no Dynamax or Tera for the AI, and the
        // team's trainer name and ball
        u8 otName[PLAYER_NAME_LENGTH + 1];
        u32 data = BLOCK_AI_DYNAMAX;
        SetMonData(mon, MON_DATA_DYNAMAX_LEVEL, &data);
        data = TYPE_MYSTERY;
        SetMonData(mon, MON_DATA_TERA_TYPE, &data);
        GetMonData(&party[0], MON_DATA_OT_NAME, otName);
        SetMonData(mon, MON_DATA_OT_NAME, otName);
        data = GetMonData(&party[0], MON_DATA_OT_GENDER);
        SetMonData(mon, MON_DATA_OT_GENDER, &data);
        data = GetMonData(&party[0], MON_DATA_POKEBALL);
        SetMonData(mon, MON_DATA_POKEBALL, &data);
    }

    // The specialty type replaces its second type; it keeps its own first one
    SetGraftedTypeOnMon(mon, 1, specialty);

    // A specialty-type attack in the category its stats favor, in place of its weakest move
    tier = level < 25 ? 0 : (level < 42 ? 1 : 2);
    physical = GetSpeciesBaseStat(species, STAT_ATK) >= GetSpeciesBaseStat(species, STAT_SPATK);
    move = sBossAceMoves[specialty][physical ? 0 : 1][tier];
    if (move != MOVE_NONE && !MonKnowsMove(mon, move))
    {
        for (i = 0; i < MAX_MON_MOVES; i++)
        {
            u32 cur = GetMonData(mon, MON_DATA_MOVE1 + i);
            u32 power = cur == MOVE_NONE ? 0 : GetMovePower(cur);
            if (cur == MOVE_NONE || power < worstPower)
            {
                worst = i;
                worstPower = power;
                if (cur == MOVE_NONE)
                    break;
            }
        }
        SetMonMoveSlot(mon, move, worst);
        RecordGraftMove(&mon->box, move);
    }
    return count + 1;
}

static void RollTrainerGrafts(struct Pokemon *party, u8 count, u16 trainerNum)
{
    u8 graftCount = GetEnemyGraftCount();
    u8 aceSlot = 0;
    bool32 boss = IsBossTrainer(trainerNum);
    bool32 boostAce = GetGraftSetting(GRAFT_SETTING_ENEMY_GRAFTS) == GRAFT_ENEMY_GRAFTS_MORE && boss;
    u8 keepType = boss ? GetPartySpecialtyType(party, count) : TYPE_NONE;

    if (IsUngraftedTrainer(trainerNum))
        return;
    u8 bossAceSlot = PARTY_SIZE;
    if (boss)
    {
        u8 before = count;
        count = TryAddBossAce(party, count, trainerNum, keepType);
        if (count > before)
            bossAceSlot = before;
    }
    BuildNearbyGraftPool();
    if (sGraftDonorCount == 0)
        return;
    // The ace is the highest-level mon (the last one on ties)
    for (u8 i = 0; i < count && i < PARTY_SIZE; i++)
    {
        if (GetMonData(&party[i], MON_DATA_SPECIES) != SPECIES_NONE
         && GetMonData(&party[i], MON_DATA_LEVEL) >= GetMonData(&party[aceSlot], MON_DATA_LEVEL))
            aceSlot = i;
    }
    for (u8 i = 0; i < count && i < PARTY_SIZE; i++)
    {
        if (GetMonData(&party[i], MON_DATA_SPECIES) != SPECIES_NONE)
            ApplyEnemyGrafts(&party[i], graftCount, boostAce && i == aceSlot, i == bossAceSlot ? KEEP_BOTH_TYPES : keepType);
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
