# Graft recolor tools

Type grafts recolor a mon's sprite (`src/graft_recolor.c`). These scripts render that recolor on the real
sprites in `graphics/pokemon`, so it can be tuned without the emulator.

- `recolor_v2.py`: the reference for the in-game algorithm. Same integer math as the C; tune here first.
- `recolor_old.py`: the previous algorithm, for A/B sheets.
- `sheet.py OUT.png ALGOS "ITEMS" [--shiny]`: contact sheets. ALGOS is a comma list of the modules above (each
  item renders the original, then one cell per algorithm). Items:
  - `mon:type1/type2[,type1/type2...]`: the mon with those effective types (after the graft), in one row.
  - `mon:all`: the mon with each type grafted into its second slot.
  ```
  python3 sheet.py out.png recolor_old,recolor_v2 "geodude:rock/fire,grass/ground mightyena:dark/fire"
  ```
- `native_check.py [N]`: builds `src/graft_recolor.c` for the host and checks it against `recolor_v2.py` on every
  sprite with N random type pairs each. Run it after changing either side; they must agree.

How it works: palette colors are grouped (up to two hue groups by a weighted 2-means, plus the grays). Outlines
stay: near-black colors, grays whose pixels mostly touch transparency, and near-whites. Each of the species'
types is matched to the group that looks most like it; a replaced type's group is repainted in the new type's
color, keeping each color's brightness. A second type with no group big enough of its own takes the brightest
part of the main group. Pixel counts come from the sprite's first frame (in game: the decompressed sprite).
