#include "global.h"
#include "bg.h"
#include "event_data.h"
#include "gpu_regs.h"
#include "graft_encounter_dex.h"
#include "main.h"
#include "malloc.h"
#include "menu.h"
#include "overworld.h"
#include "palette.h"
#include "pokedex.h"
#include "pokemon.h"
#include "region_map.h"
#include "scanline_effect.h"
#include "sound.h"
#include "sprite.h"
#include "string_util.h"
#include "task.h"
#include "text.h"
#include "text_window.h"
#include "wild_encounter.h"
#include "window.h"
#include "constants/rgb.h"
#include "constants/songs.h"

// ============================================================
// Pokédex encounter screen (R in the Pokédex list). Scrolls through every area the player has
// visited and lists the wild Pokémon there, "?????" until seen. Postgame legendaries only show
// once they can actually appear (after the first Champion win, until caught).
// ============================================================

// ---------------- Data ----------------

void MarkEncounterAreaVisited(u32 mapsec)
{
    if (mapsec < MAPSEC_NONE)
        gSaveBlock1Ptr->graftVisitedAreas[mapsec / 8] |= 1 << (mapsec % 8);
}

bool32 IsEncounterAreaVisited(u32 mapsec)
{
    if (mapsec >= MAPSEC_NONE)
        return FALSE;
    return (gSaveBlock1Ptr->graftVisitedAreas[mapsec / 8] >> (mapsec % 8)) & 1;
}

static u32 GetHeaderMapsec(u32 headerId)
{
    return Overworld_GetMapHeaderByGroupAndId(gWildMonHeaders[headerId].mapGroup, gWildMonHeaders[headerId].mapNum)->regionMapSectionId;
}

// Altering Cave has several headers for one map, and only the first is used normally
static bool32 IsDuplicateHeader(u32 headerId)
{
    for (u32 i = 0; i < headerId; i++)
    {
        if (gWildMonHeaders[i].mapGroup == gWildMonHeaders[headerId].mapGroup
         && gWildMonHeaders[i].mapNum == gWildMonHeaders[headerId].mapNum)
            return TRUE;
    }
    return FALSE;
}

static bool32 HeaderHasAnyTable(u32 headerId)
{
    for (u32 t = 0; t < TIMES_OF_DAY_COUNT; t++)
    {
        const struct WildEncounterTypes *types = &gWildMonHeaders[headerId].encounterTypes[t];
        if (types->landMonsInfo || types->waterMonsInfo || types->rockSmashMonsInfo || types->fishingMonsInfo)
            return TRUE;
    }
    return FALSE;
}

bool32 AreaHasWildEncounters(u32 mapsec)
{
    for (u32 i = 0; gWildMonHeaders[i].mapGroup != MAP_GROUP(MAP_UNDEFINED); i++)
    {
        if (GetHeaderMapsec(i) == mapsec && HeaderHasAnyTable(i))
            return TRUE;
    }
    return FALSE;
}

// Per-map, per-time totals before they're merged over the area's maps and times
struct SlotTotal
{
    u16 species;
    u8 method;
    u8 minLevel;
    u8 maxLevel;
    u8 rate;
};

static void AddToTotals(struct SlotTotal *totals, u32 *count, u16 species, u8 method, u8 minLevel, u8 maxLevel, u8 rate)
{
    for (u32 i = 0; i < *count; i++)
    {
        if (totals[i].species == species && totals[i].method == method)
        {
            totals[i].rate += rate;
            totals[i].minLevel = min(totals[i].minLevel, minLevel);
            totals[i].maxLevel = max(totals[i].maxLevel, maxLevel);
            return;
        }
    }
    if (*count >= ENCDEX_MAX_ENTRIES)
        return;
    totals[*count] = (struct SlotTotal){ species, method, minLevel, maxLevel, rate };
    (*count)++;
}

