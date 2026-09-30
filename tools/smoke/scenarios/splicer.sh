#!/bin/bash
# Default settings; Charmander L10, Magikarp L5, 2 Graft Splicers. Ends at out/splicer.ss with the start menu
# open on BAG: A opens the Bag (Items pocket), the Splicer is the first item.
set -e; . "$(dirname "$0")/../lib.sh"
"$RUN" - splicer.ss "$(intro)$(settings)$(truck)$(give_mon 4 10)$(give_mon 129 5)$(give_item 875 2)press START;wait 30;shot splicer.png"
