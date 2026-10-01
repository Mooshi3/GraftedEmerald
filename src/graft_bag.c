#include "global.h"
#include "graft_bag.h"
#include "battle_main.h"
#include "graft_data.h"
#include "pokemon.h"
#include "random.h"
#include "item.h"
#include "constants/items.h"
#include "string_util.h"
#include "constants/abilities.h"
#include "constants/moves.h"
#include "constants/characters.h"

// Slot layout: one 4-bit count per possible piece, kinds back to back.
#define STAT_VALUES 256
#define SLOTS_MOVE     MOVES_COUNT
#define SLOTS_ABILITY  ABILITIES_COUNT
#define SLOTS_TYPE     NUMBER_OF_MON_TYPES
#define SLOTS_NATURE   NUM_NATURES
#define SLOTS_STAT     (NUM_STATS * STAT_VALUES)

#define BASE_MOVE      0
#define BASE_ABILITY   (BASE_MOVE + SLOTS_MOVE)
#define BASE_TYPE      (BASE_ABILITY + SLOTS_ABILITY)
#define BASE_NATURE    (BASE_TYPE + SLOTS_TYPE)
#define BASE_STAT      (BASE_NATURE + SLOTS_NATURE)
#define SLOTS_TOTAL    (BASE_STAT + SLOTS_STAT)

STATIC_ASSERT((SLOTS_TOTAL + 1) / 2 <= sizeof(((struct SaveBlock1 *)0)->graftBag), GraftBagDoesNotFitInTheSave);

static const u16 sKindBase[GRAFT_PIECE_KIND_COUNT] = { BASE_MOVE, BASE_ABILITY, BASE_TYPE, BASE_NATURE, BASE_STAT };
static const u16 sKindSlots[GRAFT_PIECE_KIND_COUNT] = { SLOTS_MOVE, SLOTS_ABILITY, SLOTS_TYPE, SLOTS_NATURE, SLOTS_STAT };

// Slot of a piece, or -1 if it isn't a valid piece
static s32 GetPieceSlot(const struct GraftPiece *piece)
{
    u32 index;
    if (piece->kind >= GRAFT_PIECE_KIND_COUNT)
        return -1;
    if (piece->kind == GRAFT_PIECE_STAT)
    {
        if (piece->stat >= NUM_STATS || piece->value == 0 || piece->value >= STAT_VALUES)
            return -1;
        index = piece->stat * STAT_VALUES + piece->value;
    }
    else
    {
        index = piece->value;
        if (index == 0 && piece->kind != GRAFT_PIECE_TYPE && piece->kind != GRAFT_PIECE_NATURE)
            return -1; // MOVE_NONE / ABILITY_NONE aren't pieces
    }
    if (index >= sKindSlots[piece->kind])
        return -1;
    return sKindBase[piece->kind] + index;
}

static void SlotToPiece(u32 kind, u32 index, struct GraftPiece *piece)
{
    piece->kind = kind;
    if (kind == GRAFT_PIECE_STAT)
    {
        piece->stat = index / STAT_VALUES;
        piece->value = index % STAT_VALUES;
    }
    else
    {
        piece->stat = 0;
        piece->value = index;
    }
}

static u32 GetSlotCount(u32 slot)
{
    u8 byte = gSaveBlock1Ptr->graftBag[slot / 2];
    return (slot & 1) ? (byte >> 4) : (byte & 0xF);
}

static void SetSlotCount(u32 slot, u32 count)
{
    u8 *byte = &gSaveBlock1Ptr->graftBag[slot / 2];
    if (slot & 1)
        *byte = (*byte & 0x0F) | (count << 4);
    else
        *byte = (*byte & 0xF0) | count;
}

u32 GetGraftPieceCount(const struct GraftPiece *piece)
{
    s32 slot = GetPieceSlot(piece);
    return slot < 0 ? 0 : GetSlotCount(slot);
}

bool32 AddGraftPiece(const struct GraftPiece *piece)
{
    s32 slot = GetPieceSlot(piece);
    u32 count;
    if (slot < 0)
        return FALSE;
    count = GetSlotCount(slot);
    if (count >= GRAFT_PIECE_MAX_COUNT)
        return FALSE;
    SetSlotCount(slot, count + 1);
    return TRUE;
}

bool32 RemoveGraftPiece(const struct GraftPiece *piece)
{
    s32 slot = GetPieceSlot(piece);
    u32 count;
    if (slot < 0)
        return FALSE;
    count = GetSlotCount(slot);
    if (count == 0)
        return FALSE;
    SetSlotCount(slot, count - 1);
    return TRUE;
}

void ClearGraftBag(void)
{
    memset(gSaveBlock1Ptr->graftBag, 0, sizeof(gSaveBlock1Ptr->graftBag));
}

bool32 IsGraftBagEmpty(void)
{
    u32 i;
    for (i = 0; i < (SLOTS_TOTAL + 1) / 2; i++)
    {
        if (gSaveBlock1Ptr->graftBag[i] != 0)
            return FALSE;
    }
    return TRUE;
}