static void MergeIntoEntries(struct EncounterDexEntry *entries, u32 *count, const struct SlotTotal *total, u32 timeOfDay)
{
    for (u32 i = 0; i < *count; i++)
    {
        if (entries[i].species == total->species && entries[i].method == total->method)
        {
            entries[i].rate = max(entries[i].rate, total->rate);
            entries[i].minLevel = min(entries[i].minLevel, total->minLevel);
            entries[i].maxLevel = max(entries[i].maxLevel, total->maxLevel);
            entries[i].timeMask |= 1 << timeOfDay;
            return;
        }
    }
    if (*count >= ENCDEX_MAX_ENTRIES)
        return;
    entries[*count] = (struct EncounterDexEntry){ total->species, total->method, total->minLevel, total->maxLevel, total->rate, 1 << timeOfDay };
    (*count)++;
}

// Adds one table's slots. lastSlotLegend: the postgame legendary that takes the table's last slot, or SPECIES_NONE.
static void AddTable(struct SlotTotal *totals, u32 *count, const struct WildPokemonInfo *info, u32 slots, u32 firstSlot,
                     const u8 *rates, u8 method, u16 lastSlotLegend, u8 legendMin, u8 legendMax)
{
    if (info == NULL || info->wildPokemon == NULL)
        return;
    for (u32 i = firstSlot; i < firstSlot + slots; i++)
    {
        const struct WildPokemon *mon = &info->wildPokemon[i];
        if (i == firstSlot + slots - 1 && lastSlotLegend != SPECIES_NONE)
            AddToTotals(totals, count, lastSlotLegend, method, legendMin, legendMax, rates[i]);
        else if (mon->species != SPECIES_NONE)
            AddToTotals(totals, count, mon->species, method, mon->minLevel, mon->maxLevel, rates[i]);
    }
}

static u16 GetActiveLegend(u32 headerId, enum WildPokemonArea area, u32 timeOfDay, u8 *minLevel, u8 *maxLevel)
{
    u16 map = gWildMonHeaders[headerId].mapNum | (gWildMonHeaders[headerId].mapGroup << 8);
    u16 species;
    if (!FlagGet(FLAG_SYS_GAME_CLEAR))
        return SPECIES_NONE;
    species = GetPostgameLegendaryForMap(map, area, timeOfDay, minLevel, maxLevel);
    if (species != SPECIES_NONE && GetSetPokedexFlag(SpeciesToNationalPokedexNum(species), FLAG_GET_CAUGHT))
        return SPECIES_NONE;
    return species;
}

static int CompareEntries(const struct EncounterDexEntry *a, const struct EncounterDexEntry *b)
{
    if (a->method != b->method)
        return a->method - b->method;
    return b->rate - a->rate; // most common first
}

