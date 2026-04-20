#!/bin/bash
# PMU benchmark ladder across all historical commits.
# Must be run with sudo.
# Usage: sudo bash test/run_pmu_ladder_full.sh

set -e
cd "$(dirname "$0")/.."
ROOT="$(pwd)"
SRC="$ROOT/src"
TEST_BUILD="$ROOT/test/build"
HANDS="$ROOT/hands/list9.txt"
OUTDIR="$TEST_BUILD/pmu_ladder_full"
mkdir -p "$OUTDIR"

echo "=== PMU Full Commit Ladder ==="
echo "Root: $ROOT"
echo ""

run_benchmark() {
  local label="$1"
  local logfile="$OUTDIR/${label}.log"
  echo "--- Running: $label ---"
  cd "$SRC" && make -j8 2>&1 | tail -1
  cp "$SRC/build/libdds.so" "$TEST_BUILD/build/"
  cd "$TEST_BUILD"
  ./alpha_mu_prototype benchmark_alpha "$HANDS" 2 1 \
    --parallel serial --board-workers 1 --root-workers 1 \
    2>&1 | tee "$logfile"
  echo ""
}

COMMITS=(
  "170e566:0_m1max_baseline"
  "9b7d54a:1_cache_layout_stages"
  "2b4b27c:2_revert_depthlocal"
  "96d02a3:3_phase1_neon"
  "4f218e6:4_phase2_clz"
  "bcdd518:5_modernisation"
  "fa2280e:6_head"
)

for entry in "${COMMITS[@]}"; do
  commit="${entry%%:*}"
  label="${entry##*:}"
  echo "=== Checking out src/ from $commit ($label) ==="
  cd "$ROOT"
  git checkout "$commit" -- src/
  run_benchmark "$label"
done

# --- Restore HEAD ---
echo "=== Restoring HEAD ==="
cd "$ROOT"
git checkout fa2280e -- src/

# --- Summary ---
echo ""
echo "========================================="
echo "PMU FULL COMMIT LADDER SUMMARY"
echo "========================================="
for f in "$OUTDIR"/*.log; do
  label=$(basename "$f" .log)
  line=$(grep "ALPHA_MU_BENCHMARK method=" "$f" 2>/dev/null | grep -v BOARD || true)
  if [ -n "$line" ]; then
    echo ""
    echo "[$label]"
    echo "$line" | tr ' ' '\n' | grep -E "cpu_seconds|pmu_"
  fi
done
echo ""
echo "Done."

