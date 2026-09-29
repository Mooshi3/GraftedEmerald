# Grafted Emerald

Pokémon Emerald hack on **pokeemerald-expansion**. Jacob sets the vision and how it should play; Claude is lead dev and owns implementation. Grafted Emerald is the graft system on a vanilla+ Emerald, as a proof of concept. A roguelite version, and possibly Grafted Platinum/FireRed, come later.

Goal right now: a clean front-to-back playthrough that's publishable.

## Working style

- Plain, direct explanations. Flag tradeoffs and limits honestly.
- Prefer simple, centralized systems over per-map or per-call-site edits.
- The game is a sandbox: "make any Pokémon viable". Don't hard-gate strategies by default. Restrictions belong in optional new-game challenge settings.
- Build (`make`) and run the graft tests before calling something done: `make check TESTS="test/graft.c"` (filter by file path, not name). `test/save.c` pins the save struct sizes, so a save layout change fails a test on purpose.
- Where code and docs disagree, trust the code and fix the doc.

## Repo

- `origin` = Jacob's GitHub repo; `upstream` = rh-hideout/pokeemerald-expansion. Grafted branched from upstream around 2026-05-18 (commit `21c2420299`). Plan: merge the latest upstream release after the known-bug baseline is green.
- Toolchain: `arm-none-eabi-gcc` + `make`. Build output is gitignored.

## Grafting

Five graft kinds: **move, ability, type, stat, nature**.

Sources:
- **Balls** consume the caught wild mon and offer graft categories: Patch Ball (1 random category), Graft Ball (3), Splice Ball (all 5). A wild mon offers its actual ability (never its hidden one), the 4 moves it currently knows, its species' base stats, its types and its nature (`GatherGraftSourceDataBattle`, `src/graft_menu.c`).
- **Graft Splicer** (3 badges) sacrifices a party mon to graft its traits, including evolved forms and grafts it already has (`GatherGraftSourceDataPartyMon`).
- Marts: Graft Ball needs 1 badge; Splice Ball and Splicer need 3. Filtered in the shop menu (`GetItemBadgeRequirement`, `src/shop.c`).

### Data model

Graft fields live on `struct BoxPokemon`, outside the encrypted `secure` union (`include/pokemon.h`): `graftTypes[2]`, `graftAbility`, `graftFlags`, `graftStatOverrides[6]`, `graftMoves[4]`. Because they're on BoxPokemon, party, boxed, daycare and enemy mons all carry them, and every struct copy moves them. Sentinels: type `0xFF` = none, ability `0` = none, stat `0` = none.

- BoxPokemon is 100 bytes (80 in vanilla). The PC has **11 boxes**, the most that fits in the vanilla 9 storage sectors. Don't add save sectors: the slots end right before the Hall of Fame sectors, and `src/save.c` asserts that.
- Stat grafts feed `CalculateMonStats`. Ability grafts are read by `GetMonAbility`. In battle, `ApplyBattlerGraftOverrides()` sets types and ability on intro, switch-in and faint, for both sides. Use that one function; don't copy-paste override blocks.
- Move grafts are recorded in `graftMoves[]` and show up in the move relearner.
- Type grafts recolor the sprite (`ApplyGraftTypePaletteSwap`). The recolor algorithm is a hue-cluster plus luminance shade approach that still needs work. Jacob's reference tooling (`recolor.py`, `palette_lab.py`) lives outside the repo.

### Rules

- **Stat graft count cap:** 1 + badges, uncapped at 5 badges (`GetStatGraftCap`). Only stats are capped.
- **Wonder Guard husk:** Shedinja, or anything grafted with Wonder Guard, has 1 max HP and refuses HP grafts (`IsGraftHusk`, `GetStatGraftRejectReason`).
- Hidden abilities: wild mons never offer them. Ability Capsules (planned for 5 badges) plus the Splicer make them reachable, which matches the "game-breaking stuff past the halfway point" rule.

### Enemy trainer grafts

Rolled at party creation (`ApplyTrainerGrafts`, `src/graft_data.c`). There are 2 + badges grafts per mon, drawn from a donor pool: the current map's wild table, then connected maps, then a manual fallback table for gyms and hideouts. Stat grafts must be strict improvements; moves replace the weakest slot; at most one ability (never hidden); no natures. Enemies follow the husk rule. During battle, R in move select shows the opponent's grafts.

## Encounters

- Jacob's spreadsheet is the source of truth: `Pokemon_Grafted_Encounter_Tables.xlsx`, plus a "Postgame Legendaries" tab. Elora's older sheet is scrapped.
- Every non-legendary family has a home. Keep canon encounters where possible, except overused ones. Theme matters: no fish on land, deep-sea mons in the sea, and so on.
- Game-breaking abilities stay past the halfway point through placement. Huge/Pure Power holders appear from 5 badges: Meditite on Mt. Pyre, Azurill on Route 120, Marill only via surfing.
- Existing land levels are kept. Surfing scales by region: 20–30 around Dewford and Slateport, 30–35 around Lilycove and Mossdeep, 35–40 from Pacifidlog on.
- **Post-game legendaries:** after the first Champion win, each legendary takes its area's 1% slot until caught. Hoenn's own legendaries keep their canon events. Mythicals, Ultra Beasts and Paradox Pokémon are held for a later wave.
- TODO: a script that converts the sheet to `src/data/wild_encounters.json`, with audit checks built in. Until it exists, the game still has Unown placeholders from Route 117 onward.

## Roadmap / decisions log

Done (2026-09-29): save/Hall of Fame overlap fix; mart Cancel row; enemy switch-in grafts; fishing pool overread; Wonder Guard husk; graft regression tests; AI switch logic reading the player's grafts; enemy graft types lost on form change; item descriptions overflowing the Bag; catch script falling into the graft menu.

Known test failures vs upstream: every battle test that throws a ball times out (being bisected). Upstream passes these.

Next:
1. Sheet → encounter JSON converter, then the post-game legendary mechanic.
2. Move grafts: when the mon knows 4 moves, always offer "forget a move?" (Graft Ball and Splicer both), and put the forgotten move in the relearner even if it was grafted.
3. Upstream merge (expansion 1.17.1+).
4. New-game options: shiny odds (currently hard-coded `SHINY_ODDS 128`); challenge settings for level caps, graft caps of various kinds, ability/stat gates, custom Nuzlocke rules (e.g. first encounter only, N grafts per route), monotype.
5. Breeding inheritance, designed for competitive control (netplay PvP is the long-term goal): grafts pass down like IVs, and a held item makes it deterministic ("this is my Huge Power Ditto"). Approved: a parent holding a new Graft Knot passes all its grafts (if both parents hold one and conflict, each egg picks one at random). Without a knot, each graft still has a lower chance to pass from a random parent (start at 25%, tune in playtesting).
6. Ungrafter: clears grafts and their relearner entries; moves the mon currently knows stay.
7. Bottle caps / hyper training at the Battle Frontier. Expansion has the engine support, but Emerald has no source of caps and no hyper-training NPC.
8. An in-game encounter dex. GraftedDex (the out-of-game companion that reads the encounter sheet) needs UI polish first.

Debug leftovers to remove before release: Select+R sets money to 999999 (`src/overworld.c`).
