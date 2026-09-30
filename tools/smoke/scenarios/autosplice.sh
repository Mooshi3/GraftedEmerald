#!/bin/bash
# Rations + Auto-splice + Monotype Fire; party Torchic L2, Charmander L10, Magikarp L5; trainer battle vs Calvin.
# Ends at out/autosplice.ss in the battle's first turn (Torchic out). Let Torchic faint, send Charmander,
# win with Ember; the Auto-splice menu opens after the battle.
set -e; . "$(dirname "$0")/../lib.sh"
"$RUN" - autosplice.ss "$(intro)$(settings 5:1 12:3 16:10)$(truck)$(give_mon 255 2)$(give_mon 4 10)$(give_mon 129 5)$(trainer_battle 318)shot autosplice.png"
