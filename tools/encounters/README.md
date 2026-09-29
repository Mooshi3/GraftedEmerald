# Encounter tables

`Grafted_Encounter_Tables.xlsx` is the source of truth for wild encounters. Don't hand-edit
`src/data/wild_encounters.json` for Emerald maps; edit the sheet and regenerate.

```
pip install openpyxl
python3 tools/encounters/sheet_to_json.py --check   # validate only
python3 tools/encounters/sheet_to_json.py           # write the JSON and the postgame legendary table
make
```

Sheet layout:
- **Encounter Tables** tab: one row per area and time. Columns D–O are the 12 land slots (20/20/10/10/10/10/5/5/4/4/1/1).
  Water rows use the first 5 cells for surfing (60/30/5/4/1), or 2/3/5 for Old/Good/Super Rod rows
  (Time = "Old Rod Fishing" etc.). Water rows need a level range in the **Levels** column; land rows keep the
  levels already in the game.
- **Postgame Legendaries** tab: which legendary takes each area's 1% slot after the first Champion win.
- Time: Morning / Day / Night, or All. Evening falls back to Day.

The script checks names, flags legendaries on the main tab, warns about evolved forms far below their
evolution level, and reports rows that can't trigger (maps with no grass/cave/water tiles). Areas that
span several maps (Mt. Pyre floors, Seafloor Cavern rooms, Safari Zone sections...) are listed in
`AREA_MAPS` in the script. The companion GraftedDex page reads this same sheet.
