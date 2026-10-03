#include "global.h"
#include "battle_main.h"
#include "bg.h"
#include "graft_bag.h"
#include "graft_menu.h"
#include "gpu_regs.h"
#include "item_menu.h"
#include "main.h"
#include "malloc.h"
#include "menu.h"
#include "move.h"
#include "overworld.h"
#include "palette.h"
#include "party_menu.h"
#include "pokemon.h"
#include "pokemon_summary_screen.h"
#include "rogue.h"
#include "line_break.h"
#include "graphics.h"
#include "decompress.h"
#include "scanline_effect.h"
#include "sound.h"
#include "sprite.h"
#include "string_util.h"
#include "strings.h"
#include "task.h"
#include "text.h"
#include "text_window.h"
#include "window.h"
#include "constants/party_menu.h"
#include "constants/rgb.h"
#include "constants/songs.h"

// ============================================================
// Graft Bag screen (key item). A tab per piece kind; A on a piece offers Graft / Discard (and, in a
// Rogue run with the Graft vault unlock, Vault: keep it for the next run, rogue.c).
// Grafting opens the party menu (PARTY_ACTION_GRAFT_PIECE, party_menu.c), which comes back here.
// ============================================================

enum
{
    WIN_HEADER,
    WIN_LIST,
    WIN_DESC,
};

enum
{
    MODE_LIST,
    MODE_ACTION,  // Graft / (Vault) / Discard / Cancel
    MODE_DISCARD, // Yes / No
    MODE_VAULT,   // Yes / No
};

enum
{
    ACTION_GRAFT,
    ACTION_VAULT, // Rogue, with the Graft vault unlock
    ACTION_DISCARD,
    ACTION_CANCEL,
    ACTION_COUNT,
};

#define VISIBLE_ROWS 4
#define ROW_HEIGHT   14 // list rows, a little tighter than a text line so the description box gets a fifth tile row
#define MAX_LIST     (NUM_STATS * 256) // the biggest tab's possible pieces

struct GraftBagMenu
{
    struct GraftPiece pieces[MAX_LIST];
    u16 count;
    u8 mode;
    u8 actionCursor;
    u8 actions[ACTION_COUNT]; // the actions offered, in order
    u8 actionCount;
    u8 typeIconSpriteId;     // move pieces: the type and category icons, like the summary screen's
    u8 categoryIconSpriteId;
};

static EWRAM_DATA struct GraftBagMenu *sGraftBag = NULL;
// Kept across visits (and the trip to the party menu)
static EWRAM_DATA u8 sTab = 0;
static EWRAM_DATA u16 sCursor[GRAFT_PIECE_KIND_COUNT] = {0};
static EWRAM_DATA u16 sScroll[GRAFT_PIECE_KIND_COUNT] = {0};
static EWRAM_DATA MainCallback sExitCallback = NULL;
EWRAM_DATA struct GraftPiece gGraftPieceInUse = {0};

static const u8 *const sTabNames[GRAFT_PIECE_KIND_COUNT] =
{
    [GRAFT_PIECE_MOVE]    = COMPOUND_STRING("MOVES"),
    [GRAFT_PIECE_ABILITY] = COMPOUND_STRING("ABILITIES"),
    [GRAFT_PIECE_TYPE]    = COMPOUND_STRING("TYPES"),
    [GRAFT_PIECE_NATURE]  = COMPOUND_STRING("NATURES"),
    [GRAFT_PIECE_STAT]    = COMPOUND_STRING("STATS"),
};

static const u8 *const sActionNames[ACTION_COUNT] =
{
    [ACTION_GRAFT]   = COMPOUND_STRING("GRAFT"),
    [ACTION_VAULT]   = COMPOUND_STRING("VAULT"),
    [ACTION_DISCARD] = COMPOUND_STRING("DISCARD"),
    [ACTION_CANCEL]  = COMPOUND_STRING("CANCEL"),
};

static const u8 *const sStatLongNames[NUM_STATS] =
{
    COMPOUND_STRING("HP"),
    COMPOUND_STRING("Attack"),
    COMPOUND_STRING("Defense"),
    COMPOUND_STRING("Speed"),
    COMPOUND_STRING("Sp. Atk"),
    COMPOUND_STRING("Sp. Def"),
};

