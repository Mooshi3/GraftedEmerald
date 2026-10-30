# Grafted Rogue

A roguelite built on Pokémon Emerald and grafting: fight your way through floors of rooms built
from a seed, break Pokémon down into pieces (a move, an ability, a type, a nature or one base stat),
graft them onto your team, and beat each floor's Gym Leader. Lose your team and the run is over, but
the Battle Points (BP) you earned buy unlocks that help every run after.

## Patching

You need a clean **Pokémon Emerald (USA, Europe)** ROM. Its MD5 is
`605b89b67018abcea91e693a4dd25be3`. Other versions or already-patched ROMs won't work.

1. Open a BPS patcher: [Rom Patcher JS](https://www.marcrobledo.com/RomPatcher.js/) in a browser,
   or [Flips](https://github.com/Alcaro/Flips) on a computer.
2. Pick `GraftedRogue.bps` as the patch and your Emerald ROM as the file to patch.
3. Save the result and play it in any GBA emulator. [mGBA](https://mgba.io/) is recommended.

Don't ask for ROMs; the patch is all that's shared.

## A run

- **The base.** Every game starts in your secret base. Prof. Birch answers questions about how
  everything works, his assistant trades BP for unlocks, and the exit mat starts a run. Unlocks you
  own show up as decorations.
- **Floors and rooms.** A Normal run is four floors and then the Champion. Each floor is a string of
  rooms, and each room has a few doors. The sign over a door tells you what's behind it: wild
  Pokémon, a trainer, an Ace Trainer, items, a shop, a Pokémon Center, or a "?" that could be
  anything. The last door of a floor leads to its boss, a Gym Leader with an ace of their type.
- **Themes.** Rooms are woods, plains, lakes, swamps, beaches, caves, volcanoes or power plants, and
  the wild Pokémon live where they belong. The ground in each doorway shows the next room's theme.
- **Trainers** don't spot you; talk to them to battle. A trainer guarding a room keeps its doors shut
  until you win. Beat a Trainer room's trainer and you pick a graft piece from their team; an Ace
  Trainer gives a better pick and an item too.
- **Catching.** Talk to a wild Pokémon to battle it. A Poké Ball catches it; a grafting ball breaks it
  down into pieces instead (a Patch Ball one, a Graft Ball three, a Splice Ball all of them). Your
  team starts at three slots.
- **Grafting.** Open the Graft Bag, pick a piece and the Pokémon to graft it onto. In a run there's no
  limit.
- **Levels.** Your whole team gets EXP from every battle, and Pokémon that fall behind catch up fast.
  Nobody grows past the floor boss's level until the boss is beaten.
- **Fainting.** A fainted Pokémon stays on your team until you revive it or swap it out for a new
  catch. If your whole team faints, the run is over.
- **Rest stops.** After each boss: a nurse, a small shop, a free prize (pick one of three), and a
  tutor. Paying the tutor money teaches a move once; paying BP lets every Pokémon of that kind
  relearn it in later runs too.
- **Weather.** Some floors have rain, harsh sun, a sandstorm or snow in their outdoor rooms, and
  every battle there starts with it. The rest stop tells you what the next floor holds.

## BP and unlocks

Bosses pay BP, a win pays more, and every species you win with for the first time pays extra.
Spend it in the base:

| Unlock | BP | What it does |
|---|---|---|
| Starter choice | 30 | Choose your starter from three. |
| 4th / 5th / 6th team slot | 40 / 70 / 100 | A bigger team. |
| Ball pouch | 20 | Start with 3 more Poké Balls and 2 more Patch Balls. |
| First aid | 30 | Start with 2 Revives. |
| Last rites I / II / III | 60 / 90 / 120 | Once, twice or three times a floor, break a fainted Pokémon down into pieces. |
| Quick study | 40 | Your team gains half again as much EXP. |
| Good genes / Pedigree | 40 / 80 | More Pokémon have an egg move to relearn. |
| Switch style | 20 | Switch Pokémon when a foe faints. |
| Heirloom | 50 | Start each run with one of three graft pieces. |
| Graft vault | 60 | Put a piece in the vault during a run to start the next run with it. |
| Discount | 40 | Shops sell everything for a quarter less. |
| Harvest bonus | 60 | Pieces broken down from a Pokémon sometimes come with a spare copy. |
| Catch bonus | 50 | A Pokémon you catch sometimes leaves a piece behind too. |
| Favorite | 150 | A team member you name at a Pokémon Center becomes a starter choice in every run. |

## Hard mode, challenges and achievements

- **Hard mode** opens after your first win: eight floors, then the Elite Four and the Champion, with
  tougher trainers and double BP. Some Hard floors are cursed: fog over the door signs, guards in
  every room, tougher trainers, no Poké Balls lying around, or tired Pokémon Centers.
- **Challenges** for one run: Inverse battles (the type chart is flipped) or Monotype (only Pokémon of
  one type can battle). Bosses pay half again in BP.
- **Achievements** pay BP once each: beat your first boss, win with a weak Pokémon, win a Monotype or
  Inverse run, win without a faint in the finale, win with a small team, beat three Ace Trainers in a
  run, give one Pokémon all five kinds of graft, and win with 10 and then 50 different species. Prof.
  Birch keeps the list.

## Credits

- Built on [pokeemerald-expansion](https://github.com/rh-hideout/pokeemerald-expansion) by RHH (Rom
  Hacking Hideout) and its contributors, itself built on the [pret](https://github.com/pret/pokeemerald)
  decompilation of Pokémon Emerald.
- Pokémon is © Nintendo, Creatures Inc. and GAME FREAK inc. This is a free, non-commercial fan
  project and isn't affiliated with or endorsed by them.