// Fills entries (ENCDEX_MAX_ENTRIES) for an area, sorted by method then rate. Returns the count.
u32 GetEncounterAreaEntries(u32 mapsec, struct EncounterDexEntry *entries)
{
    struct SlotTotal totals[ENCDEX_MAX_ENTRIES];
    u32 count = 0, i, j;

    for (i = 0; gWildMonHeaders[i].mapGroup != MAP_GROUP(MAP_UNDEFINED); i++)
    {
        if (GetHeaderMapsec(i) != mapsec || IsDuplicateHeader(i))
            continue;
        for (u32 t = 0; t < TIMES_OF_DAY_COUNT; t++)
        {
            u32 totalCount = 0;
            u8 lmin = 0, lmax = 0;
            u16 legend;

            legend = GetActiveLegend(i, WILD_AREA_LAND, t, &lmin, &lmax);
            AddTable(totals, &totalCount, GetWildMonInfoForTime(i, WILD_AREA_LAND, t), NUM_LAND_MONS_ENCOUNTER_SLOTS, 0, gEncounterSlotRatesLand, ENCDEX_GRASS, legend, lmin, lmax);
            legend = GetActiveLegend(i, WILD_AREA_WATER, t, &lmin, &lmax);
            AddTable(totals, &totalCount, GetWildMonInfoForTime(i, WILD_AREA_WATER, t), NUM_WATER_MONS_ENCOUNTER_SLOTS, 0, gEncounterSlotRatesWater, ENCDEX_SURF, legend, lmin, lmax);
            AddTable(totals, &totalCount, GetWildMonInfoForTime(i, WILD_AREA_FISHING, t), NUM_FISHING_MONS_OLD_ROD_ENCOUNTER_SLOTS, 0, gEncounterSlotRatesFishing, ENCDEX_OLD_ROD, SPECIES_NONE, 0, 0);
            AddTable(totals, &totalCount, GetWildMonInfoForTime(i, WILD_AREA_FISHING, t), NUM_FISHING_MONS_GOOD_ROD_ENCOUNTER_SLOTS, NUM_FISHING_MONS_OLD_ROD_ENCOUNTER_SLOTS, gEncounterSlotRatesFishing, ENCDEX_GOOD_ROD, SPECIES_NONE, 0, 0);
            AddTable(totals, &totalCount, GetWildMonInfoForTime(i, WILD_AREA_FISHING, t), NUM_FISHING_MONS_SUPER_ROD_ENCOUNTER_SLOTS, NUM_FISHING_MONS_OLD_ROD_ENCOUNTER_SLOTS + NUM_FISHING_MONS_GOOD_ROD_ENCOUNTER_SLOTS, gEncounterSlotRatesFishing, ENCDEX_SUPER_ROD, SPECIES_NONE, 0, 0);
            AddTable(totals, &totalCount, GetWildMonInfoForTime(i, WILD_AREA_ROCKS, t), NUM_ROCK_SMASH_MONS_ENCOUNTER_SLOTS, 0, gEncounterSlotRatesRockSmash, ENCDEX_ROCK_SMASH, SPECIES_NONE, 0, 0);
            for (j = 0; j < totalCount; j++)
                MergeIntoEntries(entries, &count, &totals[j], t);
        }
    }

    // Insertion sort; lists are short
    for (i = 1; i < count; i++)
    {
        struct EncounterDexEntry tmp = entries[i];
        for (j = i; j > 0 && CompareEntries(&entries[j - 1], &tmp) > 0; j--)
            entries[j] = entries[j - 1];
        entries[j] = tmp;
    }
    return count;
}

// ---------------- Screen ----------------

enum
{
    WIN_HEADER,
    WIN_LIST,
};

#define VISIBLE_LINES 7
#define MAX_LINES (ENCDEX_MAX_ENTRIES + ENCDEX_METHOD_COUNT)

struct EncounterDexLine
{
    u8 isHeader;
    u8 index; // method for headers, entry index otherwise
};

struct EncounterDexState
{
    u8 areas[MAPSEC_NONE];
    u8 areaCount;
    u8 areaIndex;
    u8 lineCount;
    u8 scroll;
    u8 entryCount;
    struct EncounterDexEntry entries[ENCDEX_MAX_ENTRIES];
    struct EncounterDexLine lines[MAX_LINES];
};

static EWRAM_DATA struct EncounterDexState *sEncDex = NULL;

static const u8 *const sMethodNames[ENCDEX_METHOD_COUNT] =
{
    [ENCDEX_GRASS]      = COMPOUND_STRING("Grass & caves"),
    [ENCDEX_SURF]       = COMPOUND_STRING("Surfing"),
    [ENCDEX_OLD_ROD]    = COMPOUND_STRING("Old Rod"),
    [ENCDEX_GOOD_ROD]   = COMPOUND_STRING("Good Rod"),
    [ENCDEX_SUPER_ROD]  = COMPOUND_STRING("Super Rod"),
    [ENCDEX_ROCK_SMASH] = COMPOUND_STRING("Rock Smash"),
};

static const u8 sTimeLetters[TIMES_OF_DAY_COUNT] = { CHAR_M, CHAR_D, CHAR_E, CHAR_N };

