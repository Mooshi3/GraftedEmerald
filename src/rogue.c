// Grafted Rogue: runs of generated rooms. Every room is the one map Rogue_Room, rebuilt from a seed
// (the run's seed mixed with the floor and room number), so the same room comes out the same after a
// battle or a reload. A room's plan (trees, grass, doors, objects) is recomputed from the seed when
// it's needed instead of being stored. See docs/rogue/DESIGN.md.
#include "global.h"
#include "rogue.h"
#include "battle.h"
#include "battle_setup.h"
#include "data.h"
#include "event_data.h"
#include "event_object_movement.h"
#include "fieldmap.h"
#include "graft_bag.h"
#include "graft_challenge.h"
#include "party_menu.h"
#include "graft_settings.h"
#include "item.h"
#include "money.h"
#include "overworld.h"
#include "pokemon.h"
#include "pokemon_storage_system.h"
#include "random.h"
#include "script_menu.h"
#include "malloc.h"
#include "list_menu.h"
#include "script.h"
#include "script_pokemon_util.h"
#include "sound.h"
#include "string_util.h"
#include "constants/battle_frontier.h"
#include "constants/event_object_movement.h"
#include "constants/event_objects.h"
#include "constants/flags.h"
#include "constants/items.h"
#include "constants/metatile_labels.h"
#include "constants/rogue.h"
#include "constants/species.h"
#include "constants/trainer_types.h"
#include "constants/vars.h"

extern const u8 Rogue_Room_EventScript_WildMon[];
extern const u8 Rogue_Room_EventScript_ItemBall[];
extern const u8 Rogue_Room_EventScript_Trainer[];
extern const u8 Rogue_Room_EventScript_Boss[];
extern const u8 Rogue_Room_EventScript_Shop[];
extern const u8 Rogue_Room_EventScript_Nurse[];
extern const u8 Rogue_Room_EventScript_Guide[];
extern const u8 Rogue_Room_EventScript_UnlockShop[];
extern const u8 Rogue_Room_EventScript_GiftPieces[];
extern const u8 Rogue_Room_EventScript_Merchant[];

// The room map: 22x18 tiles, trees in 2x2 blocks. The outer two blocks are forest, the inside is
// 7x5 blocks (tiles 4..17 x 4..13). Doors are openings in the north forest at block columns 3, 5, 7.
#define ROOM_W 22
#define ROOM_H 18
#define BLOCKS_W (ROOM_W / 2)
#define BLOCKS_H (ROOM_H / 2)
#define INNER_BX_MIN 2
#define INNER_BX_MAX 8
#define INNER_BY_MIN 2
#define INNER_BY_MAX 6
#define ENTRANCE_BX (ROGUE_ENTRANCE_X / 2)
#define ENTRANCE_BY (ROGUE_ENTRANCE_Y / 2)
#define CENTER_X 10 // where a room's trainer, boss, clerk or nurse stands
#define CENTER_Y 7
#define NUM_DOORS 3
#define NO_DOOR 0xFF
#define NUM_MARKERS NUM_DOORS // object local IDs 1-3: the signs over the doors
#define MAX_ROOM_OBJECTS 5    // local IDs 4-8: what's in the room
#define NUM_OBJECT_SLOTS (NUM_MARKERS + MAX_ROOM_OBJECTS)

static const u8 sDoorBlockX[NUM_DOORS] = {3, 5, 7};

// Metatiles (gTileset_General), with collision and elevation as Route 101 uses them
#define TILE_TREE(id)  ((id) | (1 << MAPGRID_COLLISION_SHIFT))
#define TILE_GROUND(id) ((id) | (3 << MAPGRID_ELEVATION_SHIFT))
enum
{
    GROUND_GRASS,
    GROUND_TALL_GRASS,
    GROUND_PATH,
};

struct RoomObject
{
    u16 graphicsId;
    u8 x;
    u8 y;
    u8 movementType;
    u8 trainerType;
    u8 sight;
    const u8 *script;
};

struct RoomPlan
{
    bool8 tree[BLOCKS_H][BLOCKS_W];
    bool8 water[BLOCKS_H][BLOCKS_W];
    u8 ground[ROOM_H][ROOM_W];
    u8 doors[NUM_DOORS]; // room type behind each door, or NO_DOOR
    u8 numObjects;
    struct RoomObject objects[MAX_ROOM_OBJECTS];
};

static EWRAM_DATA struct RoomPlan sPlan = {0};
static EWRAM_DATA struct Trainer sTrainers[2] = {0};
static EWRAM_DATA struct TrainerMon sTrainerMons[2][PARTY_SIZE] = {0};
static EWRAM_DATA u32 sTrainerKeys[2] = {0};

// Trainers for Trainer rooms: their name, class, picture and AI come from a vanilla trainer
static const struct { u16 trainerId; u16 graphicsId; } sTrainerKinds[] =
{
    {TRAINER_CALVIN_1, OBJ_EVENT_GFX_YOUNGSTER},
    {TRAINER_TIANA,    OBJ_EVENT_GFX_LASS},
    {TRAINER_DAVIS,    OBJ_EVENT_GFX_BUG_CATCHER},
    {TRAINER_DREW,     OBJ_EVENT_GFX_CAMPER},
    {TRAINER_AUTUMN,   OBJ_EVENT_GFX_PICNICKER},
    {TRAINER_ZANDER,   OBJ_EVENT_GFX_BLACK_BELT},
    {TRAINER_LEAH,     OBJ_EVENT_GFX_HEX_MANIAC},
    {TRAINER_MELISSA,  OBJ_EVENT_GFX_BEAUTY},
    {TRAINER_YASU,     OBJ_EVENT_GFX_NINJA_BOY},
    {TRAINER_EDWARD,   OBJ_EVENT_GFX_PSYCHIC_M},
    {TRAINER_MARCEL,   OBJ_EVENT_GFX_MAN_3},
    {TRAINER_MARY,     OBJ_EVENT_GFX_WOMAN_5},
    {TRAINER_ANDREW,   OBJ_EVENT_GFX_FISHERMAN},
    {TRAINER_JERRY_1,  OBJ_EVENT_GFX_SCHOOL_KID_M},
    {TRAINER_HUEY,     OBJ_EVENT_GFX_SAILOR},
    {TRAINER_WALTER_1, OBJ_EVENT_GFX_GENTLEMAN},
};

// Floor bosses, in an order shuffled per run
struct BossInfo
{
    u16 trainerId;
    u16 graphicsId;
    enum Type type; // TYPE_NONE: any type (the Champion)
};

static const struct BossInfo sBosses[] =
{
    {TRAINER_ROXANNE_1,       OBJ_EVENT_GFX_ROXANNE,  TYPE_ROCK},
    {TRAINER_BRAWLY_1,        OBJ_EVENT_GFX_BRAWLY,   TYPE_FIGHTING},
    {TRAINER_WATTSON_1,       OBJ_EVENT_GFX_WATTSON,  TYPE_ELECTRIC},
    {TRAINER_FLANNERY_1,      OBJ_EVENT_GFX_FLANNERY, TYPE_FIRE},
    {TRAINER_NORMAN_1,        OBJ_EVENT_GFX_NORMAN,   TYPE_NORMAL},
    {TRAINER_WINONA_1,        OBJ_EVENT_GFX_WINONA,   TYPE_FLYING},
    {TRAINER_TATE_AND_LIZA_1, OBJ_EVENT_GFX_TATE,     TYPE_PSYCHIC},
    {TRAINER_JUAN_1,          OBJ_EVENT_GFX_JUAN,     TYPE_WATER},
};

// After the last floor: the Champion (Normal), or the Elite Four and then the Champion (Hard)
static const struct BossInfo sFinaleNormal[] =
{
    {TRAINER_WALLACE, OBJ_EVENT_GFX_WALLACE, TYPE_NONE},
};

static const struct BossInfo sFinaleHard[] =
{
    {TRAINER_SIDNEY,  OBJ_EVENT_GFX_SIDNEY,  TYPE_DARK},
    {TRAINER_PHOEBE,  OBJ_EVENT_GFX_PHOEBE,  TYPE_GHOST},
    {TRAINER_GLACIA,  OBJ_EVENT_GFX_GLACIA,  TYPE_ICE},
    {TRAINER_DRAKE,   OBJ_EVENT_GFX_DRAKE,   TYPE_DRAGON},
    {TRAINER_WALLACE, OBJ_EVENT_GFX_WALLACE, TYPE_NONE},
};

// What doors lead to, by weight
static const u8 sDoorWeights[ROGUE_ROOM_TYPE_COUNT] =
{
    [ROGUE_ROOM_WILD] = 30,
    [ROGUE_ROOM_TRAINER] = 26,
    [ROGUE_ROOM_ITEM] = 16,
    [ROGUE_ROOM_SHOP] = 8,
    [ROGUE_ROOM_REST] = 8,
    [ROGUE_ROOM_MYSTERY] = 12,
};

static const struct { u16 item; u8 quantity; u8 weight; u8 minFloor; } sItemBallItems[] =
{
    {ITEM_POKE_BALL,      2, 20, 1},
    {ITEM_GREAT_BALL,     2,  8, 2},
    {ITEM_PATCH_BALL,     2, 14, 1},
    {ITEM_GRAFT_BALL,     1, 12, 1},
    {ITEM_SPLICE_BALL,    1,  5, 1},
    {ITEM_GRAFT_SPLICER,  1,  4, 1},
    {ITEM_POTION,         2, 10, 1},
    {ITEM_SUPER_POTION,   2,  8, 2},
    {ITEM_REVIVE,         1,  6, 1},
    {ITEM_RARE_CANDY,     1,  6, 1},
    {ITEM_ABILITY_CAPSULE, 1, 2, 2},
    {ITEM_LEFTOVERS,      1,  2, 1},
    {ITEM_CHOICE_BAND,    1,  2, 2},
    {ITEM_CHOICE_SPECS,   1,  2, 2},
    {ITEM_LIFE_ORB,       1,  2, 2},
    {ITEM_FOCUS_SASH,     1,  2, 1},
    {ITEM_EVIOLITE,       1,  2, 1},
    {ITEM_SITRUS_BERRY,   1,  3, 1},
};

