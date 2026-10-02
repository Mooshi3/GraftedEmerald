#include "global.h"
#include "rogue.h"
#include "data.h"
#include "event_data.h"
#include "pokemon.h"
#include "test/test.h"
#include "constants/rogue.h"
#include "graft_settings.h"
#include "graft_bag.h"
#include "graft_challenge.h"
#include "item.h"
#include "battle.h"

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
    EXPECT_EQ(doors[1], ROGUE_ROOM_BOSS); // the last floor's boss: on to the finale
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
        gSpecialVar_0x8006 = ROGUE_CHALLENGE_NONE;
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

TEST("After the last floor's boss comes the Champion, then the way out")
{
    u8 doors[3];
    SetRoom(777, ROGUE_FLOORS_NORMAL, ROGUE_ROOMS_PER_FLOOR + 1, ROGUE_ROOM_BOSS);
    EXPECT_EQ(Rogue_GetRoomDoors(doors), 1);
    EXPECT_EQ(doors[1], ROGUE_ROOM_BOSS);
    gSpecialVar_0x8004 = 1;
    Rogue_TakeDoor();
    EXPECT_EQ(VarGet(VAR_ROGUE_FLOOR), ROGUE_FLOORS_NORMAL + 1);
    EXPECT_EQ(VarGet(VAR_ROGUE_ROOM), 1);
    EXPECT_EQ(VarGet(VAR_ROGUE_ROOM_TYPE), ROGUE_ROOM_BOSS);
    EXPECT_EQ(Rogue_GetBossTrainerId(), TRAINER_WALLACE);
    EXPECT_GE((u32)GetTrainerStructFromId(TRAINER_ROGUE_BOSS)->partySize, 5);
    EXPECT_EQ(Rogue_GetRoomDoors(doors), 1);
    EXPECT_EQ(doors[1], ROGUE_ROOM_HUB);
}

TEST("Rogue unlocks cost BP and apply from the next run")
{
    VarSet(VAR_ROGUE_UNLOCKS, 0);
    gSaveBlock2Ptr->frontier.battlePoints = 45;
    gSpecialVar_0x8004 = 1; // 4th team slot, 40 BP
    Rogue_BuyUnlock();
    EXPECT_EQ(gSpecialVar_Result, TRUE);
    EXPECT_EQ(gSaveBlock2Ptr->frontier.battlePoints, 5);
    gSpecialVar_0x8004 = 0; // starter choice, 30 BP: not enough left
    Rogue_BuyUnlock();
    EXPECT_EQ(gSpecialVar_Result, FALSE);
    EXPECT_EQ(gSaveBlock2Ptr->frontier.battlePoints, 5);

    VarSet(VAR_ROGUE_STATE, ROGUE_STATE_HUB);
    VarSet(VAR_ROGUE_ROOM_TYPE, ROGUE_ROOM_HUB);
    gSpecialVar_0x8004 = 1;
    gSpecialVar_0x8005 = 0;
    gSpecialVar_0x8006 = 0;
    Rogue_TakeDoor();
    EXPECT_EQ(gSpecialVar_Result, ROGUE_DOOR_ASK_CHALLENGE);
    gSpecialVar_0x8006 = ROGUE_CHALLENGE_NONE;
    Rogue_TakeDoor();
    EXPECT_EQ(gSpecialVar_Result, ROGUE_DOOR_STARTED_RUN);
    EXPECT_EQ(VarGet(VAR_ROGUE_TEAM_SLOTS), 4);
    VarSet(VAR_ROGUE_UNLOCKS, 0);
}

