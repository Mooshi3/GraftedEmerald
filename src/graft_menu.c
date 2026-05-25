#include "global.h"
#include "graft_data.h"
#include "graft_menu.h"
#include "pokemon.h"
#include "battle.h"
#include "battle_script_commands.h"
#include "battle_setup.h"
#include "battle_controllers.h"
#include "string_util.h"
#include "text.h"
#include "window.h"
#include "sound.h"
#include "party_menu.h"
#include "constants/species.h"
#include "constants/moves.h"
#include "constants/abilities.h"
#include "constants/pokemon.h"
#include "constants/songs.h"
#include "constants/party_menu.h"

u8 gGraftSelectedPartySlot = 0;
bool8 gGraftLearnMovePending = FALSE;

#define GRAFT_WEIGHT_MOVE 35
#define GRAFT_WEIGHT_NATURE 25
#define GRAFT_WEIGHT_ABILITY 15
#define GRAFT_WEIGHT_STAT 15
#define GRAFT_WEIGHT_TYPE 10



static struct GraftOption sGraftRoll[4];
static u16 sCaughtMonSpecies;
static u8 sSelectedGraftIndex;  // Which graft the player picked (0-3)

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

// sText_Skip removed - B button cancels instead
static const u8 sText_AddType[] = _("add {STR_VAR_1} type");
static const u8 sText_GiveAbility[] = _("give {STR_VAR_1} ability");
static const u8 sText_LearnMove[] = _("learn {STR_VAR_1}");
static const u8 sText_BaseStat[] = _("base {STR_VAR_1}:{STR_VAR_2}");
static const u8 sText_SetNature[] = _("set {STR_VAR_1} nature");
static const u8 sText_TypeSlot[] = _("replace type {STR_VAR_1}?");
static const u8 sText_ReplaceType1[] = _("replace {STR_VAR_1} with {STR_VAR_2}? type1");
static const u8 sText_ReplaceType2[] = _("replace {STR_VAR_1} with {STR_VAR_2}? type2");
static const u8 sText_StatCompare[] = _("base {STR_VAR_1}: {STR_VAR_2}>{STR_VAR_3}. Apply?");
static const u8 sText_Apply[] = _("apply?");
static const u8 sText_NatureApply[] = _("{STR_VAR_2}\nreplace {STR_VAR_1}?");
const u8 *GetText_NatureApply(void) { return sText_NatureApply; }

static const u8 sText_NatureNeutral[] = _("{STR_VAR_2}\nreplace {STR_VAR_1}?");
const u8 *GetText_NatureNeutral(void) { return sText_NatureNeutral; }

static const u8 sText_MoveLearn[] = _("learn {STR_VAR_1}?");

static u8 PickRandomGraftType(void)
{
    u8 roll = RandomUniform(RNG_BALLTHROW_SHAKE, 0, 99);
    if (roll < GRAFT_WEIGHT_MOVE)
        return GRAFT_MENU_MOVE;
    roll -= GRAFT_WEIGHT_MOVE;
    if (roll < GRAFT_WEIGHT_NATURE)
        return GRAFT_MENU_NATURE;
    roll -= GRAFT_WEIGHT_NATURE;
    if (roll < GRAFT_WEIGHT_ABILITY)
        return GRAFT_MENU_ABILITY;
    roll -= GRAFT_WEIGHT_ABILITY;
    if (roll < GRAFT_WEIGHT_STAT)
        return GRAFT_MENU_STAT;
    return GRAFT_MENU_TYPE;
}

// Write a single graft option's name into dest, returns length
static u32 WriteGraftOptionName(u8 index, u8 *dest)
{
    struct GraftOption *g = &sGraftRoll[index];

    switch (g->graftType)
    {
    case GRAFT_MENU_TYPE:
        StringCopy(gStringVar1, gTypesInfo[g->value].name);
        StringExpandPlaceholders(dest, sText_AddType);
        break;
    case GRAFT_MENU_ABILITY:
        StringCopy(gStringVar1, gAbilitiesInfo[g->value].name);
        StringExpandPlaceholders(dest, sText_GiveAbility);
        break;
    case GRAFT_MENU_MOVE:
        StringCopy(gStringVar1, gMovesInfo[g->value].name);
        StringExpandPlaceholders(dest, sText_LearnMove);
        break;
    case GRAFT_MENU_STAT:
        StringCopy(gStringVar1, sStatNames[g->statIndex]);
        ConvertIntToDecimalStringN(gStringVar2, g->statValue, STR_CONV_MODE_LEFT_ALIGN, 3);
        StringExpandPlaceholders(dest, sText_BaseStat);
        break;
    case GRAFT_MENU_NATURE:
        StringCopy(gStringVar1, gNaturesInfo[g->value].name);
        StringExpandPlaceholders(dest, sText_SetNature);
        break;
    }

    return StringLength(dest);
}

