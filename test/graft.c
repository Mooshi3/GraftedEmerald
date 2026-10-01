#include "global.h"
#include "graft_data.h"
#include "graft_settings.h"
#include "graft_menu.h"
#include "graft_challenge.h"
#include "graft_encounter_dex.h"
#include "battle.h"
#include "battle_controllers.h"
#include "item.h"
#include "item_use.h"
#include "script_pokemon_util.h"
#include "fieldmap.h"
#include "constants/region_map_sections.h"
#include "pokemon_storage_system.h"
#include "save.h"
#include "caps.h"
#include "text.h"
#include "test/test.h"
#include "event_data.h"
#include "pokedex.h"
#include "shop_criteria.h"
#include "graft_bag.h"
#include "overworld.h"
#include "battle_setup.h"
#include "party_menu.h"
#include "data.h"
#include "string_util.h"
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

TEST("Level cap settings: off, hard and soft")
{
    FlagClear(FLAG_BADGE01_GET);
    VarSet(VAR_GRAFTED_SETTINGS, 0);
    VarSet(VAR_GRAFTED_SETTINGS_2, 0);
    EXPECT_EQ(GetCurrentLevelCap(), MAX_LEVEL);
    EXPECT_EQ(GetSoftLevelCapExpValue(20, 1000), 1000);

    SetGraftSetting(GRAFT_SETTING_LEVEL_CAP, GRAFT_LEVEL_CAP_HARD);
    EXPECT_EQ(GetCurrentLevelCap(), 15); // Roxanne's Nosepass
    EXPECT_EQ(GetSoftLevelCapExpValue(14, 1000), 1000);
    EXPECT_EQ(GetSoftLevelCapExpValue(15, 1000), 0);

    SetGraftSetting(GRAFT_SETTING_LEVEL_CAP, GRAFT_LEVEL_CAP_SOFT);
    EXPECT_EQ(GetCurrentLevelCap(), MAX_LEVEL); // soft caps never stop leveling
    EXPECT_EQ(GetSoftLevelCapExpValue(15, 1000), 250);
    EXPECT_EQ(GetSoftLevelCapExpValue(30, 1000), 1000 / 64);

    FlagSet(FLAG_BADGE01_GET);
    EXPECT_EQ(GetLevelCapTarget(), 19);
    FlagClear(FLAG_BADGE01_GET);
    VarSet(VAR_GRAFTED_SETTINGS, 0);
    VarSet(VAR_GRAFTED_SETTINGS_2, 0);
}

static void ResetGraftSettingsForTest(void)
{
    VarSet(VAR_GRAFTED_SETTINGS, 0);
    VarSet(VAR_GRAFTED_SETTINGS_2, 0);
    for (u32 i = 0; i < NUM_BADGES; i++)
        FlagClear(FLAG_BADGE01_GET + i);
}

TEST("Graft Gates hold back type, stat and ability grafts by badge")
{
    struct Pokemon mon;
    ResetGraftSettingsForTest();
    CreateRandomMonWithIVs(&mon, SPECIES_WOBBUFFET, 50, 0);
    EXPECT(GetGraftRejectReason(&mon.box, GRAFT_MENU_ABILITY, 0) == NULL); // gates off

    SetGraftSetting(GRAFT_SETTING_GRAFT_GATES, TRUE);
    EXPECT(GetGraftRejectReason(&mon.box, GRAFT_MENU_TYPE, 0) != NULL);
    EXPECT(GetGraftRejectReason(&mon.box, GRAFT_MENU_MOVE, 0) == NULL);
    EXPECT(GetGraftRejectReason(&mon.box, GRAFT_MENU_NATURE, 0) == NULL);
    FlagSet(FLAG_BADGE01_GET);
    EXPECT(GetGraftRejectReason(&mon.box, GRAFT_MENU_TYPE, 0) == NULL);
    EXPECT(GetGraftRejectReason(&mon.box, GRAFT_MENU_STAT, STAT_ATK) != NULL);
    FlagSet(FLAG_BADGE02_GET);
    EXPECT(GetGraftRejectReason(&mon.box, GRAFT_MENU_STAT, STAT_ATK) == NULL);
    EXPECT(GetGraftRejectReason(&mon.box, GRAFT_MENU_ABILITY, 0) != NULL);
    FlagSet(FLAG_BADGE03_GET);
    EXPECT(GetGraftRejectReason(&mon.box, GRAFT_MENU_ABILITY, 0) == NULL);
    ResetGraftSettingsForTest();
}

TEST("Graft Limit: Free ignores the stat cap, Strict allows 3 grafts, replacing is free")
{
    struct Pokemon mon;
    ResetGraftSettingsForTest();
    CreateRandomMonWithIVs(&mon, SPECIES_WOBBUFFET, 50, 0);
    SetGraftedStatOnMon(&mon, STAT_ATK, 100);
    // Standard: 1 stat graft at 0 badges
    EXPECT(GetGraftRejectReason(&mon.box, GRAFT_MENU_STAT, STAT_DEF) != NULL);
    EXPECT(GetGraftRejectReason(&mon.box, GRAFT_MENU_STAT, STAT_ATK) == NULL);
    SetGraftSetting(GRAFT_SETTING_GRAFT_LIMIT, GRAFT_LIMIT_FREE);
    EXPECT(GetGraftRejectReason(&mon.box, GRAFT_MENU_STAT, STAT_DEF) == NULL);

    SetGraftSetting(GRAFT_SETTING_GRAFT_LIMIT, GRAFT_LIMIT_STRICT);
    SetGraftedTypeOnMon(&mon, 0, TYPE_FIRE);
    SetGraftedTypeOnMon(&mon, 1, TYPE_WATER); // both type slots count as one graft
    SetGraftedAbilityOnMon(&mon, ABILITY_INTIMIDATE);
    EXPECT_EQ(CountBudgetGraftsOnBoxMon(&mon.box), 3);
    EXPECT(GetGraftRejectReason(&mon.box, GRAFT_MENU_STAT, STAT_DEF) != NULL);
    EXPECT(GetGraftRejectReason(&mon.box, GRAFT_MENU_NATURE, 0) != NULL);
    EXPECT(GetGraftRejectReason(&mon.box, GRAFT_MENU_TYPE, 0) == NULL);    // replaces
    EXPECT(GetGraftRejectReason(&mon.box, GRAFT_MENU_ABILITY, 0) == NULL); // replaces
    EXPECT(GetGraftRejectReason(&mon.box, GRAFT_MENU_MOVE, 0) == NULL);    // moves never count

    SetGraftSetting(GRAFT_SETTING_GRAFT_LIMIT, GRAFT_LIMIT_BUDGET);
    EXPECT_EQ(GetGraftBudget(), 2);
    FlagSet(FLAG_BADGE01_GET);
    FlagSet(FLAG_BADGE02_GET);
    EXPECT_EQ(GetGraftBudget(), 3);
    ResetGraftSettingsForTest();
}