TEST("With Unlocks Off, a run starts with everything unlocked")
{
    SetGraftSetting(GRAFT_SETTING_ROGUE_META, FALSE);
    VarSet(VAR_ROGUE_UNLOCKS, 0);
    VarSet(VAR_ROGUE_STATE, ROGUE_STATE_HUB);
    VarSet(VAR_ROGUE_ROOM_TYPE, ROGUE_ROOM_HUB);
    gSpecialVar_0x8004 = 1;
    gSpecialVar_0x8005 = 0;
    Rogue_TakeDoor();
    EXPECT_EQ(gSpecialVar_Result, ROGUE_DOOR_ASK_MODE); // Hard mode is open too
    gSpecialVar_0x8005 = 2;
    gSpecialVar_0x8006 = 0;
    Rogue_TakeDoor();
    EXPECT_EQ(gSpecialVar_Result, ROGUE_DOOR_ASK_CHALLENGE);
    gSpecialVar_0x8006 = ROGUE_CHALLENGE_NONE;
    Rogue_TakeDoor();
    EXPECT_EQ(gSpecialVar_Result, ROGUE_DOOR_CHOOSE_STARTER);
    EXPECT_EQ(CalculatePlayerPartyCount(), 0);
    gSpecialVar_Result = 2;
    Rogue_GiveChosenStarter();
    EXPECT_EQ(CalculatePlayerPartyCount(), 1);
    EXPECT_EQ(VarGet(VAR_ROGUE_TEAM_SLOTS), 6);
    EXPECT_EQ(VarGet(VAR_ROGUE_RUN_FLAGS), 1);
    SetGraftSetting(GRAFT_SETTING_ROGUE_META, TRUE);
}

TEST("Beating the Champion pays BP and opens Hard mode")
{
    VarSet(VAR_ROGUE_UNLOCKS, 0);
    VarSet(VAR_ROGUE_RUN_FLAGS, 0);
    gSaveBlock2Ptr->frontier.battlePoints = 0;
    SetRoom(4242, 2, ROGUE_ROOMS_PER_FLOOR + 1, ROGUE_ROOM_BOSS);
    Rogue_OnBossDefeated();
    EXPECT_EQ(gSaveBlock2Ptr->frontier.battlePoints, 5);
    EXPECT(FlagGet(FLAG_BADGE02_GET));
    EXPECT_EQ(VarGet(VAR_ROGUE_UNLOCKS), 0);

    SetRoom(4242, ROGUE_FLOORS_NORMAL + 1, 1, ROGUE_ROOM_BOSS); // the Champion
    Rogue_OnBossDefeated();
    EXPECT_EQ(gSaveBlock2Ptr->frontier.battlePoints, 5 + 20);
    EXPECT_EQ(VarGet(VAR_ROGUE_UNLOCKS), 1 << 15);
    VarSet(VAR_ROGUE_UNLOCKS, 0);
}

static void SetUpFaintedTeam(void)
{
    u32 hp = 0;
    ZeroPlayerPartyMons();
    CreateMon(&gParties[B_TRAINER_PLAYER][0], SPECIES_WOBBUFFET, 20, 0, OTID_STRUCT_PLAYER_ID);
    CreateMon(&gParties[B_TRAINER_PLAYER][1], SPECIES_PIKACHU, 20, 0, OTID_STRUCT_PLAYER_ID);
    CalculateMonStats(&gParties[B_TRAINER_PLAYER][0]);
    CalculateMonStats(&gParties[B_TRAINER_PLAYER][1]);
    SetMonData(&gParties[B_TRAINER_PLAYER][1], MON_DATA_HP, &hp);
    hp = GetMonData(&gParties[B_TRAINER_PLAYER][0], MON_DATA_MAX_HP);
    SetMonData(&gParties[B_TRAINER_PLAYER][0], MON_DATA_HP, &hp);
    CalculatePlayerPartyCount();
    ClearGraftBag();
    ClearBag();
}

TEST("Rogue: nothing is lost on its own; the player is asked about each fainted Pokemon")
{
    SetRoom(99, 1, 2, ROGUE_ROOM_WILD);
    VarSet(VAR_ROGUE_UNLOCKS, 0);
    VarSet(VAR_ROGUE_RUN_FLAGS, 0);
    VarSet(VAR_TEMP_4, 0);
    SetUpFaintedTeam();
    gBattleOutcome = B_OUTCOME_WON;
    Rogue_OnBattleEnd();
    EXPECT_EQ(CalculatePlayerPartyCount(), 2);
    EXPECT_EQ(VarGet(VAR_TEMP_4), 1);

    Rogue_FindFainted();
    EXPECT_EQ(gSpecialVar_Result, TRUE);
    EXPECT_EQ(gSpecialVar_0x8004, 1);
    gSpecialVar_0x8005 = ROGUE_FAINTED_KEEP;
    Rogue_ResolveFainted();
    EXPECT_EQ(CalculatePlayerPartyCount(), 2);
    Rogue_FindFainted();
    EXPECT_EQ(gSpecialVar_Result, FALSE); // kept: not asked again

    VarSet(VAR_TEMP_4, 0);
    Rogue_OnBattleEnd();
    EXPECT_EQ(VarGet(VAR_TEMP_4), 0);

    SetUpFaintedTeam();
    Rogue_FindFainted();
    gSpecialVar_0x8005 = ROGUE_FAINTED_RELEASE;
    Rogue_ResolveFainted();
    EXPECT_EQ(CalculatePlayerPartyCount(), 1);
    EXPECT(IsGraftBagEmpty());
}

