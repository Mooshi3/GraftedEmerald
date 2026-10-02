#include "global.h"
#include "rogue.h"
#include "data.h"
#include "event_data.h"
#include "pokemon.h"
#include "test/test.h"
#include "constants/rogue.h"

// Grafted Rogue: generated rooms and trainers (src/rogue.c)

static void SetRoom(u32 seed, u32 floor, u32 room, u32 type)
{
    VarSet(VAR_ROGUE_STATE, ROGUE_STATE_RUN);
    VarSet(VAR_ROGUE_SEED_LO, seed & 0xFFFF);
    VarSet(VAR_ROGUE_SEED_HI, seed >> 16);
    VarSet(VAR_ROGUE_FLOOR, floor);
    VarSet(VAR_ROGUE_ROOM, room);
    VarSet(VAR_ROGUE_ROOM_TYPE, type);
}

static bool32 IsBannedSpecies(enum Species species)
{
    const struct SpeciesInfo *info = &gSpeciesInfo[species];
    return info->isRestrictedLegendary || info->isSubLegendary || info->isMythical || info->isUltraBeast || info->isParadox;
}

TEST("Rogue rooms always have a reachable door")
{
    u32 seed, room;
    u8 doors[3];
    for (seed = 1; seed < 200; seed++)
    {
        for (room = 1; room <= ROGUE_ROOMS_PER_FLOOR; room++)
        {
            SetRoom(seed * 7919, 1 + seed % ROGUE_FLOORS_NORMAL, room, ROGUE_ROOM_WILD + seed % 6);
            EXPECT_GT(Rogue_GetRoomDoors(doors), 0);
        }
    }
}

TEST("Rogue rooms lead to the boss after the last room of a floor")
{
    u8 doors[3];
    SetRoom(1234, 1, ROGUE_ROOMS_PER_FLOOR, ROGUE_ROOM_WILD);
    EXPECT_EQ(Rogue_GetRoomDoors(doors), 1);
    EXPECT_EQ(doors[1], ROGUE_ROOM_BOSS);
    SetRoom(1234, ROGUE_FLOORS_NORMAL, ROGUE_ROOMS_PER_FLOOR, ROGUE_ROOM_BOSS);
    EXPECT_EQ(Rogue_GetRoomDoors(doors), 1);
    EXPECT_EQ(doors[1], ROGUE_ROOM_HUB); // the last boss: the way out
}

TEST("Rogue trainers get generated parties scaled by floor")
{
    u32 seed, floor, i;
    for (seed = 1; seed < 40; seed++)
    {
        for (floor = 1; floor <= ROGUE_FLOORS_NORMAL; floor++)
        {
            const struct Trainer *trainer;
            SetRoom(seed * 104729, floor, 4, ROGUE_ROOM_TRAINER);
            trainer = GetTrainerStructFromId(TRAINER_ROGUE_TRAINER);
            EXPECT_GT((u32)trainer->partySize, 0);
            EXPECT_LE((u32)trainer->partySize, PARTY_SIZE);
            for (i = 0; i < trainer->partySize; i++)
            {
                EXPECT_NE(trainer->party[i].species, SPECIES_NONE);
                EXPECT(!IsBannedSpecies(trainer->party[i].species));
                EXPECT_GT(trainer->party[i].lvl, 0);
            }
        }
    }
}

TEST("Rogue bosses use their type")
{
    u32 seed, floor, i;
    for (seed = 1; seed < 20; seed++)
    {
        for (floor = 1; floor <= ROGUE_FLOORS_NORMAL; floor++)
        {
            const struct Trainer *boss;
            enum Type type;
            SetRoom(seed * 15485863, floor, ROGUE_ROOMS_PER_FLOOR + 1, ROGUE_ROOM_BOSS);
            boss = GetTrainerStructFromId(TRAINER_ROGUE_BOSS);
            EXPECT_GE((u32)boss->partySize, 3);
            // every boss mon shares a type with the first one's specialty
            type = GetSpeciesType(boss->party[0].species, 0);
            for (i = 0; i < boss->partySize; i++)
            {
                enum Species species = boss->party[i].species;
                bool32 shares = FALSE;
                u32 t;
                for (t = 0; t < 2; t++)
                {
                    enum Type slotType = GetSpeciesType(boss->party[0].species, t);
                    if (GetSpeciesType(species, 0) == slotType || GetSpeciesType(species, 1) == slotType)
                        shares = TRUE;
                }
                EXPECT(shares);
                (void)type;
            }
        }
    }
}

TEST("A Rogue run starts with a weak unevolved starter")
{
    u32 i;
    for (i = 0; i < 30; i++)
    {
        enum Species starter;
        VarSet(VAR_ROGUE_STATE, ROGUE_STATE_HUB);
        VarSet(VAR_ROGUE_ROOM_TYPE, ROGUE_ROOM_HUB);
        gSpecialVar_0x8004 = 1; // the start room's only door is the middle one
        Rogue_TakeDoor();
        EXPECT_EQ(gSpecialVar_Result, ROGUE_DOOR_STARTED_RUN);
        EXPECT_EQ(CalculatePlayerPartyCount(), 1);
        starter = GetMonData(&gParties[B_TRAINER_PLAYER][0], MON_DATA_SPECIES);
        EXPECT_EQ(GetSpeciesPreEvolution(starter), SPECIES_NONE);
        EXPECT_LE(GetSpeciesBaseStatTotal(starter), 320);
        EXPECT(!IsBannedSpecies(starter));
        EXPECT_EQ(VarGet(VAR_ROGUE_FLOOR), 1);
    }
}

TEST("A Rogue door leads to the room its sign shows")
{
    u32 seed, slot;
    u8 doors[3];
    for (seed = 1; seed < 300; seed++)
    {
        SetRoom(seed * 2654435761u, 1 + seed % 3, 1 + seed % (ROGUE_ROOMS_PER_FLOOR - 1), ROGUE_ROOM_WILD);
        Rogue_GetRoomDoors(doors);
        for (slot = 0; slot < 3; slot++)
        {
            if (doors[slot] != 0xFF && doors[slot] != ROGUE_ROOM_MYSTERY)
                break;
        }
        if (slot == 3)
            continue;
        gSpecialVar_0x8004 = slot;
        Rogue_TakeDoor();
        EXPECT_EQ(VarGet(VAR_ROGUE_ROOM_TYPE), doors[slot]);
        EXPECT_EQ(VarGet(VAR_ROGUE_ROOM), 2 + seed % (ROGUE_ROOMS_PER_FLOOR - 1));
    }
}