// Meta-progression: unlocks bought with BP in the start room, kept between runs (VAR_ROGUE_UNLOCKS)
enum
{
    UNLOCK_STARTER_CHOICE,
    UNLOCK_TEAM_4,
    UNLOCK_TEAM_5,
    UNLOCK_TEAM_6,
    UNLOCK_MORE_BALLS,
    UNLOCK_REVIVES,
    UNLOCK_LAST_RITES_1,
    UNLOCK_START_PIECE,
    UNLOCK_LAST_RITES_2,
    UNLOCK_LAST_RITES_3,
    UNLOCK_EXP_PARTY,
    UNLOCK_SWITCH_STYLE,
    UNLOCK_GOOD_GENES,
    UNLOCK_PEDIGREE,
    UNLOCK_SHOP_COUNT,
    UNLOCK_HARD_MODE = 15, // not bought: beating a Normal run opens it
};

static const struct
{
    const u8 *name;
    const u8 *description;
    u16 cost;
    s8 requires;
} sUnlocks[UNLOCK_SHOP_COUNT] =
{
    [UNLOCK_STARTER_CHOICE] = {COMPOUND_STRING("Starter choice"), COMPOUND_STRING("Choose your starter from three."), 30, -1},
    [UNLOCK_TEAM_4] = {COMPOUND_STRING("4th team slot"), COMPOUND_STRING("Your team can hold 4 Pokémon."), 40, -1},
    [UNLOCK_TEAM_5] = {COMPOUND_STRING("5th team slot"), COMPOUND_STRING("Your team can hold 5 Pokémon."), 70, UNLOCK_TEAM_4},
    [UNLOCK_TEAM_6] = {COMPOUND_STRING("6th team slot"), COMPOUND_STRING("Your team can hold 6 Pokémon."), 100, UNLOCK_TEAM_5},
    [UNLOCK_MORE_BALLS] = {COMPOUND_STRING("Ball pouch"), COMPOUND_STRING("Start with 3 more Poké Balls\nand 2 more Patch Balls."), 20, -1},
    [UNLOCK_REVIVES] = {COMPOUND_STRING("First aid"), COMPOUND_STRING("Start with 2 Revives."), 30, -1},
    [UNLOCK_LAST_RITES_1] = {COMPOUND_STRING("Last rites I"), COMPOUND_STRING("Once a floor, a fainted Pokémon can\nbe broken down into graft pieces."), 60, -1},
    [UNLOCK_LAST_RITES_2] = {COMPOUND_STRING("Last rites II"), COMPOUND_STRING("Last rites twice a floor."), 90, UNLOCK_LAST_RITES_1},
    [UNLOCK_LAST_RITES_3] = {COMPOUND_STRING("Last rites III"), COMPOUND_STRING("Last rites three times a floor."), 120, UNLOCK_LAST_RITES_2},
    [UNLOCK_EXP_PARTY] = {COMPOUND_STRING("Team training"), COMPOUND_STRING("The whole team gets EXP from\nevery battle."), 40, -1},
    [UNLOCK_GOOD_GENES] = {COMPOUND_STRING("Good genes"), COMPOUND_STRING("More Pokémon have an egg move to\nrelearn (35% instead of 10%)."), 40, -1},
    [UNLOCK_PEDIGREE] = {COMPOUND_STRING("Pedigree"), COMPOUND_STRING("Every Pokémon has an egg move to\nrelearn, some have two."), 80, UNLOCK_GOOD_GENES},
    [UNLOCK_SWITCH_STYLE] = {COMPOUND_STRING("Switch style"), COMPOUND_STRING("You may switch Pokémon when a\nfoe faints (Shift battle style)."), 20, -1},
    [UNLOCK_START_PIECE] = {COMPOUND_STRING("Heirloom"), COMPOUND_STRING("Pick one of three graft pieces\nto start each run with."), 50, -1},
};

#define RUN_FLAG_HARD     (1 << 0)
#define RUN_FLAG_INVERSE  (1 << 1) // challenge: Inverse battles
#define RUN_FLAG_MONOTYPE (1 << 2) // challenge: Monotype (the type is in the Monotype setting)
#define RUN_FLAG_CHALLENGE (RUN_FLAG_INVERSE | RUN_FLAG_MONOTYPE)

// Last rites used this floor: VAR_ROGUE_RUN_FLAGS bits 8-11
#define RUN_FLAG_RITES_SHIFT 8
#define RUN_FLAG_RITES_MASK  (0xF << RUN_FLAG_RITES_SHIFT)

// A fainted Pokémon the player chose to keep (waiting for a Revive): not asked about again
#define GRAFT_FLAG_ROGUE_KEPT (1 << 6)

static void PushChoice(const u8 *name, u32 id);
static void SetLastRitesUsed(u32 used);

static bool32 IsMetaOn(void)
{
    return GetGraftSetting(GRAFT_SETTING_ROGUE_META);
}

// With Unlocks Off, everything is open
static bool32 HasUnlock(u32 id)
{
    return !IsMetaOn() || ((VarGet(VAR_ROGUE_UNLOCKS) >> id) & 1);
}

// ---------------------------------------------------------------------------------------------
// Run state

bool32 Rogue_IsRunActive(void)
{
    return VarGet(VAR_ROGUE_STATE) == ROGUE_STATE_RUN;
}

static u32 GetFloor(void)
{
    return VarGet(VAR_ROGUE_FLOOR);
}

static u32 GetRoom(void)
{
    return VarGet(VAR_ROGUE_ROOM);
}

static u32 GetRoomType(void)
{
    return VarGet(VAR_ROGUE_ROOM_TYPE) & 0xFF;
}

#define ROOM_TYPE_FROM_MYSTERY 0x100 // in VAR_ROGUE_ROOM_TYPE: this room was behind a "?" door

// Events (only behind "?" doors)
enum
{
    EVENT_GIFT,     // a stranger hands over graft pieces
    EVENT_RARE,     // one wild Pokémon from a stronger pool
    EVENT_MERCHANT, // a shop with rarer stock
    EVENT_COUNT,
};

static u32 Mix32(u32 x);
static u32 GetRoomSeed(void);

static u32 GetEventKind(void)
{
    return Mix32(GetRoomSeed() ^ 0xE7E7) % EVENT_COUNT;
}

static u32 GetTeamSlots(void)
{
    u32 slots = VarGet(VAR_ROGUE_TEAM_SLOTS);
    return slots ? slots : 3;
}

static u32 GetStartingTeamSlots(void)
{
    return 3 + HasUnlock(UNLOCK_TEAM_4) + HasUnlock(UNLOCK_TEAM_5) + HasUnlock(UNLOCK_TEAM_6);
}

bool32 Rogue_IsTeamFull(void)
{
    return Rogue_IsRunActive() && CalculatePlayerPartyCount() >= GetTeamSlots();
}

// After a catch past the team cap: someone has to go (the script asks who)
void Rogue_IsTeamOverCap(void)
{
    gSpecialVar_Result = Rogue_IsRunActive() && CalculatePlayerPartyCount() > GetTeamSlots();
}

void Rogue_ReleasePartyMon(void)
{
    if (gSpecialVar_0x8004 >= PARTY_SIZE)
        return;
    ReleasePartyMon(gSpecialVar_0x8004);
    CompactPartySlots();
    CalculatePlayerPartyCount();
    UpdateFollowingPokemon();
}

static u32 Mix32(u32 x)
{
    x ^= x >> 16;
    x *= 0x7FEB352D;
    x ^= x >> 15;
    x *= 0x846CA68B;
    x ^= x >> 16;
    return x;
}

static u32 GetRunSeed(void)
{
    return VarGet(VAR_ROGUE_SEED_LO) | (VarGet(VAR_ROGUE_SEED_HI) << 16);
}

static u32 GetRoomSeed(void)
{
    if (VarGet(VAR_ROGUE_STATE) != ROGUE_STATE_RUN)
        return 0x5EED;
    return Mix32(GetRunSeed() ^ Mix32((GetFloor() << 8) | GetRoom()));
}

// The boss of a floor: the bosses in an order shuffled by the run's seed
static u32 GetFloorBoss(u32 floor)
{
    u8 order[ARRAY_COUNT(sBosses)];
    rng_value_t rng = LocalRandomSeed(Mix32(GetRunSeed() ^ 0xB055));
    u32 i;

    for (i = 0; i < ARRAY_COUNT(order); i++)
        order[i] = i;
    for (i = ARRAY_COUNT(order) - 1; i > 0; i--)
    {
        u32 j = LocalRandom(&rng) % (i + 1);
        u8 temp = order[i];
        order[i] = order[j];
        order[j] = temp;
    }
    return order[(floor - 1) % ARRAY_COUNT(order)];
}

// Hard mode: the full length (8 floors, then the Elite Four). Unlocked by a win (meta-progression).
static bool32 IsHardMode(void)
{
    return VarGet(VAR_ROGUE_RUN_FLAGS) & RUN_FLAG_HARD;
}

static u32 GetNumFloors(void)
{
    return IsHardMode() ? 8 : ROGUE_FLOORS_NORMAL;
}

static bool32 IsFinale(u32 floor)
{
    return floor > GetNumFloors();
}

static u32 GetFinaleLength(void)
{
    return IsHardMode() ? ARRAY_COUNT(sFinaleHard) : ARRAY_COUNT(sFinaleNormal);
}

// The boss fought in a room: the floor's Leader, or the finale's Elite Four / Champion in order
static const struct BossInfo *GetBossAt(u32 floor, u32 room)
{
    if (IsFinale(floor))
    {
        u32 i = min(room, GetFinaleLength()) - 1;
        return IsHardMode() ? &sFinaleHard[i] : &sFinaleNormal[i];
    }
    return &sBosses[GetFloorBoss(floor)];
}