TEST("Rogue: last rites break a fainted Pokemon down, a few times a floor")
{
    SetRoom(99, 1, 2, ROGUE_ROOM_WILD);
    VarSet(VAR_ROGUE_RUN_FLAGS, 0);
    VarSet(VAR_ROGUE_UNLOCKS, 1 << 6); // Last rites I: once a floor

    SetUpFaintedTeam();
    Rogue_FindFainted();
    gSpecialVar_0x8005 = ROGUE_FAINTED_RITES;
    Rogue_ResolveFainted();
    EXPECT_EQ(CalculatePlayerPartyCount(), 1);
    EXPECT(!IsGraftBagEmpty());

    SetUpFaintedTeam(); // used up this floor: it's a plain release
    Rogue_FindFainted();
    gSpecialVar_0x8005 = ROGUE_FAINTED_RITES;
    Rogue_ResolveFainted();
    EXPECT_EQ(CalculatePlayerPartyCount(), 1);
    EXPECT(IsGraftBagEmpty());

    // The next floor brings it back
    SetRoom(99, 1, ROGUE_ROOMS_PER_FLOOR + 1, ROGUE_ROOM_BOSS);
    gSpecialVar_0x8004 = 0;
    {
        u8 doors[3];
        u32 slot;
        Rogue_GetRoomDoors(doors);
        for (slot = 0; slot < 3 && doors[slot] == 0xFF; slot++)
            ;
        gSpecialVar_0x8004 = slot;
    }
    Rogue_TakeDoor();
    EXPECT_EQ(VarGet(VAR_ROGUE_FLOOR), 2);
    SetUpFaintedTeam();
    Rogue_FindFainted();
    gSpecialVar_0x8005 = ROGUE_FAINTED_RITES;
    Rogue_ResolveFainted();
    EXPECT(!IsGraftBagEmpty());
    VarSet(VAR_ROGUE_UNLOCKS, 0);
}

TEST("Rogue: challenges last one run and pay more BP")
{
    VarSet(VAR_ROGUE_UNLOCKS, 0);
    VarSet(VAR_ROGUE_STATE, ROGUE_STATE_HUB);
    VarSet(VAR_ROGUE_ROOM_TYPE, ROGUE_ROOM_HUB);
    gSpecialVar_0x8004 = 1;
    gSpecialVar_0x8005 = 1;
    gSpecialVar_0x8006 = ROGUE_CHALLENGE_MONOTYPE;
    gSpecialVar_0x8007 = TYPE_GHOST;
    Rogue_TakeDoor();
    EXPECT_EQ(gSpecialVar_Result, ROGUE_DOOR_STARTED_RUN);
    EXPECT_EQ(GetGraftSetting(GRAFT_SETTING_MONOTYPE), TYPE_GHOST);
    EXPECT(IsMonotypeLegal(&gParties[B_TRAINER_PLAYER][0])); // the starter got Ghost grafted on
    EXPECT_EQ(GetGraftSetting(GRAFT_SETTING_SET_MODE), TRUE); // no Switch style yet
    EXPECT_EQ(GetGraftSetting(GRAFT_SETTING_EXP_SHARE), 0);

    gSaveBlock2Ptr->frontier.battlePoints = 0;
    SetRoom(GetGraftSetting(GRAFT_SETTING_MONOTYPE), 1, ROGUE_ROOMS_PER_FLOOR + 1, ROGUE_ROOM_BOSS);
    VarSet(VAR_ROGUE_RUN_FLAGS, 1 << 2);
    Rogue_OnBossDefeated();
    EXPECT_EQ(gSaveBlock2Ptr->frontier.battlePoints, 7); // 5 and half again

    Rogue_OnWhiteOut();
    EXPECT_EQ(GetGraftSetting(GRAFT_SETTING_MONOTYPE), TYPE_NONE);
    EXPECT_EQ(GetGraftSetting(GRAFT_SETTING_INVERSE), FALSE);
}
