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

- `origin` = Jacob's GitHub repo; `upstream` = rh-hideout/pokeemerald-expansion. Grafted branched from upstream in May 2026 (`21c2420299`) and was merged up to expansion 1.17.1 (+ one post-release fix) on 2026-09-29.
- Upstream API notes that matter for graft code: use `gParties[B_TRAINER_PLAYER]` / `GetBattlerMon(battler)` (the old `gPlayerParty`/`gEnemyParty` are deprecated); battlers are built by `PokemonToBattleMon` (applies grafted types/ability); opponent parties are built in `CreateNPCTrainerParty` (`src/battle_setup.c`, where `ApplyTrainerGrafts` is hooked); eggs hatch in place (no rebuild), so grafts survive hatching; relearner categories are the `sRelearnTypes` table in `src/move_relearner.c`.
- Toolchain: `arm-none-eabi-gcc` + `make`. Build output is gitignored.

## Grafting

Five graft kinds: **move, ability, type, stat, nature**.

Sources:
- **Balls** consume the caught wild mon and offer graft categories: Patch Ball (1 random category), Graft Ball (3), Splice Ball (all 5). A wild mon offers its actual ability (never its hidden one), the 4 moves it currently knows, its species' base stats, its types and its nature (`GatherGraftSourceDataBattle`, `src/graft_menu.c`).
- **Graft Splicer** (3 badges) sacrifices a party mon to graft its traits, including evolved forms and grafts it already has (`GatherGraftSourceDataPartyMon`).
- Marts: Graft Ball needs 1 badge; Splice Ball and Splicer need 3. Filtered in the shop menu (`GetItemBadgeRequirement`, `src/shop.c`).
- **Ungrafter** (200, no badge gate, single use): clears every graft and the grafted-move relearner history; moves the mon knows stay, a grafted nature reverts to its personality nature (`UngraftMon`, `src/graft_data.c`). It isn't a grafting item, so Rations don't hide it.

### Data model

Graft fields live on `struct BoxPokemon`, outside the encrypted `secure` union (`include/pokemon.h`): `graftTypes[2]`, `graftAbility`, `graftFlags`, `graftStatOverrides[6]`, `graftMoves[4]`. Because they're on BoxPokemon, party, boxed, daycare and enemy mons all carry them, and every struct copy moves them. Sentinels: type `0xFF` = none, ability `0` = none, stat `0` = none.

- BoxPokemon is 100 bytes (80 in vanilla). The PC has **11 boxes**, the most that fits in the vanilla 9 storage sectors. Don't add save sectors: the slots end right before the Hall of Fame sectors, and `src/save.c` asserts that.
- Stat grafts feed `CalculateMonStats`. Ability grafts are read by `GetMonAbility`. In battle, `ApplyBattlerGraftOverrides()` sets types and ability on intro, switch-in and faint, for both sides. Use that one function; don't copy-paste override blocks.
- Move grafts: both the Graft Ball flow (battle script) and the Splicer ask which move to forget when the mon knows 4. Every grafted move is recorded with `RecordGraftMove()` into `graftMoves[]` (the relearner history), which dedupes and, when its 4 entries are full, evicts a move the species can relearn by level-up first, otherwise the oldest.
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
- **Post-game legendaries:** after the first Champion win (`FLAG_SYS_GAME_CLEAR`), each legendary takes its area's last 1% slot (land slot 12, surf slot 5) until it's registered as caught in the Pokédex (`TryGetPostgameLegendary`, `src/wild_encounter.c`). Harvesting with a Graft Ball does NOT count: the mon never became yours. Hoenn's own legendaries keep their canon events. Mythicals, Ultra Beasts and Paradox Pokémon are held for a later wave.
- The sheet lives at `tools/encounters/Grafted_Encounter_Tables.xlsx`. `tools/encounters/sheet_to_json.py` generates `src/data/wild_encounters.json` and `src/data/graft_postgame_legendaries.h` from it (see the README there). Never hand-edit Emerald maps in the JSON.
- Every Emerald encounter table now comes from the sheet, except Altering Cave's 8 mystery-event variants (vanilla). Aqua Hideout has no land-encounter tiles, so it has a surfing table instead; Mewtwo's postgame slot is in Artisan Cave.

## Postgame

- **Hyper training:** a Black Belt in the Battle Frontier Exchange Service Corner (right of the counter) maxes one IV for a Bottle Cap or all six for a Gold Bottle Cap. The vitamin clerk sells Bottle Caps (16 BP) and Gold Bottle Caps (48 BP). Uses the expansion's `canhypertrain`/`hypertrain` script macros.
- Script gotchas found while building it: `addvar` adds a literal, not a variable's value; and `dynmultipush` inside a `call`ed subroutine crashed the game (use skip labels instead).

## New-game settings

