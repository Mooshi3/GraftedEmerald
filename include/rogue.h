#ifndef GUARD_ROGUE_H
#define GUARD_ROGUE_H

#include "constants/opponents.h"

struct Trainer;
struct BoxPokemon;

#define ROGUE_ROOMS_PER_FLOOR 10 // rooms before the floor's boss
#define ROGUE_FLOORS_NORMAL   4

bool32 Rogue_IsRunActive(void);
bool32 Rogue_IsTeamFull(void);
void Rogue_OnWhiteOut(void);
void Rogue_InitNewGame(void);
const struct Trainer *Rogue_GetTrainer(u16 trainerId);
u32 Rogue_GetRoomDoors(u8 *doors);
u32 Rogue_GetFloorDonors(u16 *species, u32 count);
u16 Rogue_GetBossTrainerId(void);
u32 Rogue_GetEggMoveMask(struct BoxPokemon *mon, const u16 *eggMoves);
bool32 Rogue_GetPopUpName(u8 *dest);
bool32 Rogue_LivesIn(u16 species, u32 biome);
u32 Rogue_GetUnlockedTutorMoves(u16 species, u16 *moves);

static inline bool32 IsRogueTrainerId(u16 trainerId)
{
    return trainerId == TRAINER_ROGUE_TRAINER || trainerId == TRAINER_ROGUE_BOSS;
}

// Specials (data/maps/Rogue_Room/scripts.inc)
void Rogue_OnTransition(void);
void Rogue_OnLoad(void);
void Rogue_GetRoomIntro(void);
void Rogue_CanLeaveRoom(void);
void Rogue_OpenDoors(void);
void Rogue_WarpToRoom(void);
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
void Rogue_SetMetaProgression(void);
void Rogue_PushChallengeChoices(void);
void Rogue_PushTypeChoices(void);
void Rogue_GetFavorite(void);
void Rogue_SetFavorite(void);
void Rogue_BufferNextFloor(void);
void Rogue_HasTakenPrize(void);
void Rogue_PushPrizeChoices(void);
void Rogue_GivePrize(void);
void Rogue_PushTutorMoves(void);
void Rogue_SelectTutorMove(void);
void Rogue_PushTutorPayments(void);
void Rogue_CanPayTutor(void);
void Rogue_PayTutor(void);

#endif // GUARD_ROGUE_H
