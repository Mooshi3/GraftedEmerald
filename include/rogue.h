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

#endif // GUARD_ROGUE_H
