#include "global.h"
#include "battle_main.h"
#include "bg.h"
#include "event_data.h"
#include "gpu_regs.h"
#include "graft_data.h"
#include "item.h"
#include "graft_settings.h"
#include "main.h"
#include "menu.h"
#include "palette.h"
#include "scanline_effect.h"
#include "sound.h"
#include "sprite.h"
#include "string_util.h"
#include "task.h"
#include "text.h"
#include "text_window.h"
#include "window.h"
#include "constants/rgb.h"
#include "constants/items.h"
#include "constants/songs.h"

// ============================================================
// Settings storage
// ============================================================

struct GraftSettingInfo
{
    const u8 *name;
    const u8 *description;
    const u8 *const *choiceDescriptions;  // optional: describes the selected choice instead
    const u8 *const *choices;             // choice labels, or NULL to use choiceName()
    const u8 *(*choiceName)(u32 value);
    bool32 (*isValidChoice)(u32 value);   // optional: values to skip while cycling
    u8 choiceCount;
    u8 defaultChoice;
    u8 shift; // bit position in the 32-bit settings word
    u8 bits;
    s8 requires; // setting that must be non-zero for this one to matter, or -1
};

static const u8 *const sOffOn[] = { COMPOUND_STRING("Off"), COMPOUND_STRING("On") };

static const u8 *const sShinyOddsChoices[GRAFT_SHINY_ODDS_COUNT] =
{
    [GRAFT_SHINY_ODDS_NORMAL]  = COMPOUND_STRING("1/4096"),
    [GRAFT_SHINY_ODDS_BOOSTED] = COMPOUND_STRING("1/512"),
    [GRAFT_SHINY_ODDS_HIGH]    = COMPOUND_STRING("1/128"),
};

static const u8 *const sLevelCapChoices[GRAFT_LEVEL_CAP_COUNT] =
{
    [GRAFT_LEVEL_CAP_OFF]  = COMPOUND_STRING("Off"),
    [GRAFT_LEVEL_CAP_HARD] = COMPOUND_STRING("Hard"),
    [GRAFT_LEVEL_CAP_SOFT] = COMPOUND_STRING("Soft"),
};

static const u8 *const sLevelCapDescriptions[GRAFT_LEVEL_CAP_COUNT] =
{
    [GRAFT_LEVEL_CAP_OFF] = COMPOUND_STRING("No level cap."),
    [GRAFT_LEVEL_CAP_HARD] = COMPOUND_STRING("No EXP once a Pokémon reaches the\nnext Leader's strongest level."),
    [GRAFT_LEVEL_CAP_SOFT] = COMPOUND_STRING("Past the next Leader's strongest\nlevel: much less EXP, may disobey."),
};

static const u8 *const sGraftLimitChoices[GRAFT_LIMIT_COUNT] =
{
    [GRAFT_LIMIT_FREE]     = COMPOUND_STRING("Free"),
    [GRAFT_LIMIT_STANDARD] = COMPOUND_STRING("Standard"),
    [GRAFT_LIMIT_BUDGET]   = COMPOUND_STRING("Budget"),
    [GRAFT_LIMIT_STRICT]   = COMPOUND_STRING("Strict"),
};

static const u8 *const sGraftLimitDescriptions[GRAFT_LIMIT_COUNT] =
{
    [GRAFT_LIMIT_FREE] = COMPOUND_STRING("No limits on grafts."),
    [GRAFT_LIMIT_STANDARD] = COMPOUND_STRING("Stat grafts: 1 + badges per Pokémon.\nNo limit from 5 badges on."),
    [GRAFT_LIMIT_BUDGET] = COMPOUND_STRING("2 + half your badges in grafts per\nPokémon. Moves don't count."),
    [GRAFT_LIMIT_STRICT] = COMPOUND_STRING("3 grafts per Pokémon.\nMoves don't count."),
};

static const u8 *const sGraftSupplyChoices[GRAFT_SUPPLY_COUNT] =
{
    [GRAFT_SUPPLY_SHOP]    = COMPOUND_STRING("Shops"),
    [GRAFT_SUPPLY_RATIONS] = COMPOUND_STRING("Rations"),
};

static const u8 *const sHarvestChoices[GRAFT_HARVESTS_COUNT] =
{
    [GRAFT_HARVESTS_NONE]    = COMPOUND_STRING("None"),
    [GRAFT_HARVESTS_ONE]     = COMPOUND_STRING("1 per area"),
    [GRAFT_HARVESTS_PER_MON] = COMPOUND_STRING("1 per mon"),
    [GRAFT_HARVESTS_ANY]     = COMPOUND_STRING("Any"),
};

