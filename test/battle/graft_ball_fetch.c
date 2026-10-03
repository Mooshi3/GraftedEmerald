#include "global.h"
#include "test/battle.h"

// GRAFT: Ball Fetch picks up a failed grafting ball like any other ball
WILD_BATTLE_TEST("Ball Fetch picks up a grafting ball that failed")
{
    enum Item item = ITEM_NONE;

    PARAMETRIZE { item = ITEM_PATCH_BALL; }
    PARAMETRIZE { item = ITEM_GRAFT_BALL; }
    PARAMETRIZE { item = ITEM_SPLICE_BALL; }

    GIVEN {
        PLAYER(SPECIES_YAMPER) { Ability(ABILITY_BALL_FETCH); }
        OPPONENT(SPECIES_METAGROSS);
    } WHEN {
        TURN { USE_ITEM(player, item, WITH_RNG(RNG_BALLTHROW_SHAKE, MAX_u16)); }
        TURN {}
    } SCENE {
        ABILITY_POPUP(player, ABILITY_BALL_FETCH);
    } THEN {
        EXPECT_EQ(player->item, item);
    }
}
