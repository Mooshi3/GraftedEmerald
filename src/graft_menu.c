#include "global.h"
#include "graft_menu.h"
#include "battle_util.h"
#include "battle_setup.h"
#include "pokemon.h"
#include "constants/pokeball.h"
#include "graft_data.h"
#include "battle.h"
#include "sound.h"
#include "constants/songs.h"
#include "battle_main.h"
#include "move.h"
#include "text.h"
#include "string_util.h"
#include "strings.h"
#include "menu.h"
#include "task.h"
#include "new_game.h"
#include "event_data.h"
#include "item.h"
#include "party_menu.h"
#include "constants/species.h"
#include "constants/moves.h"
#include "constants/items.h"

// Global variables
u8 gGraftSelectedPartySlot = 0;
u32 gGraftSelectedPartyPersonality = 0;

u8 ResolveGraftTargetSlot(void)
{
    s32 i;
    for (i = 0; i < PARTY_SIZE; i++)
    {
        if (GetMonData(&gParties[B_TRAINER_PLAYER][i], MON_DATA_PERSONALITY) == gGraftSelectedPartyPersonality)
            return (u8)i;
    }
    return gGraftSelectedPartySlot; // fallback
}
bool8 gGraftLearnMovePending = FALSE;

// ============================================================
// Static data
// ============================================================

static struct GraftSourceData sGraftSource;
static struct GraftMenuState sGraftMenu;
static void ResetGraftCategoryOrder(void);

// Legacy compatibility
static u16 sCaughtMonSpecies;
static u8 sSelectedGraftIndex;

static const u8 sStatName_HP[] = _("HP");
static const u8 sStatName_ATK[] = _("ATK");
static const u8 sStatName_DEF[] = _("DEF");
static const u8 sStatName_SPD[] = _("SPD");
static const u8 sStatName_SPA[] = _("SPATK");
static const u8 sStatName_SPD2[] = _("SPDEF");
static const u8 *const sStatNames[] = {
    sStatName_HP, sStatName_ATK, sStatName_DEF,
    sStatName_SPD, sStatName_SPA, sStatName_SPD2
};

// Category names for the menu
static const u8 sText_CategoryAbility[] = _("ABILITY");
static const u8 sText_CategoryMove[] = _("MOVE");
static const u8 sText_CategoryType[] = _("TYPE");
static const u8 sText_CategoryNature[] = _("NATURE");
static const u8 sText_CategoryStat[] = _("STAT");

// Description formats
static const u8 sText_AbilityDesc[] = _("{STR_VAR_1}");
static const u8 sText_MoveDesc[] = _("{STR_VAR_1}");
static const u8 sText_TypeDesc[] = _("{STR_VAR_1}");
static const u8 sText_NatureDesc[] = _("{STR_VAR_1} nature");
static const u8 sText_StatDesc[] = _("{STR_VAR_1}: {STR_VAR_2}");

// Apply/confirm texts
static const u8 sText_AddType[] = _("add {STR_VAR_1} type");
static const u8 sText_GiveAbility[] = _("give {STR_VAR_1}");
static const u8 sText_LearnMove[] = _("learn {STR_VAR_1}");
static const u8 sText_BaseStat[] = _("base {STR_VAR_1}:{STR_VAR_2}");
static const u8 sText_SetNature[] = _("set {STR_VAR_1} nature");
static const u8 sText_ReplaceType1[] = _("replace type1 ({STR_VAR_1})\nwith {STR_VAR_2}?");
static const u8 sText_ReplaceType2[] = _("replace type2 ({STR_VAR_1})\nwith {STR_VAR_2}?");
static const u8 sText_StatCompare[] = _("base {STR_VAR_1}: {STR_VAR_2}>{STR_VAR_3}. Apply?");
static const u8 sText_Apply[] = _("apply?");
static const u8 sText_NatureApply[] = _("{STR_VAR_2}\nreplace {STR_VAR_1}?");

const u8 *GetText_NatureApply(void) { return sText_NatureApply; }

const u8 *GetText_StatCompare(void) { return sText_StatCompare; }
const u8 *GetText_ReplaceType1(void) { return sText_ReplaceType1; }
const u8 *GetText_ReplaceType2(void) { return sText_ReplaceType2; }
const u8 *GetText_MoveLearn(void)
{
    static const u8 sText_MoveLearn[] = _("Teach {STR_VAR_1}?");
    return sText_MoveLearn;
}

const u8 *GetStatNameString(u8 statIndex)
{
    if (statIndex >= 6) return sStatName_HP;
    return sStatNames[statIndex];
}

// ============================================================
// GatherGraftSourceDataBattle — called after catching a wild mon
// Collects ALL available graft data from the caught mon
// ============================================================

