#include "global.h"
#include "battle.h"
#include "battle_pike.h"
#include "battle_pyramid.h"
#include "event_data.h"
#include "graft_challenge.h"
#include "graft_data.h"
#include "graft_settings.h"
#include "item.h"
#include "main.h"
#include "party_menu.h"
#include "pokedex.h"
#include "pokemon.h"
#include "pokemon_storage_system.h"
#include "safari_zone.h"
#include "trainer_hill.h"
#include "constants/items.h"

// ============================================================
// Challenge rules from the new-game settings: Nuzlocke encounters and harvests,
// fainting, battle items and monotype. Graft limits and gates live in graft_data.c.
// ============================================================

// What this wild battle allows. Reset to FREE after every battle, so scripted and
// static battles (which never call Nuzlocke_OnWildEncounter) are always catchable.
static EWRAM_DATA u8 sEncounterState = NUZLOCKE_ENCOUNTER_FREE;

static u32 GetCurrentArea(void)
{
    return gMapHeader.regionMapSectionId;
}

static bool32 GetAreaBit(const u8 *bits, u32 area)
{
    return (bits[area / 8] >> (area % 8)) & 1;
}

static void SetAreaBit(u8 *bits, u32 area)
{
    bits[area / 8] |= 1 << (area % 8);
}

bool32 IsNuzlockeOn(void)
{
    return GetGraftSetting(GRAFT_SETTING_ENCOUNTERS);
}

bool32 IsFamilyCaught(enum Species species)
{
    enum Species base = species, pre;
    enum Species stack[16];
    u32 top = 0;

    while ((pre = GetSpeciesPreEvolution(base)) != SPECIES_NONE && pre != base)
        base = pre;

    stack[top++] = base;
    while (top > 0)
    {
        enum Species current = stack[--top];
        const struct Evolution *evolutions = GetSpeciesEvolutions(current);

        if (GetSetPokedexFlag(SpeciesToNationalPokedexNum(current), FLAG_GET_CAUGHT))
            return TRUE;
        if (evolutions == NULL)
            continue;
        for (u32 i = 0; evolutions[i].method != EVOLUTIONS_END && top < ARRAY_COUNT(stack); i++)
        {
            if (evolutions[i].targetSpecies != SPECIES_NONE)
                stack[top++] = evolutions[i].targetSpecies;
        }
    }
    return FALSE;
}

// Called when a random wild encounter (grass, water, fishing, rock smash) starts.
void Nuzlocke_OnWildEncounter(void)
{
    struct Pokemon *wild = &gParties[B_TRAINER_OPPONENT_A][0];
    enum Species species = GetMonData(wild, MON_DATA_SPECIES);
    u32 area = GetCurrentArea();

    sEncounterState = NUZLOCKE_ENCOUNTER_FREE;
    if (!IsNuzlockeOn() || GetSafariZoneFlag())
        return;

    if (GetGraftSetting(GRAFT_SETTING_SHINY_CLAUSE) && GetMonData(wild, MON_DATA_IS_SHINY))
        return; // shinies are always catchable and don't use up the area

    if (GetAreaBit(gSaveBlock1Ptr->graftCaughtAreas, area))
        sEncounterState = NUZLOCKE_ENCOUNTER_AREA_USED;
    else if (GetGraftSetting(GRAFT_SETTING_DUPES_CLAUSE) && IsFamilyCaught(species))
        sEncounterState = NUZLOCKE_ENCOUNTER_DUPE; // doesn't count; the area stays open
    else
        SetAreaBit(gSaveBlock1Ptr->graftCaughtAreas, area); // this is the area's encounter, caught or not
}

void Nuzlocke_OnBattleEnd(void)
{
    sEncounterState = NUZLOCKE_ENCOUNTER_FREE;
}

u32 Nuzlocke_GetEncounterState(void)
{
    return sEncounterState;
}

static bool32 IsGraftingBall(u16 ball)
{
    switch (ball)
    {
    case ITEM_PATCH_BALL:
    case ITEM_GRAFT_BALL:
    case ITEM_SPLICE_BALL:
        return TRUE;
    default:
        return FALSE;
    }
}

static bool32 CanMonTakeHarvestHere(struct Pokemon *mon)
{
    return GetMonData(mon, MON_DATA_SPECIES) != SPECIES_NONE
        && !GetMonData(mon, MON_DATA_IS_EGG)
        && !IsMonFallen(mon)
        && mon->box.graftHarvestArea != GetCurrentArea();
}