static const struct BossInfo *GetCurrentBoss(void)
{
    return GetBossAt(GetFloor(), GetRoom());
}

// Where the current room's boss door leads
static void GetBossDoorDestination(u32 *floor, u32 *room)
{
    *floor = GetFloor();
    *room = GetRoom() + 1;
    if (GetRoomType() == ROGUE_ROOM_BOSS && *floor == GetNumFloors())
    {
        *floor = GetNumFloors() + 1; // into the finale
        *room = 1;
    }
}

static u32 GetWildLevel(void)
{
    if (IsFinale(GetFloor()))
        return 4 + GetNumFloors() * 9;
    return 4 + (GetFloor() - 1) * 9 + GetRoom();
}

static u32 GetBossLevel(void)
{
    if (IsFinale(GetFloor()))
        return 4 + GetNumFloors() * 9 + 3 + GetRoom();
    return 4 + GetFloor() * 9 + 1;
}

// ---------------------------------------------------------------------------------------------
// Species

static bool32 IsSpeciesAllowed(enum Species species)
{
    const struct SpeciesInfo *info;

    if (!IsSpeciesEnabled(species) || GET_BASE_SPECIES_ID(species) != species)
        return FALSE;
    info = &gSpeciesInfo[species];
    if (info->isRestrictedLegendary || info->isSubLegendary || info->isMythical
     || info->isUltraBeast || info->isParadox)
        return FALSE;
    return TRUE;
}

static enum Species PickSpecies(rng_value_t *rng, u32 minBst, u32 maxBst, bool32 unevolvedOnly, enum Type type)
{
    u32 tries;

    for (tries = 0; tries < 2000; tries++)
    {
        enum Species species = 1 + LocalRandom(rng) % NATIONAL_DEX_COUNT;
        u32 bst;

        if (!IsSpeciesAllowed(species))
            continue;
        bst = GetSpeciesBaseStatTotal(species);
        if (bst < minBst || bst > maxBst)
            continue;
        if (unevolvedOnly && GetSpeciesPreEvolution(species) != SPECIES_NONE)
            continue;
        if (type != TYPE_NONE && GetSpeciesType(species, 0) != type && GetSpeciesType(species, 1) != type)
            continue;
        return species;
    }
    return SPECIES_ZIGZAGOON;
}

static void GetFloorBstRange(u32 floor, u32 *minBst, u32 *maxBst)
{
    static const u16 sMax[] = {330, 420, 470, 520, 560, 600};
    static const u16 sMin[] = {  0, 250, 300, 350, 380, 400};
    u32 i = min(floor, ARRAY_COUNT(sMax)) - 1;
    *minBst = sMin[i];
    *maxBst = sMax[i];
}

static enum Species PickFloorSpecies(rng_value_t *rng, enum Type type)
{
    u32 minBst, maxBst;
    GetFloorBstRange(GetFloor(), &minBst, &maxBst);
    if (type != TYPE_NONE)
        maxBst += 40;
    return PickSpecies(rng, minBst, maxBst, GetFloor() == 1 && type == TYPE_NONE, type);
}

// Enemy-graft donors: species of the current floor's pool, fixed per room
u32 Rogue_GetFloorDonors(u16 *species, u32 count)
{
    rng_value_t rng = LocalRandomSeed(GetRoomSeed() ^ 0xD0D0);
    u32 i;
    for (i = 0; i < count; i++)
        species[i] = PickFloorSpecies(&rng, TYPE_NONE);
    return count;
}

// A rare Pokémon: from two floors ahead's pool
static enum Species PickRareSpecies(rng_value_t *rng)
{
    u32 minBst, maxBst;
    GetFloorBstRange(min(GetFloor() + 2, 6), &minBst, &maxBst);
    return PickSpecies(rng, minBst, maxBst, FALSE, TYPE_NONE);
}

// A starter: not evolved, evolves, and no stronger than the first-stage starters
static enum Species PickStarter(rng_value_t *rng)
{
    u32 tries;
    for (tries = 0; tries < 2000; tries++)
    {
        enum Species species = PickSpecies(rng, 0, 320, TRUE, TYPE_NONE);
        const struct Evolution *evos = GetSpeciesEvolutions(species);
        if (evos != NULL && evos[0].method != EVOLUTIONS_END)
            return species;
    }
    return SPECIES_ZIGZAGOON;
}

// ---------------------------------------------------------------------------------------------
// Room plans

static bool32 IsTreeBlock(s32 bx, s32 by)
{
    if (bx < 0 || bx >= BLOCKS_W || by < 0 || by >= BLOCKS_H)
        return TRUE;
    return sPlan.tree[by][bx];
}

static bool32 IsWaterBlock(s32 bx, s32 by)
{
    if (bx < 0 || bx >= BLOCKS_W || by < 0 || by >= BLOCKS_H)
        return FALSE;
    return sPlan.water[by][bx];
}

static bool32 IsWaterTile(s32 x, s32 y)
{
    return x >= 0 && y >= 0 && IsWaterBlock(x / 2, y / 2);
}

static bool32 IsTreeTile(s32 x, s32 y)
{
    if (x < 0 || x >= ROOM_W || y < 0 || y >= ROOM_H)
        return TRUE;
    return sPlan.tree[y / 2][x / 2];
}

