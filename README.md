# Grafted Emerald

Two Pokémon Emerald hacks built around **grafting**: break Pokémon down into pieces (a move, an
ability, a type, a nature or one base stat) and graft those pieces onto your own team. Any Pokémon
can be made viable.

- **Grafted Emerald** is the full Hoenn adventure with grafting, Pokémon from every region in the
  wild, grafted trainers, and new-game settings that range from a relaxed run to a Nuzlocke.
- **Grafted Rogue** is a roguelite: floors of rooms built from a seed, a boss at the end of each
  floor, a new starter every run, and unlocks that carry over between runs.

## Download

Get the patches from the [Releases](https://github.com/Mooshi3/GraftedEmerald/releases) page:

| Patch | Game |
|---|---|
| `GraftedEmerald.bps` | Grafted Emerald |
| `GraftedRogue.bps` | Grafted Rogue |

Each release has notes on how the game plays:
[Grafted Emerald](tools/patch/GraftedEmerald_README.md) ·
[Grafted Rogue](https://github.com/Mooshi3/GraftedEmerald/blob/rogue/tools/patch/GraftedRogue_README.md).

## Patching

You need a clean **Pokémon Emerald (USA, Europe)** ROM. Its MD5 is
`605b89b67018abcea91e693a4dd25be3`. Other versions or already-patched ROMs won't work.

1. Open a BPS patcher: [Rom Patcher JS](https://www.marcrobledo.com/RomPatcher.js/) in a browser,
   or [Flips](https://github.com/Alcaro/Flips) on a computer.
2. Pick the `.bps` patch and your Emerald ROM.
3. Save the result and play it in any GBA emulator. [mGBA](https://mgba.io/) is recommended.

Both patches apply to the same clean ROM, so you can have both games. Don't ask for ROMs: only the
patches are shared here.

## Grafting in short

- **Pieces.** Throw a grafting ball at a wild Pokémon instead of catching it. A Patch Ball gives one
  random piece, a Graft Ball three, a Splice Ball all of them. The Graft Splicer breaks down one of
  your own Pokémon into all its pieces.
- **The Graft Bag** holds the pieces, a tab per kind. Pick a piece, then a Pokémon to graft it onto.
- **Stat Budget.** A stat piece sets one base stat, up to a base stat total that's the same for every
  species. A weak Pokémon has room to grow; a strong one has to trade.
- **Grafted types** recolor the Pokémon's sprite, and in battle R shows the opponent's grafts.

## Source code

This repository is the source, built on
[pokeemerald-expansion](https://github.com/rh-hideout/pokeemerald-expansion) 1.17.1.

| Branch | Game |
|---|---|
| `main` | Grafted Emerald |
| `rogue` | Grafted Rogue |

To build it yourself, set up the toolchain as in [docs/INSTALL.md](docs/INSTALL.md), then run
`make` (or `make release`). `tools/patch/make_patch.sh` builds the release ROM and its patch from
your own clean Emerald ROM. Developer notes live in [CLAUDE.md](CLAUDE.md) and
[docs/rogue/DESIGN.md](https://github.com/Mooshi3/GraftedEmerald/blob/rogue/docs/rogue/DESIGN.md); the expansion's own README is in
[docs/EXPANSION_README.md](docs/EXPANSION_README.md).

## Credits

- Built on [pokeemerald-expansion](https://github.com/rh-hideout/pokeemerald-expansion) by RHH (Rom
  Hacking Hideout) and its contributors, itself built on [pret's pokeemerald](https://github.com/pret/pokeemerald)
  decompilation. See [CREDITS.md](CREDITS.md).
- Grafted Emerald and Grafted Rogue by Mooshi.
- Pokémon is © Nintendo, Creatures Inc. and GAME FREAK inc. This is a free, non-commercial fan
  project and isn't affiliated with or endorsed by them.
