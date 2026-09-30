#include "global.h"
#include "graft_data.h"
#include "graft_settings.h"
#include "pokemon_storage_system.h"
#include "save.h"
#include "text.h"
#include "test/test.h"
#include "event_data.h"
#include "pokedex.h"
#include "wild_encounter.h"
#include "constants/items.h"
#include "constants/map_groups.h"

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
    CreateRandomMonWithIVs(&gParties[B_TRAINER_PLAYER][0], SPECIES_WOBBUFFET, 50, 0);
    GiveTestGrafts(&gParties[B_TRAINER_PLAYER][0]);
    u32 atk = GetMonData(&gParties[B_TRAINER_PLAYER][0], MON_DATA_ATK);

    SetBoxMonAt(TOTAL_BOXES_COUNT - 1, IN_BOX_COUNT - 1, &gParties[B_TRAINER_PLAYER][0].box);
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

TEST("Postgame legendaries take the 1% slot only after the Champion win, until caught")
{
    u16 species = SPECIES_NONE;
    u8 level = 0;
    gSaveBlock1Ptr->location.mapGroup = MAP_GROUP(MAP_ROUTE101);
    gSaveBlock1Ptr->location.mapNum = MAP_NUM(MAP_ROUTE101);
    GetSetPokedexFlag(SpeciesToNationalPokedexNum(SPECIES_KUBFU), FLAG_SET_SEEN); // seen doesn't count

    FlagClear(FLAG_SYS_GAME_CLEAR);
    EXPECT(!TryGetPostgameLegendary(WILD_AREA_LAND, &species, &level));

    FlagSet(FLAG_SYS_GAME_CLEAR);
    EXPECT(TryGetPostgameLegendary(WILD_AREA_LAND, &species, &level));
    EXPECT_EQ(species, SPECIES_KUBFU);
    EXPECT(level >= 50 && level <= 55);
    EXPECT(!TryGetPostgameLegendary(WILD_AREA_WATER, &species, &level));

    GetSetPokedexFlag(SpeciesToNationalPokedexNum(SPECIES_KUBFU), FLAG_SET_CAUGHT);
    EXPECT(!TryGetPostgameLegendary(WILD_AREA_LAND, &species, &level));
    FlagClear(FLAG_SYS_GAME_CLEAR);
}

TEST("A parent holding a Graft Knot passes all its grafts to the egg")
{
    struct Pokemon parentA, parentB, egg;
    u16 knot = ITEM_GRAFT_KNOT;
    CreateRandomMonWithIVs(&parentA, SPECIES_DITTO, 50, 0);
    CreateRandomMonWithIVs(&parentB, SPECIES_WOBBUFFET, 50, 0);
    GiveTestGrafts(&parentA);
    SetMonData(&parentA, MON_DATA_HELD_ITEM, &knot);
    CreateRandomMonWithIVs(&egg, SPECIES_WYNAUT, 1, 0);

    InheritGrafts(&egg.box, &parentA.box, &parentB.box);
    ExpectTestGrafts(&egg.box);
}

TEST("Without a Graft Knot, a parent with no grafts passes nothing")
{
    struct Pokemon parentA, parentB, egg;
    CreateRandomMonWithIVs(&parentA, SPECIES_DITTO, 50, 0);
    CreateRandomMonWithIVs(&parentB, SPECIES_WOBBUFFET, 50, 0);
    CreateRandomMonWithIVs(&egg, SPECIES_WYNAUT, 1, 0);

    InheritGrafts(&egg.box, &parentA.box, &parentB.box);
    EXPECT_EQ(egg.box.graftFlags, 0);
    EXPECT_EQ(egg.box.graftAbility, GRAFT_ABILITY_NONE);
}

TEST("Without a Graft Knot, grafts are inherited some of the time")
{
    struct Pokemon parentA, parentB, egg;
    u32 i, inherited = 0;
    CreateRandomMonWithIVs(&parentA, SPECIES_DITTO, 50, 0);
    CreateRandomMonWithIVs(&parentB, SPECIES_WOBBUFFET, 50, 0);
    SetGraftedAbilityOnMon(&parentA, ABILITY_INTIMIDATE);
    for (i = 0; i < 200; i++)
    {
        CreateRandomMonWithIVs(&egg, SPECIES_WYNAUT, 1, 0);
        InheritGrafts(&egg.box, &parentA.box, &parentB.box);
        if (egg.box.graftAbility == ABILITY_INTIMIDATE)
            inherited++;
    }
    // 25% chance: expect roughly 50 of 200, never all or none
    EXPECT_GT(inherited, 20);
    EXPECT_LT(inherited, 90);
}

