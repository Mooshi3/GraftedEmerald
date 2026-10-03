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
#include "field_screen_effect.h"
#include "fieldmap.h"
#include "wild_encounter.h"
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
#include "constants/layouts.h"
#include "constants/metatile_labels.h"
#include "constants/move_relearner.h"
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
extern const u8 Rogue_Room_EventScript_Thicket[];
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
#define MAX_STARTER_CHOICES 4 // three with the Starter choice unlock, and the favorite
#define NO_DOOR 0xFF
#define NUM_MARKERS NUM_DOORS // object local IDs 1-3: the signs over the doors
#define MAX_ROOM_OBJECTS 5    // local IDs 4-8: what's in the room
#define NUM_THICKETS (NUM_DOORS * 2) // local IDs 9-14: two small trees blocking each door of a guarded room
#define FIRST_THICKET_SLOT (NUM_MARKERS + MAX_ROOM_OBJECTS)
#define NUM_OBJECT_SLOTS (NUM_MARKERS + MAX_ROOM_OBJECTS + NUM_THICKETS)

static const u8 sDoorBlockX[NUM_DOORS] = {3, 5, 7};

// The inside of every room map, in tiles
#define INNER_X_MIN 4
#define INNER_X_MAX 17
#define INNER_Y_MIN 4
#define INNER_Y_MAX 13

// Room themes: VAR_ROGUE_ROOM_TYPE bits 9-11
enum
{
    BIOME_FOREST,
    BIOME_BEACH,
    BIOME_CAVE,
    BIOME_VOLCANO,
    BIOME_POWER_PLANT,
    BIOME_COUNT,
};
#define ROOM_BIOME_SHIFT 9
#define ROOM_BIOME_MASK  (7 << ROOM_BIOME_SHIFT)

enum
{
    ROOM_KIND_OUTDOOR, // MAP_ROGUE_ROOM: forest or beach
    ROOM_KIND_WALLED,  // MAP_ROGUE_CAVE / MAP_ROGUE_VOLCANO / MAP_ROGUE_POWER_PLANT
    ROOM_KIND_ARENA,   // a boss's Elite Four room
    ROOM_KIND_STOP,    // MAP_ROGUE_STOP: the rest stop between floors
};

enum
{
    SEA_NONE,
    SEA_LEFT,
    SEA_RIGHT,
};

// Shop and rest rooms: a Mart or a Pokémon Center (4x4 metatiles, the door on the bottom row's
// second column; Rogue_Room's warp events sit on the two doors)
enum
{
    BUILDING_NONE,
    BUILDING_MART,
    BUILDING_CENTER,
};
#define BUILDING_W 4
#define BUILDING_H 4
#define BUILDING_Y 7
#define MART_X 9
#define CENTER_BUILDING_X 10

// Elite Four rooms: the way in at the bottom, the boss in the middle
#define ARENA_ENTRANCE_X 6
#define ARENA_ENTRANCE_Y 12
#define ARENA_BOSS_X 6
#define ARENA_BOSS_Y 5

// The rest stop (a Pokémon Center's room): where the player arrives
#define STOP_ENTRANCE_X 7
#define STOP_ENTRANCE_Y 7

// Metatiles (gTileset_General), with collision and elevation as Route 101 uses them
#define TILE_TREE(id)  ((id) | (1 << MAPGRID_COLLISION_SHIFT))
#define TILE_GROUND(id) ((id) | (3 << MAPGRID_ELEVATION_SHIFT))
enum
{
    GROUND_GRASS,
    GROUND_TALL_GRASS,
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
    u8 kind;
    u8 biome;
    u8 seaSide;
    u8 building;
    u8 buildingX;
    u8 doorBiomes[NUM_DOORS]; // the theme of the room behind each door
    bool8 tree[BLOCKS_H][BLOCKS_W];
    bool8 rock[ROOM_H][ROOM_W]; // walled rooms' rocks
    bool8 water[BLOCKS_H][BLOCKS_W];
    u8 ground[ROOM_H][ROOM_W];
    u8 doors[NUM_DOORS]; // room type behind each door, or NO_DOOR
    bool8 guarded;       // a trainer (or the boss) blocks the doors until beaten
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
    UNLOCK_FAVORITE,
    UNLOCK_HARD_MODE = 15, // not bought: beating a Normal run opens it
    // VAR_ROGUE_UNLOCKS_2
    UNLOCK_DISCOUNT = 16,
    UNLOCK_HARVEST_BONUS,
    UNLOCK_CATCH_BONUS,
    UNLOCK_COUNT,
};

static const struct
{
    const u8 *name;
    const u8 *description;
    u16 cost;
    s8 requires;
} sUnlocks[UNLOCK_COUNT] =
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
    [UNLOCK_EXP_PARTY] = {COMPOUND_STRING("Quick study"), COMPOUND_STRING("Your team gains half again as\nmuch EXP."), 40, -1},
    [UNLOCK_GOOD_GENES] = {COMPOUND_STRING("Good genes"), COMPOUND_STRING("More Pokémon have an egg move to\nrelearn (35% instead of 10%)."), 40, -1},
    [UNLOCK_PEDIGREE] = {COMPOUND_STRING("Pedigree"), COMPOUND_STRING("Every Pokémon has an egg move to\nrelearn, some have two."), 80, UNLOCK_GOOD_GENES},
    [UNLOCK_SWITCH_STYLE] = {COMPOUND_STRING("Switch style"), COMPOUND_STRING("You may switch Pokémon when a\nfoe faints (Shift battle style)."), 20, -1},
    [UNLOCK_START_PIECE] = {COMPOUND_STRING("Heirloom"), COMPOUND_STRING("Pick one of three graft pieces\nto start each run with."), 50, -1},
    [UNLOCK_DISCOUNT] = {COMPOUND_STRING("Discount"), COMPOUND_STRING("Shops in a run sell everything\nfor a quarter less."), 40, -1},
    [UNLOCK_HARVEST_BONUS] = {COMPOUND_STRING("Harvest bonus"), COMPOUND_STRING("Pieces broken down from a POKéMON\nsometimes come with a spare copy."), 60, -1},
    [UNLOCK_CATCH_BONUS] = {COMPOUND_STRING("Catch bonus"), COMPOUND_STRING("A POKéMON you catch sometimes\nleaves a graft piece behind too."), 50, -1},
    [UNLOCK_FAVORITE] = {COMPOUND_STRING("Favorite"), COMPOUND_STRING("A team member you name in a\nCENTER becomes a starter choice."), 150, -1},
};

#define RUN_FLAG_HARD     (1 << 0)
#define RUN_FLAG_INVERSE  (1 << 1) // challenge: Inverse battles
#define RUN_FLAG_MONOTYPE (1 << 2) // challenge: Monotype (the type is in the Monotype setting)
#define RUN_FLAG_CHALLENGE (RUN_FLAG_INVERSE | RUN_FLAG_MONOTYPE)
#define RUN_FLAG_PRIZE_TAKEN (1 << 3) // the rest stop's free prize is taken (cleared on leaving it)

// Last rites used this floor: VAR_ROGUE_RUN_FLAGS bits 8-11
#define RUN_FLAG_RITES_SHIFT 8
#define RUN_FLAG_RITES_MASK  (0xF << RUN_FLAG_RITES_SHIFT)

// A fainted Pokémon the player chose to keep (waiting for a Revive): not asked about again
#define GRAFT_FLAG_ROGUE_KEPT (1 << 6)

static void PushChoice(const u8 *name, u32 id);
static u32 PickWeighted(rng_value_t *rng, const u8 *weights, u32 count, u32 excludeMask);
static void SetLastRitesUsed(u32 used);

static bool32 IsMetaOn(void)
{
    return GetGraftSetting(GRAFT_SETTING_ROGUE_META);
}

// With Unlocks Off, everything is open
// Unlocks 0-15 are VAR_ROGUE_UNLOCKS, 16-31 VAR_ROGUE_UNLOCKS_2
static bool32 HasUnlock(u32 id)
{
    if (!IsMetaOn())
        return TRUE;
    if (id >= 16)
        return (VarGet(VAR_ROGUE_UNLOCKS_2) >> (id - 16)) & 1;
    return (VarGet(VAR_ROGUE_UNLOCKS) >> id) & 1;
}

static void SetUnlock(u32 id)
{
    if (id >= 16)
        VarSet(VAR_ROGUE_UNLOCKS_2, VarGet(VAR_ROGUE_UNLOCKS_2) | (1 << (id - 16)));
    else
        VarSet(VAR_ROGUE_UNLOCKS, VarGet(VAR_ROGUE_UNLOCKS) | (1 << id));
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
    if (GetRoomType() == ROGUE_ROOM_REST_STOP && *floor == GetNumFloors())
    {
        *floor = GetNumFloors() + 1; // into the finale
        *room = 1;
    }
}

// Levels climb slowly over a floor (floor 1: 3 to 9) so the battles on the way can keep up; the boss
// is a couple of levels above the floor's last rooms
static u32 GetWildLevel(void)
{
    if (IsFinale(GetFloor()))
        return 3 + GetNumFloors() * 8;
    return 3 + (GetFloor() - 1) * 8 + (GetRoom() * 6) / ROGUE_ROOMS_PER_FLOOR;
}

static u32 GetBossLevel(void)
{
    if (IsFinale(GetFloor()))
        return 3 + GetNumFloors() * 8 + 2 + GetRoom();
    return 3 + (GetFloor() - 1) * 8 + 8;
}

// EXP in a run (decided with Jacob 2026-10-03, after runs rarely reached the first boss): the whole
// team learns, at double EXP (triple with Quick study), and a Pokémon 5+ levels under the cap gets
// double that again, so a new catch catches up. The hard cap is the floor's boss level (in the
// finale, the Champion's), so the boost can't outlevel the boss.
u32 Rogue_GetLevelCap(void)
{
    if (IsFinale(GetFloor()))
        return 3 + GetNumFloors() * 8 + 2 + GetFinaleLength();
    return GetBossLevel();
}