TEST("Enemy Grafts setting scales the enemy graft count")
{
    ResetGraftSettingsForTest();
    FlagSet(FLAG_BADGE01_GET);
    FlagSet(FLAG_BADGE02_GET);
    EXPECT_EQ(GetEnemyGraftCount(), 4);
    SetGraftSetting(GRAFT_SETTING_ENEMY_GRAFTS, GRAFT_ENEMY_GRAFTS_FEWER);
    EXPECT_EQ(GetEnemyGraftCount(), 2);
    SetGraftSetting(GRAFT_SETTING_ENEMY_GRAFTS, GRAFT_ENEMY_GRAFTS_MORE);
    EXPECT_EQ(GetEnemyGraftCount(), 5);
    ResetGraftSettingsForTest();
}

static void SetupWildEncounterForTest(u16 species, u32 mapsec)
{
    gMapHeader.regionMapSectionId = mapsec;
    gBattleTypeFlags = 0;
    CreateRandomMonWithIVs(&gParties[B_TRAINER_OPPONENT_A][0], species, 5, 0);
    FlagSet(FLAG_SYS_POKEDEX_GET);
}

TEST("Nuzlocke: encounters before the Pokédex don't use up the area")
{
    ResetGraftSettingsForTest();
    memset(gSaveBlock1Ptr->graftCaughtAreas, 0, sizeof(gSaveBlock1Ptr->graftCaughtAreas));
    SetGraftSetting(GRAFT_SETTING_ENCOUNTERS, TRUE);
    SetGraftSetting(GRAFT_SETTING_SHINY_CLAUSE, FALSE);
    SetupWildEncounterForTest(SPECIES_ZIGZAGOON, MAPSEC_ROUTE_101);
    FlagClear(FLAG_SYS_POKEDEX_GET);
    Nuzlocke_OnWildEncounter();
    Nuzlocke_OnBattleEnd();
    SetupWildEncounterForTest(SPECIES_ZIGZAGOON, MAPSEC_ROUTE_101); // sets the Pokédex flag
    Nuzlocke_OnWildEncounter();
    EXPECT_EQ(Nuzlocke_GetEncounterState(), NUZLOCKE_ENCOUNTER_FREE); // still the area's first
    Nuzlocke_OnBattleEnd();
    ResetGraftSettingsForTest();
}

TEST("Nuzlocke: only the first encounter in an area can be caught; grafting balls follow the harvest rule")
{
    ResetGraftSettingsForTest();
    memset(gSaveBlock1Ptr->graftCaughtAreas, 0, sizeof(gSaveBlock1Ptr->graftCaughtAreas));
    memset(gSaveBlock1Ptr->graftHarvestAreas, 0, sizeof(gSaveBlock1Ptr->graftHarvestAreas));
    SetGraftSetting(GRAFT_SETTING_ENCOUNTERS, TRUE);
    SetGraftSetting(GRAFT_SETTING_SHINY_CLAUSE, FALSE);

    SetupWildEncounterForTest(SPECIES_ZIGZAGOON, MAPSEC_ROUTE_102);
    Nuzlocke_OnWildEncounter();
    EXPECT(GetChallengeBallBlockReason(ITEM_POKE_BALL) == NULL);
    Nuzlocke_OnBattleEnd();

    SetupWildEncounterForTest(SPECIES_ZIGZAGOON, MAPSEC_ROUTE_102);
    Nuzlocke_OnWildEncounter();
    EXPECT_EQ(Nuzlocke_GetEncounterState(), NUZLOCKE_ENCOUNTER_AREA_USED);
    EXPECT(GetChallengeBallBlockReason(ITEM_POKE_BALL) != NULL);
    EXPECT(GetChallengeBallBlockReason(ITEM_GRAFT_BALL) == NULL); // harvests: Any
    SetGraftSetting(GRAFT_SETTING_HARVESTS, GRAFT_HARVESTS_ONE);
    OnWildHarvest();
    EXPECT(GetChallengeBallBlockReason(ITEM_GRAFT_BALL) != NULL);
    SetGraftSetting(GRAFT_SETTING_HARVESTS, GRAFT_HARVESTS_NONE);
    EXPECT(GetChallengeBallBlockReason(ITEM_PATCH_BALL) != NULL);
    Nuzlocke_OnBattleEnd();

    // A new area is open again; scripted battles (no OnWildEncounter) are always free
    SetupWildEncounterForTest(SPECIES_ZIGZAGOON, MAPSEC_ROUTE_103);
    Nuzlocke_OnWildEncounter();
    EXPECT(GetChallengeBallBlockReason(ITEM_POKE_BALL) == NULL);
    Nuzlocke_OnBattleEnd();
    EXPECT_EQ(Nuzlocke_GetEncounterState(), NUZLOCKE_ENCOUNTER_FREE);
    ResetGraftSettingsForTest();
}

TEST("Nuzlocke dupes clause: a caught family doesn't count and keeps the area open")
{
    ResetGraftSettingsForTest();
    memset(gSaveBlock1Ptr->graftCaughtAreas, 0, sizeof(gSaveBlock1Ptr->graftCaughtAreas));
    SetGraftSetting(GRAFT_SETTING_ENCOUNTERS, TRUE);
    GetSetPokedexFlag(SpeciesToNationalPokedexNum(SPECIES_POOCHYENA), FLAG_SET_CAUGHT);
    EXPECT(IsFamilyCaught(SPECIES_MIGHTYENA));
    EXPECT(!IsFamilyCaught(SPECIES_WURMPLE));

    SetupWildEncounterForTest(SPECIES_MIGHTYENA, MAPSEC_ROUTE_104);
    Nuzlocke_OnWildEncounter();
    EXPECT_EQ(Nuzlocke_GetEncounterState(), NUZLOCKE_ENCOUNTER_DUPE);
    Nuzlocke_OnBattleEnd();
    SetupWildEncounterForTest(SPECIES_WURMPLE, MAPSEC_ROUTE_104);
    Nuzlocke_OnWildEncounter();
    EXPECT_EQ(Nuzlocke_GetEncounterState(), NUZLOCKE_ENCOUNTER_FREE);
    Nuzlocke_OnBattleEnd();
    ResetGraftSettingsForTest();
}

