# Grafted Emerald: Rogue — design

Short runs built around grafting: pick doors, catch and harvest what's behind them, build a team out of
pieces, beat the bosses. Pokémon that faint are gone. Decisions are Jacob's (answers of 2026-10-02);
**Open** marks what's still undecided.

## The run

- **Start.** A random starter: a Pokémon that isn't evolved, isn't legendary, with a base stat total of
  320 or less (the first-stage starters are 309–318). With meta-progression on, the first runs hand you
  one; choosing from 3 is an unlock. Team size starts at **3**; more slots are unlocks (meta or mid-run).
- **Floors.** A floor is a chain of small rooms ending in a boss. Every room has 2–3 doors; a symbol over
  each door says what's behind it (Slay the Spire's map, walked instead of clicked):
  - **Wild**: one or two visible wild Pokémon. What's there is what you get.
  - **Trainer**: a trainer you can't walk around.
  - **Item**: item balls (balls, healing items, held items).
  - **Shop**: balls, grafting balls, healing items, revives.
  - **Rest**: heal the team.
  - **?**: any of the above, or an event.
  - **Boss**: the last room of the floor.
- **Bosses.** Gym Leaders from every generation, with their type specialty and a Tera-style ace. The
  finale is an Elite Four and a Champion. **Open:** trainer sprites exist only for Hoenn and Kanto (FRLG);
  leaders from Johto and Gen 4+ need sprites from somewhere (custom or community art), or a stand-in.
- **Length.** Normal mode is short (**Open:** 4 floors + a Champion?). Beating it unlocks Hard mode, the
  full length (8 floors + Elite Four + Champion), like The Binding of Isaac's unlocks.
- **Losing.** A fainted Pokémon dies for good (released after the battle). The run ends when the whole
  team is gone. Ways to soften it, as unlocks or shop items: Revives in the shop, a couple of starting
  Revives, auto-harvest (a fallen Pokémon is broken down into pieces, the existing Auto-splice), or the
  team revives on clearing a floor. **Open:** which of these.

## Grafting in a run

- Everything from Grafted Emerald applies: pieces, the Graft Bag, the Splicer, enemy grafts, boss aces.
- **No Stat Budget** (Graft Limit Free): a lucky run is allowed to be broken. A Shedinja encounter has
  earned its Wonder Guard.
- **Balls.** You start with a few Poké Balls and grafting balls (more as a meta upgrade); shops and item
  rooms have more. Catches can fail, and Patch/Graft Balls don't always give the piece you want. There's
  no grass to grind: a wild door is one or two Pokémon, take it or leave it.

## Meta-progression (optional, a setting)

- Currency: **BP**, earned per floor cleared and per boss.
- Unlocks: choose your starter from 3, more team slots, more starting balls, starting Revives,
  auto-harvest, Hard mode, **pick one piece** to start a run with.

## Not in Rogue

The encounter screen (R in the Pokédex), map-based encounter tables, the Nuzlocke area rule and the
Stat Budget. The settings screen keeps what makes sense for a run (Inverse, Monotype, Enemy Grafts,
Sleep Clause, Smart Wilds, shiny odds…) plus Rogue's own (meta-progression on/off).

## Technical plan

