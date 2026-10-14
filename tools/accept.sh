#!/usr/bin/env bash
# The acceptance run: FINAL-frontend.md section 10 and FINAL-space.md section 13.
#
#   tools/accept.sh            # build, then everything
#   NOBUILD=1 tools/accept.sh  # use the cart already in build/
#   JOBS=2 tools/accept.sh     # headless runs at once (default 1: they share the GPU and run slower together)
#
# Static checks: level_check over all five levels (tools/smoke.c), the Python
# port (tools/gapcheck.py) and the text lint (tools/lint-content.py).
# Scripted runs: every JSON under tools/accept/ and the five level nights in
# tools/scripts/level-k.json, each with screenshots at its check frames in
# build/shots/acc-*.png. Every log is grepped for the level_check errors (GAP,
# WALL, APPROACH, SPAWN, RECT, FLOOR, STENCIL, PROP), the wedge detector
# (BOT_STILL) and three.c's missing or generated render states. The claim run
# also dumps the save region (checked by tools/accept/check-save.mjs) and a
# WAV of the menus and a whole night (checked by tools/wavpeak.mjs for full
# scale samples).
#
# Green means every check above passed. The screenshots still need a person to
# look at them against the tables in the two specs; the list is printed last.
# NOVISIT lines (a station with a face no bot went near) are printed as notes:
# with two bots and 23 stations some nights skip a station or two.
set -uo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"
OUT=build/accept
SHOTS=build/shots
CART=build/do-ocracy.wasc
JOBS="${JOBS:-1}"
mkdir -p "$OUT" "$SHOTS"
fail=0
bad() { echo "FAIL $*"; fail=1; }

if [ "${NOBUILD:-0}" != 1 ]; then
  ./build.sh > "$OUT/build.log" 2>&1 || { cat "$OUT/build.log"; bad "build"; exit 1; }
  grep -q "warning:" "$OUT/build.log" && bad "build warnings (see $OUT/build.log)"
fi

echo "== static checks"
cc -std=gnu11 -O1 -Wall -I src -I third_party/three.c/include tools/smoke.c src/levels.c src/levelcheck.c -lm -o build/smoke \
  || bad "smoke does not compile"
build/smoke > "$OUT/smoke.log"; rc=$?
grep -v " NARROW \| NEAR " "$OUT/smoke.log"
[ $rc = 0 ] || bad "level_check (tools/smoke.c)"
build/smoke --json > build/levels.json && python3 -I tools/gapcheck.py build/levels.json || bad "tools/gapcheck.py"
python3 -I tools/lint-content.py || bad "tools/lint-content.py"

# Where the scripts differ from the frames in FINAL-frontend.md section 10:
# - claim.json: pad 1's A at 400 makes it a pending seat, so PLAY seats it with
#   the owner and its card is up at 1640 already (B11). The CO-OP night with two
#   idle humans burns down (the fire marshal) at about frame 5700 instead of
#   surviving to 9982, so the results are shot at 5900 (B18) and pad 1's B is at
#   6060 with the menu at 6100 (B19).
# - second-night.json: PLAY at 6260, READY at 6360 and 6420; 6300 is the lobby
#   with no flash, 6700 the night with no coach. The idle out to the title does
#   not happen in a CO-OP loop (each night burns down before anyone idles 75 s),
#   so it is regroup.json instead.
# - regroup.json: one pad plays SOLO, skips the flash and idles; it goes home in
#   the night, the bots leave, new_group() runs and 7400 is the title with no
#   owner; A and PLAY at 7500 and 7560 play the flash again at 7700.
# - attract.json: nobody touches the cabinet for 30 s (600 is the title with
#   PRESS A OR SPACE, 1800 the demo with its DEMO tag), then pad 1's A at 1900
#   ends the demo and 1960 is the main menu; PLAY at 2000 (2060 the first
#   rules card with the owner's key), B skips the flash, READY at 2160 (2200
#   the countdown); the night starts at about 2358, so 2500 shows YOU ARE
#   PAD 1 over the player and the pulsing card, 2760 the coach's first line
#   with the key named and 4960 the night under way with the cards under the
#   storefront.
# - lobby-geom.json sets HELP to OFF in SETTINGS first so the lobby is clear.
# - level-k.json come from tools/scripts and pick the night through SETTINGS.

echo "== scripted runs ($JOBS at a time)"
# name | script | frames | extra env
RUNS=(
  "idle|tools/accept/idle.json|90,600,1260,1520|"
  "attract|tools/accept/attract.json|600,1800,1960,2060,2200,2500,2760,4960|"
  "claim|tools/accept/claim.json|330,420,560,640,790,860,1050,1400,1460,1640,1760,2460,2530,2650,2800,3030,3130,5900,6100|WAV=$OUT/claim.wav SAVE=$OUT/claim.bin"
  "second|tools/accept/second-night.json|6300,6700|"
  "regroup|tools/accept/regroup.json|7400,7700|"
  "idleowner|tools/accept/idle-owner.json|1750,2960|"
  "idleowner2|tools/accept/idle-owner2.json|1750|"
  "geom|tools/accept/lobby-geom.json|960,1200|"
  "credits|tools/accept/credits.json|260,330|"
  "level0|tools/scripts/level-0.json|1300,4600,8200,12400|"
  "level1|tools/scripts/level-1.json|1300,2000,4600,12400|"
  "level2|tools/scripts/level-2.json|1300,2300,4600,12400|"
  "level3|tools/scripts/level-3.json|1300,2000,4600,13600|"
  "level4|tools/scripts/level-4.json|1300,2200,4600,10600|"
)
run_one() {
  IFS='|' read -r name script frames envs <<< "$1"
  env $envs node tools/shot.mjs "$CART" "$SHOTS/acc-$name" "$frames" "$script" > "$OUT/$name.log" 2>&1
  echo "  ran $name (exit $?)"
}
export -f run_one
export CART SHOTS OUT
printf '%s\n' "${RUNS[@]}" | xargs -P "$JOBS" -I{} bash -c 'run_one "$@"' _ {}

echo "== logs"
for r in "${RUNS[@]}"; do
  name="${r%%|*}"
  log="$OUT/$name.log"
  [ -s "$log" ] || { bad "$name: no log"; continue; }
  n=$(grep -c "^shot " "$log"); want=$(echo "$r" | cut -d'|' -f3 | tr ',' '\n' | wc -l | tr -d ' ')
  [ "$n" = "$want" ] || bad "$name: $n of $want screenshots"
  hits=$(grep -E "\[cart\] .*(GAP|WALL|APPROACH|SPAWN|RECT|FLOOR|STENCIL|PROP|BOT_STILL)|missing|generated" "$log")
  [ -z "$hits" ] || { echo "$hits"; bad "$name: log findings"; }
  grep "NOVISIT\|VISITS" "$log" | sed "s/^/  note $name: /"
done

echo "== save and audio"
node tools/accept/check-save.mjs "$OUT/claim.bin" mode=1 night_len=0 music=4 || bad "save region"
node tools/wavpeak.mjs "$OUT/claim.wav" || bad "audio peak"

echo "== screenshots to read"
ls "$SHOTS"/acc-*.png | sed 's/^/  /'
if [ $fail = 0 ]; then echo "accept: GREEN"; else echo "accept: RED"; fi
exit $fail
