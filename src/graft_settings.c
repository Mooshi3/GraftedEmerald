#include "global.h"
#include "bg.h"
#include "event_data.h"
#include "gpu_regs.h"
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
#include "constants/songs.h"

// ============================================================
// Settings storage
// ============================================================

struct GraftSettingInfo
{
    const u8 *name;
    const u8 *const *choices;
    u8 choiceCount;
    u8 defaultChoice;
    u8 shift; // bit position in VAR_GRAFTED_SETTINGS
    u8 bits;
};

static const u8 *const sShinyOddsChoices[GRAFT_SHINY_ODDS_COUNT] =
{
    [GRAFT_SHINY_ODDS_NORMAL]  = COMPOUND_STRING("1/4096"),
    [GRAFT_SHINY_ODDS_BOOSTED] = COMPOUND_STRING("1/512"),
    [GRAFT_SHINY_ODDS_HIGH]    = COMPOUND_STRING("1/128"),
};

static const struct GraftSettingInfo sGraftSettings[GRAFT_SETTINGS_COUNT] =
{
    [GRAFT_SETTING_SHINY_ODDS] = {
        .name = COMPOUND_STRING("SHINY ODDS"),
        .choices = sShinyOddsChoices,
        .choiceCount = GRAFT_SHINY_ODDS_COUNT,
        .defaultChoice = GRAFT_SHINY_ODDS_BOOSTED,
        .shift = 0,
        .bits = 2,
    },
};

// Stored as (choice XOR default), so a save where the var was never written reads as all defaults.
static u32 Encode(const struct GraftSettingInfo *info, u32 value)
{
    return (value ^ info->defaultChoice) & ((1 << info->bits) - 1);
}

u32 GetGraftSetting(enum GraftSetting setting)
{
    const struct GraftSettingInfo *info = &sGraftSettings[setting];
    u32 raw = (VarGet(VAR_GRAFTED_SETTINGS) >> info->shift) & ((1 << info->bits) - 1);
    u32 value = raw ^ info->defaultChoice;
    return value < info->choiceCount ? value : info->defaultChoice;
}

void SetGraftSetting(enum GraftSetting setting, u32 value)
{
    const struct GraftSettingInfo *info = &sGraftSettings[setting];
    u16 var = VarGet(VAR_GRAFTED_SETTINGS);
    var &= ~(((1 << info->bits) - 1) << info->shift);
    var |= Encode(info, value) << info->shift;
    VarSet(VAR_GRAFTED_SETTINGS, var);
}

u32 GetGraftSettingChoiceCount(enum GraftSetting setting)
{
    return sGraftSettings[setting].choiceCount;
}

u32 GetGraftSettingDefault(enum GraftSetting setting)
{
    return sGraftSettings[setting].defaultChoice;
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
}

enum
{
    WIN_HEADER,
    WIN_SETTINGS,
};

#define ROW_START GRAFT_SETTINGS_COUNT // last row: start the game
#define ROW_COUNT (GRAFT_SETTINGS_COUNT + 1)
#define CHOICES_X 104
#define CHOICES_WIDTH 96

#define tSelection data[0]

static const u8 sText_Header[] = _("GAME SETTINGS");
static const u8 sText_Start[] = _("START GAME");
static const u8 sText_Hint[] = _("These can't be changed later.");

static const struct WindowTemplate sWinTemplates[] =
{
    [WIN_HEADER] = { .bg = 1, .tilemapLeft = 2, .tilemapTop = 1, .width = 26, .height = 2, .paletteNum = 1, .baseBlock = 2 },
    [WIN_SETTINGS] = { .bg = 0, .tilemapLeft = 2, .tilemapTop = 5, .width = 26, .height = 14, .paletteNum = 1, .baseBlock = 0x36 },
    DUMMY_WIN_TEMPLATE
};

static const struct BgTemplate sBgTemplates[] =
{
    { .bg = 1, .charBaseIndex = 1, .mapBaseIndex = 30, .screenSize = 0, .paletteMode = 0, .priority = 0, .baseTile = 0 },
    { .bg = 0, .charBaseIndex = 1, .mapBaseIndex = 31, .screenSize = 0, .paletteMode = 0, .priority = 1, .baseTile = 0 },
};

static const u16 sBg_Pal[] = {RGB(17, 18, 31)};
static const u16 sText_Pal[] = INCGFX_U16("graphics/interface/option_menu_text.pal", ".gbapal");

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

static void Highlight(u8 row)
{
    SetGpuReg(REG_OFFSET_WIN0H, WIN_RANGE(16, DISPLAY_WIDTH - 16));
    SetGpuReg(REG_OFFSET_WIN0V, WIN_RANGE(row * 16 + 40, row * 16 + 56));
}

// Choices are spread evenly across the right half; the selected one is drawn in red.
static void DrawChoices(u8 setting)
{
    const struct GraftSettingInfo *info = &sGraftSettings[setting];
    u32 i, x, totalWidth = 0, gap;
    u8 y = setting * 16 + 1;

    FillWindowPixelRect(WIN_SETTINGS, PIXEL_FILL(1), CHOICES_X, setting * 16, CHOICES_WIDTH + 8, 16);
    for (i = 0; i < info->choiceCount; i++)
        totalWidth += GetStringWidth(FONT_NORMAL, info->choices[i], 0);
    gap = info->choiceCount > 1 && totalWidth < CHOICES_WIDTH ? (CHOICES_WIDTH - totalWidth) / (info->choiceCount - 1) : 4;
    x = CHOICES_X;
    for (i = 0; i < info->choiceCount; i++)
    {
        u8 colors[3] = {TEXT_COLOR_TRANSPARENT, TEXT_COLOR_GREEN, TEXT_COLOR_LIGHT_GREEN};
        if (i == sPendingChoices[setting])
        {
            colors[1] = TEXT_COLOR_RED;
            colors[2] = TEXT_COLOR_LIGHT_RED;
        }
        AddTextPrinterParameterized3(WIN_SETTINGS, FONT_NORMAL, x, y, colors, TEXT_SKIP_DRAW, info->choices[i]);
        x += GetStringWidth(FONT_NORMAL, info->choices[i], 0) + gap;
    }
}