void GatherGraftSourceDataBattle(void)
{
    u8 i;
    u16 species = gBattleMons[gBattlerTarget].species;

    sGraftSource.species = species;

    // Ability
    sGraftSource.ability = gBattleMons[gBattlerTarget].ability;
    sGraftSource.hasGraftedAbility = FALSE;

    // Moves
    for (i = 0; i < 4; i++)
        sGraftSource.moves[i] = gBattleMons[gBattlerTarget].moves[i];

    // Types (species base types — wild mons don't have grafts)
    sGraftSource.types[0] = gSpeciesInfo[species].types[0];
    sGraftSource.types[1] = gSpeciesInfo[species].types[1];
    sGraftSource.hasGraftedTypes[0] = FALSE;
    sGraftSource.hasGraftedTypes[1] = FALSE;

    // Nature
    sGraftSource.nature = GetNatureFromPersonality(gBattleMons[gBattlerTarget].personality);
    sGraftSource.hasGraftedNature = FALSE;

    // Stats (base stats from species info)
    sGraftSource.stats[0] = gSpeciesInfo[species].baseHP;
    sGraftSource.stats[1] = gSpeciesInfo[species].baseAttack;
    sGraftSource.stats[2] = gSpeciesInfo[species].baseDefense;
    sGraftSource.stats[3] = gSpeciesInfo[species].baseSpeed;
    sGraftSource.stats[4] = gSpeciesInfo[species].baseSpAttack;
    sGraftSource.stats[5] = gSpeciesInfo[species].baseSpDefense;
    for (i = 0; i < 6; i++)
        sGraftSource.hasGraftedStats[i] = FALSE;

    sCaughtMonSpecies = species;

    // Initialize menu state. The Splicer offers all 5 categories in their natural order;
    // without this it showed whatever order and count the last Graft Ball rolled.
    sGraftMenu.category = 0;
    sGraftMenu.subCursor = 0;
    sGraftMenu.phase = 0;
    ResetGraftCategoryOrder();
}

// ============================================================
// GatherGraftSourceDataPartyMon — called by splicer for a party mon
// Respects existing graft overrides on the source mon
// ============================================================

void GatherGraftSourceDataPartyMon(u8 partySlot)
{
    u8 i;
    struct Pokemon *mon = &gParties[B_TRAINER_PLAYER][partySlot];
    u16 species = GetMonData(mon, MON_DATA_SPECIES);
    u32 personality = GetMonData(mon, MON_DATA_PERSONALITY);

    sGraftSource.species = species;

    // Ability — check graft override first
    {
        u16 ga = GetGraftedAbility(partySlot);
        if (ga != GRAFT_ABILITY_NONE)
        {
            sGraftSource.ability = ga;
            sGraftSource.hasGraftedAbility = TRUE;
        }
        else
        {
            u8 abilityNum = GetMonData(mon, MON_DATA_ABILITY_NUM);
            sGraftSource.ability = GetAbilityBySpecies(species, abilityNum);
            sGraftSource.hasGraftedAbility = FALSE;
        }
    }

    // Moves
    for (i = 0; i < 4; i++)
        sGraftSource.moves[i] = GetMonData(mon, MON_DATA_MOVE1 + i);

    // Types — check graft overrides
    for (i = 0; i < 2; i++)
    {
        u8 gt = GetGraftedType(partySlot, i);
        if (gt != GRAFT_TYPE_NONE)
        {
            sGraftSource.types[i] = gt;
            sGraftSource.hasGraftedTypes[i] = TRUE;
        }
        else
        {
            sGraftSource.types[i] = gSpeciesInfo[species].types[i];
            sGraftSource.hasGraftedTypes[i] = FALSE;
        }
    }

    // Nature — check hidden nature (grafted)
    {
        u8 hiddenNature = GetMonData(mon, MON_DATA_HIDDEN_NATURE);
        if (hiddenNature != NATURE_HARDY)
        {
            sGraftSource.nature = hiddenNature;
            sGraftSource.hasGraftedNature = TRUE;
        }
        else
        {
            sGraftSource.nature = GetNatureFromPersonality(personality);
            sGraftSource.hasGraftedNature = FALSE;
        }
    }

    // Stats — check graft overrides
    {
        u16 gs;
        gs = GetGraftedStat(partySlot, 0);
        sGraftSource.stats[0] = (gs != GRAFT_STAT_NONE) ? gs : gSpeciesInfo[species].baseHP;
        sGraftSource.hasGraftedStats[0] = (gs != GRAFT_STAT_NONE);

        gs = GetGraftedStat(partySlot, 1);
        sGraftSource.stats[1] = (gs != GRAFT_STAT_NONE) ? gs : gSpeciesInfo[species].baseAttack;
        sGraftSource.hasGraftedStats[1] = (gs != GRAFT_STAT_NONE);

        gs = GetGraftedStat(partySlot, 2);
        sGraftSource.stats[2] = (gs != GRAFT_STAT_NONE) ? gs : gSpeciesInfo[species].baseDefense;
        sGraftSource.hasGraftedStats[2] = (gs != GRAFT_STAT_NONE);

        gs = GetGraftedStat(partySlot, 3);
        sGraftSource.stats[3] = (gs != GRAFT_STAT_NONE) ? gs : gSpeciesInfo[species].baseSpeed;
        sGraftSource.hasGraftedStats[3] = (gs != GRAFT_STAT_NONE);

        gs = GetGraftedStat(partySlot, 4);
        sGraftSource.stats[4] = (gs != GRAFT_STAT_NONE) ? gs : gSpeciesInfo[species].baseSpAttack;
        sGraftSource.hasGraftedStats[4] = (gs != GRAFT_STAT_NONE);

        gs = GetGraftedStat(partySlot, 5);
        sGraftSource.stats[5] = (gs != GRAFT_STAT_NONE) ? gs : gSpeciesInfo[species].baseSpDefense;
        sGraftSource.hasGraftedStats[5] = (gs != GRAFT_STAT_NONE);
    }

    sCaughtMonSpecies = species;

    // Initialize menu state. The Splicer offers all 5 categories in their natural order;
    // without this it showed whatever order and count the last Graft Ball rolled.
    sGraftMenu.category = 0;
    sGraftMenu.subCursor = 0;
    sGraftMenu.phase = 0;
    ResetGraftCategoryOrder();
}

