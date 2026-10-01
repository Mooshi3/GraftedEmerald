#ifndef GUARD_GRAFT_MENU_H
#define GUARD_GRAFT_MENU_H

#include "constants/pokemon.h"

// Graft kinds as GetGraftRejectReason (graft_data.c) takes them
#define GRAFT_MENU_TYPE     0
#define GRAFT_MENU_ABILITY  1
#define GRAFT_MENU_MOVE     2
#define GRAFT_MENU_STAT     3
#define GRAFT_MENU_NATURE   4

extern bool8 gGraftLearnMovePending;

// Opponent's grafts (R in move select)
void ShowEnemyGrafts(enum BattlerId battler);
void EnemyGraftViewerNextPage(struct BoxPokemon *boxMon, u16 species);
void HideEnemyGrafts(void);
bool8 IsEnemyGraftViewerOpen(void);
bool8 IsEnemyGraftViewerOnLastPage(void);

#endif // GUARD_GRAFT_MENU_H
