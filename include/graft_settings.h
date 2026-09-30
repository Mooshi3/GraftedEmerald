#ifndef GUARD_GRAFT_SETTINGS_H
#define GUARD_GRAFT_SETTINGS_H

// Per-save settings chosen once at new game, packed into VAR_GRAFTED_SETTINGS.
// Challenge settings (level caps, graft caps, Nuzlocke rules...) get added here as new entries.
enum GraftSetting
{
    GRAFT_SETTING_SHINY_ODDS,
    GRAFT_SETTINGS_COUNT,
};

enum GraftShinyOdds
{
    GRAFT_SHINY_ODDS_NORMAL,  // 1/4096
    GRAFT_SHINY_ODDS_BOOSTED, // 1/512
    GRAFT_SHINY_ODDS_HIGH,    // 1/128
    GRAFT_SHINY_ODDS_COUNT,
};

u32 GetGraftSetting(enum GraftSetting setting);
void SetGraftSetting(enum GraftSetting setting, u32 value);
u32 GetGraftSettingChoiceCount(enum GraftSetting setting);
u32 GetGraftSettingDefault(enum GraftSetting setting);
u32 GetPlayerShinyOddsThreshold(void);

// New game: the settings screen stores choices here; NewGameInitData() applies them after vars are cleared.
void ApplyPendingGraftSettings(void);
void CB2_InitGraftSettingsMenu(void);

#endif // GUARD_GRAFT_SETTINGS_H
