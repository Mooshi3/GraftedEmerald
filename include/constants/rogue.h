#ifndef GUARD_CONSTANTS_ROGUE_H
#define GUARD_CONSTANTS_ROGUE_H

// VAR_ROGUE_STATE
#define ROGUE_STATE_HUB      0 // between runs: the start room
#define ROGUE_STATE_RUN      1
#define ROGUE_STATE_RUN_LOST 2 // set on whiteout; the start room shows the message, then HUB
#define ROGUE_STATE_RUN_WON  3

// Room types (VAR_ROGUE_ROOM_TYPE, and what a door leads to)
#define ROGUE_ROOM_HUB     0
#define ROGUE_ROOM_WILD    1
#define ROGUE_ROOM_TRAINER 2
#define ROGUE_ROOM_ITEM    3
#define ROGUE_ROOM_SHOP    4
#define ROGUE_ROOM_REST    5
#define ROGUE_ROOM_MYSTERY 6 // a "?" door: any of the above, decided when you go through
#define ROGUE_ROOM_BOSS    7
#define ROGUE_ROOM_START   8 // the start room's door: begins a run
#define ROGUE_ROOM_EVENT   9 // only behind "?" doors: a gift, a rare Pokémon or a merchant
#define ROGUE_ROOM_TYPE_COUNT 10

// Rogue_TakeDoor's VAR_RESULT
#define ROGUE_DOOR_NEXT_ROOM   0
#define ROGUE_DOOR_STARTED_RUN 1 // STR_VAR_1 holds the starter's name
#define ROGUE_DOOR_WON_RUN     2
#define ROGUE_DOOR_ASK_MODE    3 // Hard mode is open: ask Normal or Hard (VAR_0x8005), then take the door again
#define ROGUE_DOOR_CHOOSE_STARTER 4 // the run started; the player picks the starter

// Where the player arrives in every room
#define ROGUE_ENTRANCE_X 10
#define ROGUE_ENTRANCE_Y 13

#endif // GUARD_CONSTANTS_ROGUE_H