TEST("Harvests per mon: an area allows one harvest per party member")
{
    ResetGraftSettingsForTest();
    ZeroPlayerPartyMons();
    CreateRandomMonWithIVs(&gParties[B_TRAINER_PLAYER][0], SPECIES_WOBBUFFET, 20, 0);
    CreateRandomMonWithIVs(&gParties[B_TRAINER_PLAYER][1], SPECIES_WYNAUT, 20, 0);
    SetGraftSetting(GRAFT_SETTING_HARVESTS, GRAFT_HARVESTS_PER_MON);
    SetupWildEncounterForTest(SPECIES_ZIGZAGOON, MAPSEC_ROUTE_102);
    EXPECT(GetChallengeBallBlockReason(ITEM_GRAFT_BALL) == NULL);
    OnWildHarvest();
    EXPECT(GetChallengeBallBlockReason(ITEM_GRAFT_BALL) == NULL); // the second party member still has one
    OnWildHarvest();
    EXPECT(GetChallengeBallBlockReason(ITEM_GRAFT_BALL) != NULL); // nobody left to take one here
    gMapHeader.regionMapSectionId = MAPSEC_ROUTE_103;
    EXPECT(GetChallengeBallBlockReason(ITEM_GRAFT_BALL) == NULL);
    ResetGraftSettingsForTest();
}

TEST("Fainting: fallen mons stay at 0 HP through healing and items; Release removes them")
{
    u16 potion = ITEM_REVIVE;
    ResetGraftSettingsForTest();
    ZeroPlayerPartyMons();
    CreateRandomMonWithIVs(&gParties[B_TRAINER_PLAYER][0], SPECIES_WOBBUFFET, 20, 0);
    CreateRandomMonWithIVs(&gParties[B_TRAINER_PLAYER][1], SPECIES_WYNAUT, 20, 0);
    SetMonData(&gParties[B_TRAINER_PLAYER][1], MON_DATA_HP, &(u16){ 0 });

    // Normal: nothing happens
    gBattleTypeFlags = 0;
    gBattleOutcome = B_OUTCOME_WON;
    EXPECT(!ApplyFaintingRulesAfterBattle(NULL));
    EXPECT(!IsMonFallen(&gParties[B_TRAINER_PLAYER][1]));

    // Graveyard: marked fallen, can't be healed or revived
    SetGraftSetting(GRAFT_SETTING_FAINTING, GRAFT_FAINTING_GRAVEYARD);
    EXPECT(!ApplyFaintingRulesAfterBattle(NULL));
    EXPECT(IsMonFallen(&gParties[B_TRAINER_PLAYER][1]));
    HealPlayerParty();
    EXPECT_EQ(GetMonData(&gParties[B_TRAINER_PLAYER][1], MON_DATA_HP), 0);
    EXPECT_GT(GetMonData(&gParties[B_TRAINER_PLAYER][0], MON_DATA_HP), 0);
    EXPECT(ExecuteTableBasedItemEffect(&gParties[B_TRAINER_PLAYER][1], potion, 1, 0)); // TRUE = can't use
    EXPECT(GetGraftRejectReason(&gParties[B_TRAINER_PLAYER][1].box, GRAFT_MENU_MOVE, 0) != NULL);

    // Release: gone after the battle
    CreateRandomMonWithIVs(&gParties[B_TRAINER_PLAYER][1], SPECIES_WYNAUT, 20, 0);
    SetMonData(&gParties[B_TRAINER_PLAYER][1], MON_DATA_HP, &(u16){ 0 });
    SetGraftSetting(GRAFT_SETTING_FAINTING, GRAFT_FAINTING_RELEASE);
    gBattleOutcome = B_OUTCOME_LOST; // a whiteout doesn't take the party
    EXPECT(!ApplyFaintingRulesAfterBattle(NULL));
    EXPECT_EQ(GetMonData(&gParties[B_TRAINER_PLAYER][1], MON_DATA_SPECIES), SPECIES_WYNAUT);
    gBattleOutcome = B_OUTCOME_WON;
    EXPECT(!ApplyFaintingRulesAfterBattle(NULL));
    EXPECT_EQ(GetMonData(&gParties[B_TRAINER_PLAYER][1], MON_DATA_SPECIES), SPECIES_NONE);
    EXPECT_EQ(GetMonData(&gParties[B_TRAINER_PLAYER][0], MON_DATA_SPECIES), SPECIES_WOBBUFFET);
    ResetGraftSettingsForTest();
}

TEST("Monotype: off-type party mons can't battle unless nobody has the type; grafted types count")
{
    ResetGraftSettingsForTest();
    ZeroPlayerPartyMons();
    CreateRandomMonWithIVs(&gParties[B_TRAINER_PLAYER][0], SPECIES_WOBBUFFET, 20, 0); // Psychic
    CreateRandomMonWithIVs(&gParties[B_TRAINER_PLAYER][1], SPECIES_ZIGZAGOON, 20, 0); // Normal
    SetGraftSetting(GRAFT_SETTING_MONOTYPE, TYPE_WATER);
    // Nobody is Water: everyone may battle
    EXPECT(IsValidForBattle(&gParties[B_TRAINER_PLAYER][0]));
    GraftMonotypeOntoStarter(&gParties[B_TRAINER_PLAYER][0]);
    EXPECT(IsValidForBattle(&gParties[B_TRAINER_PLAYER][0]));
    EXPECT(!IsValidForBattle(&gParties[B_TRAINER_PLAYER][1]));
    // Enemy parties are never restricted
    CreateRandomMonWithIVs(&gParties[B_TRAINER_OPPONENT_A][0], SPECIES_ZIGZAGOON, 20, 0);
    EXPECT(IsValidForBattle(&gParties[B_TRAINER_OPPONENT_A][0]));
    ResetGraftSettingsForTest();
}

