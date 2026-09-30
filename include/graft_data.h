#ifndef GUARD_GRAFT_DATA_H
#include "battle.h"
#define GUARD_GRAFT_DATA_H

#define GRAFT_FLAG_MOVE (1 << 0)
#define GRAFT_FLAG_ABILITY (1 << 1)
#define GRAFT_FLAG_TYPE (1 << 2)
#define GRAFT_FLAG_STAT (1 << 3)
#define GRAFT_FLAG_NATURE (1 << 4)

#define GRAFT_TYPE_NONE 0xFF
#define GRAFT_STAT_NONE 0
#define GRAFT_ABILITY_NONE 0
#define GRAFT_NATURE_NONE 0xFF

u8 GetGraftedType(u8 partySlot, u8 typeIndex);
void SetGraftedType(u8 partySlot, u8 typeIndex, u8 type);
u16 GetGraftedStat(u8 partySlot, u8 statIndex);
void SetGraftedStat(u8 partySlot, u8 statIndex, u16 stat);
u8 GetMonEffectiveBaseStat(u16 species, u8 partySlot, u8 statIndex);
u16 GetGraftedAbility(u8 partySlot);
void SetGraftedAbility(u8 partySlot, u16 ability);
void SetGraftedNature(u8 partySlot, u8 natureId);
void ClearGraftData(u8 partySlot);
void InitGraftData(void);


// Sprite tinting
void ApplyGraftTypePaletteSwap(u16 palOffset, u16 numEntries, u32 type1Color, u32 type2Color, bool8 isDualType);
bool8 GetBattlerGraftedTypeInfo(u8 battler, u8 *effectiveType1, u8 *effectiveType2);
void ApplyBattlerGraftOverrides(u32 battler);
void InheritGrafts(struct BoxPokemon *egg, struct BoxPokemon *parentA, struct BoxPokemon *parentB);
void RecordGraftMove(struct BoxPokemon *boxMon, u16 move);
u8 GetEffectiveTypeFromMon(struct Pokemon *mon, u8 typeIndex);
u8 GetEffectiveTypeFromBoxMon(struct BoxPokemon *boxMon, u8 typeIndex);

#endif // GUARD_GRAFT_DATA_H

u8 GetGraftedTypeFromBoxMon(const struct BoxPokemon *boxMon, u8 typeIndex);
u16 GetGraftedAbilityFromBoxMon(const struct BoxPokemon *boxMon);
u16 GetGraftedStatFromBoxMon(const struct BoxPokemon *boxMon, u8 statIndex);
u8 GetBadgeCount(void);
u8 GetStatGraftCap(void);
u8 CountStatGraftsOnBoxMon(const struct BoxPokemon *boxMon);
bool8 CanApplyStatGraft(const struct BoxPokemon *boxMon, u8 statIndex);
const u8 *GetStatGraftRejectReason(const struct BoxPokemon *boxMon, u8 statIndex);
const u8 *GetGraftRejectReason(const struct BoxPokemon *boxMon, u8 graftMenuType, u8 statIndex);
bool32 IsGraftKindGated(u8 graftMenuType);
u32 CountBudgetGraftsOnBoxMon(const struct BoxPokemon *boxMon);
u32 GetGraftBudget(void);
bool8 IsGraftHusk(const struct BoxPokemon *boxMon);
// Pointer-based setters (work on any Pokemon, not just gPlayerParty)
void SetGraftedStatOnMon(struct Pokemon *mon, u8 statIndex, u16 statValue);
void SetGraftedTypeOnMon(struct Pokemon *mon, u8 typeIndex, u8 type);
void SetGraftedAbilityOnMon(struct Pokemon *mon, u16 ability);
void SetGraftedNatureOnMon(struct Pokemon *mon, u8 natureId);
// Enemy trainer graft system
void BuildNearbyGraftPool(void);
void ApplyTrainerGrafts(struct Pokemon *party, u8 count, u16 trainerNum);
u32 GetEnemyGraftCount(void);
