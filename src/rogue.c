// Grafted Rogue: runs of generated rooms. Every room is the one map Rogue_Room, rebuilt from a seed
// (the run's seed mixed with the floor and room number), so the same room comes out the same after a
// battle or a reload. A room's plan (trees, grass, doors, objects) is recomputed from the seed when
// it's needed instead of being stored. See docs/rogue/DESIGN.md.
#include "global.h"
#include "rogue.h"
#include "battle_setup.h"
#include "data.h"
#include "event_data.h"
#include "event_object_movement.h"
#include "fieldmap.h"
#include "graft_bag.h"
#include "graft_settings.h"
#include "item.h"
#include "money.h"
#include "overworld.h"
#include "pokemon.h"
#include "random.h"
#include "script.h"
#include "script_pokemon_util.h"
#include "sound.h"
#include "string_util.h"
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
static const struct { u16 trainerId; u16 graphicsId; enum Type type; } sBosses[] =
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

static u32 GetTeamSlots(void)
{
    u32 slots = VarGet(VAR_ROGUE_TEAM_SLOTS);
    return slots ? slots : 3;
}

bool32 Rogue_IsTeamFull(void)
{
    return Rogue_IsRunActive() && CalculatePlayerPartyCount() >= GetTeamSlots();
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

static u32 GetWildLevel(void)
{
    return 4 + (GetFloor() - 1) * 9 + GetRoom();
}

static u32 GetBossLevel(void)
{
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
            if (IsTreeBlock(nx, ny) || seen[ny][nx])
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
    if (type == ROGUE_ROOM_BOSS && GetFloor() >= ROGUE_FLOORS_NORMAL)
    {
        sPlan.doors[1] = ROGUE_ROOM_HUB; // the run is won: back to the start
        return;
    }
    if (type != ROGUE_ROOM_BOSS && GetRoom() >= ROGUE_ROOMS_PER_FLOOR)
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
    if (IsTreeTile(x, y))
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
                           || type == ROGUE_ROOM_SHOP || type == ROGUE_ROOM_REST);
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
        break;
    case ROGUE_ROOM_WILD:
        count = 1 + (LocalRandom(&rng) % 100 < 35);
        for (i = 0; i < count; i++)
            AddObjectAnywhere(&rng, OBJ_EVENT_MON + PickFloorSpecies(&rng, TYPE_NONE), MOVEMENT_TYPE_LOOK_AROUND, Rogue_Room_EventScript_WildMon);
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
        AddObject(sBosses[GetFloorBoss(GetFloor())].graphicsId, CENTER_X, CENTER_Y, MOVEMENT_TYPE_FACE_DOWN, Rogue_Room_EventScript_Boss);
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
    case ROGUE_ROOM_BOSS:    return sBosses[GetFloorBoss(GetFloor() + (GetRoomType() == ROGUE_ROOM_BOSS))].graphicsId;
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

// ON_LOAD (after every warp and battle): draw the room
void Rogue_OnLoad(void)
{
    s32 x, y;

    BuildRoomPlan();
    for (y = 0; y < ROOM_H; y++)
    {
        for (x = 0; x < ROOM_W; x++)
        {
            u16 tile = IsTreeTile(x, y) ? GetTreeMetatile(x, y) : GetGroundMetatile(x, y);
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

void Rogue_GetRoomIntro(void)
{
    u32 type = GetRoomType();

    gSpecialVar_Result = FALSE;
    if (!Rogue_IsRunActive())
        return;
    gSpecialVar_Result = TRUE;
    if (type == ROGUE_ROOM_BOSS)
    {
        StringCopy(gStringVar1, GetTrainerNameFromId(sBosses[GetFloorBoss(GetFloor())].trainerId));
        StringExpandPlaceholders(gStringVar4, COMPOUND_STRING("{STR_VAR_1} is waiting at the\nend of the floor!"));
    }
    else if (VarGet(VAR_ROGUE_ROOM_TYPE) & ROOM_TYPE_FROM_MYSTERY)
    {
        StringCopy(gStringVar1, sRoomNames[type]);
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
    SetGraftSetting(GRAFT_SETTING_FAINTING, GRAFT_FAINTING_RELEASE);
    SetGraftSetting(GRAFT_SETTING_WILD_SPAWNS, GRAFT_WILD_SPAWNS_HIDDEN);
    SetGraftSetting(GRAFT_SETTING_LEVEL_CAP, GRAFT_LEVEL_CAP_OFF);
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
}

static void StartRun(void)
{
    u32 seed = Random32();
    rng_value_t rng;
    enum Species starter;

    ClearRunProgress();
    VarSet(VAR_ROGUE_SEED_LO, seed & 0xFFFF);
    VarSet(VAR_ROGUE_SEED_HI, seed >> 16);
    VarSet(VAR_ROGUE_STATE, ROGUE_STATE_RUN);
    VarSet(VAR_ROGUE_FLOOR, 1);
    VarSet(VAR_ROGUE_ROOM, 1);
    VarSet(VAR_ROGUE_ROOM_TYPE, ROGUE_ROOM_WILD);
    VarSet(VAR_ROGUE_TEAM_SLOTS, 3);
    SetForcedSettings();

    rng = LocalRandomSeed(Mix32(seed ^ 0x57A7));
    starter = PickStarter(&rng);
    ScriptGiveMon(starter, 5, ITEM_NONE);
    StringCopy(gStringVar1, GetSpeciesName(starter));

    AddBagItem(ITEM_POKE_BALL, 5);
    AddBagItem(ITEM_PATCH_BALL, 3);
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
    u32 door, type;

    BuildRoomPlan();
    door = sPlan.doors[gSpecialVar_0x8004 % NUM_DOORS];
    gSpecialVar_Result = ROGUE_DOOR_NEXT_ROOM;

    if (door == ROGUE_ROOM_START)
    {
        StartRun();
        gSpecialVar_Result = ROGUE_DOOR_STARTED_RUN;
        return;
    }
    if (door == ROGUE_ROOM_HUB)
    {
        EndRun(TRUE);
        gSpecialVar_Result = ROGUE_DOOR_WON_RUN;
        return;
    }

    if (GetRoomType() == ROGUE_ROOM_BOSS)
    {
        VarSet(VAR_ROGUE_FLOOR, GetFloor() + 1);
        VarSet(VAR_ROGUE_ROOM, 1);
    }
    else
    {
        VarSet(VAR_ROGUE_ROOM, GetRoom() + 1);
    }

    type = door;
    if (door == ROGUE_ROOM_MYSTERY)
    {
        rng_value_t rng = LocalRandomSeed(GetRoomSeed() ^ 0x3F3F);
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

void Rogue_OnBossDefeated(void)
{
    u32 floor = GetFloor();

    if (floor <= NUM_BADGES)
        FlagSet(FLAG_BADGE01_GET + floor - 1);
    if (floor >= ROGUE_FLOORS_NORMAL)
    {
        StringCopy(gStringVar4, COMPOUND_STRING("That was the last boss!\nThe way out is open."));
    }
    else
    {
        ConvertIntToDecimalStringN(gStringVar1, floor, STR_CONV_MODE_LEFT_ALIGN, 2);
        StringExpandPlaceholders(gStringVar4, COMPOUND_STRING("FLOOR {STR_VAR_1} cleared!\nThe way forward is open."));
    }
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
    u16 baseId = boss ? sBosses[GetFloorBoss(floor)].trainerId : sTrainerKinds[GetTrainerKind()].trainerId;
    enum Type type = boss ? sBosses[GetFloorBoss(floor)].type : TYPE_NONE;
    u32 size, level, iv, i;

    sTrainers[slot] = *GetTrainerStructFromId(baseId);
    if (boss)
    {
        size = min(floor + 2, PARTY_SIZE);
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
