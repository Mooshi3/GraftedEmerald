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
- Marts: every mart that sells Poké Balls also sells Patch/Graft/Splice Balls, the Splicer, the Graft Knot and the Ungrafter; they're appended in code (`TryBuildDynamicShopItemList`, `src/shop_criteria.c`), not listed per map. Graft Ball needs 1 badge; Splice Ball and Splicer need 3. Badge gates, Rations and Harvests None filter afterwards in the shop menu (`GetItemBadgeRequirement`, `src/shop.c`).
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

Rolled at party creation (`ApplyTrainerGrafts`, `src/graft_data.c`). There are 2 + badges grafts per mon, drawn from a donor pool: the current map's wild table, then connected maps, then a manual fallback table for gyms and hideouts. Stat grafts must be strict improvements; moves replace the weakest slot; at most one ability (never hidden); no natures. Enemies follow the husk rule. The first rival battle (Route 103) is never grafted (`IsUngraftedTrainer`). During battle, R in move select shows the opponent's grafts.

## Encounters

- Jacob's spreadsheet is the source of truth: `Pokemon_Grafted_Encounter_Tables.xlsx`, plus a "Postgame Legendaries" tab. Elora's older sheet is scrapped.
- Every non-legendary family has a home. Keep canon encounters where possible, except overused ones. Theme matters: no fish on land, deep-sea mons in the sea, and so on.
- Game-breaking abilities stay past the halfway point through placement. Huge/Pure Power holders appear from 5 badges: Meditite on Mt. Pyre, Azurill on Route 120, Marill only via surfing.
- Existing land levels are kept. Surfing scales by region: 20–30 around Dewford and Slateport, 30–35 around Lilycove and Mossdeep, 35–40 from Pacifidlog on.
- **Post-game legendaries:** after the first Champion win (`FLAG_SYS_GAME_CLEAR`), each legendary takes its area's last 1% slot (land slot 12, surf slot 5) until it's registered as caught in the Pokédex (`TryGetPostgameLegendary`, `src/wild_encounter.c`). Harvesting with a Graft Ball does NOT count: the mon never became yours. Hoenn's own legendaries keep their canon events. Mythicals, Ultra Beasts and Paradox Pokémon are held for a later wave.
- The sheet lives at `tools/encounters/Grafted_Encounter_Tables.xlsx`. `tools/encounters/sheet_to_json.py` generates `src/data/wild_encounters.json` and `src/data/graft_postgame_legendaries.h` from it (see the README there). Never hand-edit Emerald maps in the JSON.
- **In-game encounter screen:** R in the Pokédex list (`src/graft_encounter_dex.c`). Scrolls through every visited map section with wild Pokémon (`graftVisitedAreas` in SaveBlock1, marked in `LoadCurrentMapData`) and lists species by method, "?????" until seen, with level range and rate. It opens filtered to the current time of day; SELECT cycles Morning/Day/Evening/Night/All (in All, the rate is the best over times and a tag shows which times). It reads the live tables through `GetWildMonInfoForTime` (same time-of-day fallback as encounters) and derives slot rates from the `ENCOUNTER_CHANCE_*` macros. Postgame legendaries show only after `FLAG_SYS_GAME_CLEAR` and until caught, taking the last slot's 1% like in the real roll.
- Every Emerald encounter table now comes from the sheet, except Altering Cave's 8 mystery-event variants (vanilla). Aqua Hideout has no land-encounter tiles, so it has a surfing table instead; Mewtwo's postgame slot is in Artisan Cave.

## Postgame

- **Hyper training:** a Black Belt in the Battle Frontier Exchange Service Corner (right of the counter) maxes one IV for a Bottle Cap or all six for a Gold Bottle Cap. The vitamin clerk sells Bottle Caps (16 BP) and Gold Bottle Caps (48 BP). Uses the expansion's `canhypertrain`/`hypertrain` script macros.
- Script gotchas found while building it: `addvar` adds a literal, not a variable's value; and `dynmultipush` inside a `call`ed subroutine crashed the game (use skip labels instead).

## New-game settings

After Birch's intro, before the game starts, a scrolling GAME SETTINGS screen (`src/graft_settings.c`) picks per-save options; they can't be changed later. They're packed into a 32-bit word across `VAR_GRAFTED_SETTINGS` (low) and `VAR_GRAFTED_SETTINGS_2` (high), stored XOR each setting's default so an unwritten var reads as defaults. `NewGameInitData` applies the choices after vars are cleared (`ApplyPendingGraftSettings`). To add a setting: add an enum entry and a row in `sGraftSettings[]` with its own bit range (and per-choice descriptions if the modes differ); the screen picks it up and `test/graft.c` checks the text fits.

