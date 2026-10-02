# Smoke tests

Scripted play of the real ROM in a headless mGBA, with screenshots. The unit tests (`make check`) cover data and rules; this covers menus, scripts and flows that only exist on screen.

## Setup

```
make                      # builds pokeemerald.gba at the repo root
tools/smoke/build.sh      # first time only: clones mGBA at a pinned commit and builds tools/smoke/shot
```

Screenshots need Pillow for `sheet.py` (`pip install pillow`).

## Running steps

`run.py FROM TO "commands"` runs one step. `FROM` is a save state in `out/` (or `-` to boot fresh) and `TO` is the state saved at the end, so steps chain:

```
cd tools/smoke
scenarios/wild.sh                                  # boot -> new game -> setup -> out/wild.ss
./run.py wild.ss w2.ss "press UP 48;wait 10;shot a.png"
./run.py w2.ss w3.ss "press A;wait 60;shot b.png"
python3 sheet.py out/sheet.png out/a.png out/b.png  # one image to look at instead of many
```

Commands, separated by `;`:

| command | effect |
|---|---|
| `wait N` | N frames, no input (60 frames = 1 second) |
| `press KEYS [N]` | hold for N frames (default 2). Keys: `A B START SELECT UP DOWN LEFT RIGHT L R`, combine with `+` |
| `mash KEYS COUNT GAP` | press COUNT times, GAP frames apart |
| `shot NAME.png` | screenshot into `out/` |
| `reset` | soft reset; the battery save stays. With `SMOKE_ROM` pointing at another build, a state made with one ROM boots the other on its save (save-compatibility checks) |
| `heap ADDR` | print the game's heap: each allocated block with its size and the `file:line` that allocated it, then totals. ADDR is `gHeap`'s address in hex, from `grep " gHeap$" pokeemerald.map` (drop the `0x`). |

Walking one tile is `press DIR 16`. Pressing a direction for 2-4 frames only turns the player.

`lib.sh` has step builders that print command strings: `intro`, `settings ROW:RIGHTS ...`, `truck`, `give_mon SPECIES LEVEL`, `give_item ITEM QTY`, `give_bp`, `warp GROUP NUM WARP`, `trainer_battle TRAINER_ID`, `dex_on`, `all_badges_on`, `toggle_flag ID`, `pokemon_menu_on` (debug gifts don't set the flag that shows POKéMON in the start menu; don't use Utilities > Cheat start, it replaces the party), and `N FROM TO` for the debug menu's number inputs. Scenario scripts in `scenarios/` combine them; read one before writing a new one.

## Scenarios

Each ends in a documented spot; see the comment at the top of each script.

- `autosplice.sh`: Rations, Auto-splice and Monotype Fire; a trainer battle where the lead will faint.
- `graftbag.sh`: default settings, Sandygast and Splice Balls; one wild mon broken down into pieces; ends on the Graft Bag screen.
- `hyper.sh`: next to the hyper trainer at the Battle Frontier, with caps and BP.
- `splicer.sh`: two mons and two Graft Splicers, start menu open.
- `wild.sh`: Nuzlocke, 1 harvest per area and Graveyard fainting, in Petalburg Woods with Poké Balls and Graft Balls.

## Timing: what goes wrong

Most failures are inputs sent at the wrong moment. Before assuming a game bug, screenshot and check where you are.

- **Text eats input.** A press while text is printing either skips to the end of the text or does nothing. Before a menu choice, wait for the text to finish (80-150 frames for a two-line box) or take a shot to check.
- **A Yes/No box appears only after its text finishes.** Pressing DOWN too early lands on the wrong thing, and the next A picks YES.
- **Menus remember their cursor.** The Bag remembers its pocket and item. The battle move menu remembers the last move. Check with a shot before pressing A.
- **Debug menu cursors:** the Give menu returns to row 0 after each gift. The Trainers menu cursor is on row 0 after setting Trainer 1; "Try Battle" is 5 rows down (4 rows down toggles Double Battle).
- **Save states break when the ROM changes.** After `make`, a state from the old ROM loads garbled graphics or crashes. Rerun the scenario from `-`.
- **Obedience:** traded mons above the badge level disobey. Debug gifts are the player's own, so they obey, except under the Soft level cap, where mons above the cap may disobey.
- **Wild encounters are deterministic.** The same inputs from the same state give the same encounter, so a step that found a battle once will find it again.
- **Out of memory / leaks:** the `malloc.c` crash screen means the heap ran out. Take `heap` readings before and after the suspect step; a menu that returns to battle should leave the totals where they were. A battle normally uses about 89 KB of the 115 KB heap.
- **Emulator log:** `SHOTLOG=1 ./run.py ...` prints mGBA's log. A crash shows up as the game restarting at the copyright screen.

## Reporting

Report per flow: what you did, what you expected, what you saw, and the screenshot names in `out/` for anything wrong. A single contact sheet (`sheet.py`) of the frames that matter is better than many files. Separate "I couldn't get there" (an input or timing problem) from "the game did something wrong".