static const struct WindowTemplate sWinTemplates[] =
{
    [WIN_HEADER] = { .bg = 1, .tilemapLeft = 2, .tilemapTop = 1, .width = 26, .height = 2, .paletteNum = 1, .baseBlock = 2 },
    [WIN_LIST]   = { .bg = 1, .tilemapLeft = 2, .tilemapTop = 5, .width = 26, .height = 7, .paletteNum = 1, .baseBlock = 0x36 },
    [WIN_DESC]   = { .bg = 1, .tilemapLeft = 2, .tilemapTop = 14, .width = 26, .height = 5, .paletteNum = 1, .baseBlock = 0x106 },
    DUMMY_WIN_TEMPLATE
};

static const struct BgTemplate sBgTemplates[] =
{
    { .bg = 1, .charBaseIndex = 1, .mapBaseIndex = 30, .screenSize = 0, .paletteMode = 0, .priority = 0, .baseTile = 0 },
    { .bg = 0, .charBaseIndex = 1, .mapBaseIndex = 31, .screenSize = 0, .paletteMode = 0, .priority = 1, .baseTile = 0 },
};

static const u16 sBg_Pal[] = {RGB(12, 22, 14)};
static const u16 sText_Pal[] = INCGFX_U16("graphics/interface/option_menu_text.pal", ".gbapal");

// option_menu_text.pal: 2/3 orange, 4/5 red, 6/7 dark/light gray
static const u8 sColors_Normal[] = {TEXT_COLOR_TRANSPARENT, TEXT_COLOR_GREEN, TEXT_COLOR_LIGHT_GREEN};
static const u8 sColors_Header[] = {TEXT_COLOR_TRANSPARENT, TEXT_COLOR_DARK_GRAY, TEXT_COLOR_LIGHT_GRAY};
static const u8 sColors_Dim[] = {TEXT_COLOR_TRANSPARENT, TEXT_COLOR_LIGHT_GREEN, TEXT_COLOR_TRANSPARENT};
static const u8 sColors_Accent[] = {TEXT_COLOR_TRANSPARENT, TEXT_COLOR_RED, TEXT_COLOR_LIGHT_RED};

static void Task_GraftBagFadeIn(u8 taskId);
static void Task_GraftBagInput(u8 taskId);
static void Task_GraftBagExit(u8 taskId);
static void Task_GraftBagToPartyMenu(u8 taskId);
static void CB2_ReturnToGraftBag(void);

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

static void LoadTab(void)
{
    sGraftBag->count = GetGraftBagPieces(sTab, sGraftBag->pieces, MAX_LIST);
    if (sCursor[sTab] >= sGraftBag->count)
        sCursor[sTab] = sGraftBag->count ? sGraftBag->count - 1 : 0;
    if (sScroll[sTab] > sCursor[sTab])
        sScroll[sTab] = sCursor[sTab];
    if (sCursor[sTab] >= sScroll[sTab] + VISIBLE_ROWS)
        sScroll[sTab] = sCursor[sTab] - VISIBLE_ROWS + 1;
}

static void DrawHeader(void)
{
    u32 i, x = 4;
    FillWindowPixelBuffer(WIN_HEADER, PIXEL_FILL(1));
    for (i = 0; i < GRAFT_PIECE_KIND_COUNT; i++)
    {
        AddTextPrinterParameterized3(WIN_HEADER, FONT_SMALL, x, 3, i == sTab ? sColors_Accent : sColors_Dim, TEXT_SKIP_DRAW, sTabNames[i]);
        x += GetStringWidth(FONT_SMALL, sTabNames[i], 0) + 8;
    }
    CopyWindowToVram(WIN_HEADER, COPYWIN_FULL);
}