static const u8 *const sHarvestDescriptions[GRAFT_HARVESTS_COUNT] =
{
    [GRAFT_HARVESTS_NONE] = COMPOUND_STRING("Graft balls can't be used. Only\nthe Splicer makes graft pieces."),
    [GRAFT_HARVESTS_ONE] = COMPOUND_STRING("You can break down one wild\nPokémon in each area."),
    [GRAFT_HARVESTS_PER_MON] = COMPOUND_STRING("Each area allows one wild Pokémon\nper party member to be broken down."),
    [GRAFT_HARVESTS_ANY] = COMPOUND_STRING("No limit on breaking down wild\nPokémon."),
};

static const u8 *const sEnemyGraftChoices[GRAFT_ENEMY_GRAFTS_COUNT] =
{
    [GRAFT_ENEMY_GRAFTS_FEWER]  = COMPOUND_STRING("Fewer"),
    [GRAFT_ENEMY_GRAFTS_NORMAL] = COMPOUND_STRING("Normal"),
    [GRAFT_ENEMY_GRAFTS_MORE]   = COMPOUND_STRING("More"),
};


static const u8 *const sFaintingChoices[GRAFT_FAINTING_COUNT] =
{
    [GRAFT_FAINTING_NORMAL]      = COMPOUND_STRING("Normal"),
    [GRAFT_FAINTING_RELEASE]     = COMPOUND_STRING("Release"),
    [GRAFT_FAINTING_GRAVEYARD]   = COMPOUND_STRING("Graveyard"),
    [GRAFT_FAINTING_AUTO_SPLICE] = COMPOUND_STRING("Auto-splice"),
};

static const u8 *const sFaintingDescriptions[GRAFT_FAINTING_COUNT] =
{
    [GRAFT_FAINTING_NORMAL] = COMPOUND_STRING("Fainted Pokémon can be revived\nand healed as usual."),
    [GRAFT_FAINTING_RELEASE] = COMPOUND_STRING("Pokémon that faint are released\nafter the battle."),
    [GRAFT_FAINTING_GRAVEYARD] = COMPOUND_STRING("Fainted Pokémon can't be healed,\nbut the Splicer can still use them."),
    [GRAFT_FAINTING_AUTO_SPLICE] = COMPOUND_STRING("After battle, fainted Pokémon are\nbroken down into graft pieces."),
};

static const u8 *const sExpShareChoices[] = { COMPOUND_STRING("Item"), COMPOUND_STRING("Party") };

static const u8 *const sExpShareDescriptions[] =
{
    COMPOUND_STRING("Only Pokémon that battled or hold\nan Exp. Share get EXP."),
    COMPOUND_STRING("The whole party always gets EXP,\nlike in later games."),
};

static const u8 *const sWildGraftChoices[GRAFT_WILD_GRAFTS_COUNT] =
{
    [GRAFT_WILD_GRAFTS_OFF]    = COMPOUND_STRING("Off"),
    [GRAFT_WILD_GRAFTS_RARE]   = COMPOUND_STRING("Rare"),
    [GRAFT_WILD_GRAFTS_COMMON] = COMPOUND_STRING("Common"),
};

static const u8 *const sWildGraftDescriptions[GRAFT_WILD_GRAFTS_COUNT] =
{
    [GRAFT_WILD_GRAFTS_OFF] = COMPOUND_STRING("Wild Pokémon never have grafts."),
    [GRAFT_WILD_GRAFTS_RARE] = COMPOUND_STRING("1 in 20 wild Pokémon comes with\ngrafts from its neighbors."),
    [GRAFT_WILD_GRAFTS_COMMON] = COMPOUND_STRING("1 in 5 wild Pokémon comes with\ngrafts from its neighbors."),
};

static const u8 *const sNatDexChoices[GRAFT_NATDEX_COUNT] =
{
    [GRAFT_NATDEX_OFF]      = COMPOUND_STRING("Off"),
    [GRAFT_NATDEX_ON]       = COMPOUND_STRING("On"),
    [GRAFT_NATDEX_POSTGAME] = COMPOUND_STRING("Postgame"),
};

static const u8 *const sNatDexDescriptions[GRAFT_NATDEX_COUNT] =
{
    [GRAFT_NATDEX_OFF] = COMPOUND_STRING("Only Emerald's own wild Pokémon.\nNo postgame legendaries."),
    [GRAFT_NATDEX_ON] = COMPOUND_STRING("Wild Pokémon from every region,\nand legendaries after the League."),
    [GRAFT_NATDEX_POSTGAME] = COMPOUND_STRING("Emerald's own wild Pokémon until\nyou beat the League, then all."),
};

static const u8 *const sPokeCenterChoices[] = { COMPOUND_STRING("Open"), COMPOUND_STRING("Closed") };

static const u8 *const sBattleItemChoices[] = { COMPOUND_STRING("Allowed"), COMPOUND_STRING("Banned") };

static const u8 *MonotypeName(u32 value)
{
    if (value == TYPE_NONE)
        return COMPOUND_STRING("Off");
    return gTypesInfo[value].name;
}

bool32 IsMonotypeChoice(u32 value)
{
    return value == TYPE_NONE || (value < TYPE_STELLAR && value != TYPE_MYSTERY);
}

