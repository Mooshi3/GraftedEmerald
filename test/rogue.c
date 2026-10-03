#include "global.h"
#include "rogue.h"
#include "data.h"
#include "event_data.h"
#include "pokemon.h"
#include "test/test.h"
#include "constants/rogue.h"
#include "constants/weather.h"
#include "constants/move_relearner.h"
#include "graft_settings.h"
#include "graft_bag.h"
#include "graft_challenge.h"
#include "item.h"
#include "battle.h"
#include "battle_setup.h"
#include "script_pokemon_util.h"
#include "party_menu.h"
#include "script_menu.h"
#include "malloc.h"
#include "caps.h"
#include "constants/event_objects.h"

// Grafted Rogue: generated rooms and trainers (src/rogue.c)

// Frees the menu entries a Push special made (the dynamic menu would, on screen)
static void ClearChoices(void)
{
    while (!MultichoiceDynamic_StackEmpty())
        Free((void *)MultichoiceDynamic_PopElement()->name);
    MultichoiceDynamic_DestroyStack();
}

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
    for (seed = 1; seed < 100; seed++)
    {
        for (room = 1; room <= ROGUE_ROOMS_PER_FLOOR; room += 2)
        {
            // every theme (bits 9-11: forest, beach, cave, volcano, power plant)
            SetRoom(seed * 7919, 1 + seed % ROGUE_FLOORS_NORMAL, room, (ROGUE_ROOM_WILD + seed % 6) | (((room / 2 + seed) % 5) << 9));
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
    SetRoom(1234, 1, ROGUE_ROOMS_PER_FLOOR + 1, ROGUE_ROOM_BOSS);
    EXPECT_EQ(Rogue_GetRoomDoors(doors), 1);
    EXPECT_EQ(doors[1], ROGUE_ROOM_REST_STOP); // a boss's room leads to the rest stop
    SetRoom(1234, ROGUE_FLOORS_NORMAL, ROGUE_ROOMS_PER_FLOOR + 1, ROGUE_ROOM_REST_STOP);
    EXPECT_EQ(Rogue_GetRoomDoors(doors), 1);
    EXPECT_EQ(doors[1], ROGUE_ROOM_BOSS); // the last floor's rest stop: on to the finale
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
            EXPECT_GE((u32)boss->partySize, 2);
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
        EXPECT_EQ(VarGet(VAR_ROGUE_ROOM_TYPE) & 0xFF, doors[slot]);
        EXPECT_EQ(VarGet(VAR_ROGUE_ROOM), 2 + seed % (ROGUE_ROOMS_PER_FLOOR - 1));
    }
}