static void DrawList(void)
{
    u32 i;
    u8 buffer[32];
    FillWindowPixelBuffer(WIN_LIST, PIXEL_FILL(1));
    if (sGraftBag->count == 0)
    {
        AddTextPrinterParameterized3(WIN_LIST, FONT_NORMAL, 8, 1, sColors_Normal, TEXT_SKIP_DRAW, COMPOUND_STRING("No pieces here yet."));
    }
    for (i = 0; i < VISIBLE_ROWS && sScroll[sTab] + i < sGraftBag->count; i++)
    {
        u32 index = sScroll[sTab] + i;
        u32 y = i * ROW_HEIGHT;
        u8 *str;
        if (index == sCursor[sTab])
            AddTextPrinterParameterized3(WIN_LIST, FONT_NORMAL, 2, y, sColors_Accent, TEXT_SKIP_DRAW, gText_SelectorArrow2);
        GetGraftPieceName(buffer, &sGraftBag->pieces[index]);
        AddTextPrinterParameterized3(WIN_LIST, FONT_NORMAL, 12, y, sColors_Normal, TEXT_SKIP_DRAW, buffer);
        str = buffer;
        *str++ = CHAR_x;
        ConvertIntToDecimalStringN(str, GetGraftPieceCount(&sGraftBag->pieces[index]), STR_CONV_MODE_LEFT_ALIGN, 2);
        AddTextPrinterParameterized3(WIN_LIST, FONT_NORMAL, 26 * 8 - 8 - GetStringWidth(FONT_NORMAL, buffer, 0), y, sColors_Normal, TEXT_SKIP_DRAW, buffer);
    }
    // Scroll hints
    if (sScroll[sTab] > 0)
        AddTextPrinterParameterized3(WIN_LIST, FONT_SMALL, 26 * 8 - 40, 0, sColors_Dim, TEXT_SKIP_DRAW, COMPOUND_STRING("{UP_ARROW}"));
    if (sScroll[sTab] + VISIBLE_ROWS < sGraftBag->count)
        AddTextPrinterParameterized3(WIN_LIST, FONT_SMALL, 26 * 8 - 40, VISIBLE_ROWS * ROW_HEIGHT - 10, sColors_Dim, TEXT_SKIP_DRAW, COMPOUND_STRING("{DOWN_ARROW}"));
    CopyWindowToVram(WIN_LIST, COPYWIN_FULL);
}

// The description box's top-left, on screen (WIN_DESC is at tile 2, 14)
#define DESC_X (2 * 8)
#define DESC_Y (14 * 8)

static void ShowMoveIcons(enum Move move)
{
    struct Sprite *type = &gSprites[sGraftBag->typeIconSpriteId];
    struct Sprite *category = &gSprites[sGraftBag->categoryIconSpriteId];
    enum Type typeId = GetMoveType(move);

    StartSpriteAnim(type, typeId);
    type->oam.paletteNum = gTypesInfo[typeId].palette;
    type->x = DESC_X + 4 + 16;
    type->y = DESC_Y + 1 + 8;
    type->invisible = FALSE;
    category->x = DESC_X + 38 + 8;
    category->y = DESC_Y + 1 + 8;
    category->invisible = GetMoveCategory(move) == DAMAGE_CATEGORY_NONE; // no icon (and a NULL animation)
    if (!category->invisible)
        StartSpriteAnim(category, GetMoveCategory(move));
}

static void HideMoveIcons(void)
{
    gSprites[sGraftBag->typeIconSpriteId].invisible = TRUE;
    gSprites[sGraftBag->categoryIconSpriteId].invisible = TRUE;
}

// A move piece: type and category icons, power, accuracy and PP, then the move's description
static void DrawMoveDescription(enum Move move)
{
    u8 *str = gStringVar4;

    ShowMoveIcons(move);
    str = StringCopy(str, COMPOUND_STRING("POW "));
    if (GetMovePower(move) > 1)
        str = ConvertIntToDecimalStringN(str, GetMovePower(move), STR_CONV_MODE_LEFT_ALIGN, 3);
    else
        str = StringCopy(str, gText_ThreeDashes);
    str = StringCopy(str, COMPOUND_STRING("  ACC "));
    if (GetMoveAccuracy(move) != 0)
        str = ConvertIntToDecimalStringN(str, GetMoveAccuracy(move), STR_CONV_MODE_LEFT_ALIGN, 3);
    else
        str = StringCopy(str, gText_ThreeDashes);
    str = StringCopy(str, COMPOUND_STRING("  PP "));
    ConvertIntToDecimalStringN(str, GetMovePP(move), STR_CONV_MODE_LEFT_ALIGN, 2);
    AddTextPrinterParameterized3(WIN_DESC, FONT_SMALL, 60, 2, sColors_Normal, TEXT_SKIP_DRAW, gStringVar4);

    // The summary screen's description, rewrapped to this box's width (every move fits two lines)
    StringCopy(gStringVar4, GetMoveDescription(move));
    StripLineBreaks(gStringVar4);
    BreakStringAutomatic(gStringVar4, 200, 2, FONT_SMALL, HIDE_SCROLL_PROMPT);
    // Two lines under the icons
    for (str = gStringVar4; *str != EOS && *str != CHAR_NEWLINE; str++)
        ;
    if (*str == CHAR_NEWLINE)
    {
        *str = EOS;
        AddTextPrinterParameterized3(WIN_DESC, FONT_SMALL, 4, 26, sColors_Normal, TEXT_SKIP_DRAW, str + 1);
    }
    AddTextPrinterParameterized3(WIN_DESC, FONT_SMALL, 4, 14, sColors_Normal, TEXT_SKIP_DRAW, gStringVar4);
}