static void GenerateGraftRoll(u16 species)
{
    u8 i;
    bool8 hasAbility = FALSE;  // max 1 ability per roll
    bool8 hasNature = FALSE;   // max 1 nature per roll
    u16 usedMoves[4];          // dedup moves
    u8 usedMoveCount = 0;

    // Get the caught mon's actual nature and ability
    u8 caughtNature = GetNatureFromPersonality(gBattleMons[gBattlerTarget].personality);
    u16 caughtAbility = gBattleMons[gBattlerTarget].ability;

    for (i = 0; i < 4; i++)
    {
        u8 type;
        u8 attempts;

        // Reroll until we get a graft type we haven't maxed out
        for (attempts = 0; attempts < 20; attempts++)
        {
            type = PickRandomGraftType();
            if (type == GRAFT_MENU_ABILITY && hasAbility)
                continue;
            if (type == GRAFT_MENU_NATURE && hasNature)
                continue;
            break;
        }

        sGraftRoll[i].graftType = type;
        sGraftRoll[i].statIndex = 0;
        sGraftRoll[i].statValue = 0;
        sGraftRoll[i].value = 0;

        switch (type)
        {
        case GRAFT_MENU_MOVE:
{
            // Pick a random move, avoiding duplicates already in this roll
            u16 move = MOVE_NONE;
            u8 j;
            // Collect unique moves from the caught mon
            u16 uniqueMoves[4];
            u8 uniqueCount = 0;
            for (j = 0; j < MAX_MON_MOVES; j++)
            {
                u16 m = gBattleMons[gBattlerTarget].moves[j];
                if (m != MOVE_NONE)
                {
                    u8 k;
                    for (k = 0; k < uniqueCount; k++)
                    {
                        if (uniqueMoves[k] == m)
                            break;
                    }
                    if (k == uniqueCount)
                        uniqueMoves[uniqueCount++] = m;
                }
            }
            // Filter out moves already used in this roll
            u8 availableCount = 0;
            u8 availableIdx[4];
            for (j = 0; j < uniqueCount; j++)
            {
                u8 k;
                for (k = 0; k < usedMoveCount; k++)
                {
                    if (usedMoves[k] == uniqueMoves[j])
                        break;
                }
                if (k == usedMoveCount)
                    availableIdx[availableCount++] = j;
            }
            if (availableCount > 0)
            {
                u8 pick = RandomUniform(RNG_BALLTHROW_SHAKE, 0, availableCount - 1);
                move = uniqueMoves[availableIdx[pick]];
                sGraftRoll[i].value = move;
                usedMoves[usedMoveCount++] = move;
            }
            else
            {
                // All unique moves already used - pick a random one anyway
                u8 pick = RandomUniform(RNG_BALLTHROW_SHAKE, 0, (uniqueCount > 0 ? uniqueCount : 1) - 1);
                move = uniqueMoves[pick];
                sGraftRoll[i].value = move;
            }
        }
            break;
        case GRAFT_MENU_ABILITY:
            // Use the caught mon's actual ability
            sGraftRoll[i].value = caughtAbility;
            hasAbility = TRUE;
            break;
        case GRAFT_MENU_TYPE:
            {
                u8 typeSlot = RandomUniform(RNG_BALLTHROW_SHAKE, 0, 1);
                sGraftRoll[i].value = gSpeciesInfo[species].types[typeSlot];
            }
            break;
        case GRAFT_MENU_STAT:
            {
                sGraftRoll[i].statIndex = RandomUniform(RNG_BALLTHROW_SHAKE, 0, 5);
                switch (sGraftRoll[i].statIndex)
                {
                    case 0: sGraftRoll[i].statValue = gSpeciesInfo[species].baseHP; break;
                    case 1: sGraftRoll[i].statValue = gSpeciesInfo[species].baseAttack; break;
                    case 2: sGraftRoll[i].statValue = gSpeciesInfo[species].baseDefense; break;
                    case 3: sGraftRoll[i].statValue = gSpeciesInfo[species].baseSpeed; break;
                    case 4: sGraftRoll[i].statValue = gSpeciesInfo[species].baseSpAttack; break;
                    case 5: sGraftRoll[i].statValue = gSpeciesInfo[species].baseSpDefense; break;
                }
                sGraftRoll[i].value = sGraftRoll[i].statValue;
            }
            break;
        case GRAFT_MENU_NATURE:
            // Use the caught mon's actual nature
            sGraftRoll[i].value = caughtNature;
            hasNature = TRUE;
            break;
        }
    }

    sCaughtMonSpecies = species;
}

