#!/bin/bash
# Rogue: new game -> the base; answers the meta-progression question (Yes) -> out the exit (Yes)
# with no challenge: a run starts with a random starter. Ends in floor 1, room 1 (a wild room), player at
# the entrance. Jump to any room type with rogue_room (lib.sh).
set -e; . "$(dirname "$0")/../lib.sh"
"$RUN" - rogue.ss "$(intro)wait 400;shot rogue_meta.png;mash A 6 60;wait 120;press DOWN 16;wait 60;press A;wait 60;shot rogue_challenge.png;press A;wait 60;mash A 4 60;wait 300;mash A 2 40;wait 60;shot rogue.png"