static const struct GraftSettingInfo sGraftSettings[GRAFT_SETTINGS_COUNT] =
{
    [GRAFT_SETTING_SHINY_ODDS] = {
        .name = COMPOUND_STRING("Shiny Odds"),
        .description = COMPOUND_STRING("How often Pokémon you find are\nshiny. The Charm still helps."),
        .choices = sShinyOddsChoices, .choiceCount = GRAFT_SHINY_ODDS_COUNT,
        .defaultChoice = GRAFT_SHINY_ODDS_BOOSTED, .shift = 0, .bits = 2, .requires = -1,
    },
    [GRAFT_SETTING_LEVEL_CAP] = {
        .name = COMPOUND_STRING("Level Cap"),
        .description = COMPOUND_STRING("Cap at the next Leader's ace.\nHard: no EXP. Soft: much less."),
        .choices = sLevelCapChoices, .choiceDescriptions = sLevelCapDescriptions, .choiceCount = GRAFT_LEVEL_CAP_COUNT,
        .defaultChoice = GRAFT_LEVEL_CAP_OFF, .shift = 2, .bits = 2, .requires = -1,
    },
    [GRAFT_SETTING_EXP_SHARE] = {
        .name = COMPOUND_STRING("EXP Share"),
        .description = COMPOUND_STRING("Who gets EXP from a battle."),
        .choices = sExpShareChoices, .choiceDescriptions = sExpShareDescriptions, .choiceCount = 2,
        .defaultChoice = FALSE, .shift = 24, .bits = 1, .requires = -1,
    },
    [GRAFT_SETTING_GRAFT_LIMIT] = {
        .name = COMPOUND_STRING("Graft Limit"),
        .description = COMPOUND_STRING("Standard caps stat grafts by badge.\nBudget/Strict cap all but moves."),
        .choices = sGraftLimitChoices, .choiceDescriptions = sGraftLimitDescriptions, .choiceCount = GRAFT_LIMIT_COUNT,
        .defaultChoice = GRAFT_LIMIT_STANDARD, .shift = 4, .bits = 2, .requires = -1,
    },
    [GRAFT_SETTING_GRAFT_GATES] = {
        .name = COMPOUND_STRING("Graft Gates"),
        .description = COMPOUND_STRING("Types need 1 badge, stats 2,\nand abilities 3 to graft."),
        .choices = sOffOn, .choiceCount = 2,
        .defaultChoice = FALSE, .shift = 6, .bits = 1, .requires = -1,
    },
    [GRAFT_SETTING_GRAFT_SUPPLY] = {
        .name = COMPOUND_STRING("Graft Items"),
        .description = COMPOUND_STRING("Rations: not sold in shops. Each\nbadge comes with a few instead."),
        .choices = sGraftSupplyChoices, .choiceCount = GRAFT_SUPPLY_COUNT,
        .defaultChoice = GRAFT_SUPPLY_SHOP, .shift = 7, .bits = 1, .requires = -1,
    },
    [GRAFT_SETTING_HARVESTS] = {
        .name = COMPOUND_STRING("Harvests"),
        .description = COMPOUND_STRING("How many wild Pokémon you can\ngraft from in each area."),
        .choices = sHarvestChoices, .choiceDescriptions = sHarvestDescriptions, .choiceCount = GRAFT_HARVESTS_COUNT,
        .defaultChoice = GRAFT_HARVESTS_ANY, .shift = 8, .bits = 2, .requires = -1,
    },
    [GRAFT_SETTING_ENEMY_GRAFTS] = {
        .name = COMPOUND_STRING("Enemy Grafts"),
        .description = COMPOUND_STRING("How many grafts trainers'\nPokémon get."),
        .choices = sEnemyGraftChoices, .choiceCount = GRAFT_ENEMY_GRAFTS_COUNT,
        .defaultChoice = GRAFT_ENEMY_GRAFTS_NORMAL, .shift = 10, .bits = 2, .requires = -1,
    },
    [GRAFT_SETTING_WILD_GRAFTS] = {
        .name = COMPOUND_STRING("Wild Grafts"),
        .description = COMPOUND_STRING("How often wild Pokémon already\nhave grafts."),
        .choices = sWildGraftChoices, .choiceDescriptions = sWildGraftDescriptions, .choiceCount = GRAFT_WILD_GRAFTS_COUNT,
        .defaultChoice = GRAFT_WILD_GRAFTS_OFF, .shift = 25, .bits = 2, .requires = -1,
    },
    [GRAFT_SETTING_NATDEX] = {
        .name = COMPOUND_STRING("National Dex"),
        .description = COMPOUND_STRING("Which wild Pokémon appear."),
        .choices = sNatDexChoices, .choiceDescriptions = sNatDexDescriptions, .choiceCount = GRAFT_NATDEX_COUNT,
        .defaultChoice = GRAFT_NATDEX_ON, .shift = 28, .bits = 2, .requires = -1,
    },
    [GRAFT_SETTING_ENCOUNTERS] = {
        .name = COMPOUND_STRING("Nuzlocke"),
        .description = COMPOUND_STRING("You can only catch the first\nPokémon you meet in each area."),
        .choices = sOffOn, .choiceCount = 2,
        .defaultChoice = FALSE, .shift = 12, .bits = 1, .requires = -1,
    },
    [GRAFT_SETTING_DUPES_CLAUSE] = {
        .name = COMPOUND_STRING("Dupes Clause"),
        .description = COMPOUND_STRING("Families you've caught don't use\nup an area's encounter."),
        .choices = sOffOn, .choiceCount = 2,
        .defaultChoice = TRUE, .shift = 13, .bits = 1, .requires = GRAFT_SETTING_ENCOUNTERS,
    },
    [GRAFT_SETTING_SHINY_CLAUSE] = {
        .name = COMPOUND_STRING("Shiny Clause"),
        .description = COMPOUND_STRING("Shiny Pokémon can always be\ncaught."),
        .choices = sOffOn, .choiceCount = 2,
        .defaultChoice = TRUE, .shift = 14, .bits = 1, .requires = GRAFT_SETTING_ENCOUNTERS,
    },
    [GRAFT_SETTING_FAINTING] = {
        .name = COMPOUND_STRING("Fainting"),
        .description = COMPOUND_STRING("What happens to fainted Pokémon\nafter battle. See the manual."),
        .choices = sFaintingChoices, .choiceDescriptions = sFaintingDescriptions, .choiceCount = GRAFT_FAINTING_COUNT,
        .defaultChoice = GRAFT_FAINTING_NORMAL, .shift = 15, .bits = 2, .requires = -1,
    },
    [GRAFT_SETTING_BATTLE_ITEMS] = {
        .name = COMPOUND_STRING("Battle Items"),
        .description = COMPOUND_STRING("Banned: only Poké Balls can be\nused from the Bag in battle."),
        .choices = sBattleItemChoices, .choiceCount = 2,
        .defaultChoice = FALSE, .shift = 17, .bits = 1, .requires = -1,
    },
    [GRAFT_SETTING_POKECENTERS] = {
        .name = COMPOUND_STRING("Poké Centers"),
        .description = COMPOUND_STRING("Closed: nurses won't heal your\nPokémon. Other healing still works."),
        .choices = sPokeCenterChoices, .choiceCount = 2,
        .defaultChoice = FALSE, .shift = 27, .bits = 1, .requires = -1,
    },
    [GRAFT_SETTING_SET_MODE] = {
        .name = COMPOUND_STRING("Force Set"),
        .description = COMPOUND_STRING("No free switch when a foe faints.\nThe Battle Style option is locked."),
        .choices = sOffOn, .choiceCount = 2,
        .defaultChoice = FALSE, .shift = 18, .bits = 1, .requires = -1,
    },
    [GRAFT_SETTING_MONOTYPE] = {
        .name = COMPOUND_STRING("Monotype"),
        .description = COMPOUND_STRING("Only Pokémon of this type can battle.\nGrafted types count."),
        .choiceName = MonotypeName, .isValidChoice = IsMonotypeChoice, .choiceCount = NUMBER_OF_MON_TYPES,
        .defaultChoice = TYPE_NONE, .shift = 19, .bits = 5, .requires = -1,
    },
};