- **Rooms are generated, on one map.** A single room map (like the Battle Pyramid's floor map) is
  rewritten on load from the room's seed: tree border, ground, grass, ponds, flowers, and 2–3 exits on
  its north edge. Its object events (door markers, Pokémon, trainers, items, NPCs) are moved and given
  graphics from C before they spawn. Going through a door warps back to the same map with the next
  room's seed.
- **Run state in the save:** mode, floor, room, seed, team slots, unlocks; meta-progression lives apart
  from the run so a new run clears only the run.
- **Content by floor:** the wild/trainer species pool and levels come from the floor number, not from
  maps. Trainer parties are generated (hooked where `ApplyTrainerGrafts` already runs).
- Branch: `claude/grafted-rogue`; Grafted Emerald fixes get merged in.

## Milestones

1. **Prototype rooms (to show Jacob):** the room generator, doors with symbols, Wild / Trainer / Item /
   Rest / Shop rooms with simple content, a boss room, a random starter, permadeath, run over.
2. Floors and bosses: floor pools and level curve, Gym Leaders by type, the finale, Normal/Hard length.
3. Meta-progression: BP, the unlock shop, the settings.
4. Polish: events for "?" rooms, room layouts, balance.

## Prototype status (2026-10-02)

Milestone 1 is playable: `src/rogue.c`, map `data/maps/Rogue_Room`, tests in `test/rogue.c`.

- A new game starts in the start room (Prof. Birch explains, one door starts a run). A run is 4 floors of
  10 rooms + a boss (Hoenn's 8 Leaders in a per-run shuffled order, a typed party of floor + 2).
- Rooms are generated: forest border, 2–5 inside trees (never closing off a door, checked by a search),
  tall grass for looks, 2–3 doors (one boss door after room 6). Signs: Zigzagoon wild, Youngster trainer,
  item ball, clerk shop, nurse rest, Unown "?" mystery, the Leader for the boss.
- Wild rooms: 1–2 visible Pokémon from the floor's pool (floor 1: unevolved, BST ≤ 330; then wider);
  talk to one to battle it; it's gone afterwards either way. Levels: 4 + 9 per floor + room number.
- Trainer rooms: a generated party (name, class and picture borrowed from a vanilla trainer), sight 5,
  doors locked until beaten. Enemy grafts draw from the floor's species.
- The run forces Graft Limit Free, Release on faint, no level cap, harvests Any; shops have no badge
  gates. Whiteout ends the run. Team cap 3 (only grafting balls work with a full team).
- Debug: the debug menu's Scripts list jumps to a room type (`rogue_room` in tools/smoke/lib.sh;
  `tools/smoke/scenarios/rogue.sh` starts a run).

- Catching past the team cap asks who to let go after the battle.
- Bosses bring their Leader's ace. After the last floor: the Champion (Normal, 4 floors) or the Elite
  Four and the Champion (Hard, 8 floors). Beating a Normal run opens Hard mode.
- Meta-progression (the **Unlocks** new-game setting, On by default): bosses pay BP (5 a floor, 10 in the
  finale, +10 for the win, doubled on Hard). A scientist in the start room sells unlocks: Starter choice
  (30), 4th/5th/6th team slot (40/70/100), Ball pouch (+3 Poké, +2 Patch Balls, 20), First aid (2 Revives,
  30), Last rites I/II/III (60/90/120), Heirloom (start each run with one of three pieces: a strong ability,
  a 110–140 stat, a type, 50). With Unlocks Off, all of them and Hard mode are open from the start
  (confirmed by Jacob).
- Fainting (decided with Jacob 2026-10-02): nothing is lost on its own. After a battle, each Pokémon that
  fainted asks: Keep it (stays on the team, fainted, until a Revive; asked again only if it faints again
  after being revived), Let it go, or Last rites (broken down into graft pieces). Last rites I/II/III
  give 1/2/3 uses a floor, so grafting balls and the Splicer stay useful and killing your own Pokémon for
  pieces stays a limited strategy. Per floor or per run is still open (one constant to change).

- "?" doors: 35% an event (a stranger who gives an ability and a stat piece, one rare Pokémon from two
  floors ahead's pool, or a merchant with Splice Balls, Rare Candy, held items), else a random room.

Not done yet: bosses from other generations (sprites, waiting on assets), more events.
- No settings screen (Jacob 2026-10-02). A new save's start room asks once: meta-progression yes/no
  (no = everything unlocked). A run sets the rest: shiny odds 1/512, EXP Share Item (Party with the Team
  training unlock), Enemy Grafts Normal (More on Hard), Wild Grafts Off, battle items allowed, Set style
  (Shift with the Switch style unlock), no Sleep Clause, Smart Wilds on Hard only.
- Challenges, picked at the start door for one run: Inverse battles or Monotype (choose the type; the
  starter gets it grafted). Bosses pay half again in BP.

### Balance pass (2026-10-02, after Jacob's first run: the floor-1 boss was out of reach)

- Floors are 10 rooms. Levels climb slowly: wilds 3 to 9 on floor 1 (+8 a floor), trainers one above,
  the boss 2 above the last room (floor 1: level 11) with floor + 1 Pokémon plus its ace.
- Trainers carry 1 Pokémon early (2 in a floor's second half), more on later floors.
- Guarded rooms: trainer and boss rooms, and half the wild and item rooms, have a trainer; small trees
  block every door until the trainer is beaten (interacting says so). So a floor has several fights.

### Room themes (2026-10-02)

- Four themes: **forest**, **beach** (sand and a strip of sea along one side, few trees), **cave** (Granite
  Cave walls, rocks, cave mouths as doors) and **volcano** (Fiery Path walls, corridors as doors). Each floor
  has a home theme (floor 1 forest, 2 beach, 3 cave, 4 volcano, then around again) that a door's room takes
  55% of the time, 15% for each other theme.
- A door's opening shows the next room's theme: sand, cave rock, volcanic ground or grass in front of it.
- Wild Pokémon live where Jacob's encounter sheet puts their family (2026-10-03, replacing type pools:
  a fighting-type panda isn't a beach Pokémon). Each Hoenn map with encounters has a theme: caves (Granite,
  Meteor Falls, Victory Road, Seafloor, Shoal, Mt. Pyre, Sky Pillar, Mirage Tower, New Mauville…), volcano
  (Fiery Path, Jagged Pass, Mt. Chimney, Magma Hideout, Routes 111-113), forest (the other land routes);
  everything surfed or fished is a beach Pokémon, Rock Smash ones cave Pokémon. A family counts with all
  its evolutions. `sHabitatExtras` places families by hand where the maps don't say it (Clobbopus: beach).
- Trainers and bosses are talk-to-battle (no line of sight); their room's doors stay locked until beaten.
- Shops and rests are a Poké Mart or a Pokémon Center standing in a forest or beach room; walk in to buy or
  heal, walk out to the same room.
- Bosses fight in an Elite Four room that fits their type (Sidney's, Phoebe's, Glacia's or Drake's); the
  door behind them opens once they're beaten, the only way on to the next floor.
- Technical: the theme is `VAR_ROGUE_ROOM_TYPE` bits 9-11. Forest and beach are `Rogue_Room`; caves and
  volcanoes have their own maps (`Rogue_Cave`, `Rogue_Volcano`) with the same size, inside and door columns;
  `Rogue_WarpToRoom` picks the map. Debug menu > Scripts has Beach / Cave / Volcano jumps.
  `tools/rogue/render.py` draws a layout or a tileset sheet with metatile ids, for finding tiles.

### Favorite (2026-10-03)

- Unlock **Favorite** (150 BP), Rogue Legacy style: a Pokéfan in the run's Pokémon Centers makes one of
  your team your favorite (any time, replacing the old one). From then on every run offers its first stage
  (a Garchomp makes Gible a choice) next to the random starter(s), at level 5 with nothing grafted.
  Stored in `VAR_ROGUE_FAVORITE`; `VAR_ROGUE_UNLOCKS` has no free bits left (14 is Favorite, 15 Hard mode).

### Egg moves and the relearner (2026-10-03)

- A rolled egg move (10%, 35% with Good genes, one or two with Pedigree) is known from the start: wild and
  rare Pokémon (they can use it against you) and starters learn it in a free slot, else in place of their
  first move. Forgotten, it stays relearnable.
- No tutor list in the relearner (both games): it taught every tutor move for free. The TM list offers the
  TMs in the Bag. The relearner's title names the list shown (SELECT switches lists).

### Rest stop and the second unlock page (2026-10-03)

- After each floor's boss the arena's door leads to a rest stop (a Pokémon Center room, `Rogue_Stop`) before
  the next floor (or, after the last floor, before the finale): a nurse, a mart with basics (balls, potions,
  status cures; the good stock stays in shop rooms), a free prize and a move tutor. Its exit mat leads on.
- Prize: one of three offers from three different shelves (a TM, an evolution item, a held battle item, a
  Mega Stone), once per rest stop. The first Mega Stone comes with a Mega Ring.
- Tutor: up to three random tutor moves someone on the team can learn. ¥1500 teaches it once; 10 BP (with
  meta-progression) also lets that Pokémon's whole family relearn it in later runs (the relearner's TUTOR
  MOVES list in a run). Up to 295 such moves are kept, in the Battle Tower's unused saved record-mixing opponents
  (`frontier.towerRecords`; the first builds used the Battle Dome's data, moved over on first use); the oldest goes when full.
- Prices in a run: nothing costs more than ¥3000 (Rare Candy ¥1000), and selling goes by the same worth (`Rogue_GetItemWorth`), so held battle items (up to ¥100,000 in the main game) are buyable and prizes can't be sold for a fortune. A run's marts list their own stock: the main game's graft items (Graft Knot: no breeding in a run) aren't appended.
- Second unlock page (`VAR_ROGUE_UNLOCKS_2`, ids 16+): Discount (40 BP: shops a quarter off), Harvest bonus
  (60 BP: each piece broken down from a Pokémon has a 25% chance of a spare copy; balls, the Splicer, last
  rites), Catch bonus (50 BP: a real catch has a 25% chance of leaving one random piece).
- Normal mode getting easy with everything unlocked is fine (Jacob): it's the training wheels for Hard mode,
  which will scale up.

### Species won with (2026-10-03)

- Every species on the team when the Champion falls is recorded (as it is then: a Garchomp marks Garchomp;
  forms count as their base species), one bit per National Dex number in the Battle Tower's e-Reader
  trainer slot (`frontier.ereaderTrainer`, unused in Rogue). Each species won with for the first time pays
  3 BP more. A bonus, not a penalty for reusing one (that would fight the Favorite unlock).
- For later: a start room count, marks in the Pokédex, milestone rewards, "win with every starter" goals.
