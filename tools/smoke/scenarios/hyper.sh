#!/bin/bash
# Default settings; Charmander L10, Bottle Cap, Gold Bottle Cap, max BP; warps to the Battle Frontier
# Exchange Service Corner. Ends at out/hyper.ss standing 4 tiles below and 1 left of the hyper trainer:
# press UP 64, RIGHT 3, then A to talk. The vitamin clerk (Bottle Caps for BP) is at (9,5) behind the counter.
set -e; . "$(dirname "$0")/../lib.sh"
"$RUN" - hyper.ss "$(intro)$(settings)$(truck)$(give_mon 4 10)$(give_item 133 1)$(give_item 134 1)$(give_bp)$(warp 26 42 0)wait 60;press UP 48;wait 10;press LEFT 16;wait 10;press RIGHT 96;wait 10;shot hyper.png"