static u32 GetSettingsWord(void)
{
    return VarGet(VAR_GRAFTED_SETTINGS) | (VarGet(VAR_GRAFTED_SETTINGS_2) << 16);
}

static void SetSettingsWord(u32 word)
{
    VarSet(VAR_GRAFTED_SETTINGS, word & 0xFFFF);
    VarSet(VAR_GRAFTED_SETTINGS_2, word >> 16);
}

static bool32 IsValidChoice(const struct GraftSettingInfo *info, u32 value)
{
    if (value >= info->choiceCount)
        return FALSE;
    return info->isValidChoice == NULL || info->isValidChoice(value);
}

// Stored as (choice XOR default), so a save where the vars were never written reads as all defaults.
u32 GetGraftSetting(enum GraftSetting setting)
{
    const struct GraftSettingInfo *info = &sGraftSettings[setting];
    u32 raw = (GetSettingsWord() >> info->shift) & ((1 << info->bits) - 1);
    u32 value = raw ^ info->defaultChoice;
    return IsValidChoice(info, value) ? value : info->defaultChoice;
}

void SetGraftSetting(enum GraftSetting setting, u32 value)
{
    const struct GraftSettingInfo *info = &sGraftSettings[setting];
    u32 mask = ((1 << info->bits) - 1) << info->shift;
    u32 word = GetSettingsWord() & ~mask;
    word |= ((value ^ info->defaultChoice) << info->shift) & mask;
    SetSettingsWord(word);
}

u32 GetGraftSettingDefault(enum GraftSetting setting)
{
    return sGraftSettings[setting].defaultChoice;
}