TEST("Battle Items: Banned leaves only balls")
{
    ResetGraftSettingsForTest();
    ZeroPlayerPartyMons();
    CreateRandomMonWithIVs(&gParties[B_TRAINER_PLAYER][0], SPECIES_WOBBUFFET, 20, 0);
    SetGraftSetting(GRAFT_SETTING_BATTLE_ITEMS, TRUE);
    EXPECT(AreBattleItemsBanned());
    EXPECT(CannotUseItemsInBattle(ITEM_X_ATTACK, &gParties[B_TRAINER_PLAYER][0]));
    ResetGraftSettingsForTest();
}

TEST("The Ungrafter removes every graft and the relearner history, keeping known moves")
{
    struct Pokemon mon;
    u32 atk, maxHP;
    u8 nature = NATURE_ADAMANT;
    CreateMonWithIVs(&mon, SPECIES_WOBBUFFET, 50, 0, OTID_STRUCT_PRESET(0), 0);
    atk = GetMonData(&mon, MON_DATA_ATK);
    maxHP = GetMonData(&mon, MON_DATA_MAX_HP);
    GiveTestGrafts(&mon);
    SetGraftedAbilityOnMon(&mon, ABILITY_WONDER_GUARD);
    SetMonData(&mon, MON_DATA_HIDDEN_NATURE, &nature);
    mon.box.graftFlags |= GRAFT_FLAG_NATURE | GRAFT_FLAG_FALLEN;
    SetMonMoveSlot(&mon, MOVE_FLAMETHROWER, 0);
    EXPECT(BoxMonHasGrafts(&mon.box));
    EXPECT_EQ(GetMonData(&mon, MON_DATA_MAX_HP), 1);

    UngraftMon(&mon);
    EXPECT(!BoxMonHasGrafts(&mon.box));
    EXPECT_EQ(mon.box.graftFlags, GRAFT_FLAG_FALLEN); // challenge state stays
    EXPECT_EQ(mon.box.graftTypes[0], GRAFT_TYPE_NONE);
    EXPECT_EQ(mon.box.graftMoves[0], MOVE_NONE);
    EXPECT_EQ(GetMonData(&mon, MON_DATA_MOVE1), MOVE_FLAMETHROWER); // known moves stay
    EXPECT_EQ(GetMonData(&mon, MON_DATA_ATK), atk);
    EXPECT_EQ(GetMonData(&mon, MON_DATA_MAX_HP), maxHP);
    EXPECT_EQ(GetMonAbility(&mon), GetAbilityBySpecies(SPECIES_WOBBUFFET, GetMonData(&mon, MON_DATA_ABILITY_NUM)));
    EXPECT_EQ(GetMonData(&mon, MON_DATA_HIDDEN_NATURE), GetNatureFromPersonality(GetMonData(&mon, MON_DATA_PERSONALITY)));
}

static const struct EncounterDexEntry *FindEncDexEntry(const struct EncounterDexEntry *entries, u32 count, u16 species, u8 method)
{
    for (u32 i = 0; i < count; i++)
    {
        if (entries[i].species == species && entries[i].method == method)
            return &entries[i];
    }
    return NULL;
}

TEST("Encounter screen: area lists come from the wild tables, sorted by method then rate")
{
    struct EncounterDexEntry entries[ENCDEX_MAX_ENTRIES];
    u32 count, i;
    FlagClear(FLAG_SYS_GAME_CLEAR);
    EXPECT(AreaHasWildEncounters(MAPSEC_PETALBURG_WOODS));
    EXPECT(!AreaHasWildEncounters(MAPSEC_LITTLEROOT_TOWN));
    count = GetEncounterAreaEntries(MAPSEC_PETALBURG_WOODS, entries, TIMES_OF_DAY_COUNT);
    EXPECT_GT(count, 0);
    EXPECT(FindEncDexEntry(entries, count, SPECIES_SHROOMISH, ENCDEX_GRASS) != NULL);
    for (i = 0; i < count; i++)
    {
        EXPECT_GT(entries[i].rate, 0);
        EXPECT_LE(entries[i].rate, 100);
        EXPECT_NE(entries[i].timeMask, 0);
        EXPECT_LE(entries[i].minLevel, entries[i].maxLevel);
        if (i > 0)
            EXPECT(entries[i - 1].method < entries[i].method
                || (entries[i - 1].method == entries[i].method && entries[i - 1].rate >= entries[i].rate));
    }
}

TEST("Encounter screen: postgame legendaries only show after the Champion win, until caught")
{
    struct EncounterDexEntry entries[ENCDEX_MAX_ENTRIES];
    u32 count;
    const struct EncounterDexEntry *legend;
    FlagClear(FLAG_SYS_GAME_CLEAR);
    GetSetPokedexFlag(SpeciesToNationalPokedexNum(SPECIES_VIRIZION), FLAG_SET_SEEN);
    count = GetEncounterAreaEntries(MAPSEC_PETALBURG_WOODS, entries, TIMES_OF_DAY_COUNT);
    EXPECT(FindEncDexEntry(entries, count, SPECIES_VIRIZION, ENCDEX_GRASS) == NULL);

    FlagSet(FLAG_SYS_GAME_CLEAR);
    count = GetEncounterAreaEntries(MAPSEC_PETALBURG_WOODS, entries, TIMES_OF_DAY_COUNT);
    legend = FindEncDexEntry(entries, count, SPECIES_VIRIZION, ENCDEX_GRASS);
    EXPECT(legend != NULL);
    EXPECT_EQ(legend->rate, 1);
    EXPECT_EQ(legend->minLevel, 50);

    GetSetPokedexFlag(SpeciesToNationalPokedexNum(SPECIES_VIRIZION), FLAG_SET_CAUGHT);
    count = GetEncounterAreaEntries(MAPSEC_PETALBURG_WOODS, entries, TIMES_OF_DAY_COUNT);
    EXPECT(FindEncDexEntry(entries, count, SPECIES_VIRIZION, ENCDEX_GRASS) == NULL);
    FlagClear(FLAG_SYS_GAME_CLEAR);
}

TEST("Encounter screen: visited areas are remembered per map section")
{
    memset(gSaveBlock1Ptr->graftVisitedAreas, 0, sizeof(gSaveBlock1Ptr->graftVisitedAreas));
    EXPECT(!IsEncounterAreaVisited(MAPSEC_ROUTE_104));
    MarkEncounterAreaVisited(MAPSEC_ROUTE_104);
    EXPECT(IsEncounterAreaVisited(MAPSEC_ROUTE_104));
    EXPECT(!IsEncounterAreaVisited(MAPSEC_ROUTE_105));
}

