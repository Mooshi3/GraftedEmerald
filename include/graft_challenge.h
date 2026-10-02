#ifndef GUARD_GRAFT_CHALLENGE_H
#define GUARD_GRAFT_CHALLENGE_H

#include "main.h"

// Challenge rules from the new-game settings (graft_settings.h): Nuzlocke encounters and
// harvests, fainting, battle items, Force Set and monotype.

enum NuzlockeEncounterState
{
    NUZLOCKE_ENCOUNTER_FREE,      // catchable (Nuzlocke off, the area's first encounter, shiny, scripted)
    NUZLOCKE_ENCOUNTER_AREA_USED, // the area's encounter is already spent
    NUZLOCKE_ENCOUNTER_DUPE,      // dupes clause: doesn't count, can't be caught
};

bool32 IsNuzlockeOn(void);
bool32 IsFamilyCaught(enum Species species);
void Nuzlocke_OnWildEncounter(void);
void Nuzlocke_OnBattleEnd(void);
u32 Nuzlocke_GetEncounterState(void);
const u8 *GetChallengeBallBlockReason(u16 ball);
void OnWildHarvest(void);
bool32 AreOverworldWildEncountersOn(void);
bool32 AreRandomWildEncountersOn(void);
bool32 AreBattleItemsBanned(void);
bool32 IsForceSetMode(void);

bool32 IsMonFallen(struct Pokemon *mon);
bool32 IsBoxMonFallen(const struct BoxPokemon *boxMon);
bool32 ApplyFaintingRulesAfterBattle(MainCallback next);
u32 GetNextAutoSpliceDonor(void);

bool32 IsMonotypeLegal(struct Pokemon *mon);
void GraftMonotypeOntoStarter(struct Pokemon *mon);

#endif // GUARD_GRAFT_CHALLENGE_H