const u8 *GetGraftSettingName(enum GraftSetting setting)
{
    return sGraftSettings[setting].name;
}

const u8 *GetGraftSettingDescription(enum GraftSetting setting, u32 value)
{
    const struct GraftSettingInfo *info = &sGraftSettings[setting];
    if (info->choiceDescriptions != NULL)
        return info->choiceDescriptions[value];
    return info->description;
}

const u8 *GetGraftSettingChoiceName(enum GraftSetting setting, u32 value)
{
    const struct GraftSettingInfo *info = &sGraftSettings[setting];
    if (info->choiceName != NULL)
        return info->choiceName(value);
    return info->choices[value];
}

u32 GetGraftSettingChoiceCount(enum GraftSetting setting)
{
    return sGraftSettings[setting].choiceCount;
}

bool32 IsGraftSettingValidChoice(enum GraftSetting setting, u32 value)
{
    return IsValidChoice(&sGraftSettings[setting], value);
}

// Chance that a mon the player encounters is shiny, as a threshold out of 65536.
// Only used when a mon is created; stored shininess is measured against the fixed SHINY_ODDS,
// so changing this can never flip an existing mon.
u32 GetPlayerShinyOddsThreshold(void)
{
    switch (GetGraftSetting(GRAFT_SETTING_SHINY_ODDS))
    {
    case GRAFT_SHINY_ODDS_NORMAL:
        return 16;
    case GRAFT_SHINY_ODDS_HIGH:
        return 512;
    case GRAFT_SHINY_ODDS_BOOSTED:
    default:
        return 128;
    }
}

// ============================================================
// New-game settings screen
// ============================================================

static EWRAM_DATA u8 sPendingChoices[GRAFT_SETTINGS_COUNT] = {0};
static EWRAM_DATA bool8 sHasPendingChoices = FALSE;

void ApplyPendingGraftSettings(void)
{
    u32 i;
    for (i = 0; i < GRAFT_SETTINGS_COUNT; i++)
        SetGraftSetting(i, sHasPendingChoices ? sPendingChoices[i] : sGraftSettings[i].defaultChoice);
    sHasPendingChoices = FALSE;

    // Rationed graft items: a starting ration, since marts won't sell any
    if (GetGraftSetting(GRAFT_SETTING_GRAFT_SUPPLY) == GRAFT_SUPPLY_RATIONS && !AreGraftBallsUseless())
        AddBagItem(ITEM_SPLICE_BALL, 3);
}

// Harvests: None makes grafting balls useless, so marts and rations leave them out.
bool32 AreGraftBallsUseless(void)
{
    return GetGraftSetting(GRAFT_SETTING_HARVESTS) == GRAFT_HARVESTS_NONE;
}

bool32 IsPartyExpShareOn(void)
{
    return GetGraftSetting(GRAFT_SETTING_EXP_SHARE) == TRUE;
}

// Special for Common_EventScript_PkmnCenterNurse
bool16 ArePokeCentersClosed(void)
{
    return GetGraftSetting(GRAFT_SETTING_POKECENTERS) == TRUE;
}

// Special for Common_EventScript_GiveGraftRations: what this badge's ration holds.
// 0xFF nothing (not rationed), 0 Splice Balls, 1 Splice Balls and a Splicer, 2 a Splicer only.
u16 GetGraftRationSplicers(void)
{
    bool32 splicer = GetBadgeCount() >= 3;
    if (GetGraftSetting(GRAFT_SETTING_GRAFT_SUPPLY) != GRAFT_SUPPLY_RATIONS)
        return 0xFF;
    if (AreGraftBallsUseless())
        return splicer ? 2 : 0xFF;
    return splicer;
}


enum
{
    WIN_HEADER,
    WIN_LIST,
    WIN_DESCRIPTION,
};

#define ROW_START    GRAFT_SETTINGS_COUNT // last row: start the game
#define ROW_COUNT    (GRAFT_SETTINGS_COUNT + 1)
#define VISIBLE_ROWS 5
#define CHOICE_X     120
#define CHOICE_WIDTH 80

#define tSelection data[0]
#define tScroll    data[1]

static const u8 sText_Header[] = _("GAME SETTINGS");
static const u8 sText_Start[] = _("Start Game");
static const u8 sText_StartDescription[] = _("These can't be changed later.\nPress A to begin your journey.");

// Tile budget: 2 + 26*2 + 26*10 + 26*4 = 0x1A2, where the frame tiles start.
static const struct WindowTemplate sWinTemplates[] =
{
    [WIN_HEADER] = { .bg = 1, .tilemapLeft = 2, .tilemapTop = 1, .width = 26, .height = 2, .paletteNum = 1, .baseBlock = 2 },
    [WIN_LIST] = { .bg = 0, .tilemapLeft = 2, .tilemapTop = 5, .width = 26, .height = 10, .paletteNum = 1, .baseBlock = 0x36 },
    [WIN_DESCRIPTION] = { .bg = 1, .tilemapLeft = 2, .tilemapTop = 15, .width = 26, .height = 4, .paletteNum = 1, .baseBlock = 0x13A },
    DUMMY_WIN_TEMPLATE
};