TEST("Encounter slot rates add up to 100 for every method")
{
    u32 i, sum;
    for (i = 0, sum = 0; i < NUM_LAND_MONS_ENCOUNTER_SLOTS; i++) sum += gEncounterSlotRatesLand[i];
    EXPECT_EQ(sum, 100);
    for (i = 0, sum = 0; i < NUM_WATER_MONS_ENCOUNTER_SLOTS; i++) sum += gEncounterSlotRatesWater[i];
    EXPECT_EQ(sum, 100);
    for (i = 0, sum = 0; i < NUM_ROCK_SMASH_MONS_ENCOUNTER_SLOTS; i++) sum += gEncounterSlotRatesRockSmash[i];
    EXPECT_EQ(sum, 100);
    for (i = 0, sum = 0; i < NUM_FISHING_MONS_ENCOUNTER_SLOTS; i++) sum += gEncounterSlotRatesFishing[i];
    EXPECT_EQ(sum, 300); // three rods
}

TEST("Encounter screen: a single time of day lists only that time, with its own rates")
{
    struct EncounterDexEntry all[ENCDEX_MAX_ENTRIES], day[ENCDEX_MAX_ENTRIES];
    u32 allCount = GetEncounterAreaEntries(MAPSEC_PETALBURG_WOODS, all, TIMES_OF_DAY_COUNT);
    u32 dayCount = GetEncounterAreaEntries(MAPSEC_PETALBURG_WOODS, day, TIME_DAY);
    u32 i, grassTotal = 0;
    EXPECT_GT(dayCount, 0);
    EXPECT_LE(dayCount, allCount);
    for (i = 0; i < dayCount; i++)
    {
        EXPECT_EQ(day[i].timeMask, 1 << TIME_DAY);
        if (day[i].method == ENCDEX_GRASS)
            grassTotal += day[i].rate;
    }
    EXPECT_EQ(grassTotal, 100); // one time's grass table adds up exactly
}

TEST("Rations leave out grafting balls when harvests are off")
{
    u32 i;
    for (i = FLAG_BADGE01_GET; i <= FLAG_BADGE08_GET; i++)
        FlagClear(i);
    VarSet(VAR_GRAFTED_SETTINGS, 0);
    VarSet(VAR_GRAFTED_SETTINGS_2, 0);
    EXPECT_EQ(GetGraftRationSplicers(), 0xFF); // shops: no rations

    SetGraftSetting(GRAFT_SETTING_GRAFT_SUPPLY, GRAFT_SUPPLY_RATIONS);
    EXPECT_EQ(GetGraftRationSplicers(), 0); // Splice Balls only
    FlagSet(FLAG_BADGE01_GET);
    FlagSet(FLAG_BADGE02_GET);
    FlagSet(FLAG_BADGE03_GET);
    EXPECT_EQ(GetGraftRationSplicers(), 1); // Splice Balls and a Splicer

    SetGraftSetting(GRAFT_SETTING_HARVESTS, GRAFT_HARVESTS_NONE);
    EXPECT(AreGraftBallsUseless());
    EXPECT_EQ(GetGraftRationSplicers(), 2); // a Splicer only
    FlagClear(FLAG_BADGE03_GET);
    EXPECT_EQ(GetGraftRationSplicers(), 0xFF); // nothing before the Splicer unlocks
    FlagClear(FLAG_BADGE01_GET);
    FlagClear(FLAG_BADGE02_GET);
}

TEST("EXP Share and Poké Center settings")
{
    VarSet(VAR_GRAFTED_SETTINGS, 0);
    VarSet(VAR_GRAFTED_SETTINGS_2, 0);
    EXPECT(!IsGen6ExpShareEnabled());
    EXPECT(!ArePokeCentersClosed());
    EXPECT_EQ(GetGraftSetting(GRAFT_SETTING_WILD_GRAFTS), GRAFT_WILD_GRAFTS_OFF);

    SetGraftSetting(GRAFT_SETTING_EXP_SHARE, TRUE);
    SetGraftSetting(GRAFT_SETTING_POKECENTERS, TRUE);
    EXPECT(IsGen6ExpShareEnabled());
    EXPECT(ArePokeCentersClosed());
    VarSet(VAR_GRAFTED_SETTINGS, 0);
    VarSet(VAR_GRAFTED_SETTINGS_2, 0);
}

TEST("Marts that sell Poké Balls also sell the grafting items")
{
    static const u16 ballMart[] = { ITEM_POKE_BALL, ITEM_POTION, ITEM_GRAFT_BALL, ITEM_NONE };
    static const u16 herbShop[] = { ITEM_ENERGY_POWDER, ITEM_HEAL_POWDER, ITEM_NONE };
    const u16 *list = ballMart;
    u16 count = 3;

    TryBuildDynamicShopItemList(&list, &count);
    EXPECT_EQ(count, 9); // 3 + the 6 grafting items it didn't list
    EXPECT_EQ(list[2], ITEM_GRAFT_BALL);
    EXPECT_EQ(list[3], ITEM_PATCH_BALL);
    EXPECT_EQ(list[7], ITEM_UNGRAFTER);
    EXPECT_EQ(list[8], ITEM_ABILITY_CAPSULE);
    EXPECT_EQ(list[9], ITEM_NONE);
    TryFreeDynamicShopItemList(&list);
    EXPECT(list == ballMart);

    list = herbShop;
    count = 2;
    TryBuildDynamicShopItemList(&list, &count);
    EXPECT_EQ(count, 2);
    TryFreeDynamicShopItemList(&list);
}

