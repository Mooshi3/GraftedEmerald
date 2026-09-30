#!/bin/bash
# Nuzlocke On, Harvests 1 per area, Fainting Graveyard; Charmander L10, 10 Poke Balls, 5 Graft Balls; warps to
# the south entrance of Petalburg Woods. Grass is up and to the left: press UP 48, LEFT 32, then pace
# LEFT 16 / RIGHT 16 until an encounter (with this exact input history: a Shroomish, then a Smoliv).
# Sets the Pokedex flag, since Nuzlocke rules only start with it.
set -e; . "$(dirname "$0")/../lib.sh"
"$RUN" - wild.ss "$(intro)$(settings 6:2 9:1 12:2)$(truck)$(give_mon 4 10)$(give_item 1 10)$(give_item 874 5)$(dex_on)$(warp 24 11 2)shot wild.png"