static bool32 AreDoorsReachable(void)
{
    u8 seen[BLOCKS_H][BLOCKS_W] = {0};
    u8 queue[BLOCKS_W * BLOCKS_H];
    u32 head = 0, tail = 0, i;

    seen[ENTRANCE_BY][ENTRANCE_BX] = TRUE;
    queue[tail++] = ENTRANCE_BY * BLOCKS_W + ENTRANCE_BX;
    while (head < tail)
    {
        static const s8 sDirs[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
        u32 bx = queue[head] % BLOCKS_W, by = queue[head] / BLOCKS_W;
        head++;
        for (i = 0; i < 4; i++)
        {
            s32 nx = bx + sDirs[i][0], ny = by + sDirs[i][1];
            if (IsTreeBlock(nx, ny) || IsWaterBlock(nx, ny) || seen[ny][nx])
                continue;
            seen[ny][nx] = TRUE;
            queue[tail++] = ny * BLOCKS_W + nx;
        }
    }
    for (i = 0; i < NUM_DOORS; i++)
    {
        if (sPlan.doors[i] != NO_DOOR && !seen[INNER_BY_MIN][sDoorBlockX[i]])
            return FALSE;
    }
    return TRUE;
}

static u32 PickWeighted(rng_value_t *rng, const u8 *weights, u32 count, u32 excludeMask)
{
    u32 total = 0, i, roll;
    for (i = 0; i < count; i++)
    {
        if (!(excludeMask & (1u << i)))
            total += weights[i];
    }
    if (total == 0)
        return 0;
    roll = LocalRandom(rng) % total;
    for (i = 0; i < count; i++)
    {
        if (excludeMask & (1u << i))
            continue;
        if (roll < weights[i])
            return i;
        roll -= weights[i];
    }
    return 0;
}

static void ChooseDoors(rng_value_t *rng)
{
    u32 type = GetRoomType();
    u32 i, count, used = 0;

    for (i = 0; i < NUM_DOORS; i++)
        sPlan.doors[i] = NO_DOOR;

    if (VarGet(VAR_ROGUE_STATE) != ROGUE_STATE_RUN)
    {
        sPlan.doors[1] = ROGUE_ROOM_START;
        return;
    }
    if (IsFinale(GetFloor()) && GetRoom() >= GetFinaleLength())
    {
        sPlan.doors[1] = ROGUE_ROOM_HUB; // the Champion is beaten: the run is won
        return;
    }
    if ((type == ROGUE_ROOM_BOSS && GetFloor() >= GetNumFloors())
     || (type != ROGUE_ROOM_BOSS && GetRoom() >= ROGUE_ROOMS_PER_FLOOR))
    {
        sPlan.doors[1] = ROGUE_ROOM_BOSS;
        return;
    }

    count = 2 + LocalRandom(rng) % 2;
    for (i = 0; i < count; i++)
    {
        u32 door = PickWeighted(rng, sDoorWeights, ROGUE_ROOM_TYPE_COUNT, used);
        used |= 1u << door;
        if (count == 2)
            sPlan.doors[i * 2] = door;
        else
            sPlan.doors[i] = door;
    }
}

static bool32 IsTileFree(u32 x, u32 y)
{
    u32 i;
    if (IsTreeTile(x, y) || IsWaterTile(x, y))
        return FALSE;
    if (x == ROGUE_ENTRANCE_X && y >= ROGUE_ENTRANCE_Y - 1)
        return FALSE;
    for (i = 0; i < sPlan.numObjects; i++)
    {
        if (sPlan.objects[i].x == x && sPlan.objects[i].y == y)
            return FALSE;
    }
    return TRUE;
}

static void AddObject(u16 graphicsId, u32 x, u32 y, u8 movementType, const u8 *script)
{
    struct RoomObject *obj;
    if (sPlan.numObjects >= MAX_ROOM_OBJECTS)
        return;
    obj = &sPlan.objects[sPlan.numObjects++];
    obj->graphicsId = graphicsId;
    obj->x = x;
    obj->y = y;
    obj->movementType = movementType;
    obj->trainerType = TRAINER_TYPE_NONE;
    obj->sight = 0;
    obj->script = script;
}

static void AddObjectAnywhere(rng_value_t *rng, u16 graphicsId, u8 movementType, const u8 *script)
{
    u32 tries;
    for (tries = 0; tries < 100; tries++)
    {
        u32 x = 4 + LocalRandom(rng) % 14;
        u32 y = 5 + LocalRandom(rng) % 8;
        if (IsTileFree(x, y))
        {
            AddObject(graphicsId, x, y, movementType, script);
            return;
        }
    }
}

static u32 GetTrainerKind(void)
{
    return Mix32(GetRoomSeed() ^ 0x7A1E) % ARRAY_COUNT(sTrainerKinds);
}

static void BuildRoomPlan(void)
{
    rng_value_t rng = LocalRandomSeed(GetRoomSeed());
    u32 type = GetRoomType();
    bool32 hasCenterObject = (type == ROGUE_ROOM_TRAINER || type == ROGUE_ROOM_BOSS
                           || type == ROGUE_ROOM_SHOP || type == ROGUE_ROOM_REST || type == ROGUE_ROOM_EVENT);
    u32 bx, by, x, y, i, count;

    memset(&sPlan, 0, sizeof(sPlan));
    ChooseDoors(&rng);

    // Forest around the room, with an opening for each door
    for (by = 0; by < BLOCKS_H; by++)
    {
        for (bx = 0; bx < BLOCKS_W; bx++)
        {
            if (bx < INNER_BX_MIN || bx > INNER_BX_MAX || by < INNER_BY_MIN || by > INNER_BY_MAX)
                sPlan.tree[by][bx] = TRUE;
        }
    }
    for (i = 0; i < NUM_DOORS; i++)
    {
        if (sPlan.doors[i] != NO_DOOR)
        {
            sPlan.tree[0][sDoorBlockX[i]] = FALSE;
            sPlan.tree[1][sDoorBlockX[i]] = FALSE;
            for (y = 0; y < INNER_BY_MIN * 2; y++)
            {
                sPlan.ground[y][sDoorBlockX[i] * 2] = GROUND_PATH;
                sPlan.ground[y][sDoorBlockX[i] * 2 + 1] = GROUND_PATH;
            }
        }
    }

    // Sometimes a pond (1x1 to 2x2 blocks), never right under a door and with grass below it
    if (type != ROGUE_ROOM_HUB && LocalRandom(&rng) % 100 < 40)
    {
        u32 w = 1 + LocalRandom(&rng) % 2, h = 1 + LocalRandom(&rng) % 2;
        u32 bx0 = INNER_BX_MIN + LocalRandom(&rng) % (INNER_BX_MAX - INNER_BX_MIN + 2 - w);
        u32 by0 = INNER_BY_MIN + 1 + LocalRandom(&rng) % (INNER_BY_MAX - INNER_BY_MIN - h);
        bool32 ok = TRUE;
        for (by = by0; by < by0 + h; by++)
        {
            for (bx = bx0; bx < bx0 + w; bx++)
            {
                if ((bx == ENTRANCE_BX && by >= ENTRANCE_BY - 1) || (hasCenterObject && bx == CENTER_X / 2))
                    ok = FALSE;
                sPlan.water[by][bx] = TRUE;
            }
        }
        if (!ok || !AreDoorsReachable())
            memset(sPlan.water, 0, sizeof(sPlan.water));
    }

    // A few trees inside, never closing off a door
    if (type != ROGUE_ROOM_HUB)
    {
        count = 2 + LocalRandom(&rng) % 4;
        for (i = 0; i < count * 4 && count > 0; i++)
        {
            bx = INNER_BX_MIN + LocalRandom(&rng) % (INNER_BX_MAX - INNER_BX_MIN + 1);
            by = INNER_BY_MIN + LocalRandom(&rng) % (INNER_BY_MAX - INNER_BY_MIN + 1);
            if (sPlan.tree[by][bx] || (bx == ENTRANCE_BX && by == ENTRANCE_BY))
                continue;
            if (by == INNER_BY_MIN && (bx == 3 || bx == 5 || bx == 7))
                continue; // right under a door
            if (hasCenterObject && bx == CENTER_X / 2 && by <= CENTER_Y / 2 + 1)
                continue;
            if (sPlan.water[by][bx] || IsWaterBlock(bx, by - 1))
                continue; // not in a pond, nor right below one (its shore)
            sPlan.tree[by][bx] = TRUE;
            if (!AreDoorsReachable())
                sPlan.tree[by][bx] = FALSE;
            else
                count--;
        }
    }

    // Patches of tall grass (just for looks: there are no random encounters here)
    count = 2 + LocalRandom(&rng) % 3;
    for (i = 0; i < count; i++)
    {
        u32 w = 2 + LocalRandom(&rng) % 4, h = 2 + LocalRandom(&rng) % 2;
        u32 x0 = 4 + LocalRandom(&rng) % (14 - w + 1), y0 = 4 + LocalRandom(&rng) % (10 - h + 1);
        for (y = y0; y < y0 + h; y++)
        {
            for (x = x0; x < x0 + w; x++)
                sPlan.ground[y][x] = GROUND_TALL_GRASS;
        }
    }

    // What's in the room
    switch (type)
    {
    case ROGUE_ROOM_HUB:
        AddObject(OBJ_EVENT_GFX_PROF_BIRCH, 13, 9, MOVEMENT_TYPE_FACE_DOWN, Rogue_Room_EventScript_Guide);
        if (IsMetaOn())
            AddObject(OBJ_EVENT_GFX_SCIENTIST_1, 7, 9, MOVEMENT_TYPE_FACE_DOWN, Rogue_Room_EventScript_UnlockShop);
        break;
    case ROGUE_ROOM_WILD:
        count = 1 + (LocalRandom(&rng) % 100 < 35);
        for (i = 0; i < count; i++)
            AddObjectAnywhere(&rng, OBJ_EVENT_MON + PickFloorSpecies(&rng, TYPE_NONE), MOVEMENT_TYPE_LOOK_AROUND, Rogue_Room_EventScript_WildMon);
        break;
    case ROGUE_ROOM_EVENT:
        switch (GetEventKind())
        {
        case EVENT_GIFT:
            AddObject(OBJ_EVENT_GFX_OLD_MAN, CENTER_X, CENTER_Y, MOVEMENT_TYPE_FACE_DOWN, Rogue_Room_EventScript_GiftPieces);
            break;
        case EVENT_RARE:
            AddObjectAnywhere(&rng, OBJ_EVENT_MON + PickRareSpecies(&rng), MOVEMENT_TYPE_LOOK_AROUND, Rogue_Room_EventScript_WildMon);
            break;
        case EVENT_MERCHANT:
            AddObject(OBJ_EVENT_GFX_GENTLEMAN, CENTER_X, CENTER_Y, MOVEMENT_TYPE_FACE_DOWN, Rogue_Room_EventScript_Merchant);
            break;
        }
        break;
    case ROGUE_ROOM_ITEM:
        count = 2 + LocalRandom(&rng) % 2;
        for (i = 0; i < count; i++)
            AddObjectAnywhere(&rng, OBJ_EVENT_GFX_ITEM_BALL, MOVEMENT_TYPE_NONE, Rogue_Room_EventScript_ItemBall);
        break;
    case ROGUE_ROOM_SHOP:
        AddObject(OBJ_EVENT_GFX_MART_EMPLOYEE, CENTER_X, CENTER_Y, MOVEMENT_TYPE_FACE_DOWN, Rogue_Room_EventScript_Shop);
        break;
    case ROGUE_ROOM_REST:
        AddObject(OBJ_EVENT_GFX_NURSE, CENTER_X, CENTER_Y, MOVEMENT_TYPE_FACE_DOWN, Rogue_Room_EventScript_Nurse);
        break;
    case ROGUE_ROOM_TRAINER:
        AddObject(sTrainerKinds[GetTrainerKind()].graphicsId, CENTER_X, CENTER_Y, MOVEMENT_TYPE_FACE_DOWN, Rogue_Room_EventScript_Trainer);
        sPlan.objects[0].trainerType = TRAINER_TYPE_NORMAL;
        sPlan.objects[0].sight = 5;
        break;
    case ROGUE_ROOM_BOSS:
        AddObject(GetCurrentBoss()->graphicsId, CENTER_X, CENTER_Y, MOVEMENT_TYPE_FACE_DOWN, Rogue_Room_EventScript_Boss);
        sPlan.objects[0].trainerType = TRAINER_TYPE_NORMAL;
        sPlan.objects[0].sight = 5;
        break;
    }
}

// For tests: the current room's doors, and whether every one is reachable from the entrance
u32 Rogue_GetRoomDoors(u8 *doors)
{
    u32 i, count = 0;
    BuildRoomPlan();
    for (i = 0; i < NUM_DOORS; i++)
    {
        doors[i] = sPlan.doors[i];
        if (doors[i] != NO_DOOR)
            count++;
    }
    return AreDoorsReachable() ? count : 0;
}

static u16 GetDoorMarkerGraphics(u32 doorType)
{
    switch (doorType)
    {
    case ROGUE_ROOM_WILD:    return OBJ_EVENT_MON + SPECIES_ZIGZAGOON;
    case ROGUE_ROOM_TRAINER: return OBJ_EVENT_GFX_YOUNGSTER;
    case ROGUE_ROOM_ITEM:    return OBJ_EVENT_GFX_ITEM_BALL;
    case ROGUE_ROOM_SHOP:    return OBJ_EVENT_GFX_MART_EMPLOYEE;
    case ROGUE_ROOM_REST:    return OBJ_EVENT_GFX_NURSE;
    case ROGUE_ROOM_MYSTERY: return OBJ_EVENT_MON + SPECIES_UNOWN_QUESTION;
    case ROGUE_ROOM_BOSS:
    {
        u32 floor, room;
        GetBossDoorDestination(&floor, &room);
        return GetBossAt(floor, room)->graphicsId;
    }
    case ROGUE_ROOM_START:   return OBJ_EVENT_GFX_ITEM_BALL;
    case ROGUE_ROOM_HUB:     return OBJ_EVENT_GFX_PROF_BIRCH;
    }
    return OBJ_EVENT_GFX_BOY_1;
}

static void SetObjectTemplate(u32 slot, u16 graphicsId, u32 x, u32 y, u8 movementType, u8 trainerType, u8 sight, const u8 *script)
{
    struct ObjectEventTemplate *template = &gSaveBlock1Ptr->objectEventTemplates[slot];
    template->graphicsId = graphicsId;
    template->x = x;
    template->y = y;
    template->elevation = 3;
    template->movementType = movementType;
    template->movementRangeX = 1;
    template->movementRangeY = 1;
    template->trainerType = trainerType;
    template->trainerRange_berryTreeId = sight;
    template->script = script;
}

// ON_TRANSITION: place the room's objects before they spawn, hide the unused ones
void Rogue_OnTransition(void)
{
    u32 i;

    BuildRoomPlan();
    ClearTrainerFlag(TRAINER_ROGUE_TRAINER);
    ClearTrainerFlag(TRAINER_ROGUE_BOSS);
    for (i = 0; i < NUM_OBJECT_SLOTS; i++)
    {
        struct ObjectEventTemplate *template = &gSaveBlock1Ptr->objectEventTemplates[i];
        if (i < NUM_MARKERS && sPlan.doors[i] != NO_DOOR)
        {
            SetObjectTemplate(i, GetDoorMarkerGraphics(sPlan.doors[i]), sDoorBlockX[i] * 2, 2,
                              MOVEMENT_TYPE_FACE_DOWN, TRAINER_TYPE_NONE, 0, NULL);
        }
        else if (i >= NUM_MARKERS && i - NUM_MARKERS < sPlan.numObjects)
        {
            const struct RoomObject *obj = &sPlan.objects[i - NUM_MARKERS];
            SetObjectTemplate(i, obj->graphicsId, obj->x, obj->y, obj->movementType, obj->trainerType, obj->sight, obj->script);
        }
        else
        {
            FlagSet(template->flagId);
        }
    }
}

static u16 GetTreeMetatile(s32 x, s32 y)
{
    s32 bx = x / 2, by = y / 2;
    bool32 left = (x % 2) == 0;

    if ((y % 2) == 0) // top half
    {
        bool32 open = !IsTreeBlock(bx, by - 1) && !IsTreeBlock(left ? bx - 1 : bx + 1, by);
        if (left)
            return TILE_TREE(open ? 0x1D6 : 0x1D4);
        return TILE_TREE(open ? 0x1D7 : 0x1D5);
    }
    if (IsTreeBlock(bx, by + 1))
        return TILE_TREE(left ? 0x1DC : 0x1DD);
    if (left)
        return TILE_TREE(IsTreeBlock(bx - 1, by) ? 0x1E4 : 0x1E6);
    return TILE_TREE(IsTreeBlock(bx + 1, by) ? 0x1E5 : 0x1E7);
}

static u16 GetGroundMetatile(s32 x, s32 y)
{
    bool32 left = (x % 2) == 0;
    bool32 treeBelow = IsTreeTile(x, y + 1) && ((y + 1) % 2) == 0 && y + 1 < ROOM_H;

    if (IsWaterTile(x, y - 1))
        return TILE_GROUND(0x002);
    switch (sPlan.ground[y][x])
    {
    case GROUND_PATH:
        if (y == INNER_BY_MIN * 2 - 1)
            return TILE_GROUND(left ? 0x1E0 : 0x1E2);
        return TILE_GROUND(left ? 0x1D8 : 0x1DA);
    case GROUND_TALL_GRASS:
        if (treeBelow)
            return TILE_GROUND(left ? 0x1C6 : 0x1C7);
        return TILE_GROUND(METATILE_General_TallGrass);
    }
    if (treeBelow)
        return TILE_GROUND(left ? 0x1CE : 0x1CF);
    return TILE_GROUND(METATILE_General_Grass);
}

// Ponds as on Route 102: the shore is drawn into the water's top and side tiles, and into the
// grass below
static u16 GetWaterMetatile(s32 x, s32 y)
{
    bool32 top = !IsWaterTile(x, y - 1), left = !IsWaterTile(x - 1, y), right = !IsWaterTile(x + 1, y);
    u16 id;
    if (top)
        id = left ? 0x0B0 : (right ? 0x0B2 : 0x0B1);
    else
        id = left ? 0x0B8 : (right ? 0x0BA : METATILE_General_ReflectiveWater);
    return id | (1 << MAPGRID_ELEVATION_SHIFT);
}

// ON_LOAD (after every warp and battle): draw the room
void Rogue_OnLoad(void)
{
    s32 x, y;

    BuildRoomPlan();
    for (y = 0; y < ROOM_H; y++)
    {
        for (x = 0; x < ROOM_W; x++)
        {
            u16 tile;
            if (IsTreeTile(x, y))
                tile = GetTreeMetatile(x, y);
            else if (IsWaterTile(x, y))
                tile = GetWaterMetatile(x, y);
            else
                tile = GetGroundMetatile(x, y);
            MapGridSetMetatileEntryAt(x + MAP_OFFSET, y + MAP_OFFSET, tile);
        }
    }
}

// ---------------------------------------------------------------------------------------------
// Specials

static const u8 *const sRoomNames[ROGUE_ROOM_TYPE_COUNT] =
{
    [ROGUE_ROOM_WILD] = COMPOUND_STRING("wild Pokémon"),
    [ROGUE_ROOM_TRAINER] = COMPOUND_STRING("a trainer"),
    [ROGUE_ROOM_ITEM] = COMPOUND_STRING("items"),
    [ROGUE_ROOM_SHOP] = COMPOUND_STRING("a shop"),
    [ROGUE_ROOM_REST] = COMPOUND_STRING("a place to rest"),
};

static const u8 *const sEventNames[EVENT_COUNT] =
{
    [EVENT_GIFT] = COMPOUND_STRING("a generous stranger"),
    [EVENT_RARE] = COMPOUND_STRING("a rare Pokémon"),
    [EVENT_MERCHANT] = COMPOUND_STRING("a traveling merchant"),
};

void Rogue_GetRoomIntro(void)
{
    u32 type = GetRoomType();

    gSpecialVar_Result = FALSE;
    if (!Rogue_IsRunActive())
        return;
    gSpecialVar_Result = TRUE;
    if (type == ROGUE_ROOM_BOSS)
    {
        const struct BossInfo *boss = GetCurrentBoss();
        StringCopy(gStringVar1, GetTrainerNameFromId(boss->trainerId));
        if (boss->trainerId == TRAINER_WALLACE)
            StringExpandPlaceholders(gStringVar4, COMPOUND_STRING("The CHAMPION, {STR_VAR_1}, is waiting!"));
        else if (IsFinale(GetFloor()))
            StringExpandPlaceholders(gStringVar4, COMPOUND_STRING("The ELITE FOUR's {STR_VAR_1}\nis waiting!"));
        else
            StringExpandPlaceholders(gStringVar4, COMPOUND_STRING("{STR_VAR_1} is waiting at the\nend of the floor!"));
    }
    else if (VarGet(VAR_ROGUE_ROOM_TYPE) & ROOM_TYPE_FROM_MYSTERY)
    {
        StringCopy(gStringVar1, type == ROGUE_ROOM_EVENT ? sEventNames[GetEventKind()] : sRoomNames[type]);
        StringExpandPlaceholders(gStringVar4, COMPOUND_STRING("Behind the “?” door: {STR_VAR_1}!"));
    }
    else if (GetRoom() == 1)
    {
        ConvertIntToDecimalStringN(gStringVar1, GetFloor(), STR_CONV_MODE_LEFT_ALIGN, 2);
        StringExpandPlaceholders(gStringVar4, COMPOUND_STRING("FLOOR {STR_VAR_1}"));
    }
    else
    {
        gSpecialVar_Result = FALSE;
    }
}

void Rogue_CanLeaveRoom(void)
{
    u32 type = GetRoomType();

    gSpecialVar_Result = TRUE;
    if (type == ROGUE_ROOM_TRAINER && !HasTrainerBeenFought(TRAINER_ROGUE_TRAINER))
    {
        gSpecialVar_Result = FALSE;
        StringCopy(gStringVar4, COMPOUND_STRING("The trainer won't let you\nthrough without a battle!"));
    }
    else if (type == ROGUE_ROOM_BOSS && !HasTrainerBeenFought(TRAINER_ROGUE_BOSS))
    {
        gSpecialVar_Result = FALSE;
        StringCopy(gStringVar4, COMPOUND_STRING("The way is blocked until\nthe boss is beaten!"));
    }
}

static void SetForcedSettings(void)
{
    // A run allows broken builds and loses fainted Pokémon (docs/rogue/DESIGN.md)
    SetGraftSetting(GRAFT_SETTING_GRAFT_LIMIT, GRAFT_LIMIT_FREE);
    SetGraftSetting(GRAFT_SETTING_GRAFT_GATES, FALSE);
    SetGraftSetting(GRAFT_SETTING_GRAFT_SUPPLY, GRAFT_SUPPLY_SHOP);
    SetGraftSetting(GRAFT_SETTING_HARVESTS, GRAFT_HARVESTS_ANY);
    SetGraftSetting(GRAFT_SETTING_ENCOUNTERS, FALSE);
    SetGraftSetting(GRAFT_SETTING_FAINTING, GRAFT_FAINTING_NORMAL); // fainted Pokémon: Rogue_OnBattleEnd
    SetGraftSetting(GRAFT_SETTING_WILD_SPAWNS, GRAFT_WILD_SPAWNS_HIDDEN);
    SetGraftSetting(GRAFT_SETTING_LEVEL_CAP, GRAFT_LEVEL_CAP_OFF);
    // Set by the run, not chosen on a settings screen (decided with Jacob 2026-10-02)
    SetGraftSetting(GRAFT_SETTING_SHINY_ODDS, GRAFT_SHINY_ODDS_BOOSTED);
    SetGraftSetting(GRAFT_SETTING_EXP_SHARE, HasUnlock(UNLOCK_EXP_PARTY));
    SetGraftSetting(GRAFT_SETTING_ENEMY_GRAFTS, IsHardMode() ? GRAFT_ENEMY_GRAFTS_MORE : GRAFT_ENEMY_GRAFTS_NORMAL);
    SetGraftSetting(GRAFT_SETTING_WILD_GRAFTS, GRAFT_WILD_GRAFTS_OFF);
    SetGraftSetting(GRAFT_SETTING_BATTLE_ITEMS, FALSE);
    SetGraftSetting(GRAFT_SETTING_SET_MODE, !HasUnlock(UNLOCK_SWITCH_STYLE));
    SetGraftSetting(GRAFT_SETTING_SLEEP_CLAUSE, FALSE);
    SetGraftSetting(GRAFT_SETTING_SMART_WILDS, IsHardMode());
    SetGraftSetting(GRAFT_SETTING_DOUBLE_WILDS, GRAFT_DOUBLE_WILDS_OFF);
    SetGraftSetting(GRAFT_SETTING_INVERSE, (VarGet(VAR_ROGUE_RUN_FLAGS) & RUN_FLAG_INVERSE) != 0);
    if (!(VarGet(VAR_ROGUE_RUN_FLAGS) & RUN_FLAG_MONOTYPE))
        SetGraftSetting(GRAFT_SETTING_MONOTYPE, TYPE_NONE);
}

static void ClearRunProgress(void)
{
    u32 i;
    ZeroPlayerPartyMons();
    CalculatePlayerPartyCount();
    ClearBag();
    ClearGraftBag();
    for (i = 0; i < NUM_BADGES; i++)
        FlagClear(FLAG_BADGE01_GET + i);
    VarSet(VAR_ROGUE_FLOOR, 0);
    VarSet(VAR_ROGUE_ROOM, 0);
    VarSet(VAR_ROGUE_ROOM_TYPE, ROGUE_ROOM_HUB);
    // Challenges last one run
    SetGraftSetting(GRAFT_SETTING_INVERSE, FALSE);
    SetGraftSetting(GRAFT_SETTING_MONOTYPE, TYPE_NONE);
}

// The starter candidates of a run: one, or three with the Starter choice unlock
static void GetStarterCandidates(enum Species *out)
{
    rng_value_t rng = LocalRandomSeed(Mix32(GetRunSeed() ^ 0x57A7));
    u32 i, j;
    for (i = 0; i < 3; i++)
    {
        do
        {
            out[i] = PickStarter(&rng);
            for (j = 0; j < i && out[j] != out[i]; j++)
                ;
        } while (j < i);
    }
}

static void GiveStarter(enum Species species)
{
    ScriptGiveMon(species, 5, ITEM_NONE);
    GraftMonotypeOntoStarter(&gParties[B_TRAINER_PLAYER][0]);
    StringCopy(gStringVar1, GetSpeciesName(species));
}

static void StartRun(bool32 hard, u32 challenge, u32 monotype)
{
    u32 seed = Random32();
    enum Species starters[3];

    ClearRunProgress();
    VarSet(VAR_ROGUE_SEED_LO, seed & 0xFFFF);
    VarSet(VAR_ROGUE_SEED_HI, seed >> 16);
    VarSet(VAR_ROGUE_STATE, ROGUE_STATE_RUN);
    VarSet(VAR_ROGUE_FLOOR, 1);
    VarSet(VAR_ROGUE_ROOM, 1);
    VarSet(VAR_ROGUE_ROOM_TYPE, ROGUE_ROOM_WILD);
    VarSet(VAR_ROGUE_TEAM_SLOTS, GetStartingTeamSlots());
    VarSet(VAR_ROGUE_RUN_FLAGS, (hard ? RUN_FLAG_HARD : 0)
                              | (challenge == ROGUE_CHALLENGE_INVERSE ? RUN_FLAG_INVERSE : 0)
                              | (challenge == ROGUE_CHALLENGE_MONOTYPE ? RUN_FLAG_MONOTYPE : 0));
    if (challenge == ROGUE_CHALLENGE_MONOTYPE)
        SetGraftSetting(GRAFT_SETTING_MONOTYPE, monotype);
    SetForcedSettings();

    if (!HasUnlock(UNLOCK_STARTER_CHOICE))
    {
        GetStarterCandidates(starters);
        GiveStarter(starters[0]);
    }

    AddBagItem(ITEM_POKE_BALL, HasUnlock(UNLOCK_MORE_BALLS) ? 8 : 5);
    AddBagItem(ITEM_PATCH_BALL, HasUnlock(UNLOCK_MORE_BALLS) ? 5 : 3);
    if (HasUnlock(UNLOCK_REVIVES))
        AddBagItem(ITEM_REVIVE, 2);
    AddBagItem(ITEM_GRAFT_BALL, 1);
    AddBagItem(ITEM_POTION, 3);
    SetMoney(&gSaveBlock1Ptr->money, 1000);
    FlagSet(FLAG_SYS_POKEMON_GET);
    FlagSet(FLAG_SYS_POKEDEX_GET);
    FlagSet(FLAG_SYS_B_DASH);
    TryEnableGraftNationalDex();
}

static void EndRun(bool32 won)
{
    ClearRunProgress();
    VarSet(VAR_ROGUE_STATE, won ? ROGUE_STATE_RUN_WON : ROGUE_STATE_RUN_LOST);
}

void Rogue_TakeDoor(void)
{
    u32 door, type, floorBefore = GetFloor();

    BuildRoomPlan();
    door = sPlan.doors[gSpecialVar_0x8004 % NUM_DOORS];
    gSpecialVar_Result = ROGUE_DOOR_NEXT_ROOM;
    if (door == NO_DOOR) // can't happen unless the run state changed under the drawn room
        door = ROGUE_ROOM_WILD;

    if (door == ROGUE_ROOM_START)
    {
        // VAR_0x8005: 0 = not asked yet, 1 = Normal, 2 = Hard
        // VAR_0x8006: 0 = not asked yet, else ROGUE_CHALLENGE_*; VAR_0x8007: Monotype's type
        if (gSpecialVar_0x8005 == 0 && HasUnlock(UNLOCK_HARD_MODE))
        {
            gSpecialVar_Result = ROGUE_DOOR_ASK_MODE;
            return;
        }
        if (gSpecialVar_0x8006 == 0)
        {
            gSpecialVar_Result = ROGUE_DOOR_ASK_CHALLENGE;
            return;
        }
        StartRun(gSpecialVar_0x8005 == 2, gSpecialVar_0x8006, gSpecialVar_0x8007);
        gSpecialVar_Result = HasUnlock(UNLOCK_STARTER_CHOICE) ? ROGUE_DOOR_CHOOSE_STARTER : ROGUE_DOOR_STARTED_RUN;
        return;
    }
    if (door == ROGUE_ROOM_HUB)
    {
        EndRun(TRUE);
        gSpecialVar_Result = ROGUE_DOOR_WON_RUN;
        return;
    }

    if (door == ROGUE_ROOM_BOSS)
    {
        u32 floor, room;
        GetBossDoorDestination(&floor, &room);
        VarSet(VAR_ROGUE_FLOOR, floor);
        VarSet(VAR_ROGUE_ROOM, room);
    }
    else if (GetRoomType() == ROGUE_ROOM_BOSS)
    {
        VarSet(VAR_ROGUE_FLOOR, GetFloor() + 1);
        VarSet(VAR_ROGUE_ROOM, 1);
    }
    if (GetFloor() != floorBefore)
        SetLastRitesUsed(0); // last rites come back each floor
    else
    {
        VarSet(VAR_ROGUE_ROOM, GetRoom() + 1);
    }

    type = door;
    if (door == ROGUE_ROOM_MYSTERY)
    {
        rng_value_t rng = LocalRandomSeed(GetRoomSeed() ^ 0x3F3F);
        if (LocalRandom(&rng) % 100 < 35)
            type = ROGUE_ROOM_EVENT;
        else
            type = PickWeighted(&rng, sDoorWeights, ROGUE_ROOM_TYPE_COUNT, 1u << ROGUE_ROOM_MYSTERY);
        type |= ROOM_TYPE_FROM_MYSTERY;
    }
    VarSet(VAR_ROGUE_ROOM_TYPE, type);
}

void Rogue_PrepareWildMon(void)
{
    const struct ObjectEventTemplate *template = &gSaveBlock1Ptr->objectEventTemplates[gSpecialVar_LastTalked - 1];
    enum Species species = template->graphicsId - OBJ_EVENT_MON;

    CreateScriptedWildMon(species, GetWildLevel(), ITEM_NONE);
    PlayCry_Script(species, CRY_MODE_ENCOUNTER);
}

// A stranger's gift: two pieces, an ability from the floor's pool and a high stat
void Rogue_GiveGiftPieces(void)
{
    rng_value_t rng = LocalRandomSeed(GetRoomSeed() ^ 0x61F7);
    struct GraftPiece piece;
    enum Species species = PickRareSpecies(&rng);

    piece.kind = GRAFT_PIECE_ABILITY;
    piece.stat = 0;
    piece.value = GetAbilityBySpecies(species, LocalRandom(&rng) % 2);
    if (piece.value == ABILITY_NONE)
        piece.value = GetAbilityBySpecies(species, 0);
    AddGraftPiece(&piece);
    GetGraftPieceName(gStringVar1, &piece);

    piece.kind = GRAFT_PIECE_STAT;
    piece.stat = LocalRandom(&rng) % NUM_STATS;
    piece.value = 90 + LocalRandom(&rng) % 41;
    AddGraftPiece(&piece);
    GetGraftPieceName(gStringVar2, &piece);
    GiveGraftBagIfMissing();
}

void Rogue_PrepareItemBall(void)
{
    rng_value_t rng = LocalRandomSeed(GetRoomSeed() ^ (gSpecialVar_LastTalked * 0x1234567));
    u8 weights[ARRAY_COUNT(sItemBallItems)];
    u32 i, pick;

    for (i = 0; i < ARRAY_COUNT(sItemBallItems); i++)
        weights[i] = GetFloor() >= sItemBallItems[i].minFloor ? sItemBallItems[i].weight : 0;
    pick = PickWeighted(&rng, weights, ARRAY_COUNT(sItemBallItems), 0);
    gSpecialVar_0x8000 = sItemBallItems[pick].item;
    gSpecialVar_0x8001 = sItemBallItems[pick].quantity;
}

static void GiveBP(u32 amount)
{
    gSaveBlock2Ptr->frontier.battlePoints = min(gSaveBlock2Ptr->frontier.battlePoints + amount, MAX_BATTLE_FRONTIER_POINTS);
}

static u32 GetBossBP(void)
{
    u32 bp = IsFinale(GetFloor()) ? 10 : 5;
    if (IsFinale(GetFloor()) && GetRoom() >= GetFinaleLength())
        bp += 10; // the run is won
    if (IsHardMode())
        bp *= 2;
    if (VarGet(VAR_ROGUE_RUN_FLAGS) & RUN_FLAG_CHALLENGE)
        bp += bp / 2; // a challenge run pays half again
    return bp;
}

void Rogue_OnBossDefeated(void)
{
    u32 floor = GetFloor();
    u32 bp = GetBossBP();

    if (!IsFinale(floor) && floor <= NUM_BADGES)
        FlagSet(FLAG_BADGE01_GET + floor - 1);
    if (IsFinale(floor) && GetRoom() >= GetFinaleLength())
    {
        StringCopy(gStringVar4, COMPOUND_STRING("You beat the CHAMPION!\nThe way out is open."));
    }
    else if (IsFinale(floor) || floor == GetNumFloors())
    {
        StringCopy(gStringVar4, COMPOUND_STRING("The way to the next challenger\nis open."));
    }
    else
    {
        ConvertIntToDecimalStringN(gStringVar1, floor, STR_CONV_MODE_LEFT_ALIGN, 2);
        StringExpandPlaceholders(gStringVar4, COMPOUND_STRING("FLOOR {STR_VAR_1} cleared!\nThe way forward is open."));
    }
    if (IsFinale(floor) && GetRoom() >= GetFinaleLength() && !IsHardMode() && IsMetaOn()
     && !HasUnlock(UNLOCK_HARD_MODE))
    {
        VarSet(VAR_ROGUE_UNLOCKS, VarGet(VAR_ROGUE_UNLOCKS) | (1 << UNLOCK_HARD_MODE));
        StringAppend(gStringVar4, COMPOUND_STRING("\pHARD mode is open: eight floors\nand the ELITE FOUR!"));
    }
    if (IsMetaOn())
    {
        GiveBP(bp);
        ConvertIntToDecimalStringN(gStringVar2, bp, STR_CONV_MODE_LEFT_ALIGN, 3);
        StringAppend(gStringVar4, COMPOUND_STRING("\p"));
        StringExpandPlaceholders(gStringVar3, COMPOUND_STRING("You earned {STR_VAR_2} BP!"));
        StringAppend(gStringVar4, gStringVar3);
    }
}

// ---------------------------------------------------------------------------------------------
// Start of a run: starter and heirloom choices

static void PushChoice(const u8 *name, u32 id)
{
    struct ListMenuItem item;
    u8 *buffer = Alloc(100);
    StringCopy(buffer, name);
    item.name = buffer;
    item.id = id;
    MultichoiceDynamic_PushElement(item);
}

void Rogue_PushStarterChoices(void)
{
    enum Species starters[3];
    u32 i;
    GetStarterCandidates(starters);
    for (i = 0; i < 3; i++)
        PushChoice(GetSpeciesName(starters[i]), i);
}

void Rogue_GiveChosenStarter(void)
{
    enum Species starters[3];
    GetStarterCandidates(starters);
    GiveStarter(starters[gSpecialVar_Result < 3 ? gSpecialVar_Result : 0]);
}

static const u16 sHeirloomAbilities[] =
{
    ABILITY_INTIMIDATE, ABILITY_LEVITATE, ABILITY_SPEED_BOOST, ABILITY_ADAPTABILITY, ABILITY_TECHNICIAN,
    ABILITY_MAGIC_GUARD, ABILITY_REGENERATOR, ABILITY_MOXIE, ABILITY_SHEER_FORCE, ABILITY_TOUGH_CLAWS,
    ABILITY_PRANKSTER, ABILITY_MULTISCALE, ABILITY_THICK_FAT, ABILITY_GUTS, ABILITY_TINTED_LENS,
    ABILITY_SERENE_GRACE, ABILITY_SKILL_LINK, ABILITY_STURDY, ABILITY_DROUGHT, ABILITY_DRIZZLE,
};

// The heirloom's three candidates: an ability, a high stat and a type
static void GetHeirloomCandidates(struct GraftPiece *out)
{
    rng_value_t rng = LocalRandomSeed(Mix32(GetRunSeed() ^ 0x4E1B));
    out[0].kind = GRAFT_PIECE_ABILITY;
    out[0].stat = 0;
    out[0].value = sHeirloomAbilities[LocalRandom(&rng) % ARRAY_COUNT(sHeirloomAbilities)];
    out[1].kind = GRAFT_PIECE_STAT;
    out[1].stat = LocalRandom(&rng) % NUM_STATS;
    out[1].value = 110 + LocalRandom(&rng) % 31;
    out[2].kind = GRAFT_PIECE_TYPE;
    out[2].stat = 0;
    do
        out[2].value = TYPE_NORMAL + LocalRandom(&rng) % (TYPE_FAIRY - TYPE_NORMAL + 1);
    while (out[2].value == TYPE_MYSTERY);
}

void Rogue_HasHeirloom(void)
{
    gSpecialVar_Result = HasUnlock(UNLOCK_START_PIECE);
}

void Rogue_PushHeirloomChoices(void)
{
    struct GraftPiece pieces[3];
    u8 name[64];
    u32 i;
    GetHeirloomCandidates(pieces);
    for (i = 0; i < 3; i++)
    {
        GetGraftPieceName(name, &pieces[i]);
        PushChoice(name, i);
    }
}

void Rogue_GiveHeirloom(void)
{
    struct GraftPiece pieces[3];
    struct GraftPiece *piece;
    GetHeirloomCandidates(pieces);
    piece = &pieces[gSpecialVar_Result < 3 ? gSpecialVar_Result : 0];
    AddGraftPiece(piece);
    GiveGraftBagIfMissing();
    GetGraftPieceName(gStringVar1, piece);
}

// ---------------------------------------------------------------------------------------------
// The unlock shop (start room)

// ---------------------------------------------------------------------------------------------
// Fainted Pokémon. Nothing is lost on its own: after a battle the player is asked about each newly
// fainted Pokémon: keep it (fainted, until a Revive), let it go, or give it last rites (break it
// down into graft pieces; a few uses a floor, from the Last rites unlocks).

static u32 GetLastRitesPerFloor(void)
{
    return HasUnlock(UNLOCK_LAST_RITES_1) + HasUnlock(UNLOCK_LAST_RITES_2) + HasUnlock(UNLOCK_LAST_RITES_3);
}

static u32 GetLastRitesLeft(void)
{
    u32 used = (VarGet(VAR_ROGUE_RUN_FLAGS) & RUN_FLAG_RITES_MASK) >> RUN_FLAG_RITES_SHIFT;
    u32 total = GetLastRitesPerFloor();
    return used < total ? total - used : 0;
}

static void SetLastRitesUsed(u32 used)
{
    VarSet(VAR_ROGUE_RUN_FLAGS, (VarGet(VAR_ROGUE_RUN_FLAGS) & ~RUN_FLAG_RITES_MASK) | (used << RUN_FLAG_RITES_SHIFT));
}

static bool32 IsUnansweredFainted(struct Pokemon *mon)
{
    return GetMonData(mon, MON_DATA_SPECIES) != SPECIES_NONE && !GetMonData(mon, MON_DATA_IS_EGG)
        && GetMonData(mon, MON_DATA_HP) == 0 && !(mon->box.graftFlags & GRAFT_FLAG_ROGUE_KEPT);
}

// After a battle the player didn't lose (a whiteout ends the run instead)
void Rogue_OnBattleEnd(void)
{
    u32 i;
    if (!Rogue_IsRunActive() || gBattleOutcome == B_OUTCOME_LOST || gBattleOutcome == B_OUTCOME_DREW)
        return;
    for (i = 0; i < PARTY_SIZE; i++)
    {
        struct Pokemon *mon = &gParties[B_TRAINER_PLAYER][i];
        if (GetMonData(mon, MON_DATA_HP) != 0)
            mon->box.graftFlags &= ~GRAFT_FLAG_ROGUE_KEPT; // revived: ask again if it faints again
        else if (IsUnansweredFainted(mon))
            VarSet(VAR_TEMP_4, 1); // the room asks (Rogue_Room_OnFrame)
    }
}

// The next fainted Pokémon to ask about: VAR_0x8004 = its slot, STR_VAR_1 its name; VAR_RESULT FALSE if none
void Rogue_FindFainted(void)
{
    u32 i;
    gSpecialVar_Result = FALSE;
    for (i = 0; i < PARTY_SIZE; i++)
    {
        if (IsUnansweredFainted(&gParties[B_TRAINER_PLAYER][i]))
        {
            gSpecialVar_0x8004 = i;
            GetMonData(&gParties[B_TRAINER_PLAYER][i], MON_DATA_NICKNAME, gStringVar1);
            StringGet_Nickname(gStringVar1);
            gSpecialVar_Result = TRUE;
            return;
        }
    }
}

void Rogue_PushFaintedChoices(void)
{
    u8 name[32];
    PushChoice(COMPOUND_STRING("Keep it"), ROGUE_FAINTED_KEEP);
    PushChoice(COMPOUND_STRING("Let it go"), ROGUE_FAINTED_RELEASE);
    if (GetLastRitesLeft() != 0)
    {
        ConvertIntToDecimalStringN(gStringVar2, GetLastRitesLeft(), STR_CONV_MODE_LEFT_ALIGN, 1);
        StringExpandPlaceholders(name, COMPOUND_STRING("Last rites ({STR_VAR_2})"));
        PushChoice(name, ROGUE_FAINTED_RITES);
    }
}

// VAR_0x8004 = slot, VAR_0x8005 = the choice; the message goes to gStringVar4
void Rogue_ResolveFainted(void)
{
    struct Pokemon *mon = &gParties[B_TRAINER_PLAYER][gSpecialVar_0x8004 % PARTY_SIZE];

    switch (gSpecialVar_0x8005)
    {
    case ROGUE_FAINTED_RITES:
        if (GetLastRitesLeft() != 0)
        {
            struct GraftPiece pieces[GRAFT_PIECES_PER_MON];
            u32 lost;
            HarvestGraftPieces(mon, FALSE, GRAFT_HARVEST_ALL, pieces, &lost);
            GiveGraftBagIfMissing();
            SetLastRitesUsed(GetLastRitesPerFloor() - GetLastRitesLeft() + 1);
            StringExpandPlaceholders(gStringVar4, COMPOUND_STRING("{STR_VAR_1} was broken down\ninto graft pieces."));
            break;
        }
        // fall through
    case ROGUE_FAINTED_RELEASE:
        StringExpandPlaceholders(gStringVar4, COMPOUND_STRING("{STR_VAR_1} is gone for good."));
        break;
    default:
        mon->box.graftFlags |= GRAFT_FLAG_ROGUE_KEPT;
        StringExpandPlaceholders(gStringVar4, COMPOUND_STRING("{STR_VAR_1} stays with the team,\nwaiting for a Revive."));
        return;
    }
    ReleasePartyMon(gSpecialVar_0x8004 % PARTY_SIZE);
    CompactPartySlots();
    CalculatePlayerPartyCount();
    UpdateFollowingPokemon();
}

// Egg moves in the relearner: only a few, picked by the Pokémon's personality (so the same Pokémon
// always has the same ones). 10% have one; Good genes makes it 35%; with Pedigree every Pokémon has
// one and 35% have two. Returns how many of eggMoves[] were written to out.
u32 Rogue_FilterEggMoves(struct BoxPokemon *mon, const u16 *eggMoves, u16 *out)
{
    u32 total, count, i, hash = Mix32(GetBoxMonData(mon, MON_DATA_PERSONALITY) ^ 0xE66);
    u32 roll = hash % 100;

    for (total = 0; eggMoves[total] != MOVE_UNAVAILABLE; total++)
        ;
    if (total == 0)
        return 0;
    if (HasUnlock(UNLOCK_PEDIGREE))
        count = 1 + (roll < 35);
    else
        count = roll < (HasUnlock(UNLOCK_GOOD_GENES) ? 35 : 10);
    count = min(count, total);
    for (i = 0; i < count; i++)
        out[i] = eggMoves[((hash >> 8) + i) % total];
    return count;
}

// The start room's first question (VAR_0x8004: YES/NO)
void Rogue_SetMetaProgression(void)
{
    SetGraftSetting(GRAFT_SETTING_ROGUE_META, gSpecialVar_0x8004 == 1); // YES
    VarSet(VAR_ROGUE_INTRO, 1);
}

void Rogue_PushChallengeChoices(void)
{
    PushChoice(COMPOUND_STRING("No challenge"), ROGUE_CHALLENGE_NONE);
    PushChoice(COMPOUND_STRING("Inverse battles"), ROGUE_CHALLENGE_INVERSE);
    PushChoice(COMPOUND_STRING("Monotype"), ROGUE_CHALLENGE_MONOTYPE);
}

void Rogue_PushTypeChoices(void)
{
    u32 type;
    for (type = TYPE_NORMAL; type < NUMBER_OF_MON_TYPES; type++)
    {
        if (type == TYPE_MYSTERY || type == TYPE_STELLAR)
            continue;
        PushChoice(gTypesInfo[type].name, type);
    }
}

void Rogue_BufferBP(void)
{
    gSpecialVar_Result = IsMetaOn();
    ConvertIntToDecimalStringN(gStringVar1, gSaveBlock2Ptr->frontier.battlePoints, STR_CONV_MODE_LEFT_ALIGN, 4);
}

// Pushes the unlocks still for sale; VAR_RESULT = how many
void Rogue_PushUnlockChoices(void)
{
    u32 i, count = 0;
    u8 name[48];
    for (i = 0; i < UNLOCK_SHOP_COUNT; i++)
    {
        if (HasUnlock(i) || (sUnlocks[i].requires >= 0 && !HasUnlock(sUnlocks[i].requires)))
            continue;
        ConvertIntToDecimalStringN(gStringVar2, sUnlocks[i].cost, STR_CONV_MODE_LEFT_ALIGN, 3);
        StringCopy(name, sUnlocks[i].name);
        StringAppend(name, COMPOUND_STRING(" "));
        StringAppend(name, gStringVar2);
        StringAppend(name, COMPOUND_STRING("BP"));
        PushChoice(name, i);
        count++;
    }
    gSpecialVar_Result = count;
}

void Rogue_DescribeUnlock(void)
{
    u32 id = gSpecialVar_0x8004 % UNLOCK_SHOP_COUNT;
    ConvertIntToDecimalStringN(gStringVar2, sUnlocks[id].cost, STR_CONV_MODE_LEFT_ALIGN, 3);
    StringCopy(gStringVar4, sUnlocks[id].description);
    StringExpandPlaceholders(gStringVar3, COMPOUND_STRING("\pUnlock it for {STR_VAR_2} BP?"));
    StringAppend(gStringVar4, gStringVar3);
}

void Rogue_BuyUnlock(void)
{
    u32 id = gSpecialVar_0x8004 % UNLOCK_SHOP_COUNT;
    if (gSaveBlock2Ptr->frontier.battlePoints < sUnlocks[id].cost)
    {
        gSpecialVar_Result = FALSE;
        StringCopy(gStringVar4, COMPOUND_STRING("You don't have enough BP.\nBeat bosses to earn more."));
        return;
    }
    gSaveBlock2Ptr->frontier.battlePoints -= sUnlocks[id].cost;
    VarSet(VAR_ROGUE_UNLOCKS, VarGet(VAR_ROGUE_UNLOCKS) | (1 << id));
    gSpecialVar_Result = TRUE;
    StringCopy(gStringVar1, sUnlocks[id].name);
    StringExpandPlaceholders(gStringVar4, COMPOUND_STRING("{STR_VAR_1} unlocked!\nIt's yours from the next run on."));
}

// ---------------------------------------------------------------------------------------------
// Whiteout: the run is lost, back to the start room

void Rogue_OnWhiteOut(void)
{
    EndRun(FALSE);
    SetWarpDestination(MAP_GROUP(MAP_ROGUE_ROOM), MAP_NUM(MAP_ROGUE_ROOM), WARP_ID_NONE, ROGUE_ENTRANCE_X, ROGUE_ENTRANCE_Y);
}

void Rogue_InitNewGame(void)
{
    ClearRunProgress();
    VarSet(VAR_ROGUE_STATE, ROGUE_STATE_HUB);
    SetWarpDestination(MAP_GROUP(MAP_ROGUE_ROOM), MAP_NUM(MAP_ROGUE_ROOM), WARP_ID_NONE, ROGUE_ENTRANCE_X, ROGUE_ENTRANCE_Y);
}

// ---------------------------------------------------------------------------------------------
// Generated trainers

static void BuildTrainer(u32 slot, u32 key)
{
    rng_value_t rng = LocalRandomSeed(key);
    bool32 boss = (slot == 1);
    u32 floor = GetFloor();
    u16 baseId = boss ? GetCurrentBoss()->trainerId : sTrainerKinds[GetTrainerKind()].trainerId;
    enum Type type = boss ? GetCurrentBoss()->type : TYPE_NONE;
    u32 size, level, iv, i;

    sTrainers[slot] = *GetTrainerStructFromId(baseId);
    if (boss)
    {
        size = IsFinale(floor) ? 5 : min(floor + 2, PARTY_SIZE);
        level = GetBossLevel();
        iv = min(31, 6 + floor * 4);
    }
    else
    {
        size = min(floor + (GetRoom() > 3), PARTY_SIZE);
        level = GetWildLevel() + 1;
        iv = min(31, floor * 3);
    }
    if (size == 0)
        size = 1;

    for (i = 0; i < size; i++)
    {
        struct TrainerMon *mon = &sTrainerMons[slot][i];
        memset(mon, 0, sizeof(*mon));
        mon->species = PickFloorSpecies(&rng, type);
        mon->lvl = level + (boss && i == size - 1);
        mon->iv = TRAINER_PARTY_IVS(iv, iv, iv, iv, iv, iv);
        mon->gender = TRAINER_MON_RANDOM_GENDER;
        mon->nature = LocalRandom(&rng) % NUM_NATURES;
        mon->ball = POKEBALL_COUNT;
        mon->dynamaxLevel = MAX_DYNAMAX_LEVEL;
    }
    sTrainers[slot].party = sTrainerMons[slot];
    sTrainers[slot].partySize = size;
    sTrainers[slot].poolSize = 0;
    sTrainers[slot].overrideTrainer = 0;
    sTrainers[slot].battleType = TRAINER_BATTLE_TYPE_SINGLES;
}

// The Leader / Elite Four / Champion the current boss is (for its ace, src/graft_data.c)
u16 Rogue_GetBossTrainerId(void)
{
    return GetCurrentBoss()->trainerId;
}

const struct Trainer *Rogue_GetTrainer(u16 trainerId)
{
    u32 slot = (trainerId == TRAINER_ROGUE_BOSS);
    u32 key = Mix32(GetRoomSeed() + slot + 1) | 1;

    if (sTrainerKeys[slot] != key)
    {
        BuildTrainer(slot, key);
        sTrainerKeys[slot] = key;
    }
    return &sTrainers[slot];
}