- **Shiny Odds:** 1/4096, 1/512 (default, the old hard-coded rate) or 1/128, read by `ComputePlayerShinyOdds` via `GetPlayerShinyOddsThreshold`. `SHINY_ODDS` itself must never change: stored shininess is measured against it.
- **Level Cap:** Off / Hard / Soft (`src/caps.c`, replacing the compile-time `B_EXP_CAP_TYPE`). The cap is the next Gym Leader's (then the Champion's) strongest level. Hard stops EXP, candies and Day Care growth; Soft divides EXP past it (1/4 down to 1/64) and makes the player's own mons above the cap disobey sometimes (same odds as vanilla obedience, with the cap in place of the badge level).
- **EXP Share:** Item (vanilla held item) / Party (the whole party always gets EXP, via the expansion's Gen 6 path: `IsGen6ExpShareEnabled` reads the setting).
- **Obedience** is vanilla otherwise (`B_OBEDIENCE_MECHANICS GEN_7`): only traded mons can disobey. The expansion default (Gen 8+) also made your own catches disobey by met level.
- **Graft Limit:** Free / Standard (stat grafts capped at 1 + badges until 5 badges) / Budget (2 + badges/2 grafts per mon) / Strict (3). Budget counts types (both slots as one), ability, nature and each stat; moves never count. **Graft Gates:** types need 1 badge, stats 2, abilities 3. All of it, plus the husk rule and fallen mons, goes through `GetGraftRejectReason` (`src/graft_data.c`), checked when a graft target is picked in battle or with the Splicer.
- **Graft Items:** Shops / Rations. Rations: marts don't sell graft balls or Splicers; a new game starts with 3 Splice Balls and every gym badge gives 3 more (plus a Splicer from 3 badges), from `Common_EventScript_GiveGraftRations`, called by the shared `Common_EventScript_SetGymTrainers`.
- **Harvests** (grafting from wild mons with a ball): None / 1 per area / 1 per mon (each party mon, per map section, stored in `graftHarvestArea` on BoxPokemon) / Any. With None, marts and Rations leave out the grafting balls (`AreGraftBallsUseless`); Rations then give only the Splicer, from 3 badges.
- **Enemy Grafts:** Fewer (1 + badges/2) / Normal (2 + badges) / More (3 + badges; Leaders', Elite Four and Champion aces always roll an ability graft).
- **Wild Grafts:** Off (default) / Rare (5%) / Common (20%) of wild Pokémon come pre-grafted with 1 + badges/2 grafts, rolled like a trainer's from the nearby donor pool (`TryApplyWildGrafts`, hooked in `CreateWildMon`). Not before the Pokédex.
- **Nuzlocke** (`src/graft_challenge.c`): only the first random wild encounter per map section is catchable (`graftCaughtAreas` in SaveBlock1); scripted/static battles are free, and nothing counts until the Pokédex (`FLAG_SYS_POKEDEX_GET`, when balls unlock; a flag so emptying the Bag can't dodge it). Dupes clause (default on) and shiny clause (default on). Harvesting uses a grafting ball, so it follows the Harvests setting rather than the catch rule; harvesting the area's first encounter uses up its catch too.
- **Fainting:** Normal / Release (fainted mons are released after battle, held items return to the Bag) / Graveyard (`GRAFT_FLAG_FALLEN`: HP is clamped to 0 in `SetMonData`, items fail, they can't take grafts, but they can be the Splicer's donor) / Auto-splice (after battle, each fallen mon opens the Splicer as a free donor, or can be let go). Applied in `ReturnFromBattleToOverworld`; not after a whiteout (a wipe ends a Nuzlocke run, on the player's honor), and not in link, Frontier, Pyramid, Pike, Trainer Hill or partner battles (Steven, NPC followers: the partner's mons sit in the player's party then).
  Overworld poison can't make a mon faint: `OW_POISON_DAMAGE` is Gen 5+ (no overworld poison damage at all), so battles are the only place fainting happens.
- **Battle Items:** Banned leaves only balls usable from the Bag in battle (`CannotUseItemsInBattle`). **Poké Centers:** Closed makes the nurse refuse (`ArePokeCentersClosed` in `Common_EventScript_PkmnCenterNurse`); other healing (beds, story heals, whiteouts) still works. **Force Set:** battles always use Set style and the option is locked.
- **Monotype:** any type or Off. Only party mons with that type (natural or grafted) pass `IsValidForBattle` and can switch in; if no party mon has it, everyone may battle, so there's always someone to send out. In partner battles only the player's half of the party is restricted. The starter gets the type grafted as its second type.

## Smoke tests

`tools/smoke/` plays the real ROM in a headless mGBA and takes screenshots (see its README). Use it for menus, scripts and flows the unit tests can't see. To save main-session context, run smoke tests through a subagent on a cheaper model: point it at `tools/smoke/README.md`, name the flows to check, and ask for a report plus a contact sheet of the frames that matter. Decide yourself whether what it found is a real bug.

## Roadmap / decisions log

Test baseline (2026-09-29, after the 1.17.1 merge): full `make check` has 0 failures (5428 passed as of 2026-09-30; the rest are upstream's KNOWN_FAILING/TO_DO/EXPECT_FAILING). Keep it at 0.


Done (2026-09-29): save/Hall of Fame overlap fix; mart Cancel row; enemy switch-in grafts; fishing pool overread; Wonder Guard husk; graft regression tests; Graft Knot breeding inheritance; grafted-move relearner history; AI switch logic reading the player's grafts; enemy graft types lost on form change; item descriptions overflowing the Bag; catch script falling into the graft menu.; recorded-battle save overflowing its flash sector (per-battler record buffer is 268 bytes, upstream 388, because recordings store 24 grafted party mons). Done (2026-09-30): battle graft menu drawing leftover text when A was pressed mid-print; Nuzlocke starting before the Pokédex; partner battles in Fainting/Monotype; vanilla obedience. Also: new-game settings screen with shiny odds, level caps, graft limits/gates/rations/harvests, enemy graft levels, Nuzlocke rules, fainting modes, battle items, Force Set, monotype; Splicer category menu showing the last Graft Ball's shuffled categories; Ungrafter; hyper training at the Frontier; Splicer naming the wrong mon in "was released" and leaving the refusal message behind the target prompt.

Done (2026-10-01): battle grafting leaked 12 KB of heap per graft (it opened the party menu without `CloseMainBattleScreen`, so the reshow reallocated the battle gfx buffers) until a menu crashed with out of memory (`malloc.c`); backing out of the graft target menu now returns to the graft menu instead of grafting onto the last target or slot 255.

Next:
1. GraftedDex (the out-of-game companion that reads the encounter sheet) needs UI polish.
2. **Vanilla encounters** setting: build Emerald's original tables alongside the sheet's and pick by setting (the encounter screen follows via `GetWildMonInfoForTime`). Open question: postgame legendaries in vanilla mode?
3. **Graft Bag (planned redesign, needs a new save):** graft balls and the Splicer turn Pokémon into stored *pieces* instead of grafting on the spot, so a good roll can be kept for a later Pokémon. Decided with Jacob (2026-10-01):
   - **Pieces:** one trait each: a move, an ability, one type, a nature, or one stat's base value. They stack and don't remember their source.
   - **Sources:** Patch Ball gives 1 random piece of the caught wild mon; Graft Ball 3 different random pieces; Splice Ball minces the wild mon into every piece it has (4 moves, ability, each type, nature, 6 stats; never a hidden ability). The Splicer sacrifices a party mon and minces it the same way, grafts included.
   - **Using pieces:** from the Graft Bag, outside battle, onto a party mon; a type piece asks which slot it replaces; moves use the forget-a-move prompt and `RecordGraftMove`. Every rule still applies at that point through `GetGraftRejectReason` (limits, gates, husk, fallen).
   - **Ungrafter** destroys grafts; nothing returns to the bag. Pieces can be discarded, not sold.
   - **Bag full after a catch:** just say "The Graft Bag is full." and the overflow is lost.
   - **Challenge settings keep their intent:** Harvests and Nuzlocke count at the catch; Rations still hands out balls; Graft Limit/Gates apply when a piece is used.
   - **Storage:** a count per possible piece, indexed by piece (the position is the ID): ~920 moves + ~310 abilities + 18 types + 25 natures + ~730 stat values (only base values some species has, mapped by a generated table) = ~2,000 slots; 4-bit counts (max 15 each) = ~1 KB in SaveBlock1, freed by shrinking or cutting the secret bases (20 x 160 bytes). An optional total-pieces cap can gate progression (upgrades/badges); not decided.
   - Removes the in-battle graft menu (where the 2026-10-01 bugs lived): after a graft-ball catch, the pieces go straight to the bag.
4. **Ironmon:** mostly self-imposed (one usable mon, a committed pivot catch is allowed, HM users are fine), so the party can't just be capped at 1. The game-side parts already exist as settings: Battle Items banned, Poké Centers closed. Revisit if a rule turns out to need enforcing.

Speed-ups stay player-side: the Options menu and Battle Scene OFF.


