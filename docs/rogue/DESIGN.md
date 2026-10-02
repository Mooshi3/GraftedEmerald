# Grafted Emerald: Rogue — design proposal (draft for Jacob)

Short runs built around grafting: catch and harvest along the way, build a team out of pieces, beat a
gauntlet of bosses, lose everything when you black out. Each run is different; a run takes about an
hour. Nothing here is decided until Jacob says so. Recommendations are marked **Rec.**

## The run

1. **Start.** Pick a starter from 3 random choices (first stages only, any type, any region). You start
   with a few grafting balls and an empty Graft Bag.
2. **Stages.** A run is 8 stages and a finale. Each stage is a real Hoenn route (or cave) picked from a
   pool for that stage's level band, with its wild Pokémon, a few trainers, and items. You walk it, catch,
   harvest, graft, then go through the exit to the boss.
3. **Bosses.** Each stage ends with a Gym Leader, in a random order, at the level for that stage, with
   their ace (the Tera-style extra Pokémon we already have). The finale is the Elite Four and the
   Champion back to back.
4. **Between stages.** A rest stop: heal, a shop (balls, healing items, grafting items), and **one reward
   from three choices**: e.g. a bundle of Splice Balls, a rare piece (a strong ability or a 130+ stat), a
   new Pokémon, a held item.
5. **Losing.** Blacking out ends the run. **Rec:** no lives or checkpoints in the default mode; an easier
   setting could give one revive per run.

**Why real routes instead of generated maps:** Emerald's maps, encounter tables (the sheet's), trainers and
enemy-graft donor pools all exist and are tested. A generated layout like the Battle Pyramid's floors is
possible later; it's a lot of new code for the first version.

## Grafting in a run

- Everything from Grafted Emerald applies: pieces, the Graft Bag, the Stat Budget (420 + 20 per stage
  cleared works out the same as per badge), enemy grafts, boss aces.
- Grafting balls are the main resource. **Rec:** a few per stage from items and rewards, no unlimited shop
  supply, so every harvest is a choice.
- **Rec:** catching is allowed (team up to 6), and the Splicer is available from the start: a weak catch is
  still worth its pieces.

## What carries over between runs (meta-progression)

Options, roughly from least to most:

- **Nothing.** Pure roguelike.
- **Unlocks only (Rec).** Beating stages unlocks more starter choices, new reward types, harder settings
  (like a "Hard" mode with stronger enemy grafts). Never raw power.
- **One heirloom piece.** After a win (or a loss past stage 4), keep one piece for the next run.

## Settings

The existing new-game settings screen carries over for run settings: Nuzlocke rules, Monotype, Inverse,
Enemy Grafts, Fainting (Release fits a roguelike well), etc. Rogue-only settings would join it (run length,
reward pool, difficulty).

## Technical plan (once the design is settled)

- A Rogue mode flag and a run state in the save (stage, stage order, seed, rewards taken). The save has a
  little room left in SaveBlock1/vars; a run state fits in a few dozen bytes.
- A **run seed**: the stage picks, starters, rewards and boss order come from it, so a run can be shared or
  replayed.
- Stage flow by warps: the route's exit warps to the boss, the boss to the rest stop, the rest stop to the
  next stage. The story's own scripts and flags are bypassed (no Team Aqua, no HMs needed: field moves are
  given or routes are picked to not need them).
- Level scaling: wild and trainer levels set by stage, not by the map (one hook where encounters and trainer
  parties are created).
- Starts as its own branch (`claude/grafted-rogue`); Grafted Emerald fixes get merged in.

## Questions for Jacob

1. Real Hoenn routes as stages (Rec), or generated maps?
2. Run length: 8 bosses + Elite Four (about an hour?), or shorter (4–5 bosses)?
3. What carries over between runs: nothing, unlocks only (Rec), or an heirloom piece?
4. Catching allowed with a 6-mon team (Rec), or harvest-only (your team is just your starter, grafted)?
5. Blacking out ends the run, no exceptions (Rec)?
6. How do you get grafting balls: limited per stage (Rec) or a normal shop?
7. Anything from the main game you specifically want or don't want in Rogue?
