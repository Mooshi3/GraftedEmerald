#include "global.h"
#include "battle.h"
#include "event_data.h"
#include "caps.h"
#include "graft_settings.h"
#include "pokemon.h"


// GRAFT: level caps are a new-game setting (graft_settings.c), not the compile-time B_EXP_CAP_TYPE.
// The cap is the level of the next Gym Leader's (then the Champion's) strongest Pokémon.
u32 GetLevelCapTarget(void)
{
    static const u32 sLevelCapFlagMap[][2] =
    {
        {FLAG_BADGE01_GET, 15},
        {FLAG_BADGE02_GET, 19},
        {FLAG_BADGE03_GET, 24},
        {FLAG_BADGE04_GET, 29},
        {FLAG_BADGE05_GET, 31},
        {FLAG_BADGE06_GET, 33},
        {FLAG_BADGE07_GET, 42},
        {FLAG_BADGE08_GET, 46},
        {FLAG_IS_CHAMPION, 58},
    };

    u32 i;

    if (GetGraftSetting(GRAFT_SETTING_LEVEL_CAP) == GRAFT_LEVEL_CAP_OFF)
        return MAX_LEVEL;
    for (i = 0; i < ARRAY_COUNT(sLevelCapFlagMap); i++)
    {
        if (!FlagGet(sLevelCapFlagMap[i][0]))
            return sLevelCapFlagMap[i][1];
    }
    return MAX_LEVEL;
}

bool32 IsHardLevelCapActive(void)
{
    return GetGraftSetting(GRAFT_SETTING_LEVEL_CAP) == GRAFT_LEVEL_CAP_HARD;
}

// The level nothing can raise a mon past. Only the hard cap is a wall; the soft cap just shrinks EXP.
u32 GetCurrentLevelCap(void)
{
    if (IsHardLevelCapActive())
        return GetLevelCapTarget();
    return MAX_LEVEL;
}

u32 GetSoftLevelCapExpValue(u32 level, u32 expValue)
{
    static const u32 sExpScalingDown[5] = { 4, 8, 16, 32, 64 };

    u32 levelDifference;
    u32 currentLevelCap = GetLevelCapTarget();

    if (level < currentLevelCap)
        return expValue;

    switch (GetGraftSetting(GRAFT_SETTING_LEVEL_CAP))
    {
    case GRAFT_LEVEL_CAP_HARD:
        return 0;
    case GRAFT_LEVEL_CAP_SOFT:
        levelDifference = level - currentLevelCap;
        if (levelDifference > ARRAY_COUNT(sExpScalingDown) - 1)
            return expValue / sExpScalingDown[ARRAY_COUNT(sExpScalingDown) - 1];
        return expValue / sExpScalingDown[levelDifference];
    default:
        return expValue;
    }
}

u32 GetCurrentEVCap(void)
{
    static const u16 sEvCapFlagMap[][2] = {
        // Define EV caps for each milestone
        {FLAG_BADGE01_GET, MAX_TOTAL_EVS *  1 / 17},
        {FLAG_BADGE02_GET, MAX_TOTAL_EVS *  3 / 17},
        {FLAG_BADGE03_GET, MAX_TOTAL_EVS *  5 / 17},
        {FLAG_BADGE04_GET, MAX_TOTAL_EVS *  7 / 17},
        {FLAG_BADGE05_GET, MAX_TOTAL_EVS *  9 / 17},
        {FLAG_BADGE06_GET, MAX_TOTAL_EVS * 11 / 17},
        {FLAG_BADGE07_GET, MAX_TOTAL_EVS * 13 / 17},
        {FLAG_BADGE08_GET, MAX_TOTAL_EVS * 15 / 17},
        {FLAG_IS_CHAMPION, MAX_TOTAL_EVS},
    };

    if (B_EV_CAP_TYPE == EV_CAP_FLAG_LIST)
    {
        for (u32 evCap = 0; evCap < ARRAY_COUNT(sEvCapFlagMap); evCap++)
        {
            if (!FlagGet(sEvCapFlagMap[evCap][0]))
                return sEvCapFlagMap[evCap][1];
        }
    }
    else if (B_EV_CAP_TYPE == EV_CAP_VARIABLE)
    {
        return VarGet(B_EV_CAP_VARIABLE);
    }
    else if (B_EV_CAP_TYPE == EV_CAP_NO_GAIN)
    {
        return 0;
    }

    return MAX_TOTAL_EVS;
}