TEST("After the last floor's boss comes the Champion, then the way out")
{
    u8 doors[3];
    SetRoom(777, ROGUE_FLOORS_NORMAL, ROGUE_ROOMS_PER_FLOOR + 1, ROGUE_ROOM_BOSS);
    EXPECT_EQ(Rogue_GetRoomDoors(doors), 1);
    EXPECT_EQ(doors[1], ROGUE_ROOM_REST_STOP);
    gSpecialVar_0x8004 = 1;
    Rogue_TakeDoor();
    EXPECT_EQ(VarGet(VAR_ROGUE_FLOOR), ROGUE_FLOORS_NORMAL);
    EXPECT_EQ(VarGet(VAR_ROGUE_ROOM_TYPE) & 0xFF, ROGUE_ROOM_REST_STOP);
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
    ZeroPlayerPartyMons(); // no team: no new-species bonus
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

TEST("Rogue: nothing is lost on its own; with last rites left, the player is asked about each fainted Pokemon")
{
    SetRoom(99, 1, 2, ROGUE_ROOM_WILD);
    VarSet(VAR_ROGUE_UNLOCKS, 0);
    VarSet(VAR_ROGUE_RUN_FLAGS, 0);
    VarSet(VAR_TEMP_4, 0);
    SetUpFaintedTeam();
    gBattleOutcome = B_OUTCOME_WON;
    Rogue_OnBattleEnd();
    EXPECT_EQ(CalculatePlayerPartyCount(), 2);
    EXPECT_EQ(VarGet(VAR_TEMP_4), 0); // no last rites: nothing to ask
    Rogue_FindFainted();
    EXPECT_EQ(gSpecialVar_Result, FALSE);

    VarSet(VAR_ROGUE_UNLOCKS, 1 << 6); // Last rites I
    Rogue_OnBattleEnd();
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
    VarSet(VAR_ROGUE_UNLOCKS, 0);
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

    SetUpFaintedTeam(); // used up this floor: not asked, and the Pokémon stays
    Rogue_FindFainted();
    EXPECT_EQ(gSpecialVar_Result, FALSE);
    gSpecialVar_0x8004 = 1;
    gSpecialVar_0x8005 = ROGUE_FAINTED_RITES;
    Rogue_ResolveFainted();
    EXPECT_EQ(CalculatePlayerPartyCount(), 2);
    EXPECT(IsGraftBagEmpty());

    // The next floor brings it back
    SetRoom(99, 1, ROGUE_ROOMS_PER_FLOOR + 1, ROGUE_ROOM_REST_STOP);
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
    EXPECT_EQ(GetGraftSetting(GRAFT_SETTING_EXP_SHARE), 1); // the whole team always learns in a run
    EXPECT_EQ(GetGraftSetting(GRAFT_SETTING_LEVEL_CAP), GRAFT_LEVEL_CAP_HARD);

    gSaveBlock2Ptr->frontier.battlePoints = 0;
    SetRoom(GetGraftSetting(GRAFT_SETTING_MONOTYPE), 1, ROGUE_ROOMS_PER_FLOOR + 1, ROGUE_ROOM_BOSS);
    VarSet(VAR_ROGUE_RUN_FLAGS, 1 << 2);
    Rogue_OnBossDefeated();
    EXPECT_EQ(gSaveBlock2Ptr->frontier.battlePoints, 7); // 5 and half again

    Rogue_OnWhiteOut();
    EXPECT_EQ(GetGraftSetting(GRAFT_SETTING_MONOTYPE), TYPE_NONE);
    EXPECT_EQ(GetGraftSetting(GRAFT_SETTING_INVERSE), FALSE);
}

TEST("Rogue: the Favorite unlock offers the favorite's first stage as a starter")
{
    VarSet(VAR_ROGUE_UNLOCKS, (1 << 14)); // UNLOCK_FAVORITE
    ZeroPlayerPartyMons();
    CreateMon(&gParties[B_TRAINER_PLAYER][0], SPECIES_GARCHOMP, 50, 0, OTID_STRUCT_PLAYER_ID);
    gSpecialVar_0x8004 = 0;
    Rogue_SetFavorite();
    EXPECT_EQ(VarGet(VAR_ROGUE_FAVORITE), SPECIES_GIBLE);

    VarSet(VAR_ROGUE_STATE, ROGUE_STATE_HUB);
    VarSet(VAR_ROGUE_ROOM_TYPE, ROGUE_ROOM_HUB);
    gSpecialVar_0x8004 = 1;
    gSpecialVar_0x8006 = ROGUE_CHALLENGE_NONE;
    Rogue_TakeDoor();
    EXPECT_EQ(gSpecialVar_Result, ROGUE_DOOR_CHOOSE_STARTER); // a random starter or the favorite
    EXPECT_EQ(CalculatePlayerPartyCount(), 0);
    gSpecialVar_Result = 1; // the second choice: the favorite
    Rogue_GiveChosenStarter();
    EXPECT_EQ(GetMonData(&gParties[B_TRAINER_PLAYER][0], MON_DATA_SPECIES), SPECIES_GIBLE);
    EXPECT_EQ(GetMonData(&gParties[B_TRAINER_PLAYER][0], MON_DATA_LEVEL), 5);
}

TEST("Rogue: a caught wild Pokemon has no egg moves it never knew")
{
    u32 known;
    SetRoom(99, 1, 2, ROGUE_ROOM_WILD);
    ZeroPlayerPartyMons();
    CreateScriptedWildMon(SPECIES_PANCHAM, 5, ITEM_NONE);
    GiveCapturedMonToPlayer(&gParties[B_TRAINER_OPPONENT_A][0]);
    known = GetMonData(&gParties[B_TRAINER_PLAYER][0], MON_DATA_KNOWN_EGG_MOVES);
    EXPECT_EQ(known, 0);
    known = Rogue_GetEggMoveMask(&gParties[B_TRAINER_PLAYER][0].box, GetSpeciesEggMoves(SPECIES_PANCHAM));
    EXPECT_LE(__builtin_popcount(known), 2);
}

TEST("Rogue: wild Pokemon live where the encounter sheet puts their family")
{
    EXPECT(Rogue_LivesIn(SPECIES_PANCHAM, 0));     // Route 104's land: forest
    EXPECT(!Rogue_LivesIn(SPECIES_PANCHAM, 1));
    EXPECT(Rogue_LivesIn(SPECIES_CLOBBOPUS, 1));   // placed on the beach by hand
    EXPECT(Rogue_LivesIn(SPECIES_GRAPPLOCT, 1));   // and its evolution with it
    EXPECT(Rogue_LivesIn(SPECIES_MAKUHITA, 2));    // Granite Cave
    EXPECT(Rogue_LivesIn(SPECIES_MACHOP, 3));      // Fiery Path
    EXPECT(Rogue_LivesIn(SPECIES_TENTACOOL, 1));   // surfing
    EXPECT(!Rogue_LivesIn(SPECIES_TENTACOOL, 0));
    EXPECT(Rogue_LivesIn(SPECIES_MAGNEMITE, 4));   // New Mauville: the power plant
    EXPECT(Rogue_LivesIn(SPECIES_PAWMI, 4));       // the sheet's Pawmo, by hand
    EXPECT(!Rogue_LivesIn(SPECIES_MAGNEMITE, 2));
}

TEST("Rogue: every theme has wild Pokemon on every floor")
{
    u32 floor, biome, seed;
    u8 doors[3];
    for (floor = 1; floor <= ROGUE_FLOORS_NORMAL; floor++)
    {
        for (biome = 0; biome < 5; biome++)
        {
            for (seed = 1; seed < 6; seed++)
            {
                const struct ObjectEventTemplate *template;
                u16 species;
                SetRoom(seed * 31337, floor, 3, ROGUE_ROOM_WILD | (biome << 9));
                Rogue_GetRoomDoors(doors);
                Rogue_OnTransition();
                template = &gSaveBlock1Ptr->objectEventTemplates[3]; // the first thing in the room
                if (template->graphicsId < OBJ_EVENT_MON)
                    template = &gSaveBlock1Ptr->objectEventTemplates[4]; // after the room's trainer
                species = template->graphicsId - OBJ_EVENT_MON;
                EXPECT(Rogue_LivesIn(species, biome));
            }
        }
    }
}

TEST("Rogue: a starter knows the egg move it rolled")
{
    u32 i, j, checked = 0;
    VarSet(VAR_ROGUE_UNLOCKS, (1 << 12) | (1 << 13)); // Good genes, Pedigree: everyone rolls one
    for (i = 0; i < 20; i++)
    {
        enum Species base;
        const u16 *eggMoves;
        u32 mask;
        struct Pokemon *mon = &gParties[B_TRAINER_PLAYER][0];
        ZeroPlayerPartyMons();
        VarSet(VAR_ROGUE_STATE, ROGUE_STATE_HUB);
        VarSet(VAR_ROGUE_ROOM_TYPE, ROGUE_ROOM_HUB);
        gSpecialVar_0x8004 = 1;
        gSpecialVar_0x8006 = ROGUE_CHALLENGE_NONE;
        Rogue_TakeDoor();
        base = GetMonData(mon, MON_DATA_SPECIES);
        eggMoves = GetSpeciesEggMoves(base);
        mask = Rogue_GetEggMoveMask(&mon->box, eggMoves);
        for (j = 0; j < 19 && eggMoves[j] != MOVE_UNAVAILABLE; j++)
        {
            if (mask & (1u << j))
            {
                EXPECT(MonKnowsMove(mon, eggMoves[j]));
                checked++;
            }
        }
    }
    EXPECT_GT(checked, 0);
}

TEST("Rogue: the rest stop's prize is one of three, once")
{
    SetRoom(4321, 2, ROGUE_ROOMS_PER_FLOOR + 1, ROGUE_ROOM_REST_STOP);
    ClearBag();
    Rogue_HasTakenPrize();
    EXPECT_EQ(gSpecialVar_Result, FALSE);
    gSpecialVar_Result = 2;
    Rogue_GivePrize();
    Rogue_HasTakenPrize();
    EXPECT_EQ(gSpecialVar_Result, TRUE);
    // leaving clears it for the next rest stop
    gSpecialVar_0x8004 = 1;
    Rogue_TakeDoor();
    EXPECT_EQ(VarGet(VAR_ROGUE_FLOOR), 3);
    Rogue_HasTakenPrize();
    EXPECT_EQ(gSpecialVar_Result, FALSE);
}

TEST("Rogue: a tutor move bought with BP is relearnable by the family in later runs")
{
    u16 moves[8];
    memset(gSaveBlock2Ptr->frontier.domeMonIds, 0, sizeof(gSaveBlock2Ptr->frontier.domeMonIds));
    memset(gSaveBlock2Ptr->frontier.towerRecords, 0, sizeof(gSaveBlock2Ptr->frontier.towerRecords));
    VarSet(VAR_ROGUE_UNLOCKS, 0);
    SetRoom(55, 2, ROGUE_ROOMS_PER_FLOOR + 1, ROGUE_ROOM_REST_STOP);
    ZeroPlayerPartyMons();
    CreateMon(&gParties[B_TRAINER_PLAYER][0], SPECIES_MACHOKE, 30, 0, OTID_STRUCT_PLAYER_ID);
    gSaveBlock2Ptr->frontier.battlePoints = 50;
    gSpecialVar_0x8005 = MOVE_ICE_PUNCH;
    gSpecialVar_0x8006 = 1; // BP
    Rogue_CanPayTutor();
    EXPECT_EQ(gSpecialVar_Result, TRUE);
    SetMonMoveSlot(&gParties[B_TRAINER_PLAYER][0], MOVE_ICE_PUNCH, 0); // the party menu teaches it
    Rogue_PayTutor();
    EXPECT_EQ(gSaveBlock2Ptr->frontier.battlePoints, 40);
    EXPECT_EQ(Rogue_GetUnlockedTutorMoves(SPECIES_MACHAMP, moves), 1); // the whole family
    EXPECT_EQ(moves[0], MOVE_ICE_PUNCH);
    EXPECT_EQ(Rogue_GetUnlockedTutorMoves(SPECIES_GEODUDE, moves), 0);
}

TEST("Rogue: second-page unlocks are bought like the first page's")
{
    VarSet(VAR_ROGUE_UNLOCKS, 0);
    VarSet(VAR_ROGUE_UNLOCKS_2, 0);
    gSaveBlock2Ptr->frontier.battlePoints = 100;
    SetRoom(1, 1, 2, ROGUE_ROOM_SHOP);
    EXPECT_EQ(Rogue_GetShopPrice(ITEM_POTION, 1000), 1000);
    EXPECT_EQ(Rogue_GetShopPrice(ITEM_CHOICE_BAND, 100000), 3000); // nothing over ¥3000 in a run
    gSpecialVar_0x8004 = 16; // Discount
    Rogue_BuyUnlock();
    EXPECT_EQ(gSpecialVar_Result, TRUE);
    EXPECT_EQ(VarGet(VAR_ROGUE_UNLOCKS_2), 1);
    EXPECT_EQ(VarGet(VAR_ROGUE_UNLOCKS), 0);
    EXPECT_EQ(Rogue_GetShopPrice(ITEM_POTION, 1000), 750);
    EXPECT_EQ(Rogue_GetShopPrice(ITEM_CHOICE_BAND, 100000), 2250);
    EXPECT_EQ(GetItemSellPrice(ITEM_CHOICE_BAND), 3000 / ITEM_SELL_FACTOR); // no selling prizes for a fortune
}

TEST("Rogue: Harvest bonus sometimes adds a spare copy of a piece")
{
    u32 i, bonus = 0, lost;
    struct GraftPiece pieces[16];
    VarSet(VAR_ROGUE_UNLOCKS_2, 1 << 1); // Harvest bonus
    SetRoom(1, 1, 2, ROGUE_ROOM_WILD);
    CreateMon(&gParties[B_TRAINER_OPPONENT_A][0], SPECIES_ZIGZAGOON, 10, 0, OTID_STRUCT_PLAYER_ID);
    for (i = 0; i < 10; i++)
    {
        ClearGraftBag();
        HarvestGraftPieces(&gParties[B_TRAINER_OPPONENT_A][0], TRUE, 13, pieces, &lost);
        bonus += gGraftHarvestBonus;
    }
    EXPECT_GT(bonus, 0);
    VarSet(VAR_ROGUE_UNLOCKS_2, 0);
    ClearGraftBag();
    HarvestGraftPieces(&gParties[B_TRAINER_OPPONENT_A][0], TRUE, 13, pieces, &lost);
    EXPECT_EQ(gGraftHarvestBonus, 0);
}

TEST("Rogue: Catch bonus sometimes leaves a piece after a real catch, never after a harvest")
{
    u32 i, got = 0;
    VarSet(VAR_ROGUE_UNLOCKS_2, 1 << 2); // Catch bonus
    SetRoom(1, 1, 2, ROGUE_ROOM_WILD);
    CreateMon(&gParties[B_TRAINER_OPPONENT_A][0], SPECIES_ZIGZAGOON, 10, 0, OTID_STRUCT_PLAYER_ID);
    gBattleOutcome = B_OUTCOME_CAUGHT;
    gLastThrownBall = ITEM_POKE_BALL;
    for (i = 0; i < 40; i++)
    {
        VarSet(VAR_TEMP_5, 0);
        Rogue_OnBattleFinishing();
        got += VarGet(VAR_TEMP_5);
    }
    EXPECT_GT(got, 0);
    EXPECT_LT(got, 40);
    gLastThrownBall = ITEM_GRAFT_BALL;
    for (i = 0; i < 40; i++)
    {
        VarSet(VAR_TEMP_5, 0);
        Rogue_OnBattleFinishing();
        EXPECT_EQ(VarGet(VAR_TEMP_5), 0);
    }
}

TEST("Rogue: hundreds of tutor moves can be kept, and old saves' are moved over")
{
    u16 moves[MAX_RELEARNER_MOVES];
    u32 i;
    memset(gSaveBlock2Ptr->frontier.towerRecords, 0, sizeof(gSaveBlock2Ptr->frontier.towerRecords));
    // an entry saved by the first rest stop builds, in the Battle Dome's data
    ((u32 *)gSaveBlock2Ptr->frontier.domeMonIds)[0] = (SPECIES_MACHOP << 16) | MOVE_ICE_PUNCH;
    EXPECT_EQ(Rogue_GetUnlockedTutorMoves(SPECIES_MACHAMP, moves), 1);
    EXPECT_EQ(((u32 *)gSaveBlock2Ptr->frontier.domeMonIds)[0], 0);
    EXPECT_GE(sizeof(gSaveBlock2Ptr->frontier.towerRecords) / (sizeof(u32)), 295);
    // 250 different families each keep theirs
    for (i = 0; i < 250; i++)
        ((u32 *)gSaveBlock2Ptr->frontier.towerRecords)[i + 1] = ((SPECIES_CHIKORITA + i) << 16) | MOVE_SWIFT;
    EXPECT_EQ(Rogue_GetUnlockedTutorMoves(SPECIES_MACHOP, moves), 1);
    EXPECT_EQ(Rogue_GetUnlockedTutorMoves(SPECIES_CHIKORITA + 249, moves), 1);
}

TEST("Rogue: winning with a species for the first time pays a bonus, once")
{
    memset(&gSaveBlock2Ptr->frontier.ereaderTrainer, 0, sizeof(gSaveBlock2Ptr->frontier.ereaderTrainer));
    VarSet(VAR_ROGUE_UNLOCKS, 1 << 15); // Hard mode already open: no message about it
    VarSet(VAR_ROGUE_RUN_FLAGS, 0);
    ZeroPlayerPartyMons();
    CreateMon(&gParties[B_TRAINER_PLAYER][0], SPECIES_GARCHOMP, 50, 0, OTID_STRUCT_PLAYER_ID);
    CreateMon(&gParties[B_TRAINER_PLAYER][1], SPECIES_SLOWPOKE_GALAR, 50, 0, OTID_STRUCT_PLAYER_ID);
    gSaveBlock2Ptr->frontier.battlePoints = 0;
    SetRoom(4242, ROGUE_FLOORS_NORMAL + 1, 1, ROGUE_ROOM_BOSS); // the Champion
    Rogue_OnBossDefeated();
    EXPECT_EQ(gSaveBlock2Ptr->frontier.battlePoints, 20 + 2 * 3);
    EXPECT_EQ(Rogue_CountWonSpecies(), 2);

    // the same team again: no bonus; Slowpoke's regular form counts as the same species
    CreateMon(&gParties[B_TRAINER_PLAYER][1], SPECIES_SLOWPOKE, 50, 0, OTID_STRUCT_PLAYER_ID);
    gSaveBlock2Ptr->frontier.battlePoints = 0;
    Rogue_OnBossDefeated();
    EXPECT_EQ(gSaveBlock2Ptr->frontier.battlePoints, 20);
    EXPECT_EQ(Rogue_CountWonSpecies(), 2);
    VarSet(VAR_ROGUE_UNLOCKS, 0);
}

TEST("Rogue: EXP is boosted below the floor's boss level and stops at it")
{
    VarSet(VAR_ROGUE_UNLOCKS, 0);
    SetRoom(1, 1, 3, ROGUE_ROOM_WILD);
    SetGraftSetting(GRAFT_SETTING_LEVEL_CAP, GRAFT_LEVEL_CAP_HARD);
    EXPECT_EQ(Rogue_GetLevelCap(), 11);              // floor 1's boss
    EXPECT_EQ(GetCurrentLevelCap(), 11);
    EXPECT_EQ(GetSoftLevelCapExpValue(10, 100), 200); // double EXP
    EXPECT_EQ(GetSoftLevelCapExpValue(5, 100), 400);  // and double again to catch up
    EXPECT_EQ(GetSoftLevelCapExpValue(11, 100), 0);   // the cap
    VarSet(VAR_ROGUE_UNLOCKS, 1 << 10);              // Quick study
    EXPECT_EQ(GetSoftLevelCapExpValue(10, 100), 300);
    VarSet(VAR_ROGUE_UNLOCKS, 0);
    SetRoom(1, 3, 3, ROGUE_ROOM_WILD);
    EXPECT_EQ(Rogue_GetLevelCap(), 27);              // floor 3's boss
}

TEST("Rogue: trainers and Normal mode's Leaders carry no items")
{
    u32 seed, i;
    const struct Trainer *trainer;
    VarSet(VAR_ROGUE_RUN_FLAGS, 0); // Normal mode
    for (seed = 1; seed < 20; seed++)
    {
        SetRoom(seed * 7, 1 + seed % ROGUE_FLOORS_NORMAL, 4, ROGUE_ROOM_TRAINER);
        trainer = GetTrainerStructFromId(TRAINER_ROGUE_TRAINER);
        for (i = 0; i < MAX_TRAINER_ITEMS; i++)
            EXPECT_EQ(trainer->items[i], ITEM_NONE);
        SetRoom(seed * 7, 1 + seed % ROGUE_FLOORS_NORMAL, ROGUE_ROOMS_PER_FLOOR + 1, ROGUE_ROOM_BOSS);
        trainer = GetTrainerStructFromId(TRAINER_ROGUE_BOSS);
        for (i = 0; i < MAX_TRAINER_ITEMS; i++)
            EXPECT_EQ(trainer->items[i], ITEM_NONE);
    }
    // (the Champion keeps his: the test build's trainer data gives vanilla Wallace none to check)
}

TEST("Rogue: trainers pay triple, double again in trainer and Ace rooms")
{
    SetRoom(5, 1, 3, ROGUE_ROOM_WILD);
    EXPECT_EQ(Rogue_ScaleTrainerMoney(TRAINER_ROGUE_TRAINER, 100), 300); // a guard
    SetRoom(5, 1, 3, ROGUE_ROOM_TRAINER);
    EXPECT_EQ(Rogue_ScaleTrainerMoney(TRAINER_ROGUE_TRAINER, 100), 600);
    SetRoom(5, 1, 3, ROGUE_ROOM_ACE);
    EXPECT_EQ(Rogue_ScaleTrainerMoney(TRAINER_ROGUE_TRAINER, 100), 600);
    SetRoom(5, 1, ROGUE_ROOMS_PER_FLOOR + 1, ROGUE_ROOM_BOSS);
    EXPECT_EQ(Rogue_ScaleTrainerMoney(TRAINER_ROGUE_BOSS, 100), 300);
}

TEST("Rogue: beaten trainers offer pieces of their team; an Ace Trainer adds an item")
{
    ClearGraftBag();
    ClearBag();
    ZeroEnemyPartyMons();
    CreateMon(&gParties[B_TRAINER_OPPONENT_A][0], SPECIES_MACHOP, 10, 0, OTID_STRUCT_PLAYER_ID);
    CreateMon(&gParties[B_TRAINER_OPPONENT_A][1], SPECIES_ZIGZAGOON, 10, 0, OTID_STRUCT_PLAYER_ID);

    SetRoom(77, 1, 3, ROGUE_ROOM_WILD); // a guard: nothing
    Rogue_PushTrainerPieces();
    EXPECT_EQ(gSpecialVar_Result, 0);

    // offers are picked when the battle ends: the opponent party is gone by the post-battle script
    gBattleTypeFlags = BATTLE_TYPE_TRAINER;
    gBattleOutcome = B_OUTCOME_WON;
    TRAINER_BATTLE_PARAM.opponentA = TRAINER_ROGUE_TRAINER;
    SetRoom(77, 1, 3, ROGUE_ROOM_TRAINER);
    Rogue_OnBattleFinishing();
    ZeroEnemyPartyMons(); // as the battle does before the post-battle script
    Rogue_OnBattleFinishing(); // the battle calls it every frame of its fade out: the offers stay
    Rogue_PushTrainerPieces();
    ClearChoices();
    EXPECT_EQ(gSpecialVar_Result, 3);
    gSpecialVar_Result = 1;
    Rogue_GiveTrainerPiece();
    EXPECT(!IsGraftBagEmpty());
    EXPECT_EQ(gSpecialVar_0x8005, FALSE);

    SetRoom(77, 1, 3, ROGUE_ROOM_ACE);
    EXPECT_GE((u32)GetTrainerStructFromId(TRAINER_ROGUE_TRAINER)->partySize, 2); // one more Pokémon
    CreateMon(&gParties[B_TRAINER_OPPONENT_A][0], SPECIES_MACHOP, 10, 0, OTID_STRUCT_PLAYER_ID);
    CreateMon(&gParties[B_TRAINER_OPPONENT_A][1], SPECIES_ZIGZAGOON, 10, 0, OTID_STRUCT_PLAYER_ID);
    Rogue_OnBattleFinishing();
    Rogue_PushTrainerPieces();
    ClearChoices();
    EXPECT_EQ(gSpecialVar_Result, 4);
    gSpecialVar_Result = 3;
    Rogue_GiveTrainerPiece();
    EXPECT_EQ(gSpecialVar_0x8005, TRUE);
    gBattleTypeFlags = 0;
}

TEST("Rogue: the Graft vault keeps one piece for the next run")
{
    struct GraftPiece speed = {GRAFT_PIECE_STAT, 3, 120};
    struct GraftPiece fire = {GRAFT_PIECE_TYPE, 0, TYPE_FIRE};
    VarSet(VAR_ROGUE_VAULT_KIND, 0);
    VarSet(VAR_ROGUE_VAULT_VALUE, 0);
    VarSet(VAR_ROGUE_UNLOCKS_2, 0);
    SetRoom(5, 1, 2, ROGUE_ROOM_WILD);
    EXPECT(!Rogue_CanVaultPieces());
    VarSet(VAR_ROGUE_UNLOCKS_2, 1 << 3); // Graft vault
    EXPECT(Rogue_CanVaultPieces());

    ClearGraftBag();
    AddGraftPiece(&speed);
    AddGraftPiece(&fire);
    Rogue_VaultPiece(&speed);
    EXPECT_EQ(GetGraftPieceCount(&speed), 0); // out of this run
    Rogue_VaultPiece(&fire);                  // swaps: the Speed piece comes back
    EXPECT_EQ(GetGraftPieceCount(&speed), 1);
    EXPECT_EQ(GetGraftPieceCount(&fire), 0);

    ClearGraftBag(); // the next run
    Rogue_TakeVaultPiece();
    EXPECT_EQ(gSpecialVar_Result, TRUE);
    EXPECT_EQ(GetGraftPieceCount(&fire), 1);
    Rogue_TakeVaultPiece(); // emptied
    EXPECT_EQ(gSpecialVar_Result, FALSE);
    EXPECT_EQ(GetGraftPieceCount(&fire), 1);
    VarSet(VAR_ROGUE_UNLOCKS_2, 0);
}

TEST("Rogue: some floors after the first have weather")
{
    u32 seed, floor, weathered = 0, total = 0;
    for (seed = 1; seed < 30; seed++)
    {
        SetRoom(seed * 7919, 1, 1, ROGUE_ROOM_WILD);
        EXPECT_EQ(Rogue_GetFloorWeather(1), 0xFF);
        for (floor = 2; floor <= 8; floor++)
        {
            u32 weather = Rogue_GetFloorWeather(floor);
            total++;
            if (weather != 0xFF)
            {
                weathered++;
                EXPECT(weather == WEATHER_RAIN || weather == WEATHER_DROUGHT || weather == WEATHER_SANDSTORM || weather == WEATHER_SNOW);
            }
        }
    }
    EXPECT_GT(weathered, total / 5);
    EXPECT_LT(weathered, total * 3 / 5);
}
