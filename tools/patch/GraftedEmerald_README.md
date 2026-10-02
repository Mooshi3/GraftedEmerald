# Grafted Emerald

Pokémon Emerald with grafting: break wild Pokémon down into pieces (a move, an ability, a type, a
nature, or one base stat) and graft them onto your own team. Any Pokémon can be made viable.

Built on pokeemerald-expansion. Wild Pokémon from every region live in Hoenn, the trainers' Pokémon
are grafted too, and a set of new-game settings turns the game into anything from a relaxed run to a
Nuzlocke.

## Patching

You need a clean **Pokémon Emerald (USA, Europe)** ROM. Its MD5 is
`605b89b67018abcea91e693a4dd25be3`. Other versions or already-patched ROMs won't work.

1. Open a BPS patcher: [Flips](https://github.com/Alcaro/Flips) on a computer, or
   [Rom Patcher JS](https://www.marcrobledo.com/RomPatcher.js/) in a browser.
2. Pick `GraftedEmerald.bps` as the patch and your Emerald ROM as the file to patch.
3. Save the result and play it in any GBA emulator (mGBA is recommended).

Don't ask for ROMs; the patch is all that's shared.

## Grafting in short

- **Pieces.** Throw a grafting ball at a wild Pokémon instead of catching it. A **Patch Ball** gives
  one random piece, a **Graft Ball** three, a **Splice Ball** all of them. The Pokémon isn't kept.
  The **Graft Splicer** breaks down one of your own Pokémon into all its pieces (it's gone for good).
- **The Graft Bag** (a key item; you get it with your first pieces) holds them, a tab per kind.
  Pick a piece and a Pokémon to graft it on. Pieces stack up to 15 of each.
- **Stat Budget.** A stat piece sets one base stat. A Pokémon's base stat total can be grafted up to
  420, plus 20 per badge, and 600 as Champion. It's the same for every species, so a weak Pokémon has
  room to grow and a strong one has to trade. Lowering a stat is always allowed and frees room.
  Mega Evolution adds its boosts on top.
- **Shops.** Every Poké Mart that sells Poké Balls sells the grafting items too. The **Ungrafter**
  removes all of a Pokémon's grafts, the **Graft Knot** passes grafts on through the Day Care, and the
  **Ability Capsule** (5 badges) picks any of a species' abilities, hidden ones included.
- **In battle**, R in the move menu shows the opponent's grafts and L shows move details. Type
  icons next to the HP bars show grafted types.
- **The Pokédex** shows base stats, abilities and learnsets for anything you've seen. R in its list
  opens the area screen: every area you've visited and what lives there, by time of day.
- **Mega Evolution.** The old man at the top of Mt. Pyre has the Mega Ring for you after Team Aqua's
  visit. Mega Stones are sold in Lilycove Department Store once you have it.

## New-game settings

Picked once, after Professor Birch's introduction. They can't be changed later.

| Setting | Choices (default first) | What it does |
|---|---|---|
| Shiny Odds | 1/512, 1/4096, 1/128 | How often wild Pokémon are shiny. |
| Level Cap | Off, Hard, Soft | Capped at the next Gym Leader's strongest Pokémon. Hard stops EXP; Soft cuts it sharply and Pokémon over the cap may disobey. |
| EXP Share | Item, Party | Item is the classic held item; Party gives EXP to the whole party every battle. |
| Graft Limit | Stat Budget, Free, Budget, Strict | Stat Budget is described above. Budget and Strict also limit how many grafts a Pokémon can have (moves don't count). Free has no limits. |
| Graft Gates | Off, On | Types need 1 badge to graft, stats 2, abilities 3. |
| Graft Items | Shops, Rations | Rations: grafting balls aren't sold; you start with 3 Splice Balls and get more with each badge. |
| Harvests | Any, 1 per area, 1 per Pokémon, None | How many wild Pokémon you can break down in each area. |
| Enemy Grafts | Normal, Fewer, More | How many grafts trainers' Pokémon get. Gym Leaders and the Elite Four bring an extra Pokémon carrying their type. |
| Wild Grafts | Off, Rare, Common | Wild Pokémon that already have grafts. |
| National Dex | On, Off, Postgame | Off keeps Emerald's original wild Pokémon. On adds Pokémon from every region, and legendaries after the League. |
| Nuzlocke | Off, On | Only the first wild Pokémon you meet in each area can be caught. With Dupes Clause and Shiny Clause. |
| Fainting | Normal, Release, Graveyard, Auto-splice | What happens to fainted Pokémon after a battle: nothing, released, kept but unusable, or broken down into pieces. |
| Battle Items | Allowed, Banned | Banned: only Poké Balls can be used in battle. |
| Poké Centers | Open, Closed | Closed: nurses won't heal. Other healing still works. |
| Force Set | Off, On | No free switch when a foe faints. |
| Monotype | Off, any type | Only Pokémon of that type, natural or grafted, can battle. Your starter gets the type grafted on. |
| Sleep Clause | Off, On | Only one Pokémon per side can be asleep. |
| Inverse Battles | Off, On | The type chart is flipped. |
| Double Wilds | Off, Sometimes, Often | Two wild Pokémon at once. Never with Nuzlocke. |
| Smart Wilds | Off, On | Wild Pokémon choose moves like trainers. |
| Wild Pokémon | Hidden, Visible, Both | Visible: wild Pokémon walk around in the field instead of hiding in the grass. Always Hidden with Nuzlocke. |

## Credits

- Built on [pokeemerald-expansion](https://github.com/rh-hideout/pokeemerald-expansion) by the
  RHH team and contributors, itself built on the [pret](https://github.com/pret/pokeemerald)
  decompilation of Pokémon Emerald.
- Pokémon is © Nintendo, Creatures Inc. and GAME FREAK inc. This is a free fan project.