// ============================================================
// Accessors
// ============================================================

u16 GetGraftSourceAbility(void) { return sGraftSource.ability; }
u16 GetGraftSourceMove(u8 index)
{
    if (index >= 4) return MOVE_NONE;
    return sGraftSource.moves[index];
}
u8 GetGraftSourceType(u8 slot)
{
    if (slot >= 2) return 0;
    return sGraftSource.types[slot];
}
u8 GetGraftSourceNature(void) { return sGraftSource.nature; }
u16 GetGraftSourceStatValue(u8 statIndex)
{
    if (statIndex >= 6) return 0;
    return sGraftSource.stats[statIndex];
}
u16 GetGraftSourceSpecies(void) { return sCaughtMonSpecies; }

u8 GetSelectedGraftCategory(void) { return sGraftMenu.category; }
void SetSelectedGraftCategory(u8 category) { sGraftMenu.category = category; }
u8 GetGraftMenuNumCategories(void) { return sGraftMenu.numCategories; }
u8 GetGraftMenuSubCursor(void) { return sGraftMenu.subCursor; }
void SetGraftMenuSubCursor(u8 cursor) { sGraftMenu.subCursor = cursor; }

// ============================================================
// Legacy compatibility accessors (used by battle script + splicer)
// These translate the category/subCursor system into the old
// GraftOption-style values for ApplyGraftFromMenuToSlot
// ============================================================

// We keep a single "selected option" struct for legacy compat
static struct GraftOption sSelectedOption;

u8 GetSelectedGraftType(void) { return sSelectedOption.graftType; }
u16 GetSelectedGraftValue(void) { return sSelectedOption.value; }
u8 GetSelectedGraftStatIndex(void) { return sSelectedOption.statIndex; }
u16 GetSelectedGraftStatValue(void) { return sSelectedOption.statValue; }

void SetSelectedGraftFromCategory(void)
{
    switch (sGraftMenu.category)
    {
    case GRAFT_CATEGORY_ABILITY:
        sSelectedOption.graftType = GRAFT_MENU_ABILITY;
        sSelectedOption.value = sGraftSource.ability;
        sSelectedOption.statIndex = 0;
        sSelectedOption.statValue = 0;
        break;
    case GRAFT_CATEGORY_MOVE:
        sSelectedOption.graftType = GRAFT_MENU_MOVE;
        sSelectedOption.value = sGraftSource.moves[sGraftMenu.subCursor];
        sSelectedOption.statIndex = 0;
        sSelectedOption.statValue = 0;
        break;
    case GRAFT_CATEGORY_TYPE:
        sSelectedOption.graftType = GRAFT_MENU_TYPE;
        sSelectedOption.value = sGraftSource.types[sGraftMenu.subCursor];
        sSelectedOption.statIndex = 0;
        sSelectedOption.statValue = 0;
        break;
    case GRAFT_CATEGORY_NATURE:
        sSelectedOption.graftType = GRAFT_MENU_NATURE;
        sSelectedOption.value = sGraftSource.nature;
        sSelectedOption.statIndex = 0;
        sSelectedOption.statValue = 0;
        break;
    case GRAFT_CATEGORY_STAT:
        sSelectedOption.graftType = GRAFT_MENU_STAT;
        sSelectedOption.statIndex = sGraftMenu.subCursor;
        sSelectedOption.statValue = sGraftSource.stats[sGraftMenu.subCursor];
        sSelectedOption.value = sSelectedOption.statValue;
        break;
    }
}

u8 GetSelectedGraftIndex(void) { return sSelectedGraftIndex; }
void SetSelectedGraftIndex(u8 index) { sSelectedGraftIndex = index; }

// Legacy roll accessors (return 0/empty — no longer used for random rolls)
u16 GetGraftRollType(u8 index) { (void)index; return 0xFF; }
u16 GetGraftRollValue(u8 index) { (void)index; return 0; }
u8 GetGraftRollStatIndex(u8 index) { (void)index; return 0; }
u16 GetGraftRollStatValue(u8 index) { (void)index; return 0; }
u16 GetGraftRollSpecies(void) { return sCaughtMonSpecies; }