static const u8 sText_NoAreas[] = _("No areas with wild Pokémon\nvisited yet.");
static const u8 sText_Unknown[] = _("?????");

static const struct WindowTemplate sWinTemplates[] =
{
    [WIN_HEADER] = { .bg = 1, .tilemapLeft = 2, .tilemapTop = 1, .width = 26, .height = 2, .paletteNum = 1, .baseBlock = 2 },
    [WIN_LIST] = { .bg = 1, .tilemapLeft = 2, .tilemapTop = 5, .width = 26, .height = 14, .paletteNum = 1, .baseBlock = 0x36 },
    DUMMY_WIN_TEMPLATE
};

static const struct BgTemplate sBgTemplates[] =
{
    { .bg = 1, .charBaseIndex = 1, .mapBaseIndex = 30, .screenSize = 0, .paletteMode = 0, .priority = 0, .baseTile = 0 },
    { .bg = 0, .charBaseIndex = 1, .mapBaseIndex = 31, .screenSize = 0, .paletteMode = 0, .priority = 1, .baseTile = 0 },
};

static const u16 sBg_Pal[] = {RGB(17, 18, 31)};
static const u16 sText_Pal[] = INCGFX_U16("graphics/interface/option_menu_text.pal", ".gbapal");

// option_menu_text.pal: 2/3 orange, 4/5 red, 6/7 dark/light gray
static const u8 sColors_Normal[] = {TEXT_COLOR_TRANSPARENT, TEXT_COLOR_GREEN, TEXT_COLOR_LIGHT_GREEN};
static const u8 sColors_Header[] = {TEXT_COLOR_TRANSPARENT, TEXT_COLOR_DARK_GRAY, TEXT_COLOR_LIGHT_GRAY};
static const u8 sColors_Unknown[] = {TEXT_COLOR_TRANSPARENT, TEXT_COLOR_LIGHT_GREEN, TEXT_COLOR_TRANSPARENT};
static const u8 sColors_Accent[] = {TEXT_COLOR_TRANSPARENT, TEXT_COLOR_RED, TEXT_COLOR_LIGHT_RED};

static void Task_EncDexFadeIn(u8 taskId);
static void Task_EncDexInput(u8 taskId);
static void Task_EncDexFadeOut(u8 taskId);

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

static void BuildAreaList(void)
{
    u32 mapsec, current = gMapHeader.regionMapSectionId;
    sEncDex->areaCount = 0;
    sEncDex->areaIndex = 0;
    for (mapsec = 0; mapsec < MAPSEC_NONE; mapsec++)
    {
        if (!IsEncounterAreaVisited(mapsec) || !AreaHasWildEncounters(mapsec))
            continue;
        if (mapsec == current)
            sEncDex->areaIndex = sEncDex->areaCount; // open on the area the player is in
        sEncDex->areas[sEncDex->areaCount++] = mapsec;
    }
}

static void LoadArea(void)
{
    u32 i, lastMethod = ENCDEX_METHOD_COUNT;
    sEncDex->entryCount = GetEncounterAreaEntries(sEncDex->areas[sEncDex->areaIndex], sEncDex->entries);
    sEncDex->lineCount = 0;
    sEncDex->scroll = 0;
    for (i = 0; i < sEncDex->entryCount; i++)
    {
        if (sEncDex->entries[i].method != lastMethod)
        {
            lastMethod = sEncDex->entries[i].method;
            sEncDex->lines[sEncDex->lineCount++] = (struct EncounterDexLine){ TRUE, lastMethod };
        }
        sEncDex->lines[sEncDex->lineCount++] = (struct EncounterDexLine){ FALSE, i };
    }
}

