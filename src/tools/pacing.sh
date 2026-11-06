#!/bin/sh
# Pacing / difficulty runs of the main quest with the playthrough bot.
#   tools/pacing.sh [class] [difficulty 0-2] [seeds...]     e.g. tools/pacing.sh warrior 1 1 2 3
# For each seed: a god-mode run (always completes: the level / gold / kills / time curve, written to
# out/pacing/<seed>/pacing.csv) and a survival run (no god mode: where, if anywhere, the party is wiped out).
# Times are the bot's (instant dialogue, fast AI combat): a person reading and thinking takes several times longer.
cd "$(dirname "$0")/.." || exit 1
CLASS=${1:-warrior}; DIFF=${2:-1}; shift 2 2>/dev/null; SEEDS=${*:-1 2 3}
[ -x ./triffall_headless ] || make headless || exit 1
for S in $SEEDS; do
  for MODE in god survival; do
    D=out/pacing/$S-$MODE; mkdir -p "$D"
    G=god; [ $MODE = survival ] && G="#"
    sed -e "s/CLASS/$CLASS/" -e "s/DIFF/$DIFF/" -e "s/SEED/$S/" -e "s/^GODLINE$/$G/" tools/pacing_route.txt > "$D/route.txt"
    echo "== seed $S ($MODE)"
    ./triffall_headless "$D/route.txt" "$D/" | grep -E "CHECKPOINT|BOT FAIL|member "
  done
done