// ============================================================
// Write category option text for the 5-category menu
// ============================================================

static u32 WriteCategoryOptionName(u8 displayPos, u8 *dest)
{
    u8 category = GetGraftMenuRealCategory(displayPos);
    switch (category)
    {
    case GRAFT_CATEGORY_ABILITY:
        StringCopy(gStringVar1, gAbilitiesInfo[sGraftSource.ability].name);
        StringExpandPlaceholders(dest, COMPOUND_STRING("ABILITY: {STR_VAR_1}"));
        break;
    case GRAFT_CATEGORY_MOVE:
    {
        StringExpandPlaceholders(dest, COMPOUND_STRING("Graft a move"));
        break;
    }
    case GRAFT_CATEGORY_TYPE:
    {
        StringCopy(gStringVar1, gTypesInfo[sGraftSource.types[0]].name);
        if (sGraftSource.types[0] != sGraftSource.types[1])
        {
            StringAppend(gStringVar1, COMPOUND_STRING("/"));
            StringAppend(gStringVar1, gTypesInfo[sGraftSource.types[1]].name);
        }
        StringExpandPlaceholders(dest, COMPOUND_STRING("Graft a type: {STR_VAR_1}"));
        break;
    }
    case GRAFT_CATEGORY_NATURE:
    {
        StringCopy(gStringVar1, gNaturesInfo[sGraftSource.nature].name);
        if (gNaturesInfo[sGraftSource.nature].statUp != gNaturesInfo[sGraftSource.nature].statDown)
        {
            StringAppend(gStringVar1, COMPOUND_STRING(" +"));
            StringAppend(gStringVar1, GetStatNameString(gNaturesInfo[sGraftSource.nature].statUp));
            StringAppend(gStringVar1, COMPOUND_STRING(" -"));
            StringAppend(gStringVar1, GetStatNameString(gNaturesInfo[sGraftSource.nature].statDown));
        }
        StringExpandPlaceholders(dest, COMPOUND_STRING("NATURE: {STR_VAR_1}"));
    }
        break;
    case GRAFT_CATEGORY_STAT:
        StringExpandPlaceholders(dest, COMPOUND_STRING("Graft a base stat"));
        break;
    }
    return StringLength(dest);
}

// Write the detail text for a submenu (specific move, type slot, or stat)
static u32 WriteSubmenuDetail(u8 category, u8 subCursor, u8 *dest)
{
    switch (category)
    {
    case GRAFT_CATEGORY_MOVE:
    {
        u16 move = sGraftSource.moves[subCursor];
        if (move == MOVE_NONE)
            StringCopy(dest, COMPOUND_STRING("-"));
        else
        {
            StringCopy(gStringVar1, gMovesInfo[move].name);
            StringExpandPlaceholders(dest, COMPOUND_STRING("{STR_VAR_1}?"));
        }
        break;
    }
    case GRAFT_CATEGORY_TYPE:
    {
        u8 t = sGraftSource.types[subCursor];
        StringCopy(gStringVar1, gTypesInfo[t].name);
        if (subCursor == 0)
            StringExpandPlaceholders(dest, COMPOUND_STRING("type1: {STR_VAR_1}"));
        else
            StringExpandPlaceholders(dest, COMPOUND_STRING("type2: {STR_VAR_1}"));
        break;
    }
    case GRAFT_CATEGORY_STAT:
    {
        StringCopy(gStringVar1, sStatNames[subCursor]);
        ConvertIntToDecimalStringN(gStringVar2, sGraftSource.stats[subCursor], STR_CONV_MODE_LEFT_ALIGN, 3);
        StringExpandPlaceholders(dest, COMPOUND_STRING("{STR_VAR_1}: {STR_VAR_2}"));
        break;
    }
    default:
        StringCopy(dest, COMPOUND_STRING("-"));
        break;
    }
    return StringLength(dest);
}


// ============================================================
// StartGraftMenu — called from battle script to begin category menu
// ============================================================


// Category display order (shuffled for Patch/Graft balls)
static EWRAM_DATA u8 sCategoryOrder[GRAFT_CATEGORY_COUNT];

static u8 CategoryToGraftMenuType(u8 category)
{
    switch (category)
    {
    case GRAFT_CATEGORY_ABILITY: return GRAFT_MENU_ABILITY;
    case GRAFT_CATEGORY_MOVE:    return GRAFT_MENU_MOVE;
    case GRAFT_CATEGORY_TYPE:    return GRAFT_MENU_TYPE;
    case GRAFT_CATEGORY_STAT:    return GRAFT_MENU_STAT;
    default:                     return GRAFT_MENU_NATURE;
    }
}

