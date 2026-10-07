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
u32 Rogue_GetShopPrice(u16 item, u32 price);
u32 Rogue_GetItemWorth(u16 item, u32 price);
bool32 Rogue_RollHarvestBonus(void);
u32 Rogue_CountWonSpecies(void);
u32 Rogue_GetLevelCap(void);
u32 Rogue_ScaleExp(u32 level, u32 exp);
u32 Rogue_ScaleTrainerMoney(u16 trainerId, u32 money);
void Rogue_PushTrainerPieces(void);
void Rogue_GiveTrainerPiece(void);

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
void Rogue_OnBattleFinishing(void);
struct GraftPiece;
bool32 Rogue_CanVaultPieces(void);
bool32 Rogue_GetVaultPieceName(u8 *dst);
void Rogue_VaultPiece(const struct GraftPiece *piece);
void Rogue_TakeVaultPiece(void);
u32 Rogue_GetFloorWeather(u32 floor); // a WEATHER_*, or 0xFF for none
u32 Rogue_GetFloorCurse(u32 floor);
bool32 Rogue_CanTeamHitTypes(u32 type1, u32 type2);
bool32 Rogue_IsRoomOpen(void); // tests   // a ROGUE_CURSE_*
void Rogue_NurseHeal(void);
void Rogue_NextAchievement(void);
void Rogue_PushAchievements(void);
void Rogue_GetAchievementText(void);
void Rogue_OnGraftBagClosed(void);
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
void Rogue_BufferCatchBonus(void);
void Rogue_DecorateHome(void);
void Rogue_PushHelpTopics(void);
void Rogue_GetHelpText(void);

#endif // GUARD_ROGUE_H
