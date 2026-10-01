# Graft recolor tools

Type grafts recolor a mon's sprite (`src/graft_recolor.c`). These scripts render that recolor on the real
sprites in `graphics/pokemon`, so it can be tuned without the emulator.

- `recolor_v3.py`: the reference for the in-game algorithm. Same integer math as the C; tune here first.
- `recolor_old.py`: the previous algorithm, for A/B sheets.
- `sheet.py OUT.png ALGOS "ITEMS" [--shiny]`: contact sheets. ALGOS is a comma list of the modules above (each
  item renders the original, then one cell per algorithm). Items:
  - `mon:type1/type2[,type1/type2...]`: the mon with those effective types (after the graft), in one row.
  - `mon:all`: the mon with each type grafted into its second slot.
  ```
  python3 sheet.py out.png recolor_old,recolor_v2 "geodude:rock/fire,grass/ground mightyena:dark/fire"
  ```
- `golden.py check approved.json`: the recolors Jacob approved (the round-2 A/B sheets, 2026-10-01) are pinned in
  `approved.json`; this lists any that a change alters. Re-pin with `golden.py save approved.json "ITEMS"` once
  new results are approved.
- `native_check.py [N]`: builds `src/graft_recolor.c` for the host and checks it against `recolor_v3.py` on every
  sprite with N random type pairs each. Run it after changing either side; they must agree.

How it works (the goal: both of the mon's types show): palette colors are grouped (up to two hue groups by a
weighted 2-means, the grays, and the whites when they cover much of the sprite). Outlines stay: near-black
colors, grays whose pixels mostly touch transparency, and small whites. Each species type is matched to the
group that looks most like it; a type with no group of its own takes the next biggest group, else the main
group's shadows (highlights read as spots). A region is repainted in its type's color, keeping each color's
brightness, when its type was replaced or when its colors don't look like the type. Whites only stand for
whitish types (Normal, Steel, Rock, Ghost, Dark). Pixel counts come from the sprite's first frame (in game:
the decompressed sprite).