static void ShuffleCategoryOrder(void)
{
    u8 i, j, tmp, n = 0;
    u8 gated[GRAFT_CATEGORY_COUNT];
    for (i = GRAFT_CATEGORY_COUNT - 1; i > 0; i--)
    {
        j = Random() % (i + 1);
        tmp = sCategoryOrder[i];
        sCategoryOrder[i] = sCategoryOrder[j];
        sCategoryOrder[j] = tmp;
    }
    // Graft Gates: badge-gated kinds go last, so a Patch/Graft Ball doesn't offer only locked kinds
    for (i = 0, j = 0; i < GRAFT_CATEGORY_COUNT; i++)
    {
        if (IsGraftKindGated(CategoryToGraftMenuType(sCategoryOrder[i])))
            gated[n++] = sCategoryOrder[i];
        else
            sCategoryOrder[j++] = sCategoryOrder[i];
    }
    for (i = 0; i < n; i++)
        sCategoryOrder[j++] = gated[i];
}

static void ResetGraftCategoryOrder(void)
{
    u8 i;
    for (i = 0; i < GRAFT_CATEGORY_COUNT; i++)
        sCategoryOrder[i] = i;
    sGraftMenu.numCategories = GRAFT_CATEGORY_COUNT;
}

u8 GetGraftMenuRealCategory(u8 displayPos)
{
    if (displayPos >= sGraftMenu.numCategories)
        return GRAFT_CATEGORY_COUNT; // invalid
    return sCategoryOrder[displayPos];
}

void StartGraftMenu(void)
{
    u8 i;
    for (i = 0; i < GRAFT_CATEGORY_COUNT; i++)
        sCategoryOrder[i] = i;
    u8 ballId = ItemIdToBallId(gLastUsedItem);
    sGraftMenu.category = 0;
    sGraftMenu.subCursor = 0;
    sGraftMenu.phase = 0;
    if (ballId == BALL_PATCH)
    {
        sGraftMenu.numCategories = 1;
        ShuffleCategoryOrder();
    }
    else if (ballId == BALL_GRAFT)
    {
        sGraftMenu.numCategories = 3;
        ShuffleCategoryOrder();
    }
    else
    {
        sGraftMenu.numCategories = GRAFT_CATEGORY_COUNT; // Splice Ball: all 5
    }
}

// ============================================================
// UpdateGraftMenuCursor — battle version, shows category + description
// ============================================================

void UpdateGraftMenuCursor(u8 cursorPos)
{
    u16 spaceNeeded;
    u8 numSpaces;

    sGraftMenu.category = cursorPos;
    WriteCategoryOptionName(cursorPos, gStringVar4);

    // Newline for indicator
    StringAppend(gStringVar4, COMPOUND_STRING("\n"));

    // Build indicator: UP_ARROW X/5 DOWN_ARROW
    if (cursorPos > 0)
        StringCopy(gStringVar1, COMPOUND_STRING("{UP_ARROW}"));
    else
        StringCopy(gStringVar1, COMPOUND_STRING(" "));
    StringAppend(gStringVar1, COMPOUND_STRING(" "));
    ConvertIntToDecimalStringN(gStringVar2, cursorPos + 1, STR_CONV_MODE_LEFT_ALIGN, 1);
    StringAppend(gStringVar1, gStringVar2);
    StringAppend(gStringVar1, COMPOUND_STRING("/"));
    ConvertIntToDecimalStringN(gStringVar2, sGraftMenu.numCategories, STR_CONV_MODE_LEFT_ALIGN, 1);
    StringAppend(gStringVar1, gStringVar2);
    if (cursorPos < sGraftMenu.numCategories - 1)
        StringAppend(gStringVar1, COMPOUND_STRING("{DOWN_ARROW}"));
    else
        StringAppend(gStringVar1, COMPOUND_STRING(" "));

    // Right-align by padding with spaces
    spaceNeeded = 208 - GetStringWidth(FONT_NORMAL, gStringVar1, 0) - 4;
    numSpaces = spaceNeeded / 6;
    if (numSpaces > 30) numSpaces = 30;
    while (numSpaces--)
        StringAppend(gStringVar4, COMPOUND_STRING(" "));
    StringAppend(gStringVar4, gStringVar1);

    BattlePutTextOnWindow(gStringVar4, B_WIN_MSG);
}

// ============================================================
// UpdateGraftMenuCursorOverworld — party menu version for splicer
// ============================================================

void UpdateGraftMenuCursorOverworld(u8 cursorPos)
{
    sGraftMenu.category = cursorPos;
    WriteCategoryOptionName(cursorPos, gStringVar4);

    // Add pagination indicator
    StringAppend(gStringVar4, COMPOUND_STRING("\n"));
    if (cursorPos > 0)
        StringAppend(gStringVar4, COMPOUND_STRING("{UP_ARROW}"));
    else
        StringAppend(gStringVar4, COMPOUND_STRING(" "));
    StringAppend(gStringVar4, COMPOUND_STRING(" "));
    ConvertIntToDecimalStringN(gStringVar2, cursorPos + 1, STR_CONV_MODE_LEFT_ALIGN, 1);
    StringAppend(gStringVar4, gStringVar2);
    StringAppend(gStringVar4, COMPOUND_STRING("/"));
    ConvertIntToDecimalStringN(gStringVar2, sGraftMenu.numCategories, STR_CONV_MODE_LEFT_ALIGN, 1);
    StringAppend(gStringVar4, gStringVar2);
    if (cursorPos < sGraftMenu.numCategories - 1)
        StringAppend(gStringVar4, COMPOUND_STRING("{DOWN_ARROW}"));
    else
        StringAppend(gStringVar4, COMPOUND_STRING(" "));
}

