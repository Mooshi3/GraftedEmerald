// Enemy grafts on every real trainer party, on the map the trainer is battled on.
// The test build replaces gTrainers with test trainers, so the real ones are included here under their own name.
#include "global.h"
#include "graft_data.h"
#include "graft_settings.h"
#include "battle_setup.h"
#include "event_data.h"
#include "overworld.h"
#include "pokemon.h"
#include "test/test.h"
#include "constants/map_groups.h"
#include "constants/opponents.h"
#include "constants/abilities.h"
#include "battle.h"
#include "data.h"
#include "trainer_pools.h"
#include "battle_transition.h"
#include "constants/items.h"
#include "constants/moves.h"
#include "constants/trainers.h"
#include "constants/battle_ai.h"

static const struct Trainer sRealTrainers[DIFFICULTY_COUNT][TRAINERS_COUNT] =
{
#include "../src/data/trainers.h"
};

static const struct { u16 trainer; u16 map; } sTrainerMaps[] =
{
#include "graft_trainer_maps.h"
};

#define SLICES 8

static void SetMap(u32 map)
{
    gSaveBlock1Ptr->location.mapGroup = map >> 8;
    gSaveBlock1Ptr->location.mapNum = map & 0xFF;
    gMapHeader = *Overworld_GetMapHeaderByGroupAndId(map >> 8, map & 0xFF);
    // Every map's own trainers: the way in is its section's outdoor map, Ever Grande as a stand-in
    gSaveBlock1Ptr->escapeWarp.mapGroup = MAP_EVER_GRANDE_CITY >> 8;
    gSaveBlock1Ptr->escapeWarp.mapNum = MAP_EVER_GRANDE_CITY & 0xFF;
}

static void ExpectValidEnemy(struct Pokemon *mon)
{
    u32 i, j, species = GetMonData(mon, MON_DATA_SPECIES), moves = 0;
    struct BoxPokemon *box = &mon->box;

    for (i = 0; i < MAX_MON_MOVES; i++)
    {
        u32 move = GetMonData(mon, MON_DATA_MOVE1 + i);
        EXPECT_LT(move, MOVES_COUNT);
        if (move == MOVE_NONE)
            continue;
        moves++;
        for (j = i + 1; j < MAX_MON_MOVES; j++)
            EXPECT_NE(move, GetMonData(mon, MON_DATA_MOVE1 + j));
    }
    EXPECT_GT(moves, 0);
    if (box->graftFlags & GRAFT_FLAG_ABILITY)
    {
        EXPECT_NE(box->graftAbility, ABILITY_NONE);
        EXPECT_LT(box->graftAbility, ABILITIES_COUNT);
    }
    for (i = 0; i < 2; i++)
    {
        if (box->graftTypes[i] == GRAFT_TYPE_NONE)
            continue;
        EXPECT_LT(box->graftTypes[i], NUMBER_OF_MON_TYPES);
        EXPECT_NE(box->graftTypes[i], TYPE_NONE);
        EXPECT_NE(box->graftTypes[i], TYPE_MYSTERY);
    }
    for (i = 0; i < NUM_STATS; i++)
    {
        if (box->graftStatOverrides[i] != 0)
            EXPECT_GT(box->graftStatOverrides[i], GetSpeciesBaseStat(species, i));
    }
    if (GetMonAbility(mon) == ABILITY_WONDER_GUARD)
        EXPECT_EQ(GetMonData(mon, MON_DATA_MAX_HP), 1);
    EXPECT_GT(GetMonData(mon, MON_DATA_MAX_HP), 0);
    EXPECT_EQ(GetMonData(mon, MON_DATA_HP), GetMonData(mon, MON_DATA_MAX_HP));
}

// Builds the trainer's party like a real battle, rolls its grafts and checks every mon
static void RollAndCheck(u32 trainer)
{
    const struct Trainer *data = &sRealTrainers[DIFFICULTY_NORMAL][trainer];
    struct Pokemon *party = gParties[B_TRAINER_OPPONENT_A];
    u32 i, count = 0;

    ZeroEnemyPartyMons();
    CreateNPCTrainerPartyFromTrainer(party, data);
    Test_RollTrainerGrafts(party, data->partySize, trainer);
    for (i = 0; i < PARTY_SIZE; i++)
    {
        if (GetMonData(&party[i], MON_DATA_SPECIES) == SPECIES_NONE)
            continue;
        ExpectValidEnemy(&party[i]);
        count++;
    }
    EXPECT_GE(count, data->partySize); // a boss's ace is extra, never a replacement
    EXPECT_LE(count, PARTY_SIZE);
}

TEST("Every trainer's party takes enemy grafts validly on its own map")
{
    u32 slice, i, f, checked = 0;
    PARAMETRIZE { slice = 0; }
    PARAMETRIZE { slice = 1; }
    PARAMETRIZE { slice = 2; }
    PARAMETRIZE { slice = 3; }
    PARAMETRIZE { slice = 4; }
    PARAMETRIZE { slice = 5; }
    PARAMETRIZE { slice = 6; }
    PARAMETRIZE { slice = 7; }

    VarSet(VAR_GRAFTED_SETTINGS, 0);
    VarSet(VAR_GRAFTED_SETTINGS_2, 0);
    VarSet(VAR_GRAFTED_SETTINGS_3, 0);
    for (i = slice; i < ARRAY_COUNT(sTrainerMaps); i += SLICES)
    {
        u32 trainer = sTrainerMaps[i].trainer;
        if (sRealTrainers[DIFFICULTY_NORMAL][trainer].partySize == 0)
            continue;
        SetMap(sTrainerMaps[i].map);
        // Every trainer's map has donors, directly or through the fallbacks
        BuildNearbyGraftPool();
        EXPECT_NE(GetGraftDonorPoolSize(), 0);

        // The most grafts: 8 badges, Enemy Grafts: More (twice), then the fewest: no badges, Fewer
        for (f = FLAG_BADGE01_GET; f <= FLAG_BADGE08_GET; f++)
            FlagSet(f);
        SetGraftSetting(GRAFT_SETTING_ENEMY_GRAFTS, GRAFT_ENEMY_GRAFTS_MORE);
        RollAndCheck(trainer);
        RollAndCheck(trainer);
        for (f = FLAG_BADGE01_GET; f <= FLAG_BADGE08_GET; f++)
            FlagClear(f);
        SetGraftSetting(GRAFT_SETTING_ENEMY_GRAFTS, GRAFT_ENEMY_GRAFTS_FEWER);
        RollAndCheck(trainer);
        checked++;
    }
    EXPECT_GT(checked, 90);
    VarSet(VAR_GRAFTED_SETTINGS, 0);
    VarSet(VAR_GRAFTED_SETTINGS_2, 0);
    VarSet(VAR_GRAFTED_SETTINGS_3, 0);
}