static void DrawHeader(void)
{
    u8 buffer[40];
    u8 *str;
    FillWindowPixelBuffer(WIN_HEADER, PIXEL_FILL(1));
    if (sEncDex->areaCount == 0)
    {
        AddTextPrinterParameterized3(WIN_HEADER, FONT_NORMAL, 8, 1, sColors_Header, TEXT_SKIP_DRAW, COMPOUND_STRING("ENCOUNTERS"));
    }
    else
    {
        GetMapName(buffer, sEncDex->areas[sEncDex->areaIndex], 0);
        AddTextPrinterParameterized3(WIN_HEADER, FONT_NORMAL, 8, 1, sColors_Header, TEXT_SKIP_DRAW, buffer);
        str = ConvertIntToDecimalStringN(buffer, sEncDex->areaIndex + 1, STR_CONV_MODE_LEFT_ALIGN, 3);
        *str++ = CHAR_SLASH;
        ConvertIntToDecimalStringN(str, sEncDex->areaCount, STR_CONV_MODE_LEFT_ALIGN, 3);
        AddTextPrinterParameterized3(WIN_HEADER, FONT_SMALL, 26 * 8 - 8 - GetStringWidth(FONT_SMALL, buffer, 0), 3, sColors_Accent, TEXT_SKIP_DRAW, buffer);
    }
    CopyWindowToVram(WIN_HEADER, COPYWIN_FULL);
}

static void DrawLine(u32 lineIndex, u32 y)
{
    const struct EncounterDexLine *line = &sEncDex->lines[lineIndex];
    u8 buffer[32];
    u8 *str;

    if (line->isHeader)
    {
        AddTextPrinterParameterized3(WIN_LIST, FONT_NORMAL, 4, y, sColors_Accent, TEXT_SKIP_DRAW, sMethodNames[line->index]);
        return;
    }
    else
    {
        const struct EncounterDexEntry *entry = &sEncDex->entries[line->index];
        bool32 seen = GetSetPokedexFlag(SpeciesToNationalPokedexNum(entry->species), FLAG_GET_SEEN);
        u32 t;

        AddTextPrinterParameterized3(WIN_LIST, FONT_NORMAL, 12, y, seen ? sColors_Normal : sColors_Unknown, TEXT_SKIP_DRAW,
                                     seen ? GetSpeciesName(entry->species) : sText_Unknown);

        str = StringCopy(buffer, COMPOUND_STRING("Lv"));
        str = ConvertIntToDecimalStringN(str, entry->minLevel, STR_CONV_MODE_LEFT_ALIGN, 3);
        if (entry->maxLevel != entry->minLevel)
        {
            *str++ = CHAR_HYPHEN;
            str = ConvertIntToDecimalStringN(str, entry->maxLevel, STR_CONV_MODE_LEFT_ALIGN, 3);
        }
        AddTextPrinterParameterized3(WIN_LIST, FONT_SMALL, 104, y + 2, sColors_Normal, TEXT_SKIP_DRAW, buffer);

        str = ConvertIntToDecimalStringN(buffer, entry->rate, STR_CONV_MODE_LEFT_ALIGN, 3);
        *str++ = CHAR_PERCENT;
        *str = EOS;
        AddTextPrinterParameterized3(WIN_LIST, FONT_SMALL, 168 - GetStringWidth(FONT_SMALL, buffer, 0), y + 2, sColors_Normal, TEXT_SKIP_DRAW, buffer);

        // Times of day it appears, when it isn't all of them
        if (entry->timeMask != (1 << TIMES_OF_DAY_COUNT) - 1)
        {
            str = buffer;
            for (t = 0; t < TIMES_OF_DAY_COUNT; t++)
                *str++ = (entry->timeMask & (1 << t)) ? sTimeLetters[t] : CHAR_HYPHEN;
            *str = EOS;
            AddTextPrinterParameterized3(WIN_LIST, FONT_SMALL, 176, y + 2, sColors_Accent, TEXT_SKIP_DRAW, buffer);
        }
    }
}