After Birch's intro, before the game starts, a scrolling GAME SETTINGS screen (`src/graft_settings.c`) picks per-save options; they can't be changed later. They're packed into a 32-bit word across `VAR_GRAFTED_SETTINGS` (low) and `VAR_GRAFTED_SETTINGS_2` (high), stored XOR each setting's default so an unwritten var reads as defaults. `NewGameInitData` applies the choices after vars are cleared (`ApplyPendingGraftSettings`). To add a setting: add an enum entry and a row in `sGraftSettings[]` with its own bit range (and per-choice descriptions if the modes differ); the screen picks it up and `test/graft.c` checks the text fits.

- **Shiny Odds:** 1/4096, 1/512 (default, the old hard-coded rate) or 1/128, read by `ComputePlayerShinyOdds` via `GetPlayerShinyOddsThreshold`. `SHINY_ODDS` itself must never change: stored shininess is measured against it.
- **Level Cap:** Off / Hard / Soft (`src/caps.c`, replacing the compile-time `B_EXP_CAP_TYPE`). The cap is the next Gym Leader's (then the Champion's) strongest level. Hard stops EXP, candies and Day Care growth; Soft divides EXP past it (1/4 down to 1/64).
- **Graft Limit:** Free / Standard (stat grafts capped at 1 + badges until 5 badges) / Budget (2 + badges/2 grafts per mon) / Strict (3). Budget counts types (both slots as one), ability, nature and each stat; moves never count. **Graft Gates:** types need 1 badge, stats 2, abilities 3. All of it, plus the husk rule and fallen mons, goes through `GetGraftRejectReason` (`src/graft_data.c`), checked when a graft target is picked in battle or with the Splicer.
- **Graft Items:** Shops / Rations. Rations: marts don't sell graft balls or Splicers; a new game starts with 3 Splice Balls and every gym badge gives 3 more (plus a Splicer from 3 badges), from `Common_EventScript_GiveGraftRations`, called by the shared `Common_EventScript_SetGymTrainers`.
- **Harvests** (grafting from wild mons with a ball): None / 1 per area / 1 per mon (each party mon, per map section, stored in `graftHarvestArea` on BoxPokemon) / Any.
- **Enemy Grafts:** Fewer (1 + badges/2) / Normal (2 + badges) / More (3 + badges; Leaders', Elite Four and Champion aces always roll an ability graft).
- **Nuzlocke** (`src/graft_challenge.c`): only the first random wild encounter per map section is catchable (`graftCaughtAreas` in SaveBlock1); scripted/static battles are free. Dupes clause (default on) and shiny clause (default on). Harvesting uses a grafting ball, so it follows the Harvests setting rather than the catch rule; harvesting the area's first encounter uses up its catch too.
- **Fainting:** Normal / Release (fainted mons are released after battle, held items return to the Bag) / Graveyard (`GRAFT_FLAG_FALLEN`: HP is clamped to 0 in `SetMonData`, items fail, they can't take grafts, but they can be the Splicer's donor) / Auto-splice (after battle, each fallen mon opens the Splicer as a free donor, or can be let go). Applied in `ReturnFromBattleToOverworld`; not after a whiteout (a wipe ends a Nuzlocke run, on the player's honor), and not in link, Frontier, Pyramid, Pike or Trainer Hill battles. Known gap: a mon that faints from overworld poison isn't marked.
- **Battle Items:** Banned leaves only balls usable from the Bag in battle (`CannotUseItemsInBattle`). **Force Set:** battles always use Set style and the option is locked.
- **Monotype:** any type or Off. Only party mons with that type (natural or grafted) pass `IsValidForBattle` and can switch in; if no party mon has it, everyone may battle, so there's always someone to send out. The starter gets the type grafted as its second type.

## Roadmap / decisions log

Test baseline (2026-09-29, after the 1.17.1 merge): full `make check` has 0 failures (5428 passed as of 2026-09-30; the rest are upstream's KNOWN_FAILING/TO_DO/EXPECT_FAILING). Keep it at 0.


Done (2026-09-29): save/Hall of Fame overlap fix; mart Cancel row; enemy switch-in grafts; fishing pool overread; Wonder Guard husk; graft regression tests; Graft Knot breeding inheritance; grafted-move relearner history; AI switch logic reading the player's grafts; enemy graft types lost on form change; item descriptions overflowing the Bag; catch script falling into the graft menu.; recorded-battle save overflowing its flash sector (per-battler record buffer is 268 bytes, upstream 388, because recordings store 24 grafted party mons). Done (2026-09-30): new-game settings screen with shiny odds, level caps, graft limits/gates/rations/harvests, enemy graft levels, Nuzlocke rules, fainting modes, battle items, Force Set, monotype; Splicer category menu showing the last Graft Ball's shuffled categories; Ungrafter; hyper training at the Frontier; Splicer naming the wrong mon in "was released" and leaving the refusal message behind the target prompt.

Next:
1. An in-game encounter dex. GraftedDex (the out-of-game companion that reads the encounter sheet) needs UI polish first.
2. Visual smoke tests: a scripted mGBA screenshot tool exists outside the repo (built from mGBA source against the ROM; see the 2026-09-30 session). Worth checking into `tools/` if UI work keeps coming.

Debug leftovers to remove before release: Select+R sets money to 999999 (`src/overworld.c`).