// Two lines about the selected piece (move pieces have their own layout, DrawMoveDescription)
static void BufferPieceDescription(const struct GraftPiece *piece)
{
    u8 *str = gStringVar4;
    switch (piece->kind)
    {
    case GRAFT_PIECE_ABILITY:
        str = StringCopy(str, gAbilitiesInfo[piece->value].description);
        break;
    case GRAFT_PIECE_TYPE:
        str = StringCopy(str, COMPOUND_STRING("Replaces one of a Pokémon's\ntwo types."));
        break;
    case GRAFT_PIECE_NATURE:
        if (gNaturesInfo[piece->value].statUp == gNaturesInfo[piece->value].statDown)
        {
            str = StringCopy(str, COMPOUND_STRING("No stat changes."));
        }
        else
        {
            str = StringCopy(str, COMPOUND_STRING("Raises "));
            str = StringCopy(str, sStatLongNames[gNaturesInfo[piece->value].statUp]);
            str = StringCopy(str, COMPOUND_STRING(", lowers "));
            str = StringCopy(str, sStatLongNames[gNaturesInfo[piece->value].statDown]);
            *str++ = CHAR_PERIOD;
        }
        break;
    case GRAFT_PIECE_STAT:
        str = StringCopy(str, COMPOUND_STRING("Sets a Pokémon's base "));
        str = StringCopy(str, sStatLongNames[piece->stat]);
        str = StringCopy(str, COMPOUND_STRING("\nto "));
        str = ConvertIntToDecimalStringN(str, piece->value, STR_CONV_MODE_LEFT_ALIGN, 3);
        *str++ = CHAR_PERIOD;
        break;
    }
    *str = EOS;
}