TEST("Graft Bag counts pieces per piece and caps each at 15")
{
    struct GraftPiece tackle = { GRAFT_PIECE_MOVE, 0, MOVE_TACKLE };
    struct GraftPiece sturdy = { GRAFT_PIECE_ABILITY, 0, ABILITY_STURDY };
    struct GraftPiece sameNumberAbility = { GRAFT_PIECE_ABILITY, 0, MOVE_TACKLE };
    struct GraftPiece atk65 = { GRAFT_PIECE_STAT, STAT_ATK, 65 };
    struct GraftPiece def65 = { GRAFT_PIECE_STAT, STAT_DEF, 65 };
    struct GraftPiece none = { GRAFT_PIECE_MOVE, 0, MOVE_NONE };
    u32 i;

    ClearGraftBag();
    EXPECT(IsGraftBagEmpty());
    EXPECT(AddGraftPiece(&tackle));
    EXPECT(AddGraftPiece(&tackle));
    EXPECT(AddGraftPiece(&sturdy));
    EXPECT(AddGraftPiece(&atk65));
    EXPECT_EQ(GetGraftPieceCount(&tackle), 2);
    EXPECT_EQ(GetGraftPieceCount(&sturdy), 1);
    EXPECT_EQ(GetGraftPieceCount(&sameNumberAbility), 0); // kinds don't share counts
    EXPECT_EQ(GetGraftPieceCount(&atk65), 1);
    EXPECT_EQ(GetGraftPieceCount(&def65), 0);
    EXPECT(!AddGraftPiece(&none));

    for (i = 0; i < 20; i++)
        AddGraftPiece(&tackle);
    EXPECT_EQ(GetGraftPieceCount(&tackle), GRAFT_PIECE_MAX_COUNT);
    EXPECT(!AddGraftPiece(&tackle));

    EXPECT(RemoveGraftPiece(&sturdy));
    EXPECT(!RemoveGraftPiece(&sturdy));
    EXPECT_EQ(GetGraftBagKindTotal(GRAFT_PIECE_ABILITY), 0);
    EXPECT_EQ(GetGraftBagKindTotal(GRAFT_PIECE_MOVE), 1);
    ClearGraftBag();
}

TEST("The Graft Bag takes the record-mixed secret bases' save space without moving anything")
{
    EXPECT_EQ(offsetof(struct SaveBlock1, graftBag), offsetof(struct SaveBlock1, secretBases) + sizeof(struct SecretBase));
    EXPECT_EQ(offsetof(struct SaveBlock1, graftBag) + sizeof(((struct SaveBlock1 *)0)->graftBag),
              offsetof(struct SaveBlock1, secretBases) + SECRET_BASES_SAVED_COUNT * sizeof(struct SecretBase));
}

static void GiveFourTestMoves(struct Pokemon *mon)
{
    SetMonMoveSlot(mon, MOVE_COUNTER, 0);
    SetMonMoveSlot(mon, MOVE_MIRROR_COAT, 1);
    SetMonMoveSlot(mon, MOVE_SAFEGUARD, 2);
    SetMonMoveSlot(mon, MOVE_DESTINY_BOND, 3);
}

TEST("A mon is made of its moves, ability, types, nature and base stats")
{
    struct Pokemon mon;
    struct GraftPiece pieces[GRAFT_PIECES_PER_MON];
    u32 count, i, moves = 0, stats = 0, types = 0, abilities = 0, natures = 0;

    CreateMonWithIVs(&mon, SPECIES_WOBBUFFET, 30, 0, OTID_STRUCT_PRESET(0), 0); // pure Psychic
    GiveFourTestMoves(&mon);
    SetMonData(&mon, MON_DATA_HIDDEN_NATURE, &(u8){ NATURE_ADAMANT });
    SetGraftedStatOnMon(&mon, STAT_SPEED, 120);
    count = GetMonGraftPieces(&mon, FALSE, pieces);
    for (i = 0; i < count; i++)
    {
        switch (pieces[i].kind)
        {
        case GRAFT_PIECE_MOVE:    moves++; break;
        case GRAFT_PIECE_ABILITY: abilities++; EXPECT_EQ(pieces[i].value, GetMonAbility(&mon)); break;
        case GRAFT_PIECE_TYPE:    types++; EXPECT_EQ(pieces[i].value, TYPE_PSYCHIC); break;
        case GRAFT_PIECE_NATURE:  natures++; EXPECT_EQ(pieces[i].value, NATURE_ADAMANT); break;
        case GRAFT_PIECE_STAT:
            stats++;
            if (pieces[i].stat == STAT_SPEED)
                EXPECT_EQ(pieces[i].value, 120); // grafts count
            if (pieces[i].stat == STAT_HP)
                EXPECT_EQ(pieces[i].value, 190);
            break;
        }
    }
    EXPECT_EQ(moves, 4);
    EXPECT_EQ(abilities, 1);
    EXPECT_EQ(types, 1); // one piece for a single type
    EXPECT_EQ(natures, 1);
    EXPECT_EQ(stats, NUM_STATS);
    EXPECT_EQ(count, 4 + 1 + 1 + 1 + NUM_STATS);
}

TEST("Wild mons never give their hidden ability")
{
    struct Pokemon mon;
    struct GraftPiece pieces[GRAFT_PIECES_PER_MON];
    u32 count, i;
    u8 hidden = 2;

    CreateMonWithIVs(&mon, SPECIES_WOBBUFFET, 30, 0, OTID_STRUCT_PRESET(0), 0);
    SetMonData(&mon, MON_DATA_ABILITY_NUM, &hidden);
    count = GetMonGraftPieces(&mon, TRUE, pieces);
    for (i = 0; i < count; i++)
        EXPECT_NE(pieces[i].kind, GRAFT_PIECE_ABILITY);
    count = GetMonGraftPieces(&mon, FALSE, pieces); // your own mon (Splicer) does
    for (i = 0; i < count && pieces[i].kind != GRAFT_PIECE_ABILITY; i++)
        ;
    EXPECT_LT(i, count);
}

TEST("Harvests: Patch Ball 1 piece, Graft Ball 3 different, Splice Ball all; a full bag loses the rest")
{
    struct Pokemon mon;
    struct GraftPiece got[GRAFT_PIECES_PER_MON];
    u32 lost, count, i, j, total;

    CreateMonWithIVs(&mon, SPECIES_WOBBUFFET, 30, 0, OTID_STRUCT_PRESET(0), 0);
    GiveFourTestMoves(&mon);
    ClearGraftBag();
    EXPECT_EQ(HarvestGraftPieces(&mon, TRUE, 1, got, &lost), 1);
    EXPECT_EQ(lost, 0);
    EXPECT_EQ(GetGraftPieceCount(&got[0]), 1);

    ClearGraftBag();
    count = HarvestGraftPieces(&mon, TRUE, 3, got, &lost);
    EXPECT_EQ(count, 3);
    for (i = 0; i < 3; i++)
        for (j = i + 1; j < 3; j++)
            EXPECT(got[i].kind != got[j].kind || got[i].stat != got[j].stat || got[i].value != got[j].value);

    ClearGraftBag();
    total = HarvestGraftPieces(&mon, TRUE, GRAFT_HARVEST_ALL, got, &lost);
    EXPECT_EQ(total, 4 + 1 + 1 + 1 + NUM_STATS);
    for (i = 0; i < GRAFT_PIECE_MAX_COUNT - 1; i++)
        HarvestGraftPieces(&mon, TRUE, GRAFT_HARVEST_ALL, got, &lost);
    count = HarvestGraftPieces(&mon, TRUE, GRAFT_HARVEST_ALL, got, &lost);
    EXPECT_EQ(count, 0);
    EXPECT_EQ(lost, total);
    ClearGraftBag();
}

