#ifndef GUARD_GRAFT_MENU_H
#define GUARD_GRAFT_MENU_H

#include "pokemon.h"

enum GraftMenuType {
    GRAFT_MENU_MOVE,
    GRAFT_MENU_ABILITY,
    GRAFT_MENU_TYPE,
    GRAFT_MENU_STAT,
    GRAFT_MENU_NATURE,
};

struct GraftOption {
    u8 graftType;
    u16 value;
    u8 statIndex;
    u16 statValue;
};

// Global: selected party slot for graft application
extern u8 gGraftSelectedPartySlot;
extern bool8 gGraftLearnMovePending;

void StartGraftMenu(void);
void UpdateGraftMenuCursor(u8 cursorPos);
void ApplyGraftFromMenuToSlot(u8 partySlot);
u8 GetSelectedGraftIndex(void);
void SetSelectedGraftIndex(u8 index);
u8 GetSelectedGraftType(void);
u16 GetSelectedGraftValue(void);
u8 GetSelectedGraftStatIndex(void);
u16 GetSelectedGraftStatValue(void);
const u8 *GetStatNameString(u8 statIndex);
const u8 *GetText_ReplaceType1(void);
const u8 *GetText_ReplaceType2(void);
const u8 *GetText_StatCompare(void);
const u8 *GetText_Apply(void);
const u8 *GetText_MoveLearn(void);
const u8 *GetText_NatureApply(void);
const u8 *GetText_NatureNeutral(void);

#endif // GUARD_GRAFT_MENU_H