TEST("Grafted move history skips duplicates and keeps the newest moves when full")
{
    struct Pokemon mon;
    CreateRandomMonWithIVs(&mon, SPECIES_WOBBUFFET, 50, 0);
    RecordGraftMove(&mon.box, MOVE_FLAMETHROWER);
    RecordGraftMove(&mon.box, MOVE_FLAMETHROWER);
    RecordGraftMove(&mon.box, MOVE_SURF);
    RecordGraftMove(&mon.box, MOVE_THUNDERBOLT);
    RecordGraftMove(&mon.box, MOVE_ICE_BEAM);
    EXPECT_EQ(mon.box.graftMoves[0], MOVE_FLAMETHROWER);
    EXPECT_EQ(mon.box.graftMoves[3], MOVE_ICE_BEAM);
    // Full: the oldest entry makes room for the newest
    RecordGraftMove(&mon.box, MOVE_PSYCHIC);
    EXPECT_EQ(mon.box.graftMoves[0], MOVE_SURF);
    EXPECT_EQ(mon.box.graftMoves[3], MOVE_PSYCHIC);
    // An entry the species relearns by level-up anyway (Wobbuffet: Counter) is replaced first
    mon.box.graftMoves[1] = MOVE_COUNTER;
    RecordGraftMove(&mon.box, MOVE_EARTHQUAKE);
    EXPECT_EQ(mon.box.graftMoves[0], MOVE_SURF);
    EXPECT_EQ(mon.box.graftMoves[3], MOVE_EARTHQUAKE);
    EXPECT(mon.box.graftFlags & GRAFT_FLAG_MOVE);
}

TEST("New-game settings read as their defaults when the settings var is unset")
{
    VarSet(VAR_GRAFTED_SETTINGS, 0);
    EXPECT_EQ(GetGraftSetting(GRAFT_SETTING_SHINY_ODDS), GRAFT_SHINY_ODDS_BOOSTED);
    EXPECT_EQ(GetPlayerShinyOddsThreshold(), 128);
}

TEST("New-game settings round-trip and set the shiny odds")
{
    VarSet(VAR_GRAFTED_SETTINGS, 0);
    SetGraftSetting(GRAFT_SETTING_SHINY_ODDS, GRAFT_SHINY_ODDS_NORMAL);
    EXPECT_EQ(GetGraftSetting(GRAFT_SETTING_SHINY_ODDS), GRAFT_SHINY_ODDS_NORMAL);
    EXPECT_EQ(GetPlayerShinyOddsThreshold(), 16);
    SetGraftSetting(GRAFT_SETTING_SHINY_ODDS, GRAFT_SHINY_ODDS_HIGH);
    EXPECT_EQ(GetGraftSetting(GRAFT_SETTING_SHINY_ODDS), GRAFT_SHINY_ODDS_HIGH);
    EXPECT_EQ(GetPlayerShinyOddsThreshold(), 512);
    ApplyPendingGraftSettings(); // nothing chosen: back to defaults
    EXPECT_EQ(GetGraftSetting(GRAFT_SETTING_SHINY_ODDS), GRAFT_SHINY_ODDS_BOOSTED);
    VarSet(VAR_GRAFTED_SETTINGS, 0);
}

TEST("New-game settings text fits the settings screen")
{
    u32 i, value;
    for (i = 0; i < GRAFT_SETTINGS_COUNT; i++)
    {
        EXPECT_LE(GetStringWidth(FONT_NORMAL, GetGraftSettingName(i), 0), 104);
        for (value = 0; value < GetGraftSettingChoiceCount(i); value++)
        {
            if (!IsGraftSettingValidChoice(i, value))
                continue;
            EXPECT_LE(GetStringWidth(FONT_NORMAL, GetGraftSettingChoiceName(i, value), 0), 80);
            EXPECT_LE(GetStringWidth(FONT_NORMAL, GetGraftSettingDescription(i, value), 0), 200);
        }
    }
}

TEST("Every new-game setting reads back what was set")
{
    u32 i, value;
    for (i = 0; i < GRAFT_SETTINGS_COUNT; i++)
    {
        for (value = 0; value < GetGraftSettingChoiceCount(i); value++)
        {
            if (!IsGraftSettingValidChoice(i, value))
                continue;
            VarSet(VAR_GRAFTED_SETTINGS, 0);
            VarSet(VAR_GRAFTED_SETTINGS_2, 0);
            SetGraftSetting(i, value);
            EXPECT_EQ(GetGraftSetting(i), value);
            // No other setting moved off its default
            for (u32 j = 0; j < GRAFT_SETTINGS_COUNT; j++)
            {
                if (j != i)
                    EXPECT_EQ(GetGraftSetting(j), GetGraftSettingDefault(j));
            }
        }
    }
    VarSet(VAR_GRAFTED_SETTINGS, 0);
    VarSet(VAR_GRAFTED_SETTINGS_2, 0);
}
