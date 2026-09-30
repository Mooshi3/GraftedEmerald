#ifndef GUARD_GRAFT_SETTINGS_H
#define GUARD_GRAFT_SETTINGS_H

// Per-save settings chosen once at new game, packed into VAR_GRAFTED_SETTINGS (low 16 bits)
// and VAR_GRAFTED_SETTINGS_2 (high 16 bits). To add one: add it here and to sGraftSettings[].
enum GraftSetting
{
    GRAFT_SETTING_SHINY_ODDS,
    GRAFT_SETTING_LEVEL_CAP,
    GRAFT_SETTING_GRAFT_LIMIT,
    GRAFT_SETTING_GRAFT_GATES,
    GRAFT_SETTING_GRAFT_SUPPLY,
    GRAFT_SETTING_HARVESTS,
    GRAFT_SETTING_ENEMY_GRAFTS,
    GRAFT_SETTING_ENCOUNTERS,
    GRAFT_SETTING_DUPES_CLAUSE,
    GRAFT_SETTING_SHINY_CLAUSE,
    GRAFT_SETTING_FAINTING,
    GRAFT_SETTING_BATTLE_ITEMS,
    GRAFT_SETTING_SET_MODE,
    GRAFT_SETTING_MONOTYPE,
    GRAFT_SETTINGS_COUNT,
};

enum GraftShinyOdds
{
    GRAFT_SHINY_ODDS_NORMAL,  // 1/4096
    GRAFT_SHINY_ODDS_BOOSTED, // 1/512
    GRAFT_SHINY_ODDS_HIGH,    // 1/128
    GRAFT_SHINY_ODDS_COUNT,
};

enum GraftLevelCap
{
    GRAFT_LEVEL_CAP_OFF,
    GRAFT_LEVEL_CAP_HARD, // no EXP at or above the cap
    GRAFT_LEVEL_CAP_SOFT, // EXP shrinks at or above the cap
    GRAFT_LEVEL_CAP_COUNT,
};

enum GraftLimit
{
    GRAFT_LIMIT_FREE,     // no caps
    GRAFT_LIMIT_STANDARD, // stat grafts capped at 1 + badges until 5 badges
    GRAFT_LIMIT_BUDGET,   // 2 + badges/2 grafts per mon, all kinds but moves
    GRAFT_LIMIT_STRICT,   // 3 grafts per mon
    GRAFT_LIMIT_COUNT,
};

enum GraftSupply
{
    GRAFT_SUPPLY_SHOP,    // graft items sold in marts
    GRAFT_SUPPLY_RATIONS, // not sold; handed out with each badge
    GRAFT_SUPPLY_COUNT,
};

enum GraftHarvests
{
    GRAFT_HARVESTS_NONE,    // grafting balls don't work on wild mons
    GRAFT_HARVESTS_ONE,     // one harvest per area
    GRAFT_HARVESTS_PER_MON, // each party mon can take one harvest per area
    GRAFT_HARVESTS_ANY,
    GRAFT_HARVESTS_COUNT,
};

enum GraftEnemyGrafts
{
    GRAFT_ENEMY_GRAFTS_FEWER, // 1 + badges/2
    GRAFT_ENEMY_GRAFTS_NORMAL, // 2 + badges
    GRAFT_ENEMY_GRAFTS_MORE,  // 3 + badges, and leaders' aces always get an ability
    GRAFT_ENEMY_GRAFTS_COUNT,
};

enum GraftFainting
{
    GRAFT_FAINTING_NORMAL,
    GRAFT_FAINTING_RELEASE,     // fainted mons are released after battle
    GRAFT_FAINTING_GRAVEYARD,   // fainted mons can't be healed, but can be spliced
    GRAFT_FAINTING_AUTO_SPLICE, // after battle, each fainted mon is spliced into a party member
    GRAFT_FAINTING_COUNT,
};

u32 GetGraftSetting(enum GraftSetting setting);
void SetGraftSetting(enum GraftSetting setting, u32 value);
u32 GetGraftSettingDefault(enum GraftSetting setting);
u32 GetGraftSettingChoiceCount(enum GraftSetting setting);
bool32 IsGraftSettingValidChoice(enum GraftSetting setting, u32 value);
const u8 *GetGraftSettingName(enum GraftSetting setting);
const u8 *GetGraftSettingDescription(enum GraftSetting setting, u32 value);
const u8 *GetGraftSettingChoiceName(enum GraftSetting setting, u32 value);
bool32 IsMonotypeChoice(u32 value);
u32 GetPlayerShinyOddsThreshold(void);

// New game: the settings screen stores choices here; NewGameInitData() applies them after vars are cleared.
void ApplyPendingGraftSettings(void);
void CB2_InitGraftSettingsMenu(void);
u16 GetGraftRationSplicers(void);

#endif // GUARD_GRAFT_SETTINGS_H