static void DrawDescription(void)
{
    FillWindowPixelBuffer(WIN_DESC, PIXEL_FILL(1));
    HideMoveIcons();
    if (sGraftBag->mode != MODE_LIST)
    {
        u32 i, x = 8, font = FONT_NORMAL, gap = sGraftBag->actionCount > 3 ? 14 : 20;
        u32 count = sGraftBag->mode == MODE_ACTION ? sGraftBag->actionCount : 2;
        u8 *str;
        if (sGraftBag->mode == MODE_DISCARD)
        {
            str = StringCopy(gStringVar4, COMPOUND_STRING("Discard one "));
            str = GetGraftPieceName(str, &sGraftBag->pieces[sCursor[sTab]]);
            StringCopy(str, COMPOUND_STRING("?"));
        }
        else if (sGraftBag->mode == MODE_VAULT)
        {
            // "Vault X?", or "Vault X? (Y comes out)" when the vault holds Y
            str = StringCopy(gStringVar4, COMPOUND_STRING("Vault "));
            str = GetGraftPieceName(str, &sGraftBag->pieces[sCursor[sTab]]);
            str = StringCopy(str, COMPOUND_STRING("?"));
            if (Rogue_GetVaultPieceName(gStringVar3))
            {
                u8 *end = str;
                str = StringCopy(str, COMPOUND_STRING(" ("));
                str = StringCopy(str, gStringVar3);
                StringCopy(str, COMPOUND_STRING(" comes out)"));
                font = FONT_NARROW;
                if (GetStringWidth(font, gStringVar4, 0) > 200)
                    StringCopy(end, COMPOUND_STRING(" (swaps)"));
            }
        }
        else
        {
            GetGraftPieceName(gStringVar4, &sGraftBag->pieces[sCursor[sTab]]);
        }
        AddTextPrinterParameterized3(WIN_DESC, font, 4, 1, sColors_Normal, TEXT_SKIP_DRAW, gStringVar4);
        for (i = 0; i < count; i++)
        {
            const u8 *name = sGraftBag->mode == MODE_ACTION ? sActionNames[sGraftBag->actions[i]] : (i == 0 ? gText_Yes : gText_No);
            if (i == sGraftBag->actionCursor)
                AddTextPrinterParameterized3(WIN_DESC, FONT_NORMAL, x - 8, 17, sColors_Accent, TEXT_SKIP_DRAW, gText_SelectorArrow2);
            AddTextPrinterParameterized3(WIN_DESC, FONT_NORMAL, x, 17, i == sGraftBag->actionCursor ? sColors_Accent : sColors_Normal, TEXT_SKIP_DRAW, name);
            x += GetStringWidth(FONT_NORMAL, name, 0) + gap;
        }
    }
    else if (sGraftBag->count != 0 && sGraftBag->pieces[sCursor[sTab]].kind == GRAFT_PIECE_MOVE)
    {
        DrawMoveDescription(sGraftBag->pieces[sCursor[sTab]].value);
        CopyWindowToVram(WIN_DESC, COPYWIN_FULL);
        return;
    }
    else if (sGraftBag->count != 0)
    {
        BufferPieceDescription(&sGraftBag->pieces[sCursor[sTab]]);
        AddTextPrinterParameterized3(WIN_DESC, FONT_NORMAL, 4, 1, sColors_Normal, TEXT_SKIP_DRAW, gStringVar4);
    }
    else
    {
        AddTextPrinterParameterized3(WIN_DESC, FONT_SMALL, 4, 3, sColors_Header, TEXT_SKIP_DRAW,
            COMPOUND_STRING("Catch Pokémon with Patch, Graft or\nSplice Balls to get pieces."));
    }
    CopyWindowToVram(WIN_DESC, COPYWIN_FULL);
}

#define TILE_TOP_CORNER_L 0x1A2
#define TILE_TOP_EDGE     0x1A3
#define TILE_TOP_CORNER_R 0x1A4
#define TILE_LEFT_EDGE    0x1A5
#define TILE_RIGHT_EDGE   0x1A7
#define TILE_BOT_CORNER_L 0x1A8
#define TILE_BOT_EDGE     0x1A9
#define TILE_BOT_CORNER_R 0x1AA

static void DrawFrame(u32 top, u32 height)
{
    FillBgTilemapBufferRect(1, TILE_TOP_CORNER_L,  1, top, 1, 1, 7);
    FillBgTilemapBufferRect(1, TILE_TOP_EDGE,      2, top, 26, 1, 7);
    FillBgTilemapBufferRect(1, TILE_TOP_CORNER_R, 28, top, 1, 1, 7);
    FillBgTilemapBufferRect(1, TILE_LEFT_EDGE,     1, top + 1, 1, height, 7);
    FillBgTilemapBufferRect(1, TILE_RIGHT_EDGE,   28, top + 1, 1, height, 7);
    FillBgTilemapBufferRect(1, TILE_BOT_CORNER_L,  1, top + height + 1, 1, 1, 7);
    FillBgTilemapBufferRect(1, TILE_BOT_EDGE,      2, top + height + 1, 26, 1, 7);
    FillBgTilemapBufferRect(1, TILE_BOT_CORNER_R, 28, top + height + 1, 1, 1, 7);
}

static void DrawFrames(void)
{
    DrawFrame(0, 2);  // rows 0-3
    DrawFrame(4, 7);  // rows 4-12
    DrawFrame(13, 5); // rows 13-19
    CopyBgTilemapBufferToVram(1);
}

