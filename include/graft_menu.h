#ifndef GUARD_GRAFT_MENU_H
#define GUARD_GRAFT_MENU_H

#include "constants/pokemon.h"

// Graft menu types (legacy â€” used by ApplyGraftFromMenuToSlot and battle script)
#define GRAFT_MENU_TYPE     0
#define GRAFT_MENU_ABILITY  1
#define GRAFT_MENU_MOVE     2
#define GRAFT_MENU_STAT     3
#define GRAFT_MENU_NATURE   4

// Graft category types (new 5-category pick system)
#define GRAFT_CATEGORY_ABILITY  0
#define GRAFT_CATEGORY_MOVE     1
#define GRAFT_CATEGORY_TYPE     2
#define GRAFT_CATEGORY_NATURE   3
#define GRAFT_CATEGORY_STAT     4
#define GRAFT_CATEGORY_COUNT    5

// Complete graft source data (replaces random sGraftRoll)
struct GraftSourceData {
    u16 species;
    u16 ability;
    u16 moves[4];
    u8 types[2];
    u8 nature;
    u16 stats[6];
    bool8 hasGraftedAbility;
    bool8 hasGraftedTypes[2];
    bool8 hasGraftedStats[6];
    bool8 hasGraftedNature;
};

// Menu state for the category picker
struct GraftMenuState {
    u8 category;        // Currently selected category (0-4)
    u8 subCursor;       // Cursor for submenu (move index, type slot, stat index)
    u8 phase;           // Which phase of the menu we're in
    u8 confirmCursor;   // Yes/no cursor
};

// Legacy GraftOption struct (used by SetSelectedGraftFromCategory)
struct GraftOption {
    u8 graftType;
    u16 value;
    u8 statIndex;
    u16 statValue;
};

// Global: selected party slot for graft application
extern u8 gGraftSelectedPartySlot;
extern u32 gGraftSelectedPartyPersonality;
extern u8 ResolveGraftTargetSlot(void);
extern bool8 gGraftLearnMovePending;

void GatherGraftSourceDataBattle(void);
void GatherGraftSourceDataPartyMon(u8 partySlot);
void StartGraftMenu(void);
void UpdateGraftMenuCursor(u8 cursorPos);
void SetSelectedGraftFromCategory(void);

u8 GetSelectedGraftCategory(void);
void SetSelectedGraftCategory(u8 category);
u8 GetGraftMenuSubCursor(void);
void SetGraftMenuSubCursor(u8 cursor);

// Accessors for the gathered graft source data
u16 GetGraftSourceAbility(void);
u16 GetGraftSourceMove(u8 index);
u8 GetGraftSourceType(u8 slot);
u8 GetGraftSourceNature(void);
u16 GetGraftSourceStatValue(u8 statIndex);
u16 GetGraftSourceSpecies(void);

// Legacy compatibility accessors
u8 GetSelectedGraftType(void);
u16 GetSelectedGraftValue(void);
u8 GetSelectedGraftStatIndex(void);
u16 GetSelectedGraftStatValue(void);
u8 GetSelectedGraftIndex(void);
void SetSelectedGraftIndex(u8 index);

// Overworld splicer display
void UpdateGraftMenuCursorOverworld(u8 cursorPos);

// Text getters
const u8 *GetText_ReplaceType1(void);
const u8 *GetText_ReplaceType2(void);
const u8 *GetText_MoveLearn(void);
const u8 *GetText_NatureApply(void);
const u8 *GetText_StatCompare(void);
const u8 *GetStatNameString(u8 statIndex);

// Apply
bool8 ApplyGraftFromMenuToSlot(u8 partySlot);

// Legacy roll accessors (stubs for compat)
u16 GetGraftRollType(u8 index);
u16 GetGraftRollValue(u8 index);
u8 GetGraftRollStatIndex(u8 index);
u16 GetGraftRollStatValue(u8 index);
u16 GetGraftRollSpecies(void);

#endif // GUARD_GRAFT_MENU_H
