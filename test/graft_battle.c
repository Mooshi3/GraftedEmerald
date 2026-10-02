// Grafted abilities in real battles. Any ability can be grafted onto any species, including the ones
// that only make sense with one species' forms; on anything else they must do nothing, not crash.
#include "global.h"
#include "graft_data.h"
#include "test/battle.h"

#define GRAFT_ABILITY(ability) SetGraftedAbilityOnMon(gBattleTestRunnerState->data.currentMon, ability)

SINGLE_BATTLE_TEST("Grafted form-change abilities do nothing on other species")
{
    enum Ability ability;
    PARAMETRIZE { ability = ABILITY_DISGUISE; }
    PARAMETRIZE { ability = ABILITY_ICE_FACE; }
    PARAMETRIZE { ability = ABILITY_GULP_MISSILE; }
    PARAMETRIZE { ability = ABILITY_SCHOOLING; }
    PARAMETRIZE { ability = ABILITY_ZEN_MODE; }
    PARAMETRIZE { ability = ABILITY_SHIELDS_DOWN; }
    PARAMETRIZE { ability = ABILITY_POWER_CONSTRUCT; }
    PARAMETRIZE { ability = ABILITY_STANCE_CHANGE; }
    PARAMETRIZE { ability = ABILITY_BATTLE_BOND; }
    PARAMETRIZE { ability = ABILITY_ZERO_TO_HERO; }
    PARAMETRIZE { ability = ABILITY_HUNGER_SWITCH; }
    PARAMETRIZE { ability = ABILITY_FORECAST; }
    PARAMETRIZE { ability = ABILITY_FLOWER_GIFT; }
    PARAMETRIZE { ability = ABILITY_COMMANDER; }
    PARAMETRIZE { ability = ABILITY_MULTITYPE; }
    PARAMETRIZE { ability = ABILITY_RKS_SYSTEM; }
    PARAMETRIZE { ability = ABILITY_TERA_SHIFT; }
    PARAMETRIZE { ability = ABILITY_ILLUSION; }
    PARAMETRIZE { ability = ABILITY_MIMICRY; }
    PARAMETRIZE { ability = ABILITY_EMBODY_ASPECT_TEAL_MASK; }
    GIVEN {
        PLAYER(SPECIES_WOBBUFFET) { GRAFT_ABILITY(ability); }
        PLAYER(SPECIES_WYNAUT);
        OPPONENT(SPECIES_WOBBUFFET) { GRAFT_ABILITY(ability); }
    } WHEN {
        TURN { MOVE(opponent, MOVE_SUPER_FANG); MOVE(player, MOVE_TACKLE); }
        TURN { MOVE(opponent, MOVE_SCRATCH); MOVE(player, MOVE_SUPER_FANG); }
        TURN { SWITCH(player, 1); MOVE(opponent, MOVE_SCRATCH); }
        TURN { SWITCH(player, 0); MOVE(opponent, MOVE_SCRATCH); }
    } SCENE {
        // Disguise and Ice Face don't block the hits: the damage lands
        ANIMATION(ANIM_TYPE_MOVE, MOVE_SUPER_FANG, opponent);
        HP_BAR(player);
        ANIMATION(ANIM_TYPE_MOVE, MOVE_SCRATCH, opponent);
        HP_BAR(player);
        ANIMATION(ANIM_TYPE_MOVE, MOVE_SUPER_FANG, player);
        HP_BAR(opponent);
    } THEN {
        EXPECT_EQ(player->species, SPECIES_WOBBUFFET);
        EXPECT_EQ(opponent->species, SPECIES_WOBBUFFET);
        EXPECT_EQ(player->ability, ability); // the graft comes back on switch-in
    }
}

// Signature moves grafted onto another species: they fail or work as a plain move, never crash
SINGLE_BATTLE_TEST("Grafted signature moves on other species don't break the battle")
{
    enum Move move;
    PARAMETRIZE { move = MOVE_AURA_WHEEL; }
    PARAMETRIZE { move = MOVE_RELIC_SONG; }
    PARAMETRIZE { move = MOVE_HYPERSPACE_FURY; }
    PARAMETRIZE { move = MOVE_IVY_CUDGEL; }
    PARAMETRIZE { move = MOVE_RAGING_BULL; }
    PARAMETRIZE { move = MOVE_DARK_VOID; }
    PARAMETRIZE { move = MOVE_MULTI_ATTACK; }
    PARAMETRIZE { move = MOVE_JUDGMENT; }
    PARAMETRIZE { move = MOVE_TECHNO_BLAST; }
    PARAMETRIZE { move = MOVE_TERA_STARSTORM; }
    PARAMETRIZE { move = MOVE_ORDER_UP; }
    PARAMETRIZE { move = MOVE_GIGATON_HAMMER; }
    PARAMETRIZE { move = MOVE_DOODLE; }
    PARAMETRIZE { move = MOVE_REVIVAL_BLESSING; }
    GIVEN {
        PLAYER(SPECIES_WOBBUFFET) { Moves(move, MOVE_CELEBRATE); }
        PLAYER(SPECIES_WYNAUT);
        OPPONENT(SPECIES_WOBBUFFET);
    } WHEN {
        TURN { MOVE(player, move); }
        TURN { MOVE(player, MOVE_CELEBRATE); }
    } THEN {
        EXPECT_EQ(player->species, SPECIES_WOBBUFFET);
        EXPECT_EQ(opponent->species, SPECIES_WOBBUFFET);
    }
}

// Mega Evolution is a form change: grafted types and a grafted ability stay through it
SINGLE_BATTLE_TEST("Grafted types and ability stay through Mega Evolution")
{
    GIVEN {
        PLAYER(SPECIES_CHARIZARD)
        {
            Item(ITEM_CHARIZARDITE_X);
            SetGraftedTypeOnMon(gBattleTestRunnerState->data.currentMon, 1, TYPE_WATER);
            GRAFT_ABILITY(ABILITY_INTIMIDATE);
        }
        OPPONENT(SPECIES_WOBBUFFET);
    } WHEN {
        TURN { MOVE(player, MOVE_CELEBRATE, gimmick: GIMMICK_MEGA); }
    } SCENE {
        ANIMATION(ANIM_TYPE_GENERAL, B_ANIM_MEGA_EVOLUTION, player);
    } THEN {
        EXPECT_EQ(player->species, SPECIES_CHARIZARD_MEGA_X);
        EXPECT_EQ(player->types[0], TYPE_FIRE);
        EXPECT_EQ(player->types[1], TYPE_WATER); // the grafted slot, not Mega X's Dragon
        EXPECT_EQ(player->ability, ABILITY_INTIMIDATE); // not Tough Claws
    }
}