static const u8 sText_NuzlockeAreaUsed[] = _("You already met this area's\nPokémon. It can't be caught!\p");
static const u8 sText_NuzlockeDupe[] = _("You've caught this family before.\nIt can't be caught!\p");
static const u8 sText_HarvestOff[] = _("Grafting from wild Pokémon\nisn't allowed!\p");
static const u8 sText_HarvestUsed[] = _("You've already grafted from\na Pokémon in this area!\p");
static const u8 sText_HarvestTeamUsed[] = _("Your whole team has already\ntaken a graft in this area!\p");

// Returns why this ball can't be thrown under the challenge rules, or NULL.
const u8 *GetChallengeBallBlockReason(u16 ball)
{
    if (gBattleTypeFlags & BATTLE_TYPE_TRAINER)
        return NULL; // the normal "can't catch a trainer's Pokémon" handling applies

    if (IsGraftingBall(ball))
    {
        switch (GetGraftSetting(GRAFT_SETTING_HARVESTS))
        {
        case GRAFT_HARVESTS_NONE:
            return sText_HarvestOff;
        case GRAFT_HARVESTS_ONE:
            if (GetAreaBit(gSaveBlock1Ptr->graftHarvestAreas, GetCurrentArea()))
                return sText_HarvestUsed;
            break;
        case GRAFT_HARVESTS_PER_MON:
            for (u32 i = 0; i < PARTY_SIZE; i++)
            {
                if (CanMonTakeHarvestHere(&gParties[B_TRAINER_PLAYER][i]))
                    return NULL;
            }
            return sText_HarvestTeamUsed;
        }
        return NULL; // a harvest doesn't need the area's catch
    }

    switch (sEncounterState)
    {
    case NUZLOCKE_ENCOUNTER_AREA_USED:
        return sText_NuzlockeAreaUsed;
    case NUZLOCKE_ENCOUNTER_DUPE:
        return sText_NuzlockeDupe;
    default:
        return NULL;
    }
}

// A grafting ball caught its target and the graft menu is opening.
void OnWildHarvest(void)
{
    if (GetGraftSetting(GRAFT_SETTING_HARVESTS) == GRAFT_HARVESTS_ONE)
        SetAreaBit(gSaveBlock1Ptr->graftHarvestAreas, GetCurrentArea());
}

static const u8 sText_HarvestMonUsed[] = _("It already took a graft\nin this area.");

// Harvests per mon: the party member picked to receive a wild graft.
const u8 *GetHarvestTargetRejectReason(struct Pokemon *mon)
{
    if (GetGraftSetting(GRAFT_SETTING_HARVESTS) == GRAFT_HARVESTS_PER_MON && !CanMonTakeHarvestHere(mon))
        return sText_HarvestMonUsed;
    return NULL;
}

void OnHarvestTargetChosen(struct Pokemon *mon)
{
    mon->box.graftHarvestArea = GetCurrentArea();
}

bool32 AreBattleItemsBanned(void)
{
    return GetGraftSetting(GRAFT_SETTING_BATTLE_ITEMS);
}

bool32 IsForceSetMode(void)
{
    return GetGraftSetting(GRAFT_SETTING_SET_MODE);
}

// ============================================================
// Fainting
// ============================================================

bool32 IsMonFallen(struct Pokemon *mon)
{
    return (mon->box.graftFlags & GRAFT_FLAG_FALLEN) != 0;
}

bool32 IsBoxMonFallen(const struct BoxPokemon *boxMon)
{
    return (boxMon->graftFlags & GRAFT_FLAG_FALLEN) != 0;
}

static bool32 DoFaintingRulesApply(void)
{
    if (GetGraftSetting(GRAFT_SETTING_FAINTING) == GRAFT_FAINTING_NORMAL)
        return FALSE;
    if (gBattleTypeFlags & (BATTLE_TYPE_LINK | BATTLE_TYPE_RECORDED | BATTLE_TYPE_FRONTIER | BATTLE_TYPE_FIRST_BATTLE))
        return FALSE;
    if (CurrentBattlePyramidLocation() != PYRAMID_LOCATION_NONE || InBattlePike() || InTrainerHillChallenge())
        return FALSE;
    // A whiteout ends a Nuzlocke run; the game doesn't take the party away on top of that.
    if (gBattleOutcome == B_OUTCOME_LOST || gBattleOutcome == B_OUTCOME_DREW || gBattleOutcome == B_OUTCOME_FORFEITED)
        return FALSE;
    return TRUE;
}