static void CB2_InitGraftBag(void)
{
    u32 i;
    switch (gMain.state)
    {
    default:
    case 0:
        SetVBlankCallback(NULL);
        sGraftBag = AllocZeroed(sizeof(*sGraftBag));
        if (sTab >= GRAFT_PIECE_KIND_COUNT)
            sTab = 0;
        LoadTab();
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
        SetGpuReg(REG_OFFSET_DISPCNT, DISPCNT_OBJ_ON | DISPCNT_OBJ_1D_MAP);
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
        // Move pieces' type and category icons (the summary screen's sprites)
        LoadCompressedSpriteSheet(&gSpriteSheet_MoveTypes);
        LoadPalette(gMoveTypes_Pal, OBJ_PLTT_ID(13), 3 * PLTT_SIZE_4BPP);
        LoadCompressedSpriteSheet(&gSpriteSheet_CategoryIcons);
        LoadSpritePalette(&gSpritePal_CategoryIcons);
        sGraftBag->typeIconSpriteId = CreateSprite(&gSpriteTemplate_MoveTypes, 0, 0, 0);
        sGraftBag->categoryIconSpriteId = CreateSprite(&gSpriteTemplate_CategoryIcons, 0, 0, 0);
        // The category icons' animation 0 (DAMAGE_CATEGORY_NONE) is NULL: the sprite must never sit on it,
        // or AnimateSprites reads animation commands from address 0 (a crash, depending on what's there)
        StartSpriteAnim(&gSprites[sGraftBag->categoryIconSpriteId], DAMAGE_CATEGORY_STATUS);
        gSprites[sGraftBag->typeIconSpriteId].oam.priority = 0;
        gSprites[sGraftBag->categoryIconSpriteId].oam.priority = 0;
        HideMoveIcons();
        gMain.state++;
        break;
    case 4:
        PutWindowTilemap(WIN_HEADER);
        PutWindowTilemap(WIN_LIST);
        PutWindowTilemap(WIN_DESC);
        DrawHeader();
        DrawList();
        DrawDescription();
        DrawFrames();
        CreateTask(Task_GraftBagFadeIn, 0);
        gMain.state++;
        break;
    case 5:
        BeginNormalPaletteFade(PALETTES_ALL, 0, 16, 0, RGB_BLACK);
        SetVBlankCallback(VBlankCB);
        SetMainCallback2(MainCB2);
        return;
    }
}

void OpenGraftBag(MainCallback exitCallback)
{
    sExitCallback = exitCallback;
    gMain.state = 0;
    SetMainCallback2(CB2_InitGraftBag);
}

static void CB2_ReturnToGraftBag(void)
{
    gGraftLearnMovePending = FALSE; // every exit from the piece's party menu comes here
    gMain.state = 0;
    SetMainCallback2(CB2_InitGraftBag);
}

static void Task_GraftBagFadeIn(u8 taskId)
{
    if (!gPaletteFade.active)
        gTasks[taskId].func = Task_GraftBagInput;
}

static void Redraw(void)
{
    DrawList();
    DrawDescription();
}

static void MoveCursor(s32 delta)
{
    s32 cursor = sCursor[sTab] + delta;
    if (sGraftBag->count == 0)
        return;
    if (cursor < 0)
        cursor = 0;
    if (cursor >= sGraftBag->count)
        cursor = sGraftBag->count - 1;
    if (cursor == sCursor[sTab])
        return;
    sCursor[sTab] = cursor;
    if (sCursor[sTab] < sScroll[sTab])
        sScroll[sTab] = sCursor[sTab];
    if (sCursor[sTab] >= sScroll[sTab] + VISIBLE_ROWS)
        sScroll[sTab] = sCursor[sTab] - VISIBLE_ROWS + 1;
    PlaySE(SE_SELECT);
    Redraw();
}

