#!/bin/bash
# Builds the release ROM and a .bps patch of it against a clean Pokémon Emerald (USA, Europe), then checks
# the patch by applying it. The game itself is never committed or shared; publish the .bps with
# GraftedRogue_README.md (copied next to it as README.md). This is the rogue branch: it makes GraftedRogue.bps.
#
# usage: tools/patch/make_patch.sh BASE.gba [OUT.bps]
#   BASE.gba: Pokémon - Emerald Version (USA, Europe), MD5 605b89b67018abcea91e693a4dd25be3
#   needs Floating IPS: FLIPS=/path/to/flips, or `flips` on PATH (https://github.com/Alcaro/Flips; `make` builds it)
set -e
BASE_MD5=605b89b67018abcea91e693a4dd25be3
base="$1"
out="${2:-GraftedRogue.bps}"
flips="${FLIPS:-$(command -v flips || true)}"
cd "$(dirname "$0")/../.."

[ -f "$base" ] || { echo "usage: $0 BASE.gba [OUT.bps]"; exit 1; }
[ -x "$flips" ] || { echo "Floating IPS not found: set FLIPS=/path/to/flips"; exit 1; }
[ "$(md5sum "$base" | cut -d' ' -f1)" = "$BASE_MD5" ] || { echo "$base is not Emerald (USA, Europe) (MD5 $BASE_MD5)"; exit 1; }

make release -j"$(nproc)"
"$flips" --create --bps-delta "$base" pokeemerald-release.gba "$out"
check="$(mktemp)"
"$flips" --apply "$out" "$base" "$check" >/dev/null
cmp -s "$check" pokeemerald-release.gba || { rm -f "$check"; echo "the patch doesn't reproduce the ROM"; exit 1; }
rm -f "$check"
cp tools/patch/GraftedRogue_README.md "$(dirname "$out")/README.md"
echo "$out: applies to Emerald (USA, Europe) and gives pokeemerald-release.gba (MD5 $(md5sum pokeemerald-release.gba | cut -d' ' -f1))"
echo "$(dirname "$out")/README.md: the release notes to publish with it"
