#!/bin/sh
# Runs every headless test script; prints the failures and exits non-zero if any failed.
#   tests/run_all.sh [pattern]      e.g. tests/run_all.sh "editor_*"
cd "$(dirname "$0")/.." || exit 1
[ -x ./triffall_headless ] || make headless || exit 1
pass=0; fail=0; failed=""
for t in tests/${1:-*}.txt; do
  n=$(basename "$t" .txt); d=out/tests/$n; rm -rf "$d"; mkdir -p "$d"
  if ./triffall_headless "$t" "$d/" > "$d.log" 2>&1; then pass=$((pass+1)); else fail=$((fail+1)); failed="$failed $n"; fi
done
echo "passed $pass, failed $fail${failed:+:$failed}  (logs in out/tests/)"
[ "$fail" -eq 0 ]