// ============================================================
// Apply the selected graft to a specific party slot
// ============================================================

bool8 ApplyGraftFromMenuToSlot(u8 partySlot)
{
    SetSelectedGraftFromCategory();
    switch (sSelectedOption.graftType)
    {
    case GRAFT_MENU_MOVE:
        // Both graft flows handle moves before getting here (with a forget-a-move prompt when
        // the mon knows 4). Never overwrite a move silently: only fill an empty slot.
        if (GiveMoveToMon(&gParties[B_TRAINER_PLAYER][partySlot], sSelectedOption.value) == MON_HAS_MAX_MOVES)
            return FALSE;
        RecordGraftMove(&gParties[B_TRAINER_PLAYER][partySlot].box, sSelectedOption.value);
        break;
    case GRAFT_MENU_ABILITY:
        SetGraftedAbility(partySlot, sSelectedOption.value);
        break;
    case GRAFT_MENU_TYPE:
        SetGraftedType(partySlot, sGraftMenu.subCursor, (u8)sSelectedOption.value);
        break;
    case GRAFT_MENU_STAT:
        if (!CanApplyStatGraft(&gParties[B_TRAINER_PLAYER][partySlot].box, sSelectedOption.statIndex))
            return FALSE;
        SetGraftedStat(partySlot, sSelectedOption.statIndex, sSelectedOption.statValue);
        break;
    case GRAFT_MENU_NATURE:
        SetGraftedNature(partySlot, sSelectedOption.value);
        break;
    }
    return TRUE;
}






// ============================================================
// Enemy Graft Viewer - R button in move select
// Uses B_WIN_MOVE_DESCRIPTION (same window as L button move info)
// ============================================================

static void EnemyGraftViewerDrawPage(struct BoxPokemon *boxMon, u16 species, u8 graftFlags, u8 page);
static EWRAM_DATA bool8 sEnemyGraftViewerOpen;
static EWRAM_DATA u8 sEnemyGraftViewerPage;
static EWRAM_DATA u8 sEnemyGraftViewerFlags;

void ShowEnemyGrafts(enum BattlerId battler)
{
    struct BoxPokemon *boxMon;
    u8 graftFlags;
    u8 partySlot;
    enum BattlerId enemyBattler;
    u16 species;

    enemyBattler = GetOppositeBattler(battler);
    partySlot = gBattlerPartyIndexes[enemyBattler];
    boxMon = &GetBattlerMon(enemyBattler)->box;
    graftFlags = boxMon->graftFlags;
    species = GetBoxMonData(boxMon, MON_DATA_SPECIES, NULL);

    sEnemyGraftViewerOpen = TRUE;
    sEnemyGraftViewerFlags = graftFlags;

    // Find first page that actually has grafts
    {
        u8 firstPage = 0;
        while (firstPage < 5)
        {
            if ((firstPage == 0 && (graftFlags & GRAFT_FLAG_TYPE))
             || (firstPage == 1 && (graftFlags & GRAFT_FLAG_ABILITY))
             || (firstPage == 2 && (graftFlags & GRAFT_FLAG_MOVE))
             || (firstPage == 3 && (graftFlags & GRAFT_FLAG_STAT))
             || (firstPage == 4 && (graftFlags & GRAFT_FLAG_NATURE)))
                break;
            firstPage++;
        }
        sEnemyGraftViewerPage = firstPage;
    }

    LoadMessageBoxAndBorderGfx();
    FillWindowPixelBuffer(B_WIN_MOVE_DESCRIPTION, PIXEL_FILL(0));
    DrawStdWindowFrame(B_WIN_MOVE_DESCRIPTION, FALSE);

    EnemyGraftViewerDrawPage(boxMon, species, graftFlags, sEnemyGraftViewerPage);

    BattlePutTextOnWindow(gStringVar4, B_WIN_MOVE_DESCRIPTION);
    CopyWindowToVram(B_WIN_MOVE_DESCRIPTION, COPYWIN_FULL);
}

void EnemyGraftViewerNextPage(struct BoxPokemon *boxMon, u16 species)
{
    u8 graftFlags = sEnemyGraftViewerFlags;
    u8 page = sEnemyGraftViewerPage + 1;
    u8 tried = 0;

    while (tried < 5)
    {
        if (page >= 5)
            page = 0;
        if ((page == 0 && (graftFlags & GRAFT_FLAG_TYPE))
         || (page == 1 && (graftFlags & GRAFT_FLAG_ABILITY))
         || (page == 2 && (graftFlags & GRAFT_FLAG_MOVE))
         || (page == 3 && (graftFlags & GRAFT_FLAG_STAT))
         || (page == 4 && (graftFlags & GRAFT_FLAG_NATURE)))
            break;
        page++;
        tried++;
    }
    if (tried >= 5)
        return;

    sEnemyGraftViewerPage = page;
    FillWindowPixelBuffer(B_WIN_MOVE_DESCRIPTION, PIXEL_FILL(0));
    DrawStdWindowFrame(B_WIN_MOVE_DESCRIPTION, FALSE);
    EnemyGraftViewerDrawPage(boxMon, species, graftFlags, page);
    BattlePutTextOnWindow(gStringVar4, B_WIN_MOVE_DESCRIPTION);
    CopyWindowToVram(B_WIN_MOVE_DESCRIPTION, COPYWIN_FULL);
}

