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
