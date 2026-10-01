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

// GRAFT: a move piece from the Graft Bag is being taught through the forget-a-move flow
// (party_menu.c records the move and uses up the piece when it's learned)
bool8 gGraftLearnMovePending = FALSE;

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

static const u8 *GetStatNameString(u8 statIndex)
{
    if (statIndex >= 6) return sStatName_HP;
    return sStatNames[statIndex];
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
    enum BattlerId enemyBattler;
    u16 species;

    enemyBattler = GetOppositeBattler(battler);
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
