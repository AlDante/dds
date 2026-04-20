#!/bin/zsh
# CPU-time benchmark ladder: builds libdds.so from each commit and runs
# the serial alpha-mu benchmark using the current test binary.
#
# Usage: ./run_cpu_benchmark_ladder.sh
#
# Output: one log file per commit in test/build/cpu_ladder/

set -e

REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
SRC_DIR="$REPO_ROOT/src"
TEST_DIR="$REPO_ROOT/test"
OUTPUT_DIR="$TEST_DIR/build/cpu_ladder"
HANDS="$REPO_ROOT/hands/list9.txt"

mkdir -p "$OUTPUT_DIR"

# Commits to benchmark (oldest to newest).
# Each entry: "short_sha label"
COMMITS=(
  "170e566 m1-max-path"
  "9b7d54a cache-layout-stages"
  "2b4b27c revert-depthlocal"
  "96d02a3 phase1-neon"
  "4f218e6 phase2-clz"
  "bcdd518 modernisation"
  "fa2280e head-reverted-quicktricks"
)

CURRENT_BRANCH="$(git -C "$REPO_ROOT" rev-parse --abbrev-ref HEAD)"
CURRENT_SHA="$(git -C "$REPO_ROOT" rev-parse HEAD)"

echo "=== CPU Benchmark Ladder ==="
echo "Output: $OUTPUT_DIR"
echo "Benchmark: serial, list9, depth 2"
echo ""

for entry in "${COMMITS[@]}"; do
  SHA="${entry%% *}"
  LABEL="${entry#* }"
  LOGFILE="$OUTPUT_DIR/${LABEL}.log"

  echo "--- [$LABEL] commit $SHA ---"

  # Checkout just the src/ directory from this commit
  git -C "$REPO_ROOT" checkout "$SHA" -- src/

  # Build the library
  (cd "$SRC_DIR" && make -j8 2>&1 | tail -1)

  # Restore src/ to current HEAD (so git status stays clean for next iteration)
  # Actually we'll restore at the end; just run the benchmark now.

  # Run the benchmark
  echo "  Running benchmark..."
  DYLD_LIBRARY_PATH="$SRC_DIR/build" \
    "$TEST_DIR/build/alpha_mu_prototype" benchmark_alpha "$HANDS" 2 0 \
    --parallel serial --board-workers 1 --root-workers 1 --dds-thread-id 0 \
    2>&1 | tee "$LOGFILE"

  echo "  Done: $LOGFILE"
  echo ""

  # Restore src/ to HEAD before next iteration
  git -C "$REPO_ROOT" checkout "$CURRENT_SHA" -- src/
done

# Rebuild the library from HEAD
echo "--- Rebuilding HEAD library ---"
(cd "$SRC_DIR" && make -j8 2>&1 | tail -1)

echo ""
echo "=== All benchmarks complete ==="
echo "Results in: $OUTPUT_DIR"

# Print summary
echo ""
echo "| Commit | Label | CPU seconds | Per board (CPU) | Wall seconds | Per board (wall) |"
echo "| --- | --- | ---: | ---: | ---: | ---: |"
for entry in "${COMMITS[@]}"; do
  LABEL="${entry#* }"
  LOGFILE="$OUTPUT_DIR/${LABEL}.log"
  if [ -f "$LOGFILE" ]; then
    LINE=$(grep "^ALPHA_MU_BENCHMARK method=" "$LOGFILE" | tail -1)
    TOTAL=$(echo "$LINE" | grep -o 'total_seconds=[0-9.]*' | cut -d= -f2)
    CPU=$(echo "$LINE" | grep -o 'cpu_seconds=[0-9.]*' | cut -d= -f2)
    PER_BOARD=$(echo "$LINE" | grep -o 'per_board_seconds=[0-9.]*' | cut -d= -f2)
    BOARDS=$(echo "$LINE" | grep -o 'boards=[0-9]*' | cut -d= -f2)
    if [ -n "$CPU" ] && [ -n "$BOARDS" ] && [ "$BOARDS" -gt 0 ]; then
      CPU_PER_BOARD=$(echo "scale=3; $CPU / $BOARDS" | bc)
    else
      CPU_PER_BOARD="N/A"
    fi
    SHA="${entry%% *}"
    echo "| $SHA | $LABEL | ${CPU:-N/A} | ${CPU_PER_BOARD} | ${TOTAL:-N/A} | ${PER_BOARD:-N/A} |"
  fi
done