static void HandleListInput(u8 taskId)
{
    if (JOY_NEW(B_BUTTON))
    {
        PlaySE(SE_SELECT);
        BeginNormalPaletteFade(PALETTES_ALL, 0, 0, 16, RGB_BLACK);
        gTasks[taskId].func = Task_GraftBagExit;
    }
    else if (JOY_NEW(DPAD_LEFT | L_BUTTON) || JOY_NEW(DPAD_RIGHT | R_BUTTON))
    {
        if (JOY_NEW(DPAD_RIGHT | R_BUTTON))
            sTab = (sTab + 1) % GRAFT_PIECE_KIND_COUNT;
        else
            sTab = (sTab + GRAFT_PIECE_KIND_COUNT - 1) % GRAFT_PIECE_KIND_COUNT;
        PlaySE(SE_SELECT);
        LoadTab();
        DrawHeader();
        Redraw();
    }
    else if (JOY_REPEAT(DPAD_UP))
    {
        MoveCursor(-1);
    }
    else if (JOY_REPEAT(DPAD_DOWN))
    {
        MoveCursor(1);
    }
    else if (JOY_NEW(A_BUTTON) && sGraftBag->count != 0)
    {
        PlaySE(SE_SELECT);
        sGraftBag->mode = MODE_ACTION;
        sGraftBag->actionCursor = 0;
        sGraftBag->actionCount = 0;
        sGraftBag->actions[sGraftBag->actionCount++] = ACTION_GRAFT;
        if (Rogue_CanVaultPieces())
            sGraftBag->actions[sGraftBag->actionCount++] = ACTION_VAULT;
        sGraftBag->actions[sGraftBag->actionCount++] = ACTION_DISCARD;
        sGraftBag->actions[sGraftBag->actionCount++] = ACTION_CANCEL;
        DrawDescription();
    }
}

static void HandleChoiceInput(u8 taskId)
{
    u32 count = sGraftBag->mode == MODE_ACTION ? sGraftBag->actionCount : 2;
    if (JOY_NEW(DPAD_LEFT) && sGraftBag->actionCursor > 0)
    {
        PlaySE(SE_SELECT);
        sGraftBag->actionCursor--;
        DrawDescription();
    }
    else if (JOY_NEW(DPAD_RIGHT) && sGraftBag->actionCursor < count - 1)
    {
        PlaySE(SE_SELECT);
        sGraftBag->actionCursor++;
        DrawDescription();
    }
    else if (JOY_NEW(B_BUTTON))
    {
        PlaySE(SE_SELECT);
        sGraftBag->mode = MODE_LIST;
        DrawDescription();
    }
    else if (JOY_NEW(A_BUTTON))
    {
        PlaySE(SE_SELECT);
        if (sGraftBag->mode == MODE_ACTION)
        {
            switch (sGraftBag->actions[sGraftBag->actionCursor])
            {
            case ACTION_GRAFT:
                gGraftPieceInUse = sGraftBag->pieces[sCursor[sTab]];
                BeginNormalPaletteFade(PALETTES_ALL, 0, 0, 16, RGB_BLACK);
                gTasks[taskId].func = Task_GraftBagToPartyMenu;
                return;
            case ACTION_DISCARD:
                sGraftBag->mode = MODE_DISCARD;
                sGraftBag->actionCursor = 1; // default to No
                break;
            case ACTION_VAULT:
                sGraftBag->mode = MODE_VAULT;
                sGraftBag->actionCursor = 1;
                break;
            default:
                sGraftBag->mode = MODE_LIST;
                break;
            }
            DrawDescription();
        }
        else // MODE_DISCARD, MODE_VAULT
        {
            if (sGraftBag->actionCursor == 0)
            {
                if (sGraftBag->mode == MODE_VAULT)
                    Rogue_VaultPiece(&sGraftBag->pieces[sCursor[sTab]]);
                else
                    RemoveGraftPiece(&sGraftBag->pieces[sCursor[sTab]]);
                LoadTab();
            }
            sGraftBag->mode = MODE_LIST;
            Redraw();
        }
    }
}

static void Task_GraftBagInput(u8 taskId)
{
    if (sGraftBag->mode == MODE_LIST)
        HandleListInput(taskId);
    else
        HandleChoiceInput(taskId);
}

static void FreeGraftBagScreen(void)
{
    FreeAllWindowBuffers();
    TRY_FREE_AND_SET_NULL(sGraftBag);
}

static void Task_GraftBagExit(u8 taskId)
{
    if (!gPaletteFade.active)
    {
        DestroyTask(taskId);
        FreeGraftBagScreen();
        Rogue_OnGraftBagClosed(); // the Patchwork achievement
        SetMainCallback2(sExitCallback);
    }
}

static void Task_GraftBagToPartyMenu(u8 taskId)
{
    if (!gPaletteFade.active)
    {
        DestroyTask(taskId);
        FreeGraftBagScreen();
        OpenPartyMenuForGraftPiece(CB2_ReturnToGraftBag);
    }
}