TEST("Graft piece names")
{
    u8 buffer[32];
    struct GraftPiece atk = { GRAFT_PIECE_STAT, STAT_ATK, 65 };
    struct GraftPiece rock = { GRAFT_PIECE_TYPE, 0, TYPE_ROCK };
    GetGraftPieceName(buffer, &atk);
    EXPECT_EQ(StringCompare(buffer, COMPOUND_STRING("ATK 65")), 0);
    GetGraftPieceName(buffer, &rock);
    EXPECT_EQ(StringCompare(buffer, COMPOUND_STRING("Rock type")), 0);
}

static bool32 ActiveTablesAreEmeraldOnly(void)
{
    u32 i, t, s;
    for (i = 0; gWildMonHeaders[i].mapGroup != MAP_GROUP(MAP_UNDEFINED); i++)
    {
        for (t = 0; t < TIMES_OF_DAY_COUNT; t++)
        {
            const struct WildPokemonInfo *land = gWildMonHeaders[i].encounterTypes[t].landMonsInfo;
            if (land == NULL)
                continue;
            for (s = 0; s < NUM_LAND_MONS_ENCOUNTER_SLOTS; s++)
            {
                if (SpeciesToNationalPokedexNum(land->wildPokemon[s].species) > NATIONAL_DEX_DEOXYS)
                    return FALSE;
            }
        }
    }
    return TRUE;
}

TEST("National Dex setting: Off uses Emerald's own tables, Postgame switches after the League")
{
    const struct WildPokemonHeader *sheet;
    u16 species = SPECIES_NONE;
    u8 level = 0;

    VarSet(VAR_GRAFTED_SETTINGS, 0);
    VarSet(VAR_GRAFTED_SETTINGS_2, 0);
    FlagClear(FLAG_SYS_GAME_CLEAR);
    EXPECT_EQ(GetGraftSetting(GRAFT_SETTING_NATDEX), GRAFT_NATDEX_ON); // default: the sheet
    sheet = GetActiveWildMonHeaders();
    EXPECT(!ActiveTablesAreEmeraldOnly());

    SetGraftSetting(GRAFT_SETTING_NATDEX, GRAFT_NATDEX_OFF);
    EXPECT(GetActiveWildMonHeaders() != sheet);
    EXPECT(ActiveTablesAreEmeraldOnly());

    SetGraftSetting(GRAFT_SETTING_NATDEX, GRAFT_NATDEX_POSTGAME);
    EXPECT(ActiveTablesAreEmeraldOnly());
    FlagSet(FLAG_SYS_GAME_CLEAR);
    EXPECT(GetActiveWildMonHeaders() == sheet);

    // Postgame legendaries come with the National Dex, never with Off
    gSaveBlock1Ptr->location.mapGroup = MAP_GROUP(MAP_ROUTE102); // Mesprit, not caught by any test
    gSaveBlock1Ptr->location.mapNum = MAP_NUM(MAP_ROUTE102);
    EXPECT(TryGetPostgameLegendary(WILD_AREA_LAND, &species, &level));
    SetGraftSetting(GRAFT_SETTING_NATDEX, GRAFT_NATDEX_OFF);
    EXPECT(!TryGetPostgameLegendary(WILD_AREA_LAND, &species, &level));

    FlagClear(FLAG_SYS_GAME_CLEAR);
    VarSet(VAR_GRAFTED_SETTINGS, 0);
    VarSet(VAR_GRAFTED_SETTINGS_2, 0);
}

static u32 DonorPoolSizeAt(u32 map, u32 escapeMap)
{
    gSaveBlock1Ptr->location.mapGroup = map >> 8;
    gSaveBlock1Ptr->location.mapNum = map & 0xFF;
    gMapHeader = *Overworld_GetMapHeaderByGroupAndId(map >> 8, map & 0xFF);
    gSaveBlock1Ptr->escapeWarp.mapGroup = escapeMap >> 8;
    gSaveBlock1Ptr->escapeWarp.mapNum = escapeMap & 0xFF;
    BuildNearbyGraftPool();
    return GetGraftDonorPoolSize();
}

TEST("Boss rooms with no wild Pokémon still get an enemy graft donor pool")
{
    VarSet(VAR_GRAFTED_SETTINGS, 0);
    VarSet(VAR_GRAFTED_SETTINGS_2, 0);
    EXPECT_NE(DonorPoolSizeAt(MAP_EVER_GRANDE_CITY_SIDNEYS_ROOM, MAP_EVER_GRANDE_CITY), 0);
    EXPECT_NE(DonorPoolSizeAt(MAP_EVER_GRANDE_CITY_CHAMPIONS_ROOM, MAP_EVER_GRANDE_CITY), 0);
    EXPECT_NE(DonorPoolSizeAt(MAP_AQUA_HIDEOUT_B2F, MAP_LILYCOVE_CITY), 0);
    EXPECT_NE(DonorPoolSizeAt(MAP_MAGMA_HIDEOUT_4F, MAP_JAGGED_PASS), 0);
    EXPECT_NE(DonorPoolSizeAt(MAP_MOSSDEEP_CITY_SPACE_CENTER_2F, MAP_MOSSDEEP_CITY), 0);
    EXPECT_NE(DonorPoolSizeAt(MAP_RUSTBORO_CITY_GYM, MAP_RUSTBORO_CITY), 0);
}

// Every enemy graft must leave a valid mon: real moves (no duplicates), a real ability, real types,
// stat grafts that are improvements, full HP (1 for a Wonder Guard husk).
static void ExpectValidGraftedEnemy(struct Pokemon *mon, u32 trainerId)
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
    (void)trainerId;
}

