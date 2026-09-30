#ifndef GUARD_GRAFT_ENCOUNTER_DEX_H
#define GUARD_GRAFT_ENCOUNTER_DEX_H

// Pokédex encounter screen: every area the player has visited and the wild Pokémon found there.

enum EncounterDexMethod
{
    ENCDEX_GRASS,
    ENCDEX_SURF,
    ENCDEX_OLD_ROD,
    ENCDEX_GOOD_ROD,
    ENCDEX_SUPER_ROD,
    ENCDEX_ROCK_SMASH,
    ENCDEX_METHOD_COUNT,
};

struct EncounterDexEntry
{
    u16 species;
    u8 method;
    u8 minLevel;
    u8 maxLevel;
    u8 rate;     // best chance per encounter of this method, over the area's maps and times
    u8 timeMask; // 1 << TimeOfDay for each time it appears
};

#define ENCDEX_MAX_ENTRIES 64

void MarkEncounterAreaVisited(u32 mapsec);
bool32 IsEncounterAreaVisited(u32 mapsec);
bool32 AreaHasWildEncounters(u32 mapsec);
u32 GetEncounterAreaEntries(u32 mapsec, struct EncounterDexEntry *entries);
void CB2_OpenEncounterDex(void);

#endif // GUARD_GRAFT_ENCOUNTER_DEX_H