static void DrawList(void)
{
    u32 i;
    FillWindowPixelBuffer(WIN_LIST, PIXEL_FILL(1));
    if (sEncDex->areaCount == 0)
    {
        AddTextPrinterParameterized3(WIN_LIST, FONT_NORMAL, 8, 1, sColors_Normal, TEXT_SKIP_DRAW, sText_NoAreas);
    }
    else
    {
        for (i = 0; i < VISIBLE_LINES && sEncDex->scroll + i < sEncDex->lineCount; i++)
            DrawLine(sEncDex->scroll + i, i * 16 + 1);
        // Legend and controls on the last line
        AddTextPrinterParameterized3(WIN_LIST, FONT_SMALL, 4, VISIBLE_LINES * 16 + 1, sColors_Header, TEXT_SKIP_DRAW,
                                     COMPOUND_STRING("{DPAD_LEFTRIGHT} Area  {DPAD_UPDOWN} Scroll  M D E N: time of day"));
    }
    CopyWindowToVram(WIN_LIST, COPYWIN_FULL);
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

// Opened from the Pokédex list with R; B goes back to the Pokédex.
void CB2_OpenEncounterDex(void)
{
    u32 i;
    switch (gMain.state)
    {
    default:
    case 0:
        SetVBlankCallback(NULL);
        sEncDex = AllocZeroed(sizeof(*sEncDex));
        BuildAreaList();
        if (sEncDex->areaCount != 0)
            LoadArea();
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
        gMain.state++;
        break;
    case 4:
        PutWindowTilemap(WIN_HEADER);
        PutWindowTilemap(WIN_LIST);
        DrawHeader();
        DrawList();
        DrawFrames();
        CreateTask(Task_EncDexFadeIn, 0);
        gMain.state++;
        break;
    case 5:
        BeginNormalPaletteFade(PALETTES_ALL, 0, 16, 0, RGB_BLACK);
        SetVBlankCallback(VBlankCB);
        SetMainCallback2(MainCB2);
        return;
    }
}

static void Task_EncDexFadeIn(u8 taskId)
{
    if (!gPaletteFade.active)
        gTasks[taskId].func = Task_EncDexInput;
}

static void Task_EncDexInput(u8 taskId)
{
    if (JOY_NEW(B_BUTTON))
    {
        PlaySE(SE_PC_OFF);
        BeginNormalPaletteFade(PALETTES_ALL, 0, 0, 16, RGB_BLACK);
        gTasks[taskId].func = Task_EncDexFadeOut;
        return;
    }
    if (sEncDex->areaCount == 0)
        return;

    if (JOY_REPEAT(DPAD_LEFT) || JOY_REPEAT(DPAD_RIGHT) || JOY_NEW(L_BUTTON) || JOY_NEW(R_BUTTON))
    {
        if (sEncDex->areaCount > 1)
        {
            if (JOY_REPEAT(DPAD_RIGHT) || JOY_NEW(R_BUTTON))
                sEncDex->areaIndex = (sEncDex->areaIndex + 1) % sEncDex->areaCount;
            else
                sEncDex->areaIndex = (sEncDex->areaIndex + sEncDex->areaCount - 1) % sEncDex->areaCount;
            PlaySE(SE_SELECT);
            LoadArea();
            DrawHeader();
            DrawList();
        }
    }
    else if (JOY_REPEAT(DPAD_DOWN) && sEncDex->scroll + VISIBLE_LINES < sEncDex->lineCount)
    {
        sEncDex->scroll++;
        DrawList();
    }
    else if (JOY_REPEAT(DPAD_UP) && sEncDex->scroll > 0)
    {
        sEncDex->scroll--;
        DrawList();
    }
}

static void Task_EncDexFadeOut(u8 taskId)
{
    if (!gPaletteFade.active)
    {
        DestroyTask(taskId);
        FreeAllWindowBuffers();
        TRY_FREE_AND_SET_NULL(sEncDex);
        SetMainCallback2(CB2_OpenPokedex);
    }
}