u32 GetGraftBagPieces(u32 kind, struct GraftPiece *out, u32 maxCount)
{
    u32 i, n = 0;
    if (kind >= GRAFT_PIECE_KIND_COUNT)
        return 0;
    for (i = 0; i < sKindSlots[kind] && n < maxCount; i++)
    {
        if (GetSlotCount(sKindBase[kind] + i) != 0)
            SlotToPiece(kind, i, &out[n++]);
    }
    return n;
}

u32 GetGraftBagKindTotal(u32 kind)
{
    u32 i, n = 0;
    if (kind >= GRAFT_PIECE_KIND_COUNT)
        return 0;
    for (i = 0; i < sKindSlots[kind]; i++)
    {
        if (GetSlotCount(sKindBase[kind] + i) != 0)
            n++;
    }
    return n;
}

static u32 AddPiece(struct GraftPiece *out, u32 n, u32 kind, u32 stat, u32 value)
{
    out[n].kind = kind;
    out[n].stat = stat;
    out[n].value = value;
    if (GetPieceSlot(&out[n]) < 0)
        return n; // not a valid piece (no move, no ability)
    return n + 1;
}

u32 GetMonGraftPieces(struct Pokemon *mon, bool32 isWild, struct GraftPiece *out)
{
    u16 species = GetMonData(mon, MON_DATA_SPECIES);
    u32 n = 0, i;
    u16 ability;
    u8 type1 = GetEffectiveTypeFromMon(mon, 0), type2 = GetEffectiveTypeFromMon(mon, 1);

    for (i = 0; i < MAX_MON_MOVES; i++)
        n = AddPiece(out, n, GRAFT_PIECE_MOVE, 0, GetMonData(mon, MON_DATA_MOVE1 + i));

    // Wild mons never give their hidden ability (a grafted ability is fine)
    ability = mon->box.graftAbility;
    if (ability == ABILITY_NONE && !(isWild && GetMonData(mon, MON_DATA_ABILITY_NUM) == 2))
        ability = GetAbilityBySpecies(species, GetMonData(mon, MON_DATA_ABILITY_NUM));
    n = AddPiece(out, n, GRAFT_PIECE_ABILITY, 0, ability);

    n = AddPiece(out, n, GRAFT_PIECE_TYPE, 0, type1);
    if (type2 != type1)
        n = AddPiece(out, n, GRAFT_PIECE_TYPE, 0, type2);

    n = AddPiece(out, n, GRAFT_PIECE_NATURE, 0, GetMonData(mon, MON_DATA_HIDDEN_NATURE));

    for (i = 0; i < NUM_STATS; i++)
    {
        u32 value = mon->box.graftStatOverrides[i];
        if (value == 0)
            value = GetSpeciesBaseStat(species, i);
        n = AddPiece(out, n, GRAFT_PIECE_STAT, i, value);
    }
    return n;
}

u32 HarvestGraftPieces(struct Pokemon *mon, bool32 isWild, u32 numPieces, struct GraftPiece *out, u32 *lostCount)
{
    struct GraftPiece all[GRAFT_PIECES_PER_MON];
    u32 count = GetMonGraftPieces(mon, isWild, all);
    u32 i, got = 0;

    // Random pieces: shuffle, then take the first numPieces (all different)
    for (i = count; i > 1; i--)
    {
        u32 j = Random() % i;
        struct GraftPiece tmp = all[i - 1];
        all[i - 1] = all[j];
        all[j] = tmp;
    }
    if (numPieces > count)
        numPieces = count;

    *lostCount = 0;
    for (i = 0; i < numPieces; i++)
    {
        if (AddGraftPiece(&all[i]))
            out[got++] = all[i];
        else
            (*lostCount)++;
    }
    return got;
}

static const u8 *const sStatShortNames[NUM_STATS] =
{
    COMPOUND_STRING("HP"),
    COMPOUND_STRING("ATK"),
    COMPOUND_STRING("DEF"),
    COMPOUND_STRING("SPEED"),
    COMPOUND_STRING("SP. ATK"),
    COMPOUND_STRING("SP. DEF"),
};

u8 *GetGraftPieceName(u8 *dest, const struct GraftPiece *piece)
{
    switch (piece->kind)
    {
    case GRAFT_PIECE_MOVE:
        return StringCopy(dest, GetMoveName(piece->value));
    case GRAFT_PIECE_ABILITY:
        return StringCopy(dest, gAbilitiesInfo[piece->value].name);
    case GRAFT_PIECE_TYPE:
        dest = StringCopy(dest, gTypesInfo[piece->value].name);
        return StringCopy(dest, COMPOUND_STRING(" type"));
    case GRAFT_PIECE_NATURE:
        dest = StringCopy(dest, gNaturesInfo[piece->value].name);
        return StringCopy(dest, COMPOUND_STRING(" nature"));
    case GRAFT_PIECE_STAT:
        dest = StringCopy(dest, sStatShortNames[piece->stat]);
        *dest++ = CHAR_SPACE;
        return ConvertIntToDecimalStringN(dest, piece->value, STR_CONV_MODE_LEFT_ALIGN, 3);
    }
    *dest = EOS;
    return dest;
}

void GiveGraftBagIfMissing(void)
{
    if (!CheckBagHasItem(ITEM_GRAFT_BAG, 1))
        AddBagItem(ITEM_GRAFT_BAG, 1);
}
