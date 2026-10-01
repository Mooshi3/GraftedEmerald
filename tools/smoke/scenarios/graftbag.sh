#!/bin/bash
# Default settings; Sandygast L10 and 10 Splice Balls; catches a wild mon in Petalburg Woods with a
# Splice Ball (it's broken down into Graft Bag pieces), then opens the Graft Bag from the Bag's Key
# Items (second row; the first is the Sleeping Bag). Ends on the Graft Bag screen, Moves tab.
set -e; . "$(dirname "$0")/../lib.sh"
"$RUN" - graftbag.ss "$(intro)$(settings 0:0)$(truck)$(give_mon 769 10)$(give_item 878 10)$(dex_on)$(warp 24 11 2)press UP 48;press LEFT 32;$(for i in $(seq 6); do echo -n 'press LEFT 16;press RIGHT 16;'; done)wait 400;press UP 32;press LEFT 64;$(for i in $(seq 10); do echo -n 'press LEFT 16;press RIGHT 16;'; done)wait 500;press A;wait 120;press RIGHT;wait 10;press A;wait 120;wait 120;press RIGHT;wait 10;press A;wait 150;press RIGHT;wait 30;press A;wait 30;press A;wait 1200;press A;wait 400;wait 20;press START 4;wait 30;press DOWN;wait 6;press A;wait 120;press RIGHT;wait 20;press RIGHT;wait 20;press RIGHT;wait 30;press DOWN;wait 10;press A;wait 30;press A;wait 120;shot graftbag.png"