static void EnemyGraftViewerDrawPage(struct BoxPokemon *boxMon, u16 species, u8 graftFlags, u8 page)
{
    u8 *ptr = &gStringVar4[0];
    u8 i;
    u16 moveId;

    // Compute pagination
    u8 graftCount = 0, pageIndex = 0, lastGraftPage = 0xFF;
    for (i = 0; i < 5; i++)
    {
        bool8 has = FALSE;
        if (i == 0 && (graftFlags & GRAFT_FLAG_TYPE)) has = TRUE;
        else if (i == 1 && (graftFlags & GRAFT_FLAG_ABILITY)) has = TRUE;
        else if (i == 2 && (graftFlags & GRAFT_FLAG_MOVE)) has = TRUE;
        else if (i == 3 && (graftFlags & GRAFT_FLAG_STAT)) has = TRUE;
        else if (i == 4 && (graftFlags & GRAFT_FLAG_NATURE)) has = TRUE;
        if (has) { graftCount++; if (i == page) pageIndex = graftCount; lastGraftPage = i; }
    }

    if (page == 0 && (graftFlags & GRAFT_FLAG_TYPE))
    {
        ptr = StringCopy(ptr, COMPOUND_STRING("TYPES "));
        ptr = ConvertIntToDecimalStringN(ptr, pageIndex, STR_CONV_MODE_LEFT_ALIGN, 1);
        ptr = StringAppend(ptr, COMPOUND_STRING("/"));
        ptr = ConvertIntToDecimalStringN(ptr, graftCount, STR_CONV_MODE_LEFT_ALIGN, 1);
        if (page == lastGraftPage) ptr = StringAppend(ptr, COMPOUND_STRING(" R:close"));
        else ptr = StringAppend(ptr, COMPOUND_STRING(" R:next"));
        ptr = StringAppend(ptr, gText_NewLine);
        for (i = 0; i < 2; i++)
        {
            if (boxMon->graftTypes[i] != 0xFF)
            {
                ptr = StringAppend(ptr, COMPOUND_STRING("Type "));
                ptr = ConvertIntToDecimalStringN(ptr, i + 1, STR_CONV_MODE_LEFT_ALIGN, 1);
                ptr = StringAppend(ptr, COMPOUND_STRING(": "));
                ptr = StringCopy(ptr, gTypesInfo[boxMon->graftTypes[i]].name);
                ptr = StringAppend(ptr, COMPOUND_STRING(" (was "));
                ptr = StringCopy(ptr, gTypesInfo[gSpeciesInfo[species].types[i]].name);
                ptr = StringAppend(ptr, COMPOUND_STRING(")"));
                ptr = StringAppend(ptr, gText_NewLine);
            }
        }
    }
    else if (page == 1 && (graftFlags & GRAFT_FLAG_ABILITY))
    {
        ptr = StringCopy(ptr, COMPOUND_STRING("ABILITY "));
        ptr = ConvertIntToDecimalStringN(ptr, pageIndex, STR_CONV_MODE_LEFT_ALIGN, 1);
        ptr = StringAppend(ptr, COMPOUND_STRING("/"));
        ptr = ConvertIntToDecimalStringN(ptr, graftCount, STR_CONV_MODE_LEFT_ALIGN, 1);
        if (page == lastGraftPage) ptr = StringAppend(ptr, COMPOUND_STRING(" R:close"));
        else ptr = StringAppend(ptr, COMPOUND_STRING(" R:next"));
        ptr = StringAppend(ptr, gText_NewLine);
        ptr = StringCopy(ptr, gAbilitiesInfo[boxMon->graftAbility].name);
        ptr = StringAppend(ptr, gText_NewLine);
    }
    else if (page == 2 && (graftFlags & GRAFT_FLAG_MOVE))
    {
        ptr = StringCopy(ptr, COMPOUND_STRING("MOVES "));
        ptr = ConvertIntToDecimalStringN(ptr, pageIndex, STR_CONV_MODE_LEFT_ALIGN, 1);
        ptr = StringAppend(ptr, COMPOUND_STRING("/"));
        ptr = ConvertIntToDecimalStringN(ptr, graftCount, STR_CONV_MODE_LEFT_ALIGN, 1);
        if (page == lastGraftPage) ptr = StringAppend(ptr, COMPOUND_STRING(" R:close"));
        else ptr = StringAppend(ptr, COMPOUND_STRING(" R:next"));
        ptr = StringAppend(ptr, gText_NewLine);
        for (i = 0; i < MAX_MON_MOVES; i++)
        {
            moveId = boxMon->graftMoves[i];
            if (moveId != MOVE_NONE)
            {
                ptr = StringCopy(ptr, gMovesInfo[moveId].name);
                ptr = StringAppend(ptr, gText_NewLine);
            }
        }
    }
    else if (page == 3 && (graftFlags & GRAFT_FLAG_STAT))
    {
        ptr = StringCopy(ptr, COMPOUND_STRING("STATS "));
        ptr = ConvertIntToDecimalStringN(ptr, pageIndex, STR_CONV_MODE_LEFT_ALIGN, 1);
        ptr = StringAppend(ptr, COMPOUND_STRING("/"));
        ptr = ConvertIntToDecimalStringN(ptr, graftCount, STR_CONV_MODE_LEFT_ALIGN, 1);
        if (page == lastGraftPage) ptr = StringAppend(ptr, COMPOUND_STRING(" R:close"));
        else ptr = StringAppend(ptr, COMPOUND_STRING(" R:next"));
        ptr = StringAppend(ptr, gText_NewLine);
        for (i = 0; i < 6; i++)
        {
            if (boxMon->graftStatOverrides[i] != 0)
            {
                ptr = StringAppend(ptr, GetStatNameString(i));
                ptr = StringAppend(ptr, COMPOUND_STRING(": "));
                ptr = ConvertIntToDecimalStringN(ptr, (&gSpeciesInfo[species].baseHP)[i], STR_CONV_MODE_LEFT_ALIGN, 3);
                ptr = StringAppend(ptr, COMPOUND_STRING(">"));
                ptr = ConvertIntToDecimalStringN(ptr, boxMon->graftStatOverrides[i], STR_CONV_MODE_LEFT_ALIGN, 3);
                ptr = StringAppend(ptr, gText_NewLine);
            }
        }
    }
    else if (page == 4 && (graftFlags & GRAFT_FLAG_NATURE))
    {
        ptr = StringCopy(ptr, COMPOUND_STRING("NATURE "));
        ptr = ConvertIntToDecimalStringN(ptr, pageIndex, STR_CONV_MODE_LEFT_ALIGN, 1);
        ptr = StringAppend(ptr, COMPOUND_STRING("/"));
        ptr = ConvertIntToDecimalStringN(ptr, graftCount, STR_CONV_MODE_LEFT_ALIGN, 1);
        if (page == lastGraftPage) ptr = StringAppend(ptr, COMPOUND_STRING(" R:close"));
        else ptr = StringAppend(ptr, COMPOUND_STRING(" R:next"));
        ptr = StringAppend(ptr, gText_NewLine);
        ptr = StringCopy(ptr, gNaturesInfo[boxMon->hiddenNatureModifier].name);
        ptr = StringAppend(ptr, gText_NewLine);
    }
    else
    {
        ptr = StringCopy(ptr, COMPOUND_STRING("No grafts. R:close"));
        ptr = StringAppend(ptr, gText_NewLine);
    }
}