u32 Rogue_ScaleExp(u32 level, u32 exp)
{
    u32 cap = Rogue_GetLevelCap();
    exp = HasUnlock(UNLOCK_EXP_PARTY) ? exp * 3 : exp * 2;
    if (level + 5 <= cap)
        exp *= 2;
    return exp;
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
// Room themes and maps
//
// A room is outdoors (forest or beach: MAP_ROGUE_ROOM, trees in 2x2 blocks), walled (cave or volcano:
// their own maps, a rectangle of rock walls) or a boss arena (an Elite Four room). All the room maps
// are 22x18 with the same inside (tiles 4..17 x 4..13), entrance (10,13) and door columns 6, 10, 14.

static const u8 sFloorHomeBiome[] = {BIOME_FOREST, BIOME_BEACH, BIOME_CAVE, BIOME_VOLCANO, BIOME_POWER_PLANT};

static u32 GetRoomBiome(void)
{
    if (VarGet(VAR_ROGUE_STATE) != ROGUE_STATE_RUN)
        return BIOME_FOREST;
    return (VarGet(VAR_ROGUE_ROOM_TYPE) & ROOM_BIOME_MASK) >> ROOM_BIOME_SHIFT;
}

static bool32 IsBuildingRoom(u32 type)
{
    return type == ROGUE_ROOM_SHOP || type == ROGUE_ROOM_REST;
}

// Each floor has a home theme most rooms take; buildings stand outdoors
static u32 PickBiome(rng_value_t *rng, u32 type, u32 floor)
{
    u8 weights[BIOME_COUNT] = {12, 12, 12, 12, 12};
    u32 home = sFloorHomeBiome[(max(floor, 1) - 1) % ARRAY_COUNT(sFloorHomeBiome)];

    weights[home] = 55;
    if (IsBuildingRoom(type))
    {
        weights[BIOME_CAVE] = weights[BIOME_VOLCANO] = weights[BIOME_POWER_PLANT] = 0;
        if (home == BIOME_BEACH)
            weights[BIOME_FOREST] = 15;
        else
            weights[BIOME_BEACH] = 15;
    }
    return PickWeighted(rng, weights, BIOME_COUNT, 0);
}

static u32 GetRoomKind(void)
{
    if (VarGet(VAR_ROGUE_STATE) == ROGUE_STATE_RUN && GetRoomType() == ROGUE_ROOM_BOSS)
        return ROOM_KIND_ARENA;
    if (VarGet(VAR_ROGUE_STATE) == ROGUE_STATE_RUN && GetRoomType() == ROGUE_ROOM_REST_STOP)
        return ROOM_KIND_STOP;
    switch (GetRoomBiome())
    {
    case BIOME_CAVE:
    case BIOME_VOLCANO:
    case BIOME_POWER_PLANT:
        return ROOM_KIND_WALLED;
    }
    return ROOM_KIND_OUTDOOR;
}

// Hoenn maps with encounter tables and the theme of their land Pokémon (BIOME_COUNT: no land table).
// Surfed and fished Pokémon are beach Pokémon, Rock Smash ones cave Pokémon, wherever they are.
static const struct { u16 map; u8 landBiome; } sHabitatMaps[] =
{
    {MAP_ROUTE124, BIOME_COUNT},
    {MAP_SAFARI_ZONE_SOUTH, BIOME_FOREST},
    {MAP_UNDERWATER_ROUTE126, BIOME_COUNT},
    {MAP_ABANDONED_SHIP_ROOMS_B1F, BIOME_COUNT},
    {MAP_GRANITE_CAVE_B2F, BIOME_CAVE},
    {MAP_METEOR_FALLS_B1F_2R, BIOME_CAVE},
    {MAP_ROUTE106, BIOME_COUNT},
    {MAP_ROUTE107, BIOME_COUNT},
    {MAP_ROUTE108, BIOME_COUNT},
    {MAP_ROUTE122, BIOME_COUNT},
    {MAP_MT_PYRE_2F, BIOME_CAVE},
    {MAP_MT_PYRE_3F, BIOME_CAVE},
    {MAP_MT_PYRE_4F, BIOME_CAVE},
    {MAP_MT_PYRE_5F, BIOME_CAVE},
    {MAP_MT_PYRE_6F, BIOME_CAVE},
    {MAP_MT_PYRE_SUMMIT, BIOME_CAVE},
    {MAP_GRANITE_CAVE_STEVENS_ROOM, BIOME_CAVE},
    {MAP_ROUTE125, BIOME_COUNT},
    {MAP_ROUTE126, BIOME_COUNT},
    {MAP_ROUTE127, BIOME_COUNT},
    {MAP_ROUTE128, BIOME_COUNT},
    {MAP_ROUTE129, BIOME_COUNT},
    {MAP_ROUTE131, BIOME_COUNT},
    {MAP_ROUTE132, BIOME_COUNT},
    {MAP_ROUTE133, BIOME_COUNT},
    {MAP_ROUTE134, BIOME_COUNT},
    {MAP_ABANDONED_SHIP_HIDDEN_FLOOR_CORRIDORS, BIOME_COUNT},
    {MAP_SEAFLOOR_CAVERN_ROOM1, BIOME_CAVE},
    {MAP_SEAFLOOR_CAVERN_ROOM2, BIOME_CAVE},
    {MAP_SEAFLOOR_CAVERN_ROOM3, BIOME_CAVE},
    {MAP_SEAFLOOR_CAVERN_ROOM4, BIOME_CAVE},
    {MAP_SEAFLOOR_CAVERN_ROOM5, BIOME_CAVE},
    {MAP_SEAFLOOR_CAVERN_ROOM6, BIOME_CAVE},
    {MAP_SEAFLOOR_CAVERN_ROOM7, BIOME_CAVE},
    {MAP_SEAFLOOR_CAVERN_ROOM8, BIOME_CAVE},
    {MAP_CAVE_OF_ORIGIN_ENTRANCE, BIOME_CAVE},
    {MAP_CAVE_OF_ORIGIN_UNUSED_RUBY_SAPPHIRE_MAP1, BIOME_CAVE},
    {MAP_CAVE_OF_ORIGIN_UNUSED_RUBY_SAPPHIRE_MAP2, BIOME_CAVE},
    {MAP_CAVE_OF_ORIGIN_UNUSED_RUBY_SAPPHIRE_MAP3, BIOME_CAVE},
    {MAP_NEW_MAUVILLE_ENTRANCE, BIOME_POWER_PLANT},
    {MAP_SAFARI_ZONE_SOUTHWEST, BIOME_FOREST},
    {MAP_SAFARI_ZONE_NORTH, BIOME_FOREST},
    {MAP_SAFARI_ZONE_NORTHWEST, BIOME_FOREST},
    {MAP_METEOR_FALLS_1F_2R, BIOME_CAVE},
    {MAP_SHOAL_CAVE_LOW_TIDE_STAIRS_ROOM, BIOME_CAVE},
    {MAP_SHOAL_CAVE_LOW_TIDE_LOWER_ROOM, BIOME_CAVE},
    {MAP_SHOAL_CAVE_LOW_TIDE_INNER_ROOM, BIOME_CAVE},
    {MAP_LILYCOVE_CITY, BIOME_COUNT},
    {MAP_DEWFORD_TOWN, BIOME_COUNT},
    {MAP_SLATEPORT_CITY, BIOME_COUNT},
    {MAP_MOSSDEEP_CITY, BIOME_COUNT},
    {MAP_PACIFIDLOG_TOWN, BIOME_COUNT},
    {MAP_EVER_GRANDE_CITY, BIOME_COUNT},
    {MAP_PETALBURG_CITY, BIOME_COUNT},
    {MAP_UNDERWATER_ROUTE124, BIOME_COUNT},
    {MAP_SHOAL_CAVE_LOW_TIDE_ICE_ROOM, BIOME_CAVE},
    {MAP_SOOTOPOLIS_CITY, BIOME_COUNT},
    {MAP_SAFARI_ZONE_SOUTHEAST, BIOME_FOREST},
    {MAP_SAFARI_ZONE_NORTHEAST, BIOME_FOREST},
    {MAP_MAGMA_HIDEOUT_2F_1R, BIOME_VOLCANO},
    {MAP_MAGMA_HIDEOUT_2F_2R, BIOME_VOLCANO},
    {MAP_MAGMA_HIDEOUT_3F_1R, BIOME_VOLCANO},
    {MAP_MAGMA_HIDEOUT_3F_2R, BIOME_VOLCANO},
    {MAP_MAGMA_HIDEOUT_4F, BIOME_VOLCANO},
    {MAP_MAGMA_HIDEOUT_3F_3R, BIOME_VOLCANO},
    {MAP_MAGMA_HIDEOUT_2F_3R, BIOME_VOLCANO},
    {MAP_MIRAGE_TOWER_1F, BIOME_CAVE},
    {MAP_MIRAGE_TOWER_2F, BIOME_CAVE},
    {MAP_MIRAGE_TOWER_3F, BIOME_CAVE},
    {MAP_MIRAGE_TOWER_4F, BIOME_CAVE},
    {MAP_DESERT_UNDERPASS, BIOME_CAVE},
    {MAP_ARTISAN_CAVE_B1F, BIOME_CAVE},
    {MAP_ARTISAN_CAVE_1F, BIOME_CAVE},
    {MAP_ALTERING_CAVE, BIOME_CAVE},
    {MAP_METEOR_FALLS_STEVENS_CAVE, BIOME_CAVE},
    {MAP_ROUTE101, BIOME_FOREST},
    {MAP_ROUTE102, BIOME_FOREST},
    {MAP_ROUTE103, BIOME_FOREST},
    {MAP_ROUTE104, BIOME_FOREST},
    {MAP_PETALBURG_WOODS, BIOME_FOREST},
    {MAP_RUSTURF_TUNNEL, BIOME_CAVE},
    {MAP_ROUTE105, BIOME_COUNT},
    {MAP_ROUTE109, BIOME_COUNT},
    {MAP_ROUTE110, BIOME_FOREST},
    {MAP_ROUTE111, BIOME_VOLCANO},
    {MAP_ROUTE112, BIOME_VOLCANO},
    {MAP_FIERY_PATH, BIOME_VOLCANO},
    {MAP_JAGGED_PASS, BIOME_VOLCANO},
    {MAP_ROUTE113, BIOME_VOLCANO},
    {MAP_ROUTE114, BIOME_FOREST},
    {MAP_ROUTE115, BIOME_FOREST},
    {MAP_METEOR_FALLS_1F_1R, BIOME_CAVE},
    {MAP_METEOR_FALLS_B1F_1R, BIOME_CAVE},
    {MAP_ROUTE116, BIOME_FOREST},
    {MAP_ROUTE117, BIOME_FOREST},
    {MAP_ROUTE118, BIOME_FOREST},
    {MAP_ROUTE119, BIOME_FOREST},
    {MAP_GRANITE_CAVE_1F, BIOME_CAVE},
    {MAP_GRANITE_CAVE_B1F, BIOME_CAVE},
    {MAP_MT_CHIMNEY, BIOME_VOLCANO},
    {MAP_MT_PYRE_EXTERIOR, BIOME_CAVE},
    {MAP_MT_PYRE_1F, BIOME_CAVE},
    {MAP_MAGMA_HIDEOUT_1F, BIOME_VOLCANO},
    {MAP_NEW_MAUVILLE_INSIDE, BIOME_POWER_PLANT},
    {MAP_ROUTE120, BIOME_FOREST},
    {MAP_ROUTE121, BIOME_FOREST},
    {MAP_ROUTE123, BIOME_FOREST},
    {MAP_SHOAL_CAVE_LOW_TIDE_ENTRANCE_ROOM, BIOME_CAVE},
    {MAP_ROUTE130, BIOME_FOREST},
    {MAP_SEAFLOOR_CAVERN_ENTRANCE, BIOME_CAVE},
    {MAP_CAVE_OF_ORIGIN_1F, BIOME_CAVE},
    {MAP_VICTORY_ROAD_1F, BIOME_CAVE},
    {MAP_VICTORY_ROAD_B1F, BIOME_CAVE},
    {MAP_VICTORY_ROAD_B2F, BIOME_CAVE},
    {MAP_SKY_PILLAR_1F, BIOME_CAVE},
    {MAP_SKY_PILLAR_3F, BIOME_CAVE},
    {MAP_SKY_PILLAR_5F, BIOME_CAVE},
    {MAP_AQUA_HIDEOUT_1F, BIOME_COUNT},
};

// Families placed by hand where the sheet's maps don't say it (an octopus lives on the beach even
// if the sheet puts it on a route's land)
static const struct { u16 species; u8 biome; } sHabitatExtras[] =
{
    {SPECIES_CLOBBOPUS, BIOME_BEACH},
    {SPECIES_PAWMI, BIOME_POWER_PLANT},   // the sheet places Pawmo and Charjabug: their first stages too
    {SPECIES_GRUBBIN, BIOME_POWER_PLANT},
};

// Which families live in each theme: every species on the theme's encounter tables (Jacob's sheet)
// and everything it evolves into. Built once from the live tables.
static EWRAM_DATA u8 sHabitats[BIOME_COUNT][(NUM_SPECIES + 7) / 8] = {0};
static EWRAM_DATA const struct WildPokemonHeader *sHabitatsFrom = NULL;

static void AddToHabitat(u32 biome, enum Species species, u32 depth)
{
    const struct Evolution *evos;
    u32 i;
    if (species == SPECIES_NONE || species >= NUM_SPECIES || depth > 3)
        return;
    sHabitats[biome][species / 8] |= 1 << (species % 8);
    evos = GetSpeciesEvolutions(species);
    for (i = 0; evos != NULL && evos[i].method != EVOLUTIONS_END; i++)
        AddToHabitat(biome, evos[i].targetSpecies, depth + 1);
}

static void AddTableToHabitat(u32 biome, const struct WildPokemonInfo *info, u32 count)
{
    u32 i;
    if (info == NULL || biome >= BIOME_COUNT)
        return;
    for (i = 0; i < count; i++)
        AddToHabitat(biome, info->wildPokemon[i].species, 0);
}

static void BuildHabitats(void)
{
    const struct WildPokemonHeader *headers = gWildMonHeaders;
    u32 i, j, t;

    if (sHabitatsFrom == headers)
        return;
    memset(sHabitats, 0, sizeof(sHabitats));
    for (i = 0; headers[i].mapGroup != MAP_GROUP(MAP_UNDEFINED); i++)
    {
        u16 map = (headers[i].mapGroup << 8) | headers[i].mapNum;
        for (j = 0; j < ARRAY_COUNT(sHabitatMaps) && sHabitatMaps[j].map != map; j++)
            ;
        if (j == ARRAY_COUNT(sHabitatMaps))
            continue; // not a Hoenn map
        for (t = 0; t < TIMES_OF_DAY_COUNT; t++)
        {
            const struct WildEncounterTypes *types = &headers[i].encounterTypes[t];
            AddTableToHabitat(sHabitatMaps[j].landBiome, types->landMonsInfo, NUM_LAND_MONS_ENCOUNTER_SLOTS);
            AddTableToHabitat(BIOME_BEACH, types->waterMonsInfo, NUM_WATER_MONS_ENCOUNTER_SLOTS);
            AddTableToHabitat(BIOME_BEACH, types->fishingMonsInfo, NUM_FISHING_MONS_ENCOUNTER_SLOTS);
            AddTableToHabitat(BIOME_CAVE, types->rockSmashMonsInfo, NUM_ROCK_SMASH_MONS_ENCOUNTER_SLOTS);
        }
    }
    for (i = 0; i < ARRAY_COUNT(sHabitatExtras); i++)
        AddToHabitat(sHabitatExtras[i].biome, sHabitatExtras[i].species, 0);
    sHabitatsFrom = headers;
}

static bool32 LivesIn(enum Species species, u32 biome)
{
    return species < NUM_SPECIES && (sHabitats[biome][species / 8] & (1 << (species % 8)));
}

// For tests: biome 0 forest, 1 beach, 2 cave, 3 volcano
bool32 Rogue_LivesIn(u16 species, u32 biome)
{
    BuildHabitats();
    return biome < BIOME_COUNT && LivesIn(species, biome);
}

// Wild Pokémon live where Jacob's encounter sheet puts their family
static enum Species PickRoomSpecies(rng_value_t *rng)
{
    u32 biome = GetRoomBiome(), minBst, maxBst, tries;
    bool32 unevolvedOnly = (GetFloor() == 1);

    BuildHabitats();
    GetFloorBstRange(GetFloor(), &minBst, &maxBst);
    for (tries = 0; tries < 4000; tries++)
    {
        enum Species species = 1 + LocalRandom(rng) % NATIONAL_DEX_COUNT;
        u32 bst;
        if (!LivesIn(species, biome) || !IsSpeciesAllowed(species))
            continue;
        bst = GetSpeciesBaseStatTotal(species);
        if (bst < minBst || bst > maxBst || (unevolvedOnly && GetSpeciesPreEvolution(species) != SPECIES_NONE))
            continue;
        return species;
    }
    return PickFloorSpecies(rng, TYPE_NONE);
}

// Bosses fight in an Elite Four room that fits their type
static u16 GetArenaMap(enum Type type)
{
    switch (type)
    {
    case TYPE_GHOST:
    case TYPE_PSYCHIC:
    case TYPE_FAIRY:
        return MAP_ROGUE_ARENA_PHOEBE;
    case TYPE_ICE:
    case TYPE_WATER:
    case TYPE_FLYING:
        return MAP_ROGUE_ARENA_GLACIA;
    case TYPE_DARK:
    case TYPE_FIGHTING:
    case TYPE_POISON:
    case TYPE_NORMAL:
        return MAP_ROGUE_ARENA_SIDNEY;
    default:
        return MAP_ROGUE_ARENA_DRAKE;
    }
}

static u16 GetRoomMap(void)
{
    switch (GetRoomKind())
    {
    case ROOM_KIND_ARENA:
        return GetArenaMap(GetCurrentBoss()->type);
    case ROOM_KIND_STOP:
        return MAP_ROGUE_STOP;
    case ROOM_KIND_WALLED:
        switch (GetRoomBiome())
        {
        case BIOME_CAVE:
            return MAP_ROGUE_CAVE;
        case BIOME_POWER_PLANT:
            return MAP_ROGUE_POWER_PLANT;
        }
        return MAP_ROGUE_VOLCANO;
    }
    return MAP_ROGUE_ROOM;
}

static bool32 IsRogueMap(u32 mapGroup, u32 mapNum)
{
    static const u16 sMaps[] = {MAP_ROGUE_ROOM, MAP_ROGUE_CAVE, MAP_ROGUE_VOLCANO, MAP_ROGUE_POWER_PLANT, MAP_ROGUE_ARENA_SIDNEY,
                                MAP_ROGUE_ARENA_PHOEBE, MAP_ROGUE_ARENA_GLACIA, MAP_ROGUE_ARENA_DRAKE,
                                MAP_ROGUE_STOP};
    u32 i;
    for (i = 0; i < ARRAY_COUNT(sMaps); i++)
    {
        if (MAP_GROUP(sMaps[i]) == mapGroup && MAP_NUM(sMaps[i]) == mapNum)
            return TRUE;
    }
    return FALSE;
}

// The door script's warp: to the map of the room the run is now in
void Rogue_WarpToRoom(void)
{
    u16 map = GetRoomMap();
    s32 x = ROGUE_ENTRANCE_X, y = ROGUE_ENTRANCE_Y;
    if (GetRoomKind() == ROOM_KIND_ARENA)
    {
        x = ARENA_ENTRANCE_X;
        y = ARENA_ENTRANCE_Y;
    }
    else if (GetRoomKind() == ROOM_KIND_STOP)
    {
        x = STOP_ENTRANCE_X;
        y = STOP_ENTRANCE_Y;
    }
    SetWarpDestination(MAP_GROUP(map), MAP_NUM(map), WARP_ID_NONE, x, y);
    DoWarp();
    ResetInitialPlayerAvatarState();
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
    return sPlan.kind == ROOM_KIND_OUTDOOR && sPlan.tree[y / 2][x / 2];
}

// Beach: a strip of sea along one side (the sea tile and its shore, which can't be walked on)
static bool32 IsSeaTile(s32 x, s32 y)
{
    if (sPlan.seaSide == SEA_NONE || y < INNER_Y_MIN || y > INNER_Y_MAX)
        return FALSE;
    if (sPlan.seaSide == SEA_LEFT)
        return x == INNER_X_MIN || x == INNER_X_MIN + 1;
    return x == INNER_X_MAX || x == INNER_X_MAX - 1;
}

static bool32 IsBuildingTile(s32 x, s32 y)
{
    if (sPlan.building == BUILDING_NONE)
        return FALSE;
    return x >= sPlan.buildingX && x < sPlan.buildingX + BUILDING_W
        && y > BUILDING_Y && y < BUILDING_Y + BUILDING_H; // the roof's top row overhangs walkable ground
}

static u32 GetBuildingDoorX(void)
{
    return sPlan.buildingX + 1;
}

static u32 GetDoorX(u32 door)
{
    return sDoorBlockX[door] * 2;
}

// Walled rooms: the ways through the top wall (a cave mouth's middle, or a volcano corridor)
static bool32 IsWallOpening(s32 x, s32 y)
{
    u32 i;
    for (i = 0; i < NUM_DOORS; i++)
    {
        s32 gx = GetDoorX(i);
        if (sPlan.doors[i] == NO_DOOR || y > INNER_Y_MIN - 1)
            continue;
        if (sPlan.biome == BIOME_CAVE ? (x == gx && y == INNER_Y_MIN - 1) : (x >= gx && x <= gx + 1))
            return TRUE;
    }
    return FALSE;
}

static bool32 IsBlockedTile(s32 x, s32 y)
{
    if (x < 0 || x >= ROOM_W || y < 0 || y >= ROOM_H)
        return TRUE;
    if (sPlan.kind == ROOM_KIND_WALLED)
    {
        if (x >= INNER_X_MIN && x <= INNER_X_MAX && y >= INNER_Y_MIN && y <= INNER_Y_MAX)
            return sPlan.rock[y][x];
        return !IsWallOpening(x, y);
    }
    return IsTreeTile(x, y) || IsWaterTile(x, y) || IsSeaTile(x, y) || IsBuildingTile(x, y) || sPlan.rock[y][x];
}

// Every door, and the building's door, can be walked to from the entrance
static bool32 AreDoorsReachable(void)
{
    // seen: 0 = not yet, 1 = blocked, 2 = reached
    u8 seen[ROOM_H][ROOM_W];
    u16 queue[ROOM_W * ROOM_H];
    u32 head = 0, tail = 0, i;
    s32 x, y;

    for (y = 0; y < ROOM_H; y++)
    {
        for (x = 0; x < ROOM_W; x++)
            seen[y][x] = IsBlockedTile(x, y);
    }
    seen[ROGUE_ENTRANCE_Y][ROGUE_ENTRANCE_X] = 2;
    queue[tail++] = ROGUE_ENTRANCE_Y * ROOM_W + ROGUE_ENTRANCE_X;
    while (head < tail)
    {
        static const s8 sDirs[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
        x = queue[head] % ROOM_W;
        y = queue[head] / ROOM_W;
        head++;
        for (i = 0; i < 4; i++)
        {
            s32 nx = x + sDirs[i][0], ny = y + sDirs[i][1];
            if (nx < 0 || nx >= ROOM_W || ny < 0 || ny >= ROOM_H || seen[ny][nx])
                continue;
            seen[ny][nx] = 2;
            queue[tail++] = ny * ROOM_W + nx;
        }
    }
    for (i = 0; i < NUM_DOORS; i++)
    {
        u32 gx = GetDoorX(i);
        if (sPlan.doors[i] != NO_DOOR && seen[INNER_Y_MIN][gx] != 2 && !(sPlan.biome != BIOME_CAVE && seen[INNER_Y_MIN][gx + 1] == 2))
            return FALSE;
    }
    if (sPlan.building != BUILDING_NONE && seen[BUILDING_Y + BUILDING_H][GetBuildingDoorX()] != 2)
        return FALSE;
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
    u32 nextFloor = GetFloor() + (type == ROGUE_ROOM_REST_STOP);

    for (i = 0; i < NUM_DOORS; i++)
    {
        sPlan.doors[i] = NO_DOOR;
        sPlan.doorBiomes[i] = BIOME_FOREST;
    }

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
    if (type == ROGUE_ROOM_BOSS)
    {
        // An arena has one way out: to the rest stop, or in the finale to the next challenger
        sPlan.doors[1] = IsFinale(GetFloor()) ? ROGUE_ROOM_BOSS : ROGUE_ROOM_REST_STOP;
        return;
    }
    if (type == ROGUE_ROOM_REST_STOP)
    {
        // The rest stop leads into the next floor, or after the last floor to the finale
        if (GetFloor() >= GetNumFloors())
        {
            sPlan.doors[1] = ROGUE_ROOM_BOSS;
            return;
        }
        sPlan.doors[1] = PickWeighted(rng, sDoorWeights, ROGUE_ROOM_TYPE_COUNT, 0);
        sPlan.doorBiomes[1] = PickBiome(rng, sPlan.doors[1], nextFloor);
        return;
    }
    if (GetRoom() >= ROGUE_ROOMS_PER_FLOOR)
    {
        sPlan.doors[1] = ROGUE_ROOM_BOSS;
        return;
    }

    count = 2 + LocalRandom(rng) % 2;
    for (i = 0; i < count; i++)
    {
        u32 door = PickWeighted(rng, sDoorWeights, ROGUE_ROOM_TYPE_COUNT, used);
        u32 slot = (count == 2) ? i * 2 : i;
        used |= 1u << door;
        sPlan.doors[slot] = door;
        sPlan.doorBiomes[slot] = PickBiome(rng, door, nextFloor);
    }
}

static bool32 IsTileFree(u32 x, u32 y)
{
    u32 i;
    if (IsBlockedTile(x, y) || IsSeaTile(x, y))
        return FALSE;
    if (x == ROGUE_ENTRANCE_X && y >= ROGUE_ENTRANCE_Y - 1)
        return FALSE;
    if (y == INNER_Y_MIN)
        return FALSE; // keep the row under the doors clear
    if (sPlan.building != BUILDING_NONE && x == GetBuildingDoorX() && y == BUILDING_Y + BUILDING_H)
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
        u32 x = INNER_X_MIN + LocalRandom(rng) % (INNER_X_MAX - INNER_X_MIN + 1);
        u32 y = INNER_Y_MIN + 1 + LocalRandom(rng) % (INNER_Y_MAX - INNER_Y_MIN);
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

// Trainer and boss rooms have a trainer at the doors; so do half the wild and item rooms, so a floor
// has enough battles to keep the team's levels up with the boss
static bool32 IsRoomGuarded(u32 type)
{
    if (type == ROGUE_ROOM_TRAINER || type == ROGUE_ROOM_BOSS)
        return TRUE;
    if (type == ROGUE_ROOM_WILD || type == ROGUE_ROOM_ITEM)
        return Mix32(GetRoomSeed() ^ 0x6A7D) % 100 < 50;
    return FALSE;
}

static void AddTrainerObject(void)
{
    // Trainers and bosses don't spot the player: talk to them to battle (Jacob, 2026-10-03)
    AddObject(sTrainerKinds[GetTrainerKind()].graphicsId, CENTER_X, CENTER_Y, MOVEMENT_TYPE_FACE_DOWN, Rogue_Room_EventScript_Trainer);
}

// Outdoor rooms: the forest border with an opening per door, and trees, ponds or sea inside
static void PlanOutdoorRoom(rng_value_t *rng, u32 type, bool32 hasCenterObject)
{
    u32 bx, by, x, y, i, count;
    bool32 beach = (sPlan.biome == BIOME_BEACH);

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
        }
    }
    if (beach)
        sPlan.seaSide = (LocalRandom(rng) % 2) ? SEA_LEFT : SEA_RIGHT;

    // A Mart or a Pokémon Center to walk into
    if (type == ROGUE_ROOM_SHOP)
    {
        sPlan.building = BUILDING_MART;
        sPlan.buildingX = MART_X;
    }
    else if (type == ROGUE_ROOM_REST)
    {
        sPlan.building = BUILDING_CENTER;
        sPlan.buildingX = CENTER_BUILDING_X;
    }

    // Forest: sometimes a pond (1x1 to 2x2 blocks), never right under a door and with grass below it
    if (!beach && type != ROGUE_ROOM_HUB && sPlan.building == BUILDING_NONE && LocalRandom(rng) % 100 < 40)
    {
        u32 w = 1 + LocalRandom(rng) % 2, h = 1 + LocalRandom(rng) % 2;
        u32 bx0 = INNER_BX_MIN + LocalRandom(rng) % (INNER_BX_MAX - INNER_BX_MIN + 2 - w);
        u32 by0 = INNER_BY_MIN + 1 + LocalRandom(rng) % (INNER_BY_MAX - INNER_BY_MIN - h);
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

    // A few trees inside (fewer on a beach), never closing off a door
    if (type != ROGUE_ROOM_HUB)
    {
        count = beach ? LocalRandom(rng) % 3 : 2 + LocalRandom(rng) % 4;
        for (i = 0; i < count * 4 && count > 0; i++)
        {
            bx = INNER_BX_MIN + LocalRandom(rng) % (INNER_BX_MAX - INNER_BX_MIN + 1);
            by = INNER_BY_MIN + LocalRandom(rng) % (INNER_BY_MAX - INNER_BY_MIN + 1);
            if (sPlan.tree[by][bx] || (bx == ENTRANCE_BX && by == ENTRANCE_BY))
                continue;
            if (by == INNER_BY_MIN && (bx == 3 || bx == 5 || bx == 7))
                continue; // right under a door
            if (hasCenterObject && bx == CENTER_X / 2 && by <= CENTER_Y / 2 + 1)
                continue;
            if (sPlan.water[by][bx] || IsWaterBlock(bx, by - 1))
                continue; // not in a pond, nor right below one (its shore)
            if (sPlan.building != BUILDING_NONE && bx >= sPlan.buildingX / 2 - 1 && bx <= (sPlan.buildingX + BUILDING_W) / 2
             && by >= BUILDING_Y / 2 && by <= (BUILDING_Y + BUILDING_H) / 2 + 1)
                continue; // around the building
            if (beach && (bx == INNER_BX_MIN || bx == INNER_BX_MAX))
                continue; // the sea side
            sPlan.tree[by][bx] = TRUE;
            if (!AreDoorsReachable())
                sPlan.tree[by][bx] = FALSE;
            else
                count--;
        }
    }

    // Patches of tall grass in a forest (just for looks: there are no random encounters here)
    if (!beach)
    {
        count = 2 + LocalRandom(rng) % 3;
        for (i = 0; i < count; i++)
        {
            u32 w = 2 + LocalRandom(rng) % 4, h = 2 + LocalRandom(rng) % 2;
            u32 x0 = 4 + LocalRandom(rng) % (14 - w + 1), y0 = 4 + LocalRandom(rng) % (10 - h + 1);
            for (y = y0; y < y0 + h; y++)
            {
                for (x = x0; x < x0 + w; x++)
                    sPlan.ground[y][x] = GROUND_TALL_GRASS;
            }
        }
    }
}

// Walled rooms (cave, volcano): a rectangle of rock walls with a few rocks inside
static void PlanWalledRoom(rng_value_t *rng, bool32 hasCenterObject)
{
    u32 i, count = 3 + LocalRandom(rng) % 5;

    for (i = 0; i < count * 4 && count > 0; i++)
    {
        u32 x = INNER_X_MIN + LocalRandom(rng) % (INNER_X_MAX - INNER_X_MIN + 1);
        u32 y = INNER_Y_MIN + 1 + LocalRandom(rng) % (INNER_Y_MAX - INNER_Y_MIN - 1);
        if (sPlan.rock[y][x] || (x == ROGUE_ENTRANCE_X && y >= ROGUE_ENTRANCE_Y - 2))
            continue;
        if (hasCenterObject && x >= CENTER_X - 1 && x <= CENTER_X + 1 && y <= CENTER_Y + 1)
            continue;
        sPlan.rock[y][x] = TRUE;
        if (!AreDoorsReachable())
            sPlan.rock[y][x] = FALSE;
        else
            count--;
    }
}

static void BuildRoomPlan(void)
{
    rng_value_t rng = LocalRandomSeed(GetRoomSeed());
    u32 type = GetRoomType();
    bool32 guarded = IsRoomGuarded(type);
    bool32 hasCenterObject = (guarded || type == ROGUE_ROOM_EVENT);
    u32 i, count;

    memset(&sPlan, 0, sizeof(sPlan));
    sPlan.guarded = guarded;
    sPlan.kind = GetRoomKind();
    sPlan.biome = GetRoomBiome();
    ChooseDoors(&rng);

    if (sPlan.kind == ROOM_KIND_STOP)
        return; // its people are on the map
    if (sPlan.kind == ROOM_KIND_ARENA)
    {
        AddObject(GetCurrentBoss()->graphicsId, ARENA_BOSS_X, ARENA_BOSS_Y, MOVEMENT_TYPE_FACE_DOWN, Rogue_Room_EventScript_Boss);
        return;
    }
    if (sPlan.kind == ROOM_KIND_WALLED)
        PlanWalledRoom(&rng, hasCenterObject);
    else
        PlanOutdoorRoom(&rng, type, hasCenterObject);

    // What's in the room
    switch (type)
    {
    case ROGUE_ROOM_HUB:
        AddObject(OBJ_EVENT_GFX_PROF_BIRCH, 13, 9, MOVEMENT_TYPE_FACE_DOWN, Rogue_Room_EventScript_Guide);
        if (IsMetaOn())
            AddObject(OBJ_EVENT_GFX_SCIENTIST_1, 7, 9, MOVEMENT_TYPE_FACE_DOWN, Rogue_Room_EventScript_UnlockShop);
        break;
    case ROGUE_ROOM_WILD:
        if (guarded)
            AddTrainerObject();
        count = 1 + (LocalRandom(&rng) % 100 < 35);
        for (i = 0; i < count; i++)
            AddObjectAnywhere(&rng, OBJ_EVENT_MON + PickRoomSpecies(&rng), MOVEMENT_TYPE_LOOK_AROUND, Rogue_Room_EventScript_WildMon);
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
        if (guarded)
            AddTrainerObject();
        count = 2 + LocalRandom(&rng) % 2;
        for (i = 0; i < count; i++)
            AddObjectAnywhere(&rng, OBJ_EVENT_GFX_ITEM_BALL, MOVEMENT_TYPE_NONE, Rogue_Room_EventScript_ItemBall);
        break;
    case ROGUE_ROOM_TRAINER:
        AddTrainerObject();
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
    if (sPlan.kind == ROOM_KIND_ARENA)
        return count;
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
    bool32 cave, rocks;

    BuildRoomPlan();
    cave = (sPlan.kind == ROOM_KIND_WALLED && sPlan.biome == BIOME_CAVE);
    rocks = (sPlan.kind == ROOM_KIND_WALLED && sPlan.biome != BIOME_VOLCANO); // a cave's or power plant's doors
    ClearTrainerFlag(TRAINER_ROGUE_TRAINER);
    ClearTrainerFlag(TRAINER_ROGUE_BOSS);
    // Leaving the room's Mart or Pokémon Center comes back out in front of its door
    if (sPlan.building != BUILDING_NONE)
        SetDynamicWarpWithCoords(0, gSaveBlock1Ptr->location.mapGroup, gSaveBlock1Ptr->location.mapNum, WARP_ID_NONE,
                                 GetBuildingDoorX(), BUILDING_Y + BUILDING_H);
    for (i = 0; i < NUM_OBJECT_SLOTS; i++)
    {
        struct ObjectEventTemplate *template = &gSaveBlock1Ptr->objectEventTemplates[i];
        bool32 doorShown = sPlan.kind != ROOM_KIND_ARENA;
        if (i < NUM_MARKERS && doorShown && sPlan.doors[i] != NO_DOOR)
        {
            SetObjectTemplate(i, GetDoorMarkerGraphics(sPlan.doors[i]), GetDoorX(i), cave ? 1 : 2,
                              MOVEMENT_TYPE_FACE_DOWN, TRAINER_TYPE_NONE, 0, NULL);
        }
        else if (i >= NUM_MARKERS && i < FIRST_THICKET_SLOT && i - NUM_MARKERS < sPlan.numObjects)
        {
            const struct RoomObject *obj = &sPlan.objects[i - NUM_MARKERS];
            SetObjectTemplate(i, obj->graphicsId, obj->x, obj->y, obj->movementType, obj->trainerType, obj->sight, obj->script);
        }
        else if (i >= FIRST_THICKET_SLOT && doorShown && sPlan.guarded && sPlan.doors[(i - FIRST_THICKET_SLOT) / 2] != NO_DOOR
              && !(cave && (i - FIRST_THICKET_SLOT) % 2))
        {
            u32 door = (i - FIRST_THICKET_SLOT) / 2;
            SetObjectTemplate(i, rocks ? OBJ_EVENT_GFX_BREAKABLE_ROCK : OBJ_EVENT_GFX_CUTTABLE_TREE,
                              GetDoorX(door) + (i - FIRST_THICKET_SLOT) % 2, 3,
                              MOVEMENT_TYPE_NONE, TRAINER_TYPE_NONE, 0, Rogue_Room_EventScript_Thicket);
        }
        else
        {
            FlagSet(template->flagId);
        }
    }
}

// ---------------------------------------------------------------------------------------------
// Drawing

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

// Doorway ground for a cave or volcano room behind it: plain mountain ground from the General
// tileset (no behavior: 0x1A0-0x1A9 looked close but are secret base entrances and warp there)
#define DOOR_TILE_CAVE    0x07D
#define DOOR_TILE_VOLCANO 0x06D
#define DOOR_TILE_POWER   0x0AF // metal stairs

// The door openings' ground shows the next room's theme
static u16 GetDoorGroundMetatile(s32 x, s32 y)
{
    u32 i;
    bool32 left = (x % 2) == 0;
    for (i = 0; i < NUM_DOORS; i++)
    {
        if (sPlan.doors[i] == NO_DOOR || (x != GetDoorX(i) && x != GetDoorX(i) + 1))
            continue;
        if (IsBuildingRoom(sPlan.doors[i]) || sPlan.doors[i] == ROGUE_ROOM_BOSS || sPlan.doors[i] > ROGUE_ROOM_BOSS)
            break; // a dirt path
        switch (sPlan.doorBiomes[i])
        {
        case BIOME_BEACH:
            return TILE_GROUND(0x124);
        case BIOME_CAVE:
            return TILE_GROUND(DOOR_TILE_CAVE);
        case BIOME_VOLCANO:
            return TILE_GROUND(DOOR_TILE_VOLCANO);
        case BIOME_POWER_PLANT:
            return TILE_GROUND(DOOR_TILE_POWER);
        }
        break;
    }
    if (y == INNER_BY_MIN * 2 - 1)
        return TILE_GROUND(left ? 0x1E0 : 0x1E2);
    return TILE_GROUND(left ? 0x1D8 : 0x1DA);
}

static u16 GetBuildingMetatile(s32 x, s32 y)
{
    static const u16 sMart[BUILDING_H][BUILDING_W] =
    {
        {0x028, 0x029, 0x029, 0x029},
        {0x030, 0x031, 0x032, 0x033},
        {0x038, 0x039, 0x03A, 0x03B},
        {0x060, 0x041, 0x042, 0x043},
    };
    static const u16 sCenter[BUILDING_H][BUILDING_W] =
    {
        {0x048, 0x049, 0x04A, 0x04B},
        {0x050, 0x051, 0x052, 0x053},
        {0x058, 0x059, 0x05A, 0x05B},
        {0x060, 0x061, 0x062, 0x063},
    };
    u32 col = x - sPlan.buildingX, row = y - BUILDING_Y;
    u16 id = (sPlan.building == BUILDING_MART ? sMart : sCenter)[row][col];
    return row == 0 ? TILE_GROUND(id) : TILE_TREE(id); // the roof's top row is walkable
}

static u16 GetOutdoorGroundMetatile(s32 x, s32 y)
{
    bool32 left = (x % 2) == 0;
    bool32 treeBelow = IsTreeTile(x, y + 1) && ((y + 1) % 2) == 0 && y + 1 < ROOM_H;
    bool32 beach = (sPlan.biome == BIOME_BEACH);

    if (y < INNER_Y_MIN)
        return GetDoorGroundMetatile(x, y);
    if (IsWaterTile(x, y - 1))
        return TILE_GROUND(0x002);
    if (sPlan.ground[y][x] == GROUND_TALL_GRASS)
        return TILE_GROUND(treeBelow ? (left ? 0x1C6 : 0x1C7) : METATILE_General_TallGrass);
    if (treeBelow)
        return TILE_GROUND(left ? 0x1CE : 0x1CF);
    return TILE_GROUND(beach ? 0x124 : METATILE_General_Grass);
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

// Sea as on Route 109: the shore tile has the sand
static u16 GetSeaMetatile(s32 x, s32 y)
{
    bool32 shore = (sPlan.seaSide == SEA_LEFT) ? (x == INNER_X_MIN + 1) : (x == INNER_X_MAX - 1);
    u16 id = shore ? (sPlan.seaSide == SEA_LEFT ? 0x123 : 0x125) : 0x170;
    return id | (1 << MAPGRID_ELEVATION_SHIFT);
}

static void DrawOutdoorRoom(void)
{
    s32 x, y;
    for (y = 0; y < ROOM_H; y++)
    {
        for (x = 0; x < ROOM_W; x++)
        {
            u16 tile;
            if (IsTreeTile(x, y))
                tile = GetTreeMetatile(x, y);
            else if (IsWaterTile(x, y))
                tile = GetWaterMetatile(x, y);
            else if (IsSeaTile(x, y))
                tile = GetSeaMetatile(x, y);
            else if (sPlan.building != BUILDING_NONE && x >= sPlan.buildingX && x < sPlan.buildingX + BUILDING_W
                  && y >= BUILDING_Y && y < BUILDING_Y + BUILDING_H)
                tile = GetBuildingMetatile(x, y);
            else
                tile = GetOutdoorGroundMetatile(x, y);
            MapGridSetMetatileEntryAt(x + MAP_OFFSET, y + MAP_OFFSET, tile);
        }
    }
}

// Walled rooms' tiles (Granite Cave and Fiery Path)
struct WallTiles
{
    u16 fill, floor, floorAlt, rock, rockAlt;
    u16 top, topLeft, topRight, left, right, leftTop, rightTop, bottom, bottomLeft, bottomRight;
};

static const struct WallTiles sCaveTiles =
{
    .fill = 0x211, .floor = 0x201, .floorAlt = 0x201, .rock = 0x202, .rockAlt = 0x203,
    .top = 0x219, .topLeft = 0x21B, .topRight = 0x21C, .left = 0x212, .right = 0x210, .leftTop = 0x212, .rightTop = 0x210,
    .bottom = 0x221, .bottomLeft = 0x224, .bottomRight = 0x224,
};

static const struct WallTiles sVolcanoTiles =
{
    .fill = 0x271, .floor = 0x308, .floorAlt = 0x310, .rock = 0x30D, .rockAlt = 0x30D,
    .top = 0x274, .topLeft = 0x27B, .topRight = 0x27C, .left = 0x307, .right = 0x306, .leftTop = 0x30F, .rightTop = 0x30E,
    .bottom = 0x30C, .bottomLeft = 0x27F, .bottomRight = 0x27E,
};

// The ground in front of a walled room's door shows the next room's theme (all from the General
// tileset, which every room map shares)
static bool32 GetWalledDoorPreview(s32 x, s32 y, u16 *tile)
{
    u32 i;
    if (y != INNER_Y_MIN)
        return FALSE;
    for (i = 0; i < NUM_DOORS; i++)
    {
        u32 biome = sPlan.doorBiomes[i];
        if (sPlan.doors[i] == NO_DOOR || x < GetDoorX(i) || x > GetDoorX(i) + (sPlan.biome != BIOME_CAVE))
            continue;
        if (biome == BIOME_FOREST && sPlan.doors[i] < ROGUE_ROOM_BOSS && !IsBuildingRoom(sPlan.doors[i]))
            *tile = TILE_GROUND(METATILE_General_Grass);
        else if (biome == BIOME_BEACH)
            *tile = TILE_GROUND(0x124);
        else if (biome == sPlan.biome)
            return FALSE;
        else if (biome == BIOME_CAVE)
            *tile = TILE_GROUND(DOOR_TILE_CAVE);
        else if (biome == BIOME_VOLCANO)
            *tile = TILE_GROUND(DOOR_TILE_VOLCANO);
        else if (biome == BIOME_POWER_PLANT)
            *tile = TILE_GROUND(DOOR_TILE_POWER);
        else
            return FALSE;
        return TRUE;
    }
    return FALSE;
}

// The power plant (New Mauville's tiles, gTileset_BikeShop): a three-tile top wall, corridors as doors
static u16 GetPowerPlantMetatile(s32 x, s32 y)
{
    u32 i;
    u16 preview;

    if (x >= INNER_X_MIN && x <= INNER_X_MAX && y >= INNER_Y_MIN && y <= INNER_Y_MAX)
    {
        if (sPlan.rock[y][x])
            return TILE_TREE(((x + y) % 2) ? 0x2C1 : 0x2C0); // crates
        if (GetWalledDoorPreview(x, y, &preview))
            return preview;
        return TILE_GROUND(0x210);
    }
    for (i = 0; i < NUM_DOORS; i++)
    {
        s32 gx = GetDoorX(i);
        if (sPlan.doors[i] == NO_DOOR || y >= INNER_Y_MIN)
            continue;
        if (x == gx || x == gx + 1)
            return TILE_GROUND(0x210);
        if (x == gx - 1)
            return TILE_TREE(y == 1 ? 0x26A : 0x272);
        if (x == gx + 2)
            return TILE_TREE(y == 1 ? 0x268 : 0x270);
    }
    if (y >= 1 && y <= 3 && x >= INNER_X_MIN && x <= INNER_X_MAX)
        return TILE_TREE(y == 1 ? 0x21F : (y == 2 ? 0x227 : 0x22F));
    if (y == 1 && x == INNER_X_MIN - 1)
        return TILE_TREE(0x268);
    if (y == 1 && x == INNER_X_MAX + 1)
        return TILE_TREE(0x26A);
    if (y >= 2 && y <= INNER_Y_MAX && x == INNER_X_MIN - 1)
        return TILE_TREE(0x270);
    if (y >= 2 && y <= INNER_Y_MAX && x == INNER_X_MAX + 1)
        return TILE_TREE(0x272);
    if (y == INNER_Y_MAX + 1 && x >= INNER_X_MIN - 1 && x <= INNER_X_MAX + 1)
        return TILE_TREE(x == INNER_X_MIN - 1 ? 0x278 : (x == INNER_X_MAX + 1 ? 0x27A : 0x237));
    if (y == INNER_Y_MAX + 2 && x >= INNER_X_MIN - 1 && x <= INNER_X_MAX + 1)
        return TILE_TREE(x == INNER_X_MIN - 1 ? 0x280 : (x == INNER_X_MAX + 1 ? 0x282 : 0x227));
    return TILE_TREE(0x208);
}

static u16 GetWalledMetatile(s32 x, s32 y)
{
    const struct WallTiles *t = (sPlan.biome == BIOME_CAVE) ? &sCaveTiles : &sVolcanoTiles;
    u32 i;
    u16 preview;

    if (sPlan.biome == BIOME_POWER_PLANT)
        return GetPowerPlantMetatile(x, y);

    if (x >= INNER_X_MIN && x <= INNER_X_MAX && y >= INNER_Y_MIN && y <= INNER_Y_MAX)
    {
        if (sPlan.rock[y][x])
            return TILE_TREE(((x + y) % 2) ? t->rock : t->rockAlt);
        if (GetWalledDoorPreview(x, y, &preview))
            return preview;
        return TILE_GROUND((Mix32(x * 31 + y * 17 + GetRoomSeed()) % 10) ? t->floor : t->floorAlt);
    }

    // The doors: cave mouths, or corridors through the volcano's rock
    for (i = 0; i < NUM_DOORS; i++)
    {
        s32 gx = GetDoorX(i);
        if (sPlan.doors[i] == NO_DOOR)
            continue;
        if (sPlan.biome == BIOME_CAVE)
        {
            if (x >= gx - 1 && x <= gx + 1 && y == INNER_Y_MIN - 2)
                return TILE_TREE(0x20B + (x - gx + 1));
            if (x >= gx - 1 && x <= gx + 1 && y == INNER_Y_MIN - 1)
                return x == gx ? TILE_GROUND(0x214) : TILE_TREE(0x213 + (x - gx + 1));
        }
        else if (y < INNER_Y_MIN)
        {
            if (x == gx || x == gx + 1)
                return TILE_GROUND(t->floor);
            if (x == gx - 1)
                return TILE_TREE(y == INNER_Y_MIN - 1 ? t->topRight : t->right);
            if (x == gx + 2)
                return TILE_TREE(y == INNER_Y_MIN - 1 ? t->topLeft : t->left);
        }
    }

    if (y == INNER_Y_MIN - 1 && x >= INNER_X_MIN && x <= INNER_X_MAX)
        return TILE_TREE(t->top);
    if (y == INNER_Y_MIN - 1 && x == INNER_X_MIN - 1)
        return TILE_TREE(t->topLeft);
    if (y == INNER_Y_MIN - 1 && x == INNER_X_MAX + 1)
        return TILE_TREE(t->topRight);
    if (y >= INNER_Y_MIN && y <= INNER_Y_MAX && x == INNER_X_MIN - 1)
        return TILE_TREE(y == INNER_Y_MIN ? t->leftTop : t->left);
    if (y >= INNER_Y_MIN && y <= INNER_Y_MAX && x == INNER_X_MAX + 1)
        return TILE_TREE(y == INNER_Y_MIN ? t->rightTop : t->right);
    if (y == INNER_Y_MAX + 1 && x >= INNER_X_MIN && x <= INNER_X_MAX)
        return TILE_TREE(t->bottom);
    if (y == INNER_Y_MAX + 1 && x == INNER_X_MIN - 1)
        return TILE_TREE(t->bottomLeft);
    if (y == INNER_Y_MAX + 1 && x == INNER_X_MAX + 1)
        return TILE_TREE(t->bottomRight);
    return TILE_TREE(t->fill);
}

static void DrawWalledRoom(void)
{
    s32 x, y;
    for (y = 0; y < ROOM_H; y++)
    {
        for (x = 0; x < ROOM_W; x++)
            MapGridSetMetatileEntryAt(x + MAP_OFFSET, y + MAP_OFFSET, GetWalledMetatile(x, y));
    }
}

// The arena's doors: the way in closes behind the player, the way out opens once the boss is beaten
static void SetArenaDoors(bool32 open)
{
    s32 x;
    for (x = 5; x <= 7; x++)
    {
        MapGridSetMetatileIdAt(x + MAP_OFFSET, 12 + MAP_OFFSET, METATILE_EliteFour_EntryDoor_ClosedTop | MAPGRID_COLLISION_MASK);
        MapGridSetMetatileIdAt(x + MAP_OFFSET, 13 + MAP_OFFSET, METATILE_EliteFour_EntryDoor_ClosedBottom | MAPGRID_COLLISION_MASK);
    }
    if (open)
    {
        MapGridSetMetatileIdAt(6 + MAP_OFFSET, 1 + MAP_OFFSET, METATILE_EliteFour_OpenDoor_Frame);
        MapGridSetMetatileIdAt(6 + MAP_OFFSET, 2 + MAP_OFFSET, METATILE_EliteFour_OpenDoor_Opening);
    }
}

// ON_LOAD (after every warp and battle): draw the room
void Rogue_OnLoad(void)
{
    BuildRoomPlan();
    switch (sPlan.kind)
    {
    case ROOM_KIND_ARENA:
        SetArenaDoors(HasTrainerBeenFought(TRAINER_ROGUE_BOSS));
        break;
    case ROOM_KIND_WALLED:
        DrawWalledRoom();
        break;
    default:
        DrawOutdoorRoom();
        break;
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
    [ROGUE_ROOM_REST_STOP] = COMPOUND_STRING("the rest stop"),
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
    else
    {
        gSpecialVar_Result = FALSE;
    }
}

// The room's trainer is beaten: the small trees blocking the doors go
void Rogue_OpenDoors(void)
{
    u32 i;
    for (i = FIRST_THICKET_SLOT; i < NUM_OBJECT_SLOTS; i++)
    {
        FlagSet(gSaveBlock1Ptr->objectEventTemplates[i].flagId);
        RemoveObjectEventByLocalIdAndMap(i + 1, gSaveBlock1Ptr->location.mapNum, gSaveBlock1Ptr->location.mapGroup);
    }
}

// The map name popup in a run: where the player is
bool32 Rogue_GetPopUpName(u8 *dest)
{
    if (!Rogue_IsRunActive() || !IsRogueMap(gSaveBlock1Ptr->location.mapGroup, gSaveBlock1Ptr->location.mapNum))
        return FALSE;
    if (IsFinale(GetFloor()))
    {
        StringCopy(dest, GetCurrentBoss()->trainerId == TRAINER_WALLACE ? COMPOUND_STRING("CHAMPION") : COMPOUND_STRING("ELITE FOUR"));
        return TRUE;
    }
    ConvertIntToDecimalStringN(gStringVar1, GetFloor(), STR_CONV_MODE_LEFT_ALIGN, 2);
    if (GetRoomType() == ROGUE_ROOM_BOSS)
    {
        StringExpandPlaceholders(dest, COMPOUND_STRING("FLOOR {STR_VAR_1}  BOSS"));
    }
    else if (GetRoomType() == ROGUE_ROOM_REST_STOP)
    {
        StringExpandPlaceholders(dest, COMPOUND_STRING("REST STOP"));
    }
    else
    {
        ConvertIntToDecimalStringN(gStringVar2, GetRoom(), STR_CONV_MODE_LEFT_ALIGN, 2);
        ConvertIntToDecimalStringN(gStringVar3, ROGUE_ROOMS_PER_FLOOR, STR_CONV_MODE_LEFT_ALIGN, 2);
        StringExpandPlaceholders(dest, COMPOUND_STRING("FLOOR {STR_VAR_1}  {STR_VAR_2}/{STR_VAR_3}"));
    }
    return TRUE;
}

void Rogue_CanLeaveRoom(void)
{
    u32 type = GetRoomType();

    BuildRoomPlan();
    gSpecialVar_Result = TRUE;
    if (type != ROGUE_ROOM_BOSS && sPlan.guarded && !HasTrainerBeenFought(TRAINER_ROGUE_TRAINER))
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
    SetGraftSetting(GRAFT_SETTING_LEVEL_CAP, GRAFT_LEVEL_CAP_HARD); // the floor's boss level (Rogue_GetLevelCap)
    // Set by the run, not chosen on a settings screen (decided with Jacob 2026-10-02)
    SetGraftSetting(GRAFT_SETTING_SHINY_ODDS, GRAFT_SHINY_ODDS_BOOSTED);
    SetGraftSetting(GRAFT_SETTING_EXP_SHARE, TRUE); // the whole team learns (Jacob 2026-10-03: runs were too hard)
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

// The Favorite unlock's species (its first stage), or SPECIES_NONE
static enum Species GetFavorite(void)
{
    enum Species species = VarGet(VAR_ROGUE_FAVORITE);
    if (!HasUnlock(UNLOCK_FAVORITE) || species == SPECIES_NONE || species >= NUM_SPECIES || !IsSpeciesEnabled(species))
        return SPECIES_NONE;
    return species;
}

// The starter candidates of a run: one, or three with the Starter choice unlock, plus the favorite.
// Returns how many.
static u32 GetStarterCandidates(enum Species *out)
{
    rng_value_t rng = LocalRandomSeed(Mix32(GetRunSeed() ^ 0x57A7));
    u32 i, j, count = HasUnlock(UNLOCK_STARTER_CHOICE) ? 3 : 1;
    enum Species favorite = GetFavorite();

    for (i = 0; i < count; i++)
    {
        do
        {
            out[i] = PickStarter(&rng);
            for (j = 0; j < i && out[j] != out[i]; j++)
                ;
        } while (j < i || out[i] == favorite);
    }
    if (favorite != SPECIES_NONE)
        out[count++] = favorite;
    return count;
}

// A Pokémon's rolled egg moves (Rogue_GetEggMoveMask) are known from the start: in a free slot, else
// in place of its first (oldest) move. They stay relearnable if forgotten.
static void TeachRolledEggMoves(struct Pokemon *mon)
{
    enum Species base = GetMonData(mon, MON_DATA_SPECIES);
    const u16 *eggMoves;
    u32 mask, i, depth;

    for (depth = 0; depth < 3 && GetSpeciesPreEvolution(base) != SPECIES_NONE; depth++)
        base = GetSpeciesPreEvolution(base);
    eggMoves = GetSpeciesEggMoves(base);
    mask = Rogue_GetEggMoveMask(&mon->box, eggMoves);
    for (i = 0; i < KNOWN_EGG_MOVES_COUNT && eggMoves[i] != MOVE_UNAVAILABLE; i++)
    {
        if (!(mask & (1u << i)) || MonKnowsMove(mon, eggMoves[i]))
            continue;
        if (GiveMoveToMon(mon, eggMoves[i]) == MON_HAS_MAX_MOVES)
            SetMonMoveSlot(mon, eggMoves[i], 0);
    }
}

static void GiveStarter(enum Species species)
{
    ScriptGiveMon(species, 5, ITEM_NONE);
    TeachRolledEggMoves(&gParties[B_TRAINER_PLAYER][0]);
    GraftMonotypeOntoStarter(&gParties[B_TRAINER_PLAYER][0]);
    StringCopy(gStringVar1, GetSpeciesName(species));
}

static void StartRun(bool32 hard, u32 challenge, u32 monotype)
{
    u32 seed = Random32();
    enum Species starters[MAX_STARTER_CHOICES];

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

    if (GetStarterCandidates(starters) == 1)
        GiveStarter(starters[0]);

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
    u32 door, type, biome, floorBefore = GetFloor();

    BuildRoomPlan();
    door = sPlan.doors[gSpecialVar_0x8004 % NUM_DOORS];
    biome = sPlan.doorBiomes[gSpecialVar_0x8004 % NUM_DOORS];
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
        gSpecialVar_Result = (CalculatePlayerPartyCount() == 0) ? ROGUE_DOOR_CHOOSE_STARTER : ROGUE_DOOR_STARTED_RUN;
        return;
    }
    if (door == ROGUE_ROOM_HUB)
    {
        EndRun(TRUE);
        gSpecialVar_Result = ROGUE_DOOR_WON_RUN;
        return;
    }

    if (GetRoomType() == ROGUE_ROOM_REST_STOP)
        VarSet(VAR_ROGUE_RUN_FLAGS, VarGet(VAR_ROGUE_RUN_FLAGS) & ~RUN_FLAG_PRIZE_TAKEN);
    if (door == ROGUE_ROOM_BOSS)
    {
        u32 floor, room;
        GetBossDoorDestination(&floor, &room);
        VarSet(VAR_ROGUE_FLOOR, floor);
        VarSet(VAR_ROGUE_ROOM, room);
    }
    else if (door == ROGUE_ROOM_REST_STOP)
    {
        // same floor and room number: the rest stop belongs to the floor just cleared
    }
    else if (GetRoomType() == ROGUE_ROOM_REST_STOP)
    {
        VarSet(VAR_ROGUE_FLOOR, GetFloor() + 1);
        VarSet(VAR_ROGUE_ROOM, 1);
    }
    else
    {
        VarSet(VAR_ROGUE_ROOM, GetRoom() + 1);
    }
    if (GetFloor() != floorBefore)
        SetLastRitesUsed(0); // last rites come back each floor

    type = door;
    if (door == ROGUE_ROOM_MYSTERY)
    {
        rng_value_t rng = LocalRandomSeed(GetRoomSeed() ^ 0x3F3F);
        if (LocalRandom(&rng) % 100 < 35)
            type = ROGUE_ROOM_EVENT;
        else
            type = PickWeighted(&rng, sDoorWeights, ROGUE_ROOM_TYPE_COUNT, 1u << ROGUE_ROOM_MYSTERY);
        if (IsBuildingRoom(type) && biome != BIOME_FOREST && biome != BIOME_BEACH)
            biome = BIOME_FOREST; // buildings stand outdoors
        type |= ROOM_TYPE_FROM_MYSTERY;
    }
    VarSet(VAR_ROGUE_ROOM_TYPE, type | (biome << ROOM_BIOME_SHIFT));
}

void Rogue_PrepareWildMon(void)
{
    const struct ObjectEventTemplate *template = &gSaveBlock1Ptr->objectEventTemplates[gSpecialVar_LastTalked - 1];
    enum Species species = template->graphicsId - OBJ_EVENT_MON;

    CreateScriptedWildMon(species, GetWildLevel(), ITEM_NONE);
    TeachRolledEggMoves(&gParties[B_TRAINER_OPPONENT_A][0]);
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

// Species the player has won a run with, one bit per National Dex number, kept between runs in the
// Battle Tower's e-Reader trainer slot (only the Battle Tower, Mystery Gift and Mystery Events touch
// it; Rogue has none of them)
#define sWonSpecies ((u8 *)&gSaveBlock2Ptr->frontier.ereaderTrainer)
STATIC_ASSERT(sizeof(gSaveBlock2Ptr->frontier.ereaderTrainer) * 8 > NATIONAL_DEX_COUNT, WonSpeciesFit);

#define NEW_SPECIES_BP 3

static bool32 HasWonWith(u32 dexNum)
{
    return dexNum != 0 && dexNum <= NATIONAL_DEX_COUNT && (sWonSpecies[dexNum / 8] & (1 << (dexNum % 8)));
}

static u32 GetSpeciesDexNum(enum Species species)
{
    return SpeciesToNationalPokedexNum(GET_BASE_SPECIES_ID(species));
}

// For the start room and tests: how many species the player has won with
u32 Rogue_CountWonSpecies(void)
{
    u32 i, count = 0;
    for (i = 1; i <= NATIONAL_DEX_COUNT; i++)
    {
        if (HasWonWith(i))
            count++;
    }
    return count;
}

// The run is won: mark the team's species; returns how many are new
static u32 RecordWonSpecies(void)
{
    u32 i, newCount = 0;
    for (i = 0; i < PARTY_SIZE; i++)
    {
        struct Pokemon *mon = &gParties[B_TRAINER_PLAYER][i];
        enum Species species = GetMonData(mon, MON_DATA_SPECIES);
        u32 dexNum;
        if (species == SPECIES_NONE || GetMonData(mon, MON_DATA_IS_EGG))
            continue;
        dexNum = GetSpeciesDexNum(species);
        if (dexNum == 0 || dexNum > NATIONAL_DEX_COUNT || HasWonWith(dexNum))
            continue;
        sWonSpecies[dexNum / 8] |= 1 << (dexNum % 8);
        newCount++;
    }
    return newCount;
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
        SetUnlock(UNLOCK_HARD_MODE);
        StringAppend(gStringVar4, COMPOUND_STRING("\pHARD mode is open: eight floors\nand the ELITE FOUR!"));
    }
    // The run is won: a bonus for each species on the team never won with before
    if (IsFinale(floor) && GetRoom() >= GetFinaleLength())
    {
        u32 newSpecies = RecordWonSpecies();
        if (newSpecies != 0)
        {
            ConvertIntToDecimalStringN(gStringVar2, newSpecies, STR_CONV_MODE_LEFT_ALIGN, 2);
            ConvertIntToDecimalStringN(gStringVar1, Rogue_CountWonSpecies(), STR_CONV_MODE_LEFT_ALIGN, 4);
            StringAppend(gStringVar4, COMPOUND_STRING("\p"));
            StringExpandPlaceholders(gStringVar3, newSpecies == 1
                ? COMPOUND_STRING("A first win with this species!\nSpecies won with: {STR_VAR_1}")
                : COMPOUND_STRING("{STR_VAR_2} species won with for the first\ntime! Species won with: {STR_VAR_1}"));
            StringAppend(gStringVar4, gStringVar3);
            bp += newSpecies * NEW_SPECIES_BP;
        }
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
    enum Species starters[MAX_STARTER_CHOICES];
    u32 i, count = GetStarterCandidates(starters);
    for (i = 0; i < count; i++)
        PushChoice(GetSpeciesName(starters[i]), i);
}

void Rogue_GiveChosenStarter(void)
{
    enum Species starters[MAX_STARTER_CHOICES];
    u32 count = GetStarterCandidates(starters);
    GiveStarter(starters[gSpecialVar_Result < count ? gSpecialVar_Result : 0]);
}

// The Favorite unlock (a POKéFAN in the run's POKéMON CENTERS): VAR_RESULT = whether it's unlocked,
// STR_VAR_1 = the current favorite, VAR_0x8005 = whether there is one
void Rogue_GetFavorite(void)
{
    enum Species favorite = GetFavorite();
    gSpecialVar_Result = HasUnlock(UNLOCK_FAVORITE) && Rogue_IsRunActive();
    gSpecialVar_0x8005 = (favorite != SPECIES_NONE);
    if (favorite != SPECIES_NONE)
        StringCopy(gStringVar1, GetSpeciesName(favorite));
}

// VAR_0x8004 = the party slot picked; STR_VAR_1 = the favorite (the first stage of its species)
void Rogue_SetFavorite(void)
{
    enum Species species = GetMonData(&gParties[B_TRAINER_PLAYER][gSpecialVar_0x8004], MON_DATA_SPECIES);
    u32 depth;
    for (depth = 0; depth < 3 && GetSpeciesPreEvolution(species) != SPECIES_NONE; depth++)
        species = GetSpeciesPreEvolution(species);
    VarSet(VAR_ROGUE_FAVORITE, species);
    StringCopy(gStringVar1, GetSpeciesName(species));
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
static void TryCatchBonus(void);

void Rogue_OnBattleEnd(void)
{
    u32 i;
    if (!Rogue_IsRunActive() || gBattleOutcome == B_OUTCOME_LOST || gBattleOutcome == B_OUTCOME_DREW)
        return;
    TryCatchBonus();
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

// Egg moves in the relearner, on top of the ones a Pokémon has known: a few, picked by its
// personality (so the same Pokémon always gets the same ones). 10% have one; Good genes makes it 35%;
// with Pedigree every Pokémon has one and 35% have two. Returns a mask over eggMoves[].
u32 Rogue_GetEggMoveMask(struct BoxPokemon *mon, const u16 *eggMoves)
{
    u32 total, count, i, mask = 0, hash = Mix32(GetBoxMonData(mon, MON_DATA_PERSONALITY) ^ 0xE66);
    u32 roll = hash % 100;

    for (total = 0; total < KNOWN_EGG_MOVES_COUNT && eggMoves[total] != MOVE_UNAVAILABLE; total++)
        ;
    if (total == 0)
        return 0;
    if (HasUnlock(UNLOCK_PEDIGREE))
        count = 1 + (roll < 35);
    else
        count = roll < (HasUnlock(UNLOCK_GOOD_GENES) ? 35 : 10);
    for (i = 0; i < count; i++)
        mask |= 1u << (((hash >> 8) + i) % total);
    return mask;
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
    for (i = 0; i < UNLOCK_COUNT; i++)
    {
        if (sUnlocks[i].name == NULL || HasUnlock(i) || (sUnlocks[i].requires >= 0 && !HasUnlock(sUnlocks[i].requires)))
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
    u32 id = gSpecialVar_0x8004 % UNLOCK_COUNT;
    ConvertIntToDecimalStringN(gStringVar2, sUnlocks[id].cost, STR_CONV_MODE_LEFT_ALIGN, 3);
    StringCopy(gStringVar4, sUnlocks[id].description);
    StringExpandPlaceholders(gStringVar3, COMPOUND_STRING("\pUnlock it for {STR_VAR_2} BP?"));
    StringAppend(gStringVar4, gStringVar3);
}

void Rogue_BuyUnlock(void)
{
    u32 id = gSpecialVar_0x8004 % UNLOCK_COUNT;
    if (gSaveBlock2Ptr->frontier.battlePoints < sUnlocks[id].cost)
    {
        gSpecialVar_Result = FALSE;
        StringCopy(gStringVar4, COMPOUND_STRING("You don't have enough BP.\nBeat bosses to earn more."));
        return;
    }
    gSaveBlock2Ptr->frontier.battlePoints -= sUnlocks[id].cost;
    SetUnlock(id);
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
        size = IsFinale(floor) ? 5 : min(floor + 1, 5); // plus the Leader's ace
        level = GetBossLevel();
        iv = min(31, 6 + floor * 4);
    }
    else
    {
        size = min(1 + (floor - 1) / 2 + (GetRoom() > ROGUE_ROOMS_PER_FLOOR / 2), PARTY_SIZE);
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

// ---------------------------------------------------------------------------------------------
// The rest stop between floors: a free prize (one of three) and a move tutor

extern const u16 gTutorMoves[];

#define TUTOR_PRICE_MONEY 1500
#define TUTOR_PRICE_BP    10
#define NUM_TUTOR_OFFERS  3

enum
{
    PRIZE_TM,
    PRIZE_EVO_ITEM,
    PRIZE_BATTLE_ITEM,
    PRIZE_MEGA_STONE,
    PRIZE_SHELF_COUNT,
};

static const u16 sPrizeEvoItems[] =
{
    ITEM_FIRE_STONE, ITEM_WATER_STONE, ITEM_THUNDER_STONE, ITEM_LEAF_STONE, ITEM_MOON_STONE,
    ITEM_SUN_STONE, ITEM_SHINY_STONE, ITEM_DUSK_STONE, ITEM_DAWN_STONE, ITEM_ICE_STONE, ITEM_LINKING_CORD,
};

static const u16 sPrizeBattleItems[] =
{
    ITEM_LIFE_ORB, ITEM_CHOICE_BAND, ITEM_CHOICE_SPECS, ITEM_CHOICE_SCARF, ITEM_LEFTOVERS, ITEM_FOCUS_SASH,
    ITEM_ASSAULT_VEST, ITEM_EXPERT_BELT, ITEM_ROCKY_HELMET, ITEM_EVIOLITE, ITEM_LIGHT_CLAY,
    ITEM_WEAKNESS_POLICY, ITEM_MUSCLE_BAND, ITEM_WISE_GLASSES, ITEM_SCOPE_LENS, ITEM_SHELL_BELL,
    ITEM_AIR_BALLOON, ITEM_HEAVY_DUTY_BOOTS, ITEM_BLACK_SLUDGE, ITEM_SITRUS_BERRY,
};

static enum Item PickPrize(rng_value_t *rng, u32 shelf)
{
    u32 i, count, pick;
    switch (shelf)
    {
    case PRIZE_TM:
        return GetTMHMItemId(1 + LocalRandom(rng) % NUM_TECHNICAL_MACHINES);
    case PRIZE_EVO_ITEM:
        return sPrizeEvoItems[LocalRandom(rng) % ARRAY_COUNT(sPrizeEvoItems)];
    case PRIZE_BATTLE_ITEM:
        return sPrizeBattleItems[LocalRandom(rng) % ARRAY_COUNT(sPrizeBattleItems)];
    default:
        for (i = ITEM_NONE + 1, count = 0; i < ITEMS_COUNT; i++)
        {
            if (GetItemHoldEffect(i) == HOLD_EFFECT_MEGA_STONE)
                count++;
        }
        if (count == 0)
            return ITEM_LIFE_ORB;
        pick = LocalRandom(rng) % count;
        for (i = ITEM_NONE + 1; i < ITEMS_COUNT; i++)
        {
            if (GetItemHoldEffect(i) == HOLD_EFFECT_MEGA_STONE && pick-- == 0)
                return i;
        }
        return ITEM_LIFE_ORB;
    }
}

// Three offers from three different shelves, fixed per rest stop
static void GetPrizeOffers(enum Item *out)
{
    rng_value_t rng = LocalRandomSeed(GetRoomSeed() ^ 0x9812);
    u32 skip = LocalRandom(&rng) % PRIZE_SHELF_COUNT, shelf, n = 0;
    for (shelf = 0; shelf < PRIZE_SHELF_COUNT; shelf++)
    {
        if (shelf != skip)
            out[n++] = PickPrize(&rng, shelf);
    }
}

void Rogue_HasTakenPrize(void)
{
    gSpecialVar_Result = (VarGet(VAR_ROGUE_RUN_FLAGS) & RUN_FLAG_PRIZE_TAKEN) != 0;
}

// A prize's name: a TM shows its move ("TM Swords Dance")
static void GetPrizeName(enum Item item, u8 *dest)
{
    enum Move move = ItemIdToBattleMoveId(item);
    if (GetItemPocket(item) == POCKET_TM_HM && move != MOVE_NONE)
    {
        StringCopy(dest, COMPOUND_STRING("TM "));
        StringAppend(dest, GetMoveName(move));
    }
    else
    {
        CopyItemName(item, dest);
    }
}

void Rogue_PushPrizeChoices(void)
{
    enum Item offers[PRIZE_SHELF_COUNT - 1];
    u32 i;
    GetPrizeOffers(offers);
    for (i = 0; i < ARRAY_COUNT(offers); i++)
    {
        GetPrizeName(offers[i], gStringVar3);
        PushChoice(gStringVar3, i);
    }
}

// VAR_RESULT: the offer taken. STR_VAR_1 = its name; VAR_0x8005 = whether a Mega Ring came with it
void Rogue_GivePrize(void)
{
    enum Item offers[PRIZE_SHELF_COUNT - 1];
    enum Item item;

    GetPrizeOffers(offers);
    item = offers[gSpecialVar_Result < ARRAY_COUNT(offers) ? gSpecialVar_Result : 0];
    AddBagItem(item, 1);
    GetPrizeName(item, gStringVar1);
    gSpecialVar_0x8005 = FALSE;
    if (GetItemHoldEffect(item) == HOLD_EFFECT_MEGA_STONE && !CheckBagHasItem(ITEM_MEGA_RING, 1))
    {
        AddBagItem(ITEM_MEGA_RING, 1);
        gSpecialVar_0x8005 = TRUE;
    }
    VarSet(VAR_ROGUE_RUN_FLAGS, VarGet(VAR_ROGUE_RUN_FLAGS) | RUN_FLAG_PRIZE_TAKEN);
}

// STR_VAR_1: where the rest stop's exit leads
void Rogue_BufferNextFloor(void)
{
    if (GetFloor() >= GetNumFloors())
    {
        StringCopy(gStringVar1, IsHardMode() ? COMPOUND_STRING("the ELITE FOUR") : COMPOUND_STRING("the CHAMPION"));
        return;
    }
    ConvertIntToDecimalStringN(gStringVar2, GetFloor() + 1, STR_CONV_MODE_LEFT_ALIGN, 2);
    StringExpandPlaceholders(gStringVar1, COMPOUND_STRING("FLOOR {STR_VAR_2}"));
}

// Tutor moves bought with BP: (first-stage species << 16) | move, kept between runs in the Battle
// Tower's saved record-mixing opponents, which only the Battle Tower and link
// record mixing touch (295 entries); Rogue has neither. Builds from 2026-10-03 kept them in the Battle Dome's data
// (24 entries): they're moved over on first use.
#define TUTOR_UNLOCKS_COUNT (sizeof(gSaveBlock2Ptr->frontier.towerRecords) / (sizeof(u32)))
#define sTutorUnlocks ((u32 *)gSaveBlock2Ptr->frontier.towerRecords)
#define OLD_TUTOR_UNLOCKS_COUNT (sizeof(gSaveBlock2Ptr->frontier.domeMonIds) / sizeof(u32))
#define sOldTutorUnlocks ((u32 *)gSaveBlock2Ptr->frontier.domeMonIds)

static void UnlockTutorEntry(u32 entry);

static void MoveOldTutorUnlocks(void)
{
    u32 i;
    for (i = 0; i < OLD_TUTOR_UNLOCKS_COUNT; i++)
    {
        if (sOldTutorUnlocks[i] != 0)
        {
            UnlockTutorEntry(sOldTutorUnlocks[i]);
            sOldTutorUnlocks[i] = 0;
        }
    }
}

static enum Species GetFirstStage(enum Species species)
{
    u32 depth;
    for (depth = 0; depth < 3 && GetSpeciesPreEvolution(species) != SPECIES_NONE; depth++)
        species = GetSpeciesPreEvolution(species);
    return species;
}

// For the relearner's tutor list in a run: the moves bought with BP for this Pokémon's family
u32 Rogue_GetUnlockedTutorMoves(u16 species, u16 *moves)
{
    u32 i, count = 0;
    MoveOldTutorUnlocks();
    species = GetFirstStage(species);
    for (i = 0; i < TUTOR_UNLOCKS_COUNT && count < MAX_RELEARNER_MOVES; i++)
    {
        u32 entry = sTutorUnlocks[i];
        if (entry != 0 && (entry >> 16) == species)
            moves[count++] = entry & 0xFFFF;
    }
    return count;
}

static void UnlockTutorMove(enum Species species, enum Move move)
{
    MoveOldTutorUnlocks();
    UnlockTutorEntry((GetFirstStage(species) << 16) | move);
}

static void UnlockTutorEntry(u32 entry)
{
    u32 i;
    for (i = 0; i < TUTOR_UNLOCKS_COUNT; i++)
    {
        if (sTutorUnlocks[i] == entry)
            return;
        if (sTutorUnlocks[i] == 0)
        {
            sTutorUnlocks[i] = entry;
            return;
        }
    }
    // full: the oldest goes
    for (i = 0; i + 1 < TUTOR_UNLOCKS_COUNT; i++)
        sTutorUnlocks[i] = sTutorUnlocks[i + 1];
    sTutorUnlocks[TUTOR_UNLOCKS_COUNT - 1] = entry;
}

static bool32 CanTeamLearnTutorMove(enum Move move)
{
    u32 i;
    for (i = 0; i < PARTY_SIZE; i++)
    {
        struct Pokemon *mon = &gParties[B_TRAINER_PLAYER][i];
        enum Species species = GetMonData(mon, MON_DATA_SPECIES);
        if (species != SPECIES_NONE && !GetMonData(mon, MON_DATA_IS_EGG)
         && CanLearnTeachableMove(species, move) && !MonKnowsMove(mon, move))
            return TRUE;
    }
    return FALSE;
}

// Up to three tutor moves someone on the team can learn, fixed per rest stop (and team)
static u32 GetTutorOffers(u16 *out)
{
    rng_value_t rng = LocalRandomSeed(GetRoomSeed() ^ 0x7070);
    u32 total, i, count = 0, tries;

    for (total = 0; gTutorMoves[total] != MOVE_UNAVAILABLE; total++)
        ;
    for (tries = 0; tries < 400 && count < NUM_TUTOR_OFFERS && total > 0; tries++)
    {
        enum Move move = gTutorMoves[LocalRandom(&rng) % total];
        for (i = 0; i < count && out[i] != move; i++)
            ;
        if (i == count && CanTeamLearnTutorMove(move))
            out[count++] = move;
    }
    return count;
}

// VAR_RESULT: how many offers were pushed
void Rogue_PushTutorMoves(void)
{
    u16 offers[NUM_TUTOR_OFFERS];
    u32 i, count = GetTutorOffers(offers);
    for (i = 0; i < count; i++)
        PushChoice(GetMoveName(offers[i]), i);
    gSpecialVar_Result = count;
}

// VAR_RESULT: the offer picked. VAR_0x8005 = the move; STR_VAR_1-3 = its name and prices
void Rogue_SelectTutorMove(void)
{
    u16 offers[NUM_TUTOR_OFFERS];
    u32 count = GetTutorOffers(offers);
    gSpecialVar_0x8005 = (gSpecialVar_Result < count) ? offers[gSpecialVar_Result] : offers[0];
    StringCopy(gStringVar1, GetMoveName(gSpecialVar_0x8005));
    ConvertIntToDecimalStringN(gStringVar2, TUTOR_PRICE_MONEY, STR_CONV_MODE_LEFT_ALIGN, 5);
    ConvertIntToDecimalStringN(gStringVar3, TUTOR_PRICE_BP, STR_CONV_MODE_LEFT_ALIGN, 3);
}

// The choices: pay money (this run), pay BP (all runs; only with meta-progression)
void Rogue_PushTutorPayments(void)
{
    StringCopy(gStringVar3, COMPOUND_STRING("Pay ¥"));
    ConvertIntToDecimalStringN(gStringVar3 + StringLength(gStringVar3), TUTOR_PRICE_MONEY, STR_CONV_MODE_LEFT_ALIGN, 5);
    PushChoice(gStringVar3, 0);
    if (IsMetaOn())
    {
        StringCopy(gStringVar3, COMPOUND_STRING("Pay "));
        ConvertIntToDecimalStringN(gStringVar3 + StringLength(gStringVar3), TUTOR_PRICE_BP, STR_CONV_MODE_LEFT_ALIGN, 3);
        StringAppend(gStringVar3, COMPOUND_STRING(" BP"));
        PushChoice(gStringVar3, 1);
    }
}

static EWRAM_DATA u8 sTutorKnewMask = 0; // which party slots knew the move before teaching

// VAR_0x8005 = the move, VAR_0x8006 = 0 money / 1 BP; VAR_RESULT = whether the player can pay
void Rogue_CanPayTutor(void)
{
    u32 i;
    if (gSpecialVar_0x8006 == 1)
        gSpecialVar_Result = gSaveBlock2Ptr->frontier.battlePoints >= TUTOR_PRICE_BP;
    else
        gSpecialVar_Result = IsEnoughMoney(&gSaveBlock1Ptr->money, TUTOR_PRICE_MONEY);
    sTutorKnewMask = 0;
    for (i = 0; i < PARTY_SIZE; i++)
    {
        if (MonKnowsMove(&gParties[B_TRAINER_PLAYER][i], gSpecialVar_0x8005))
            sTutorKnewMask |= 1 << i;
    }
}

// After the move was taught: pay, and with BP remember it for the family. Message in gStringVar4.
void Rogue_PayTutor(void)
{
    u32 i;
    struct Pokemon *learner = NULL;

    StringCopy(gStringVar1, GetMoveName(gSpecialVar_0x8005));
    for (i = 0; i < PARTY_SIZE; i++)
    {
        if (!(sTutorKnewMask & (1 << i)) && MonKnowsMove(&gParties[B_TRAINER_PLAYER][i], gSpecialVar_0x8005))
            learner = &gParties[B_TRAINER_PLAYER][i];
    }
    if (gSpecialVar_0x8006 == 1)
    {
        gSaveBlock2Ptr->frontier.battlePoints -= min(gSaveBlock2Ptr->frontier.battlePoints, TUTOR_PRICE_BP);
        if (learner != NULL)
        {
            UnlockTutorMove(GetMonData(learner, MON_DATA_SPECIES), gSpecialVar_0x8005);
            StringCopy(gStringVar2, GetSpeciesName(GetFirstStage(GetMonData(learner, MON_DATA_SPECIES))));
            StringExpandPlaceholders(gStringVar4, COMPOUND_STRING("From now on, every {STR_VAR_2} can\nrelearn {STR_VAR_1}!"));
            return;
        }
    }
    else
    {
        RemoveMoney(&gSaveBlock1Ptr->money, TUTOR_PRICE_MONEY);
    }
    StringExpandPlaceholders(gStringVar4, COMPOUND_STRING("Use {STR_VAR_1} well!"));
}

// ---------------------------------------------------------------------------------------------
// Second-page unlocks' hooks

// Prices in a run: a run's money (about ¥1000-2000 a floor early on) can't pay the main game's prices
// for held battle items (up to ¥100,000), so nothing costs more than ¥3000; Rare Candy ¥1000. A
// quarter off with Discount.
#define RUN_MAX_PRICE 3000
// An item's worth in a run (selling goes by it too, so a prize can't be sold for a fortune)
u32 Rogue_GetItemWorth(u16 item, u32 price)
{
    if (!Rogue_IsRunActive())
        return price;
    if (item == ITEM_RARE_CANDY)
        return 1000;
    return min(price, RUN_MAX_PRICE);
}

u32 Rogue_GetShopPrice(u16 item, u32 price)
{
    price = Rogue_GetItemWorth(item, price);
    if (Rogue_IsRunActive() && HasUnlock(UNLOCK_DISCOUNT))
        price -= price / 4;
    return price;
}

// Each piece broken down from a Pokémon: a 25% chance of a spare copy with Harvest bonus
bool32 Rogue_RollHarvestBonus(void)
{
    return Rogue_IsRunActive() && HasUnlock(UNLOCK_HARVEST_BONUS) && Random() % 100 < 25;
}

static EWRAM_DATA struct GraftPiece sCatchBonusPiece = {0};

// A real catch (not a grafting ball): a 25% chance of one random piece with Catch bonus. The room
// says so after the battle (VAR_TEMP_5, Rogue_Room_OnFrame).
static void TryCatchBonus(void)
{
    u32 lost;
    if (gBattleOutcome != B_OUTCOME_CAUGHT || !HasUnlock(UNLOCK_CATCH_BONUS) || Random() % 100 >= 25)
        return;
    if (gLastThrownBall == ITEM_PATCH_BALL || gLastThrownBall == ITEM_GRAFT_BALL || gLastThrownBall == ITEM_SPLICE_BALL)
        return;
    GiveGraftBagIfMissing();
    if (HarvestGraftPieces(&gParties[B_TRAINER_OPPONENT_A][0], TRUE, 1, &sCatchBonusPiece, &lost) != 0)
        VarSet(VAR_TEMP_5, 1);
}

// STR_VAR_1: the catch bonus's piece
void Rogue_BufferCatchBonus(void)
{
    VarSet(VAR_TEMP_5, 0);
    GetGraftPieceName(gStringVar1, &sCatchBonusPiece);
}
