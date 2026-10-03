#include "global.h"
#include "rogue.h"
#include "item.h"
#include "script.h"
#include "event_data.h"
#include "malloc.h"
#include "shop_criteria.h"
#include "constants/hold_effects.h"
#include "constants/map_groups.h"

static EWRAM_DATA const u16 *sDynamicShopItemListRef = NULL;

// Remove the UNUSED if you'll use the functions!
static UNUSED bool32 ShopCriteriaByBadgeCount(u32 count);
static UNUSED bool32 ShopCriteriaByFlag(u32 flagId);
static UNUSED bool32 ShopCriteriaByVar(u32 varId, u32 varValue);

// GRAFT: every mart that sells Poké Balls also sells the grafting items, added here so no
// map's list can miss them. Badge gates and Rations filter them later (GetItemBadgeRequirement).
static const u16 sGraftMartItems[] =
{
    ITEM_PATCH_BALL,
    ITEM_GRAFT_BALL,
    ITEM_SPLICE_BALL,
    ITEM_GRAFT_SPLICER,
    ITEM_GRAFT_KNOT,
    ITEM_UNGRAFTER,
    ITEM_ABILITY_CAPSULE, // picks any natural ability, hidden included (5 badges)
};

static bool32 ListHasItem(const u16 *list, u32 count, u16 item)
{
    for (u32 i = 0; i < count; i++)
    {
        if (list[i] == item)
            return TRUE;
    }
    return FALSE;
}

// GRAFT: once the player has the Mega Ring (Mt. Pyre's summit), the battle-item clerk on Lilycove
// Department Store's 3rd floor also sells every Mega Stone.
static bool32 IsMegaStoneMart(const u16 *list, u32 count)
{
    return FlagGet(FLAG_RECEIVED_MEGA_RING)
        && gSaveBlock1Ptr->location.mapGroup == MAP_GROUP(MAP_LILYCOVE_CITY_DEPARTMENT_STORE_3F)
        && gSaveBlock1Ptr->location.mapNum == MAP_NUM(MAP_LILYCOVE_CITY_DEPARTMENT_STORE_3F)
        && ListHasItem(list, count, ITEM_X_ATTACK);
}

static u32 CountMegaStones(void)
{
    u32 n = 0;
    for (u32 item = ITEM_NONE + 1; item < ITEMS_COUNT; item++)
    {
        if (GetItemHoldEffect(item) == HOLD_EFFECT_MEGA_STONE)
            n++;
    }
    return n;
}

void TryBuildDynamicShopItemList(const u16 **ogItemList, u16 *resultingTotal)
{
    sDynamicShopItemListRef = *ogItemList;

    bool32 megaStones = IsMegaStoneMart(*ogItemList, *resultingTotal);
    u16 *list = AllocZeroed((*resultingTotal + ARRAY_COUNT(sGraftMartItems) + (megaStones ? CountMegaStones() : 0) + 1) * sizeof(u16));
    u32 overallIdx = 0, idx = 0;
    bool32 sellsBalls = FALSE;

    while (idx < *resultingTotal)
    {
        enum Item item = sDynamicShopItemListRef[idx];

        if (IsItemShopCriteriaFulfilled(item))
        {
            list[overallIdx] = item;
            overallIdx++;
            if (GetItemPocket(item) == POCKET_POKE_BALLS)
                sellsBalls = TRUE;
        }

        idx++;
    }

    // ROGUE: a run's marts list their own stock (no Graft Knot: there's no breeding in a run)
    if (sellsBalls && !Rogue_IsRunActive())
    {
        for (idx = 0; idx < ARRAY_COUNT(sGraftMartItems); idx++)
        {
            if (!ListHasItem(list, overallIdx, sGraftMartItems[idx]))
                list[overallIdx++] = sGraftMartItems[idx];
        }
    }

    if (megaStones)
    {
        for (u32 item = ITEM_NONE + 1; item < ITEMS_COUNT; item++)
        {
            if (GetItemHoldEffect(item) == HOLD_EFFECT_MEGA_STONE)
                list[overallIdx++] = item;
        }
    }

    list[overallIdx] = ITEM_NONE;

    *ogItemList = list;
    *resultingTotal = overallIdx;
}

void TryFreeDynamicShopItemList(const u16 **ogItemList)
{
    Free((u16 *)*ogItemList);
    *ogItemList = sDynamicShopItemListRef;
}

// Add new Criterias below!

static UNUSED bool32 ShopCriteriaByBadgeCount(u32 count)
{
    u32 badgeCount = 0;

    for (u32 badgeFlag = FLAG_BADGE01_GET; badgeFlag < FLAG_BADGE01_GET + NUM_BADGES; badgeFlag++)
    {
        if (FlagGet(badgeFlag))
            badgeCount++;
    }

    if (badgeCount >= count)
        return TRUE;

    return FALSE;
}

// These two below are somewhat identical to ShopCriteriaByBadgeCount
// but uses only one specific event var/flag check. Useful if you need
// a specific badge flag instead of just the badge total.

static UNUSED bool32 ShopCriteriaByFlag(u32 flagId)
{
    if (FlagGet(flagId))
        return TRUE;

    return FALSE;
}

static UNUSED bool32 ShopCriteriaByVar(u32 varId, u32 varValue)
{
    if (VarGet(varId) >= varValue)
        return TRUE;

    return FALSE;
}
