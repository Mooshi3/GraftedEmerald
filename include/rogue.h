#ifndef GUARD_ROGUE_H
#define GUARD_ROGUE_H

#include "constants/opponents.h"

struct Trainer;

#define ROGUE_ROOMS_PER_FLOOR 6 // rooms before the floor's boss
#define ROGUE_FLOORS_NORMAL   4

bool32 Rogue_IsRunActive(void);
bool32 Rogue_IsTeamFull(void);
void Rogue_OnWhiteOut(void);
void Rogue_InitNewGame(void);
const struct Trainer *Rogue_GetTrainer(u16 trainerId);
u32 Rogue_GetRoomDoors(u8 *doors);
u32 Rogue_GetFloorDonors(u16 *species, u32 count);
u16 Rogue_GetBossTrainerId(void);

static inline bool32 IsRogueTrainerId(u16 trainerId)
{
    return trainerId == TRAINER_ROGUE_TRAINER || trainerId == TRAINER_ROGUE_BOSS;
}

// Specials (data/maps/Rogue_Room/scripts.inc)
void Rogue_OnTransition(void);
void Rogue_OnLoad(void);
void Rogue_GetRoomIntro(void);
void Rogue_CanLeaveRoom(void);
void Rogue_TakeDoor(void);
void Rogue_PrepareWildMon(void);
void Rogue_PrepareItemBall(void);
void Rogue_OnBossDefeated(void);
void Rogue_IsTeamOverCap(void);
void Rogue_ReleasePartyMon(void);
void Rogue_PushStarterChoices(void);
void Rogue_GiveChosenStarter(void);
void Rogue_HasHeirloom(void);
void Rogue_PushHeirloomChoices(void);
void Rogue_GiveHeirloom(void);
void Rogue_BufferBP(void);
void Rogue_PushUnlockChoices(void);
void Rogue_DescribeUnlock(void);
void Rogue_BuyUnlock(void);
void Rogue_GiveGiftPieces(void);
void Rogue_OnBattleEnd(void);
void Rogue_FindFainted(void);
void Rogue_PushFaintedChoices(void);
void Rogue_ResolveFainted(void);

#endif // GUARD_ROGUE_H