TEST("Enemy grafts leave every species valid (fuzz, max grafts, several donor pools)")
{
    static const u16 sMaps[] = { MAP_ROUTE119, MAP_VICTORY_ROAD_B1F, MAP_EVER_GRANDE_CITY_DRAKES_ROOM, MAP_MAGMA_HIDEOUT_4F };
    struct Pokemon *mon = &gParties[B_TRAINER_OPPONENT_A][0];
    u32 m, s, f, grafted = 0, mons = 0;

    VarSet(VAR_GRAFTED_SETTINGS, 0);
    VarSet(VAR_GRAFTED_SETTINGS_2, 0);
    SetGraftSetting(GRAFT_SETTING_ENEMY_GRAFTS, GRAFT_ENEMY_GRAFTS_MORE);
    for (f = FLAG_BADGE01_GET; f <= FLAG_BADGE08_GET; f++)
        FlagSet(f);
    for (m = 0; m < ARRAY_COUNT(sMaps); m++)
    {
        DonorPoolSizeAt(sMaps[m], MAP_EVER_GRANDE_CITY);
        // Every species, with a different map's pool for each quarter of the dex
        for (s = 1 + m; s < NUM_SPECIES; s += ARRAY_COUNT(sMaps))
        {
            if (!IsSpeciesEnabled(s) || gSpeciesInfo[s].baseHP == 0)
                continue;
            ZeroEnemyPartyMons();
            CreateMonWithIVs(mon, s, 50, 0, OTID_STRUCT_PRESET(0), 0);
            GiveMonInitialMoveset(mon);
            Test_RollTrainerGrafts(mon, 1, 1); // an ordinary trainer (bosses also get an ace)
            ExpectValidGraftedEnemy(mon, s);
            mons++;
            if (mon->box.graftFlags != 0)
                grafted++;
        }
    }
    EXPECT_GT(mons, 1000);
    EXPECT_GT(grafted * 10, mons * 9); // nearly every mon got something
    for (f = FLAG_BADGE01_GET; f <= FLAG_BADGE08_GET; f++)
        FlagClear(f);
    VarSet(VAR_GRAFTED_SETTINGS, 0);
    VarSet(VAR_GRAFTED_SETTINGS_2, 0);
}

TEST("Bosses keep their specialty type and bring a wrong-type ace with it grafted on")
{
    struct Pokemon *party = gParties[B_TRAINER_OPPONENT_A];
    u32 i, run, aceSpecies;

    VarSet(VAR_GRAFTED_SETTINGS, 0);
    VarSet(VAR_GRAFTED_SETTINGS_2, 0);
    for (i = FLAG_BADGE01_GET; i <= FLAG_BADGE08_GET; i++)
        FlagSet(i); // the most grafts: every type slot gets tried
    DonorPoolSizeAt(MAP_ROUTE119, MAP_EVER_GRANDE_CITY);
    for (run = 0; run < 20; run++)
    {
        // Roxanne's first team
        ZeroEnemyPartyMons();
        CreateMonWithIVs(&party[0], SPECIES_GEODUDE, 12, 0, OTID_STRUCT_PRESET(0), 0);
        CreateMonWithIVs(&party[1], SPECIES_GEODUDE, 12, 0, OTID_STRUCT_PRESET(0), 0);
        CreateMonWithIVs(&party[2], SPECIES_NOSEPASS, 15, 0, OTID_STRUCT_PRESET(0), 0);
        Test_RollTrainerGrafts(party, 3, TRAINER_ROXANNE_1);
        for (i = 0; i < 4; i++)
        {
            EXPECT(GetEffectiveTypeFromMon(&party[i], 0) == TYPE_ROCK || GetEffectiveTypeFromMon(&party[i], 1) == TYPE_ROCK);
            ExpectValidGraftedEnemy(&party[i], TRAINER_ROXANNE_1);
        }
        // The ace: Sandygast at the ace's level, Rock grafted on, a Rock attack
        EXPECT_EQ(GetMonData(&party[3], MON_DATA_SPECIES), SPECIES_SANDYGAST);
        EXPECT_EQ(GetMonData(&party[3], MON_DATA_LEVEL), 15);
        EXPECT_EQ(party[3].box.graftTypes[0], GRAFT_TYPE_NONE); // keeps its own Ghost type
        EXPECT_EQ(party[3].box.graftTypes[1], TYPE_ROCK);
        EXPECT(MonKnowsMove(&party[3], MOVE_ROCK_TOMB) || MonKnowsMove(&party[3], MOVE_ANCIENT_POWER));
    }
    // Rematches level the ace up through its evolutions; the Elite Four have one too
    ZeroEnemyPartyMons();
    CreateMonWithIVs(&party[0], SPECIES_MACHAMP, 50, 0, OTID_STRUCT_PRESET(0), 0);
    Test_RollTrainerGrafts(party, 1, TRAINER_BRAWLY_5);
    aceSpecies = GetMonData(&party[1], MON_DATA_SPECIES);
    EXPECT_EQ(aceSpecies, SPECIES_LOPUNNY);
    ZeroEnemyPartyMons();
    CreateMonWithIVs(&party[0], SPECIES_SHELGON, 52, 0, OTID_STRUCT_PRESET(0), 0);
    CreateMonWithIVs(&party[1], SPECIES_ALTARIA, 54, 0, OTID_STRUCT_PRESET(0), 0);
    CreateMonWithIVs(&party[2], SPECIES_SALAMENCE, 55, 0, OTID_STRUCT_PRESET(0), 0);
    Test_RollTrainerGrafts(party, 3, TRAINER_DRAKE);
    EXPECT_EQ(GetMonData(&party[3], MON_DATA_SPECIES), SPECIES_CHARIZARD);
    EXPECT_EQ(party[3].box.graftTypes[1], TYPE_DRAGON);
    // No room: a full party gets no ace
    ZeroEnemyPartyMons();
    for (i = 0; i < PARTY_SIZE; i++)
        CreateMonWithIVs(&party[i], SPECIES_WAILORD, 58, 0, OTID_STRUCT_PRESET(0), 0);
    Test_RollTrainerGrafts(party, PARTY_SIZE, TRAINER_WALLACE);
    for (i = 0; i < PARTY_SIZE; i++)
        EXPECT_EQ(GetMonData(&party[i], MON_DATA_SPECIES), SPECIES_WAILORD);
    for (i = FLAG_BADGE01_GET; i <= FLAG_BADGE08_GET; i++)
        FlagClear(i);
}