static void ReleasePartyMon(u32 slot)
{
    u16 item = GetMonData(&gParties[B_TRAINER_PLAYER][slot], MON_DATA_HELD_ITEM);
    if (item != ITEM_NONE)
        AddBagItem(item, 1); // held items come back instead of vanishing with the mon
    ZeroMonData(&gParties[B_TRAINER_PLAYER][slot]);
}

// After a battle: applies the Fainting setting to party mons at 0 HP. Returns TRUE if it took
// over the main callback (Auto-splice) and will continue to 'next' itself.
bool32 ApplyFaintingRulesAfterBattle(MainCallback next)
{
    u32 i, fallen = 0;

    if (!DoFaintingRulesApply())
        return FALSE;

    for (i = 0; i < PARTY_SIZE; i++)
    {
        struct Pokemon *mon = &gParties[B_TRAINER_PLAYER][i];
        if (GetMonData(mon, MON_DATA_SPECIES) == SPECIES_NONE || GetMonData(mon, MON_DATA_IS_EGG))
            continue;
        if (GetMonData(mon, MON_DATA_HP) != 0 || IsMonFallen(mon))
            continue;

        if (GetGraftSetting(GRAFT_SETTING_FAINTING) == GRAFT_FAINTING_RELEASE)
        {
            ReleasePartyMon(i);
        }
        else
        {
            mon->box.graftFlags |= GRAFT_FLAG_FALLEN;
            fallen++;
        }
    }
    CompactPartySlots();
    CalculatePlayerPartyCount();

    if (fallen != 0 && GetGraftSetting(GRAFT_SETTING_FAINTING) == GRAFT_FAINTING_AUTO_SPLICE)
        return StartAutoSplice(next);
    return FALSE;
}

// Auto-splice: the first fallen party mon still waiting to be spliced, or PARTY_SIZE.
u32 GetNextAutoSpliceDonor(void)
{
    if (GetGraftSetting(GRAFT_SETTING_FAINTING) != GRAFT_FAINTING_AUTO_SPLICE)
        return PARTY_SIZE;
    for (u32 i = 0; i < PARTY_SIZE; i++)
    {
        struct Pokemon *mon = &gParties[B_TRAINER_PLAYER][i];
        if (GetMonData(mon, MON_DATA_SPECIES) != SPECIES_NONE && IsMonFallen(mon))
            return i;
    }
    return PARTY_SIZE;
}

// ============================================================
// Monotype
// ============================================================

static bool32 MonHasMonotype(struct Pokemon *mon, u32 type)
{
    return GetEffectiveTypeFromMon(mon, 0) == type || GetEffectiveTypeFromMon(mon, 1) == type;
}

// Monotype: can this player party mon battle? If no party mon has the type at all,
// everyone may battle, so a party can never be left with nobody to send out.
bool32 IsMonotypeLegal(struct Pokemon *mon)
{
    u32 type = GetGraftSetting(GRAFT_SETTING_MONOTYPE);
    struct Pokemon *party = gParties[B_TRAINER_PLAYER];

    if (type == TYPE_NONE || mon < party || mon >= party + PARTY_SIZE)
        return TRUE;
    if (MonHasMonotype(mon, type))
        return TRUE;
    for (u32 i = 0; i < PARTY_SIZE; i++)
    {
        if (GetMonData(&party[i], MON_DATA_SPECIES) != SPECIES_NONE
         && !GetMonData(&party[i], MON_DATA_IS_EGG)
         && MonHasMonotype(&party[i], type))
            return FALSE;
    }
    return TRUE;
}

// Monotype: the starter gets the chosen type grafted on as its second type, so the run can begin.
void GraftMonotypeOntoStarter(struct Pokemon *mon)
{
    u32 type = GetGraftSetting(GRAFT_SETTING_MONOTYPE);
    if (type == TYPE_NONE || MonHasMonotype(mon, type))
        return;
    SetGraftedTypeOnMon(mon, 1, type);
}
