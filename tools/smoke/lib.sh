# Step fragments for smoke scenarios. Source it: . "$(dirname "$0")/../lib.sh"
# Each function prints a command string for run.py. Timings are ones that worked; see README.
SMOKE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
RUN="$SMOKE/run.py"
N() { python3 "$SMOKE/num.py" "$1" "$2"; }                    # debug number input: from $1 to $2
R() { for i in $(seq "$1"); do echo -n "press $2;wait 4;"; done; } # press key $2, $1 times

# Title screen -> New Game -> Birch intro -> GAME SETTINGS screen (cursor on row 0)
intro() { echo -n "wait 900;press START;wait 120;press A;wait 200;$(for i in $(seq 35); do echo -n 'mash A 15 8;'; done)wait 60;"; }

# On the settings screen (main game only: the Rogue branch has none): settings "ROW:RIGHTS ...", then start the game.
# Rows (Rogue branch): 0 unlocks, 1 shiny, 2 exp share, 3 enemy grafts, 4 wild grafts, 5 battle items,
# 6 force set, 7 monotype, 8 sleep clause, 9 inverse, 10 smart wilds. (The main game's scenarios use the
# full list; they don't apply on this branch.)
settings() {
    local row=0 r n
    for pair in "$@"; do
        r=${pair%%:*}; n=${pair##*:}
        R $((r - row)) DOWN; R "$n" RIGHT; row=$r
    done
    echo -n "wait 10;shot settings.png;press START;wait 5;press A;wait 400;"
}

# Moving truck until the player can move (inside the truck, before the house)
truck() { echo -n "wait 600;mash A 40 20;wait 200;mash B 6 20;"; }

# Debug menu (R+START) -> Give X -> Pokemon (Basic). The Give menu's cursor returns to row 0 after each gift.
give_mon() { echo -n "press R+START 3;wait 30;$(R 3 DOWN)press A;wait 20;press DOWN;wait 4;press A;wait 20;$(N 1 "$1");press A;wait 20;$(N 1 "$2");press A;wait 60;press B;wait 20;press B;wait 30;"; }
# Debug -> Give X -> Give item: give_item ITEM_ID QTY
give_item() { echo -n "press R+START 3;wait 30;$(R 3 DOWN)press A;wait 20;press A;wait 20;$(N 1 "$1");press A;wait 20;$(N 1 "$2");press A;wait 60;press B;wait 20;press B;wait 30;"; }
# Debug -> Give X -> Max Battle Points
give_bp() { echo -n "press R+START 3;wait 30;$(R 3 DOWN)press A;wait 20;$(R 7 DOWN)press A;wait 60;press B;wait 20;press B;wait 30;"; }
# Debug -> Utilities -> Warp to map warp: warp GROUP NUM WARPID (see include/constants/map_groups.h)
warp() { echo -n "press R+START 3;wait 30;press A;wait 20;press DOWN;wait 3;press A;wait 20;$(N 0 "$1");press A;wait 20;$(N 0 "$2");press A;wait 20;$(N 0 "$3");press A;wait 300;"; }
# Debug -> Trainers -> set Trainer 1 and Try Battle: trainer_battle TRAINER_ID (include/constants/opponents.h).
# Enemy grafts roll from the map the player is on, as in a real battle (warp to the trainer's map first).
trainer_battle() { echo -n "press R+START 3;wait 30;$(R 6 DOWN)press A;wait 20;press DOWN;wait 3;press A;wait 20;$(N 0 "$1");press A;wait 20;$(R 5 DOWN)press A;wait 400;mash B 8 30;"; }
# Debug -> Flags & Vars toggles. Debug gifts don't set story flags; Nuzlocke rules start with the Pokedex.
flag_toggle() { echo -n "press R+START 3;wait 30;$(R 8 DOWN)press A;wait 20;$(R "$1" DOWN)press A;wait 30;press B;wait 20;press B;wait 30;"; }
dex_on() { flag_toggle 4; }
all_badges_on() { flag_toggle 10; }
# Debug -> Flags & Vars -> Set Flag XYZ: toggle_flag FLAG_ID (decimal; see include/constants/flags.h). It toggles.
toggle_flag() { echo -n "press R+START 3;wait 30;$(R 8 DOWN)press A;wait 20;press A;wait 20;$(N 1 "$1");press A;wait 30;press B;wait 20;press B;wait 20;press B;wait 30;"; }
# Debug gifts don't set FLAG_SYS_POKEMON_GET (0x860), so START has no POKEMON row until this runs
pokemon_menu_on() { toggle_flag 2144; }
# Rogue (branch claude/grafted-rogue): Debug -> Scripts -> slot N (0-based). Entering a room can show a message
# ("FLOOR 2", a boss); dismiss it (press A) before opening the debug menu again. 0 wild, 1 trainer, 2 items,
# 3 shop, 4 rest, 5 boss, 6 the last room before the boss, 7 next floor, 8 event, 9 beach, 10 cave,
# 11 volcano, 12 rest stop, 13 power plant, 14 plains, 15 lake, 16 swamp (0 is a woods room). Only in a run.
rogue_room() { echo -n "press R+START 3;wait 30;$(R 5 DOWN)press A;wait 20;$(R "$1" DOWN)press A;wait 300;"; }