void HideEnemyGrafts(void)
{
    if (sEnemyGraftViewerOpen)
    {
        sEnemyGraftViewerOpen = FALSE;
        sEnemyGraftViewerPage = 0;
        if (gCategoryIconSpriteId != 0xFF)
        {
            DestroySprite(&gSprites[gCategoryIconSpriteId]);
            gCategoryIconSpriteId = 0xFF;
        }
        FillWindowPixelBuffer(B_WIN_MOVE_DESCRIPTION, PIXEL_FILL(0));
        ClearStdWindowAndFrame(B_WIN_MOVE_DESCRIPTION, FALSE);
        CopyWindowToVram(B_WIN_MOVE_DESCRIPTION, COPYWIN_GFX);
        PlaySE(SE_SELECT);
    }
}

bool8 IsEnemyGraftViewerOpen(void)
{
    return sEnemyGraftViewerOpen;
}

bool8 IsEnemyGraftViewerOnLastPage(void)
{
    u8 graftFlags = sEnemyGraftViewerFlags;
    u8 currentPage = sEnemyGraftViewerPage;
    u8 lastPage = 0xFF;
    u8 graftCount = 0;
    u8 i;
    for (i = 0; i < 5; i++)
    {
        if ((i == 0 && (graftFlags & GRAFT_FLAG_TYPE))
         || (i == 1 && (graftFlags & GRAFT_FLAG_ABILITY))
         || (i == 2 && (graftFlags & GRAFT_FLAG_MOVE))
         || (i == 3 && (graftFlags & GRAFT_FLAG_STAT))
         || (i == 4 && (graftFlags & GRAFT_FLAG_NATURE)))
        {
            lastPage = i;
            graftCount++;
        }
    }
    if (graftCount == 0)
        return TRUE;
    return (currentPage == lastPage);
}
// Old GenerateGraftRoll stubs (kept for compat, not used anymore)
// ============================================================

// These are no-ops now. The battle script and splicer call
// GatherGraftSourceDataBattle/GatherGraftSourceDataPartyMon instead.

static void GenerateGraftRoll(u16 species) { (void)species; }
void GenerateGraftRollFromPartyMon(u8 partySlot) { (void)partySlot; }
