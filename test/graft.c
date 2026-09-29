#include "global.h"
#include "graft_data.h"
#include "pokemon_storage_system.h"
#include "save.h"
#include "test/test.h"

// Regression tests for graft persistence. Grafts live on BoxPokemon, so every
// path that copies a mon (party <-> PC, BoxMonToMon, save/load) must carry them.

static void GiveTestGrafts(struct Pokemon *mon)
{
    SetGraftedTypeOnMon(mon, 0, TYPE_FIRE);
    SetGraftedTypeOnMon(mon, 1, TYPE_STEEL);
    SetGraftedAbilityOnMon(mon, ABILITY_INTIMIDATE);
    SetGraftedStatOnMon(mon, STAT_ATK, 150);
    mon->box.graftMoves[0] = MOVE_FLAMETHROWER;
    mon->box.graftFlags |= GRAFT_FLAG_MOVE;
}

static void ExpectTestGrafts(struct BoxPokemon *boxMon)
{
    EXPECT_EQ(boxMon->graftTypes[0], TYPE_FIRE);
    EXPECT_EQ(boxMon->graftTypes[1], TYPE_STEEL);
    EXPECT_EQ(boxMon->graftAbility, ABILITY_INTIMIDATE);
    EXPECT_EQ(boxMon->graftStatOverrides[STAT_ATK], 150);
    EXPECT_EQ(boxMon->graftMoves[0], MOVE_FLAMETHROWER);
    EXPECT_EQ(boxMon->graftFlags, GRAFT_FLAG_TYPE | GRAFT_FLAG_ABILITY | GRAFT_FLAG_STAT | GRAFT_FLAG_MOVE);
}

TEST("New mons have no grafts")
{
    struct Pokemon mon;
    CreateRandomMonWithIVs(&mon, SPECIES_WOBBUFFET, 50, 0);
    EXPECT_EQ(mon.box.graftTypes[0], GRAFT_TYPE_NONE);
    EXPECT_EQ(mon.box.graftTypes[1], GRAFT_TYPE_NONE);
    EXPECT_EQ(mon.box.graftAbility, GRAFT_ABILITY_NONE);
    EXPECT_EQ(mon.box.graftFlags, 0);
    for (u32 i = 0; i < NUM_STATS; i++)
        EXPECT_EQ(mon.box.graftStatOverrides[i], 0);
    for (u32 i = 0; i < MAX_MON_MOVES; i++)
        EXPECT_EQ(mon.box.graftMoves[i], MOVE_NONE);
}

TEST("Grafts round-trip through BoxMonToMon")
{
    struct Pokemon mon1, mon2;
    CreateRandomMonWithIVs(&mon1, SPECIES_WOBBUFFET, 50, 0);
    GiveTestGrafts(&mon1);
    BoxMonToMon(&mon1.box, &mon2);
    ExpectTestGrafts(&mon2.box);
    EXPECT_EQ(GetMonData(&mon2, MON_DATA_ATK), GetMonData(&mon1, MON_DATA_ATK));
    EXPECT_EQ(GetMonAbility(&mon2), ABILITY_INTIMIDATE);
}

TEST("Grafts survive depositing a mon in the PC and withdrawing it")
{
    struct Pokemon mon;
    CreateRandomMonWithIVs(&gPlayerParty[0], SPECIES_WOBBUFFET, 50, 0);
    GiveTestGrafts(&gPlayerParty[0]);
    u32 atk = GetMonData(&gPlayerParty[0], MON_DATA_ATK);

    SetBoxMonAt(TOTAL_BOXES_COUNT - 1, IN_BOX_COUNT - 1, &gPlayerParty[0].box);
    ExpectTestGrafts(GetBoxedMonPtr(TOTAL_BOXES_COUNT - 1, IN_BOX_COUNT - 1));

    BoxMonAtToMon(TOTAL_BOXES_COUNT - 1, IN_BOX_COUNT - 1, &mon);
    ExpectTestGrafts(&mon.box);
    EXPECT_EQ(GetMonData(&mon, MON_DATA_ATK), atk);
}

TEST("Stat grafts replace the base stat in stat calculation")
{
    struct Pokemon mon;
    u32 atkBefore, atkAfter;
    // Wobbuffet has base 33 Attack.
    CreateMonWithIVs(&mon, SPECIES_WOBBUFFET, 100, 0, OTID_STRUCT_PRESET(0), 0);
    SetMonData(&mon, MON_DATA_HIDDEN_NATURE, &(u8){ NATURE_HARDY });
    CalculateMonStats(&mon);
    atkBefore = GetMonData(&mon, MON_DATA_ATK);
    SetGraftedStatOnMon(&mon, STAT_ATK, 133);
    atkAfter = GetMonData(&mon, MON_DATA_ATK);
    EXPECT_EQ(atkAfter - atkBefore, 2 * (133 - 33));
}

TEST("Save slots do not overlap the Hall of Fame sectors")
{
    EXPECT_LE(NUM_SECTORS_PER_SLOT * NUM_SAVE_SLOTS, SECTOR_ID_HOF_1);
    EXPECT_LE(sizeof(struct PokemonStorage), SECTOR_DATA_SIZE * (SECTOR_ID_PKMN_STORAGE_END - SECTOR_ID_PKMN_STORAGE_START + 1));
}

TEST("Grafting Wonder Guard makes the host a 1 HP husk, and removing it restores HP")
{
    struct Pokemon mon;
    CreateRandomMonWithIVs(&mon, SPECIES_WOBBUFFET, 50, 0);
    u32 maxHP = GetMonData(&mon, MON_DATA_MAX_HP);
    SetGraftedAbilityOnMon(&mon, ABILITY_WONDER_GUARD);
    EXPECT_EQ(GetMonData(&mon, MON_DATA_MAX_HP), 1);
    EXPECT_EQ(GetMonData(&mon, MON_DATA_HP), 1);
    SetGraftedAbilityOnMon(&mon, ABILITY_INTIMIDATE);
    EXPECT_EQ(GetMonData(&mon, MON_DATA_MAX_HP), maxHP);
}

TEST("HP grafts are refused on husks but other stats are not")
{
    struct Pokemon mon;
    CreateRandomMonWithIVs(&mon, SPECIES_WOBBUFFET, 50, 0);
    EXPECT(CanApplyStatGraft(&mon.box, STAT_HP));
    SetGraftedAbilityOnMon(&mon, ABILITY_WONDER_GUARD);
    EXPECT(!CanApplyStatGraft(&mon.box, STAT_HP));
    EXPECT(CanApplyStatGraft(&mon.box, STAT_DEF));

    CreateRandomMonWithIVs(&mon, SPECIES_SHEDINJA, 50, 0);
    EXPECT(!CanApplyStatGraft(&mon.box, STAT_HP));
    EXPECT_EQ(GetMonData(&mon, MON_DATA_MAX_HP), 1);
}