// Apply the selected graft to a specific party slot
// Also directly updates gBattleMons for immediate effect in battle
void ApplyGraftFromMenuToSlot(u8 partySlot)
{
    struct GraftOption *g = &sGraftRoll[sSelectedGraftIndex];

    switch (g->graftType)
    {
    case GRAFT_MENU_MOVE:
        ApplyMoveGraft(partySlot, g->value);
        break;
    case GRAFT_MENU_ABILITY:
        SetGraftedAbility(partySlot, g->value);
        break;
    case GRAFT_MENU_TYPE:
        SetGraftedType(partySlot, 0, (u8)g->value);
        break;
    case GRAFT_MENU_STAT:
        SetGraftedStat(partySlot, g->statIndex, g->statValue);
        break;
    case GRAFT_MENU_NATURE:
        SetGraftedNature(partySlot, g->value);
        break;
    }


}

u8 GetSelectedGraftIndex(void)
{
    return sSelectedGraftIndex;
}

void SetSelectedGraftIndex(u8 index)
{
    sSelectedGraftIndex = index;
}

void StartGraftMenu(void)
{
    GenerateGraftRoll(gBattleMons[gBattlerTarget].species);
    // Show first option
    UpdateGraftMenuCursor(0);
}


// Update the message window to show the currently selected graft option
void UpdateGraftMenuCursor(u8 cursorPos)
{
    u8 numSpaces;
    u16 spaceNeeded;

    // Build the graft description
    WriteGraftOptionName(cursorPos, gStringVar4);

    // Newline to put indicator on its own line
    StringAppend(gStringVar4, COMPOUND_STRING("\n"));

    // Build indicator: UP_ARROW 2/4 DOWN_ARROW
    if (cursorPos > 0)
        StringCopy(gStringVar1, COMPOUND_STRING("{UP_ARROW}"));
    else
        StringCopy(gStringVar1, COMPOUND_STRING(" "));

    StringAppend(gStringVar1, COMPOUND_STRING(" "));
    ConvertIntToDecimalStringN(gStringVar2, cursorPos + 1, STR_CONV_MODE_LEFT_ALIGN, 1);
    StringAppend(gStringVar1, gStringVar2);
    StringAppend(gStringVar1, COMPOUND_STRING("/4"));

    if (cursorPos < 3)
        StringAppend(gStringVar1, COMPOUND_STRING("{DOWN_ARROW}"));
    else
        StringAppend(gStringVar1, COMPOUND_STRING(" "));

    // Right-align by padding with spaces (window is 208px, ~6px per space)
    spaceNeeded = 208 - GetStringWidth(FONT_NORMAL, gStringVar1, 0) - 4;
    numSpaces = spaceNeeded / 6;
    if (numSpaces > 30) numSpaces = 30;
    while (numSpaces--)
        StringAppend(gStringVar4, COMPOUND_STRING(" "));

    StringAppend(gStringVar4, gStringVar1);

    // Single BattlePutTextOnWindow call handles everything
    BattlePutTextOnWindow(gStringVar4, B_WIN_MSG);
}

u8 GetSelectedGraftType(void)
{
    return sGraftRoll[sSelectedGraftIndex].graftType;
}

// Accessor for selected graft's value (type id, stat index, etc)
u16 GetSelectedGraftValue(void)
{
    return sGraftRoll[sSelectedGraftIndex].value;
}

// Accessor for selected graft's stat index
u8 GetSelectedGraftStatIndex(void)
{
    return sGraftRoll[sSelectedGraftIndex].statIndex;
}

// Accessor for selected graft's stat value
u16 GetSelectedGraftStatValue(void)
{
    return sGraftRoll[sSelectedGraftIndex].statValue;
}

// Get the stat name string for a stat index
const u8 *GetStatNameString(u8 statIndex)
{
    return sStatNames[statIndex];
}

// Get text strings for sub-interactions
const u8 *GetText_ReplaceType1(void) { return sText_ReplaceType1; }
const u8 *GetText_ReplaceType2(void) { return sText_ReplaceType2; }
const u8 *GetText_StatCompare(void) { return sText_StatCompare; }
const u8 *GetText_Apply(void) { return sText_Apply; }
const u8 *GetText_MoveLearn(void) { return sText_MoveLearn; }