static const struct BgTemplate sBgTemplates[] =
{
    { .bg = 1, .charBaseIndex = 1, .mapBaseIndex = 30, .screenSize = 0, .paletteMode = 0, .priority = 0, .baseTile = 0 },
    { .bg = 0, .charBaseIndex = 1, .mapBaseIndex = 31, .screenSize = 0, .paletteMode = 0, .priority = 1, .baseTile = 0 },
};

static const u16 sBg_Pal[] = {RGB(17, 18, 31)};
static const u16 sText_Pal[] = INCGFX_U16("graphics/interface/option_menu_text.pal", ".gbapal");

// option_menu_text.pal: 2/3 orange (labels), 4/5 red (values), 6/7 dark/light gray
static const u8 sColors_Label[]    = {TEXT_COLOR_TRANSPARENT, TEXT_COLOR_DARK_GRAY, TEXT_COLOR_LIGHT_GRAY};
static const u8 sColors_Value[]    = {TEXT_COLOR_TRANSPARENT, TEXT_COLOR_RED, TEXT_COLOR_LIGHT_RED};
static const u8 sColors_Inactive[] = {TEXT_COLOR_TRANSPARENT, TEXT_COLOR_LIGHT_GREEN, TEXT_COLOR_TRANSPARENT};
static const u8 sColors_Text[]     = {TEXT_COLOR_TRANSPARENT, TEXT_COLOR_GREEN, TEXT_COLOR_LIGHT_GREEN};

static void Task_GraftSettingsFadeIn(u8 taskId);
static void Task_GraftSettingsInput(u8 taskId);
static void Task_GraftSettingsFadeOut(u8 taskId);

static void MainCB2(void)
{
    RunTasks();
    AnimateSprites();
    BuildOamBuffer();
    UpdatePaletteFade();
}

static void VBlankCB(void)
{
    LoadOam();
    ProcessSpriteCopyRequests();
    TransferPlttBuffer();
}

static void Highlight(u32 visibleRow)
{
    SetGpuReg(REG_OFFSET_WIN0H, WIN_RANGE(16, DISPLAY_WIDTH - 16));
    SetGpuReg(REG_OFFSET_WIN0V, WIN_RANGE(visibleRow * 16 + 40, visibleRow * 16 + 56));
}

static bool32 IsSettingActive(u32 setting)
{
    s8 requires = sGraftSettings[setting].requires;
    return requires < 0 || sPendingChoices[requires] != 0;
}

static void DrawRow(u32 row, u32 visibleRow, bool32 selected)
{
    u32 y = visibleRow * 16 + 1;
    FillWindowPixelRect(WIN_LIST, PIXEL_FILL(1), 0, visibleRow * 16, 26 * 8, 16);
    if (row == ROW_START)
    {
        AddTextPrinterParameterized3(WIN_LIST, FONT_NORMAL, 8, y, sColors_Text, TEXT_SKIP_DRAW, sText_Start);
    }
    else
    {
        bool32 active = IsSettingActive(row);
        const u8 *value = GetGraftSettingChoiceName(row, sPendingChoices[row]);
        u32 width = GetStringWidth(FONT_NORMAL, value, 0);
        u32 x = CHOICE_X + (CHOICE_WIDTH - width) / 2;

        AddTextPrinterParameterized3(WIN_LIST, FONT_NORMAL, 8, y, active ? sColors_Text : sColors_Inactive, TEXT_SKIP_DRAW, sGraftSettings[row].name);
        AddTextPrinterParameterized3(WIN_LIST, FONT_NORMAL, x, y, active ? sColors_Value : sColors_Inactive, TEXT_SKIP_DRAW, value);
        if (selected)
        {
            static const u8 sLeft[] = {CHAR_LEFT_ARROW, EOS};
            static const u8 sRight[] = {CHAR_RIGHT_ARROW, EOS};
            AddTextPrinterParameterized3(WIN_LIST, FONT_NORMAL, CHOICE_X - 8, y, sColors_Label, TEXT_SKIP_DRAW, sLeft);
            AddTextPrinterParameterized3(WIN_LIST, FONT_NORMAL, CHOICE_X + CHOICE_WIDTH, y, sColors_Label, TEXT_SKIP_DRAW, sRight);
        }
    }
}