static void DrawAll(void)
{
    u32 i;
    FillWindowPixelBuffer(WIN_HEADER, PIXEL_FILL(1));
    AddTextPrinterParameterized(WIN_HEADER, FONT_NORMAL, sText_Header, 8, 1, TEXT_SKIP_DRAW, NULL);
    CopyWindowToVram(WIN_HEADER, COPYWIN_FULL);

    FillWindowPixelBuffer(WIN_SETTINGS, PIXEL_FILL(1));
    for (i = 0; i < GRAFT_SETTINGS_COUNT; i++)
    {
        AddTextPrinterParameterized(WIN_SETTINGS, FONT_NORMAL, sGraftSettings[i].name, 8, i * 16 + 1, TEXT_SKIP_DRAW, NULL);
        DrawChoices(i);
    }
    AddTextPrinterParameterized(WIN_SETTINGS, FONT_NORMAL, sText_Start, 8, ROW_START * 16 + 1, TEXT_SKIP_DRAW, NULL);
    AddTextPrinterParameterized(WIN_SETTINGS, FONT_SMALL, sText_Hint, 8, 12 * 8, TEXT_SKIP_DRAW, NULL);
    CopyWindowToVram(WIN_SETTINGS, COPYWIN_FULL);
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
    FillBgTilemapBufferRect(1, TILE_TOP_EDGE,      2,  0, 27,  1,  7);
    FillBgTilemapBufferRect(1, TILE_TOP_CORNER_R, 28,  0,  1,  1,  7);
    FillBgTilemapBufferRect(1, TILE_LEFT_EDGE,     1,  1,  1,  2,  7);
    FillBgTilemapBufferRect(1, TILE_RIGHT_EDGE,   28,  1,  1,  2,  7);
    FillBgTilemapBufferRect(1, TILE_BOT_CORNER_L,  1,  3,  1,  1,  7);
    FillBgTilemapBufferRect(1, TILE_BOT_EDGE,      2,  3, 27,  1,  7);
    FillBgTilemapBufferRect(1, TILE_BOT_CORNER_R, 28,  3,  1,  1,  7);

    FillBgTilemapBufferRect(1, TILE_TOP_CORNER_L,  1,  4,  1,  1,  7);
    FillBgTilemapBufferRect(1, TILE_TOP_EDGE,      2,  4, 26,  1,  7);
    FillBgTilemapBufferRect(1, TILE_TOP_CORNER_R, 28,  4,  1,  1,  7);
    FillBgTilemapBufferRect(1, TILE_LEFT_EDGE,     1,  5,  1, 18,  7);
    FillBgTilemapBufferRect(1, TILE_RIGHT_EDGE,   28,  5,  1, 18,  7);
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
        PutWindowTilemap(WIN_HEADER);
        PutWindowTilemap(WIN_SETTINGS);
        DrawAll();
        DrawFrames();
        gMain.state++;
        break;
    case 5:
    {
        u8 taskId = CreateTask(Task_GraftSettingsFadeIn, 0);
        gTasks[taskId].tSelection = 0;
        Highlight(0);
        BeginNormalPaletteFade(PALETTES_ALL, 0, 16, 0, RGB_BLACK);
        SetVBlankCallback(VBlankCB);
        SetMainCallback2(MainCB2);
        return;
    }
    }
}

static void Task_GraftSettingsFadeIn(u8 taskId)
{
    if (!gPaletteFade.active)
        gTasks[taskId].func = Task_GraftSettingsInput;
}

static void Task_GraftSettingsInput(u8 taskId)
{
    s16 *sel = &gTasks[taskId].tSelection;

    if (JOY_NEW(A_BUTTON) && *sel == ROW_START)
    {
        PlaySE(SE_SELECT);
        BeginNormalPaletteFade(PALETTES_ALL, 0, 0, 16, RGB_BLACK);
        gTasks[taskId].func = Task_GraftSettingsFadeOut;
    }
    else if (JOY_NEW(DPAD_UP))
    {
        *sel = (*sel > 0) ? *sel - 1 : ROW_COUNT - 1;
        Highlight(*sel);
    }
    else if (JOY_NEW(DPAD_DOWN))
    {
        *sel = (*sel < ROW_COUNT - 1) ? *sel + 1 : 0;
        Highlight(*sel);
    }
    else if (*sel < GRAFT_SETTINGS_COUNT && (JOY_NEW(DPAD_LEFT) || JOY_NEW(DPAD_RIGHT)))
    {
        u32 count = sGraftSettings[*sel].choiceCount;
        u8 *choice = &sPendingChoices[*sel];
        if (JOY_NEW(DPAD_RIGHT))
            *choice = (*choice + 1) % count;
        else
            *choice = (*choice + count - 1) % count;
        DrawChoices(*sel);
        CopyWindowToVram(WIN_SETTINGS, COPYWIN_GFX);
    }
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
