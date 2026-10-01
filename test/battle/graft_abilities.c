#include "global.h"
#include "test/battle.h"

// GRAFT: ability grafts (the player's pieces and enemy grafts) can put any ability on any species,
// including abilities that change a specific species' form. The battle must carry on normally when
// the holder has no such form: these run a few turns with each of them on Wobbuffet, on both sides.

SINGLE_BATTLE_TEST("Grafted form-changing and special abilities don't break battles on other species (player)")
{
    enum Ability ability;
    PARAMETRIZE { ability = ABILITY_ZEN_MODE; }
    PARAMETRIZE { ability = ABILITY_SCHOOLING; }
    PARAMETRIZE { ability = ABILITY_DISGUISE; }
    PARAMETRIZE { ability = ABILITY_STANCE_CHANGE; }
    PARAMETRIZE { ability = ABILITY_POWER_CONSTRUCT; }
    PARAMETRIZE { ability = ABILITY_SHIELDS_DOWN; }
    PARAMETRIZE { ability = ABILITY_BATTLE_BOND; }
    PARAMETRIZE { ability = ABILITY_ICE_FACE; }
    PARAMETRIZE { ability = ABILITY_GULP_MISSILE; }
    PARAMETRIZE { ability = ABILITY_HUNGER_SWITCH; }
    PARAMETRIZE { ability = ABILITY_ZERO_TO_HERO; }
    PARAMETRIZE { ability = ABILITY_FORECAST; }
    PARAMETRIZE { ability = ABILITY_FLOWER_GIFT; }
    PARAMETRIZE { ability = ABILITY_MIMICRY; }
    PARAMETRIZE { ability = ABILITY_COMMANDER; }
    PARAMETRIZE { ability = ABILITY_IMPOSTER; }
    PARAMETRIZE { ability = ABILITY_ILLUSION; }
    PARAMETRIZE { ability = ABILITY_MULTITYPE; }
    PARAMETRIZE { ability = ABILITY_RKS_SYSTEM; }
    PARAMETRIZE { ability = ABILITY_TERA_SHIFT; }
    PARAMETRIZE { ability = ABILITY_POWER_OF_ALCHEMY; }
    PARAMETRIZE { ability = ABILITY_RECEIVER; }
    PARAMETRIZE { ability = ABILITY_TRACE; }
    PARAMETRIZE { ability = ABILITY_PROTOSYNTHESIS; }
    PARAMETRIZE { ability = ABILITY_QUARK_DRIVE; }
    PARAMETRIZE { ability = ABILITY_COMATOSE; }
    PARAMETRIZE { ability = ABILITY_WONDER_GUARD; }
    PARAMETRIZE { ability = ABILITY_NEUTRALIZING_GAS; }
    PARAMETRIZE { ability = ABILITY_EMBODY_ASPECT_TEAL_MASK; }
    PARAMETRIZE { ability = ABILITY_POISON_PUPPETEER; }
    PARAMETRIZE { ability = ABILITY_TERAFORM_ZERO; }
    PARAMETRIZE { ability = ABILITY_TRUANT; }
    PARAMETRIZE { ability = ABILITY_SLOW_START; }
    PARAMETRIZE { ability = ABILITY_DEFEATIST; }
    PARAMETRIZE { ability = ABILITY_MOODY; }
    PARAMETRIZE { ability = ABILITY_HARVEST; }
    GIVEN {
        PLAYER(SPECIES_WOBBUFFET) { Ability(ability); Item(ITEM_SITRUS_BERRY); }
        OPPONENT(SPECIES_WYNAUT);
    } WHEN {
        TURN { MOVE(player, MOVE_SURF); MOVE(opponent, MOVE_SUPER_FANG); }
        TURN { MOVE(player, MOVE_TACKLE); MOVE(opponent, MOVE_SUPER_FANG); }
        TURN { MOVE(player, MOVE_TACKLE); MOVE(opponent, MOVE_TACKLE); }
    }
}

SINGLE_BATTLE_TEST("Grafted form-changing and special abilities don't break battles on other species (opponent)")
{
    enum Ability ability;
    PARAMETRIZE { ability = ABILITY_ZEN_MODE; }
    PARAMETRIZE { ability = ABILITY_SCHOOLING; }
    PARAMETRIZE { ability = ABILITY_DISGUISE; }
    PARAMETRIZE { ability = ABILITY_STANCE_CHANGE; }
    PARAMETRIZE { ability = ABILITY_POWER_CONSTRUCT; }
    PARAMETRIZE { ability = ABILITY_SHIELDS_DOWN; }
    PARAMETRIZE { ability = ABILITY_BATTLE_BOND; }
    PARAMETRIZE { ability = ABILITY_ICE_FACE; }
    PARAMETRIZE { ability = ABILITY_GULP_MISSILE; }
    PARAMETRIZE { ability = ABILITY_HUNGER_SWITCH; }
    PARAMETRIZE { ability = ABILITY_ZERO_TO_HERO; }
    PARAMETRIZE { ability = ABILITY_FORECAST; }
    PARAMETRIZE { ability = ABILITY_FLOWER_GIFT; }
    PARAMETRIZE { ability = ABILITY_MIMICRY; }
    PARAMETRIZE { ability = ABILITY_COMMANDER; }
    PARAMETRIZE { ability = ABILITY_IMPOSTER; }
    PARAMETRIZE { ability = ABILITY_ILLUSION; }
    PARAMETRIZE { ability = ABILITY_MULTITYPE; }
    PARAMETRIZE { ability = ABILITY_RKS_SYSTEM; }
    PARAMETRIZE { ability = ABILITY_TERA_SHIFT; }
    PARAMETRIZE { ability = ABILITY_POWER_OF_ALCHEMY; }
    PARAMETRIZE { ability = ABILITY_RECEIVER; }
    PARAMETRIZE { ability = ABILITY_TRACE; }
    PARAMETRIZE { ability = ABILITY_PROTOSYNTHESIS; }
    PARAMETRIZE { ability = ABILITY_QUARK_DRIVE; }
    PARAMETRIZE { ability = ABILITY_COMATOSE; }
    PARAMETRIZE { ability = ABILITY_WONDER_GUARD; }
    PARAMETRIZE { ability = ABILITY_NEUTRALIZING_GAS; }
    PARAMETRIZE { ability = ABILITY_EMBODY_ASPECT_TEAL_MASK; }
    PARAMETRIZE { ability = ABILITY_POISON_PUPPETEER; }
    PARAMETRIZE { ability = ABILITY_TERAFORM_ZERO; }
    PARAMETRIZE { ability = ABILITY_TRUANT; }
    PARAMETRIZE { ability = ABILITY_SLOW_START; }
    PARAMETRIZE { ability = ABILITY_DEFEATIST; }
    PARAMETRIZE { ability = ABILITY_MOODY; }
    PARAMETRIZE { ability = ABILITY_HARVEST; }
    GIVEN {
        PLAYER(SPECIES_WYNAUT);
        OPPONENT(SPECIES_WOBBUFFET) { Ability(ability); Item(ITEM_SITRUS_BERRY); }
    } WHEN {
        TURN { MOVE(opponent, MOVE_SURF); MOVE(player, MOVE_SUPER_FANG); }
        TURN { MOVE(opponent, MOVE_TACKLE); MOVE(player, MOVE_SUPER_FANG); }
        TURN { MOVE(opponent, MOVE_TACKLE); MOVE(player, MOVE_TACKLE); }
    }
}