static void DrawList(u8 taskId)
{
    s16 selection = gTasks[taskId].tSelection;
    s16 scroll = gTasks[taskId].tScroll;
    u32 i;

    FillWindowPixelBuffer(WIN_LIST, PIXEL_FILL(1));
    for (i = 0; i < VISIBLE_ROWS && scroll + i < ROW_COUNT; i++)
        DrawRow(scroll + i, i, scroll + i == selection);
    CopyWindowToVram(WIN_LIST, COPYWIN_GFX);

    // On BG1 so the row highlight doesn't dim it; a divider line separates it from the list
    FillWindowPixelBuffer(WIN_DESCRIPTION, PIXEL_FILL(1));
    FillWindowPixelRect(WIN_DESCRIPTION, PIXEL_FILL(TEXT_COLOR_LIGHT_GREEN), 4, 1, 26 * 8 - 8, 1);
    AddTextPrinterParameterized3(WIN_DESCRIPTION, FONT_NORMAL, 8, 3, sColors_Label, TEXT_SKIP_DRAW,
                                 selection == ROW_START ? sText_StartDescription : GetGraftSettingDescription(selection, sPendingChoices[selection]));
    CopyWindowToVram(WIN_DESCRIPTION, COPYWIN_GFX);

    Highlight(selection - scroll);
}

#define TILE_TOP_CORNER_L 0x1A2
#define TILE_TOP_EDGE     0x1A3
#define TILE_TOP_CORNER_R 0x1A4
#define TILE_LEFT_EDGE    0x1A5
#define TILE_RIGHT_EDGE   0x1A7
#define TILE_BOT_CORNER_L 0x1A8
#define TILE_BOT_EDGE     0x1A9
#define TILE_BOT_CORNER_R 0x1AA

static void DrawFrames(void)
{
    FillBgTilemapBufferRect(1, TILE_TOP_CORNER_L,  1,  0,  1,  1,  7);
    FillBgTilemapBufferRect(1, TILE_TOP_EDGE,      2,  0, 26,  1,  7);
    FillBgTilemapBufferRect(1, TILE_TOP_CORNER_R, 28,  0,  1,  1,  7);
    FillBgTilemapBufferRect(1, TILE_LEFT_EDGE,     1,  1,  1,  2,  7);
    FillBgTilemapBufferRect(1, TILE_RIGHT_EDGE,   28,  1,  1,  2,  7);
    FillBgTilemapBufferRect(1, TILE_BOT_CORNER_L,  1,  3,  1,  1,  7);
    FillBgTilemapBufferRect(1, TILE_BOT_EDGE,      2,  3, 26,  1,  7);
    FillBgTilemapBufferRect(1, TILE_BOT_CORNER_R, 28,  3,  1,  1,  7);

    FillBgTilemapBufferRect(1, TILE_TOP_CORNER_L,  1,  4,  1,  1,  7);
    FillBgTilemapBufferRect(1, TILE_TOP_EDGE,      2,  4, 26,  1,  7);
    FillBgTilemapBufferRect(1, TILE_TOP_CORNER_R, 28,  4,  1,  1,  7);
    FillBgTilemapBufferRect(1, TILE_LEFT_EDGE,     1,  5,  1, 14,  7);
    FillBgTilemapBufferRect(1, TILE_RIGHT_EDGE,   28,  5,  1, 14,  7);
    FillBgTilemapBufferRect(1, TILE_BOT_CORNER_L,  1, 19,  1,  1,  7);
    FillBgTilemapBufferRect(1, TILE_BOT_EDGE,      2, 19, 26,  1,  7);
    FillBgTilemapBufferRect(1, TILE_BOT_CORNER_R, 28, 19,  1,  1,  7);
    CopyBgTilemapBufferToVram(1);
}

// Shown after Birch's intro, before the game starts. gMain.savedCallback is where to go next.
void CB2_InitGraftSettingsMenu(void)
{
    u32 i;
    switch (gMain.state)
    {
    default:
    case 0:
        SetVBlankCallback(NULL);
        for (i = 0; i < GRAFT_SETTINGS_COUNT; i++)
            sPendingChoices[i] = sGraftSettings[i].defaultChoice;
        sHasPendingChoices = TRUE;
        gMain.state++;
        break;
    case 1:
        DmaClearLarge16(3, (void *)(VRAM), VRAM_SIZE, 0x1000);
        DmaClear32(3, OAM, OAM_SIZE);
        DmaClear16(3, PLTT, PLTT_SIZE);
        SetGpuReg(REG_OFFSET_DISPCNT, 0);
        ResetBgsAndClearDma3BusyFlags(0);
        InitBgsFromTemplates(0, sBgTemplates, ARRAY_COUNT(sBgTemplates));
        for (i = 0; i < 4; i++)
        {
            ChangeBgX(i, 0, BG_COORD_SET);
            ChangeBgY(i, 0, BG_COORD_SET);
        }
        InitWindows(sWinTemplates);
        DeactivateAllTextPrinters();
        SetGpuReg(REG_OFFSET_WIN0H, 0);
        SetGpuReg(REG_OFFSET_WIN0V, 0);
        SetGpuReg(REG_OFFSET_WININ, WININ_WIN0_BG0);
        SetGpuReg(REG_OFFSET_WINOUT, WINOUT_WIN01_BG0 | WINOUT_WIN01_BG1 | WINOUT_WIN01_CLR);
        SetGpuReg(REG_OFFSET_BLDCNT, BLDCNT_TGT1_BG0 | BLDCNT_EFFECT_DARKEN);
        SetGpuReg(REG_OFFSET_BLDALPHA, 0);
        SetGpuReg(REG_OFFSET_BLDY, 4);
        SetGpuReg(REG_OFFSET_DISPCNT, DISPCNT_WIN0_ON | DISPCNT_OBJ_ON | DISPCNT_OBJ_1D_MAP);
        ShowBg(0);
        ShowBg(1);
        gMain.state++;
        break;
    case 2:
        ResetPaletteFade();
        ScanlineEffect_Stop();
        ResetTasks();
        ResetSpriteData();
        gMain.state++;
        break;
    case 3:
        LoadBgTiles(1, GetWindowFrameTilesPal(gSaveBlock2Ptr->optionsWindowFrameType)->tiles, 0x120, 0x1A2);
        LoadPalette(sBg_Pal, BG_PLTT_ID(0), sizeof(sBg_Pal));
        LoadPalette(GetWindowFrameTilesPal(gSaveBlock2Ptr->optionsWindowFrameType)->pal, BG_PLTT_ID(7), PLTT_SIZE_4BPP);
        LoadPalette(sText_Pal, BG_PLTT_ID(1), sizeof(sText_Pal));
        gMain.state++;
        break;
    case 4:
    {
        u8 taskId = CreateTask(Task_GraftSettingsFadeIn, 0);
        gTasks[taskId].tSelection = 0;
        gTasks[taskId].tScroll = 0;
        PutWindowTilemap(WIN_HEADER);
        PutWindowTilemap(WIN_LIST);
        PutWindowTilemap(WIN_DESCRIPTION);
        FillWindowPixelBuffer(WIN_HEADER, PIXEL_FILL(1));
        AddTextPrinterParameterized(WIN_HEADER, FONT_NORMAL, sText_Header, 8, 1, TEXT_SKIP_DRAW, NULL);
        CopyWindowToVram(WIN_HEADER, COPYWIN_FULL);
        DrawList(taskId);
        CopyWindowToVram(WIN_LIST, COPYWIN_FULL);
        CopyWindowToVram(WIN_DESCRIPTION, COPYWIN_FULL);
        DrawFrames();
        gMain.state++;
        break;
    }
    case 5:
        BeginNormalPaletteFade(PALETTES_ALL, 0, 16, 0, RGB_BLACK);
        SetVBlankCallback(VBlankCB);
        SetMainCallback2(MainCB2);
        return;
    }
}

static void Task_GraftSettingsFadeIn(u8 taskId)
{
    if (!gPaletteFade.active)
        gTasks[taskId].func = Task_GraftSettingsInput;
}

static u32 CycleChoice(u32 setting, u32 value, bool32 forward)
{
    u32 count = sGraftSettings[setting].choiceCount;
    do
    {
        value = forward ? (value + 1) % count : (value + count - 1) % count;
    } while (!IsValidChoice(&sGraftSettings[setting], value));
    return value;
}

static void Task_GraftSettingsInput(u8 taskId)
{
    s16 *sel = &gTasks[taskId].tSelection;
    s16 *scroll = &gTasks[taskId].tScroll;

    if (JOY_NEW(A_BUTTON) && *sel == ROW_START)
    {
        PlaySE(SE_SELECT);
        BeginNormalPaletteFade(PALETTES_ALL, 0, 0, 16, RGB_BLACK);
        gTasks[taskId].func = Task_GraftSettingsFadeOut;
        return;
    }
    if (JOY_NEW(START_BUTTON))
    {
        // Jump to Start Game
        *sel = ROW_START;
    }
    else if (JOY_REPEAT(DPAD_UP))
    {
        *sel = (*sel > 0) ? *sel - 1 : ROW_COUNT - 1;
    }
    else if (JOY_REPEAT(DPAD_DOWN))
    {
        *sel = (*sel < ROW_COUNT - 1) ? *sel + 1 : 0;
    }
    else if (*sel < GRAFT_SETTINGS_COUNT && (JOY_REPEAT(DPAD_LEFT) || JOY_REPEAT(DPAD_RIGHT)))
    {
        sPendingChoices[*sel] = CycleChoice(*sel, sPendingChoices[*sel], JOY_REPEAT(DPAD_RIGHT));
        PlaySE(SE_SELECT);
    }
    else
    {
        return;
    }

    if (*sel < *scroll)
        *scroll = *sel;
    else if (*sel >= *scroll + VISIBLE_ROWS)
        *scroll = *sel - VISIBLE_ROWS + 1;
    DrawList(taskId);
}

static void Task_GraftSettingsFadeOut(u8 taskId)
{
    if (!gPaletteFade.active)
    {
        DestroyTask(taskId);
        FreeAllWindowBuffers();
        SetMainCallback2(gMain.savedCallback);
    }
}
