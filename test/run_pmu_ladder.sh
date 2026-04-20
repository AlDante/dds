#!/bin/bash
# PMU benchmark ladder: measures cycles, instructions, branch mispredictions,
# and L1D cache misses for each cache-layout variant.
# Must be run with sudo.
# Usage: sudo bash test/run_pmu_ladder.sh

set -e
cd "$(dirname "$0")/.."
ROOT="$(pwd)"
SRC="$ROOT/src"
TEST_BUILD="$ROOT/test/build"
HANDS="$ROOT/hands/list9.txt"
OUTDIR="$TEST_BUILD/pmu_ladder"
mkdir -p "$OUTDIR"

echo "=== PMU Benchmark Ladder ==="
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

# --- Variant 0: Baseline (170e566) ---
echo "=== Restoring baseline (170e566) ==="
cd "$ROOT"
git checkout 170e566 -- src/dds.h src/Memory.h src/ABsearch.cpp src/ABsearch_m1max.cpp src/ABsearch.h
run_benchmark "0_baseline"

# --- Variant 1: +8.1 Hot/cold ThreadData split ---
echo "=== Applying 8.1: Hot/cold ThreadData split ==="
cd "$ROOT"
git checkout 170e566 -- src/dds.h src/Memory.h src/ABsearch.cpp src/ABsearch_m1max.cpp src/ABsearch.h
# Apply hot/cold split to Memory.h (move analysisFlag + memUsed to cold struct)
python3 -c "
import re
path = '$SRC/Memory.h'
with open(path) as f:
    content = f.read()

old = '''struct ThreadData
{
  int nodeTypeStore[DDS_HANDS];
  int iniDepth;
  bool val;

  unsigned short int suit[DDS_HANDS][DDS_SUITS];
  int trump;

  pos lookAheadPos; // Recursive alpha-beta data
  bool analysisFlag;
  unsigned short int lowestWin[50][DDS_SUITS];
  WinnersType winners[13];
  moveType forbiddenMoves[14];
  moveType bestMove[50];
  moveType bestMoveTT[50];

  double memUsed;
  int nodes;
  int trickNodes;'''

new = '''struct ThreadDataHot
{
  int nodeTypeStore[DDS_HANDS];
  int iniDepth;
  bool val;

  unsigned short int suit[DDS_HANDS][DDS_SUITS];
  int trump;

  pos lookAheadPos; // Recursive alpha-beta data
  unsigned short int lowestWin[50][DDS_SUITS];
  WinnersType winners[13];
  moveType forbiddenMoves[14];
  moveType bestMove[50];
  moveType bestMoveTT[50];
  int nodes;
  int trickNodes;'''

content = content.replace(old, new)

# Insert ThreadDataCold and ThreadData after the closing brace of what was ThreadData
old2 = '''};


class Memory'''
new2 = '''};


struct ThreadDataCold
{
  bool analysisFlag;
  double memUsed;
};


struct ThreadData:
  public ThreadDataHot,
  public ThreadDataCold
{
};


class Memory'''
content = content.replace(old2, new2, 1)
with open(path, 'w') as f:
    f.write(content)
print('Applied 8.1 hot/cold split')
"
run_benchmark "1_hotcold"

# --- Variant 2: +8.5 pos field reorder ---
echo "=== Applying 8.5: pos field reorder ==="
cd "$ROOT"
git checkout 170e566 -- src/dds.h src/Memory.h src/ABsearch.cpp src/ABsearch_m1max.cpp src/ABsearch.h
python3 -c "
path = '$SRC/dds.h'
with open(path) as f:
    content = f.read()

old = '''struct pos
{
  unsigned short int rankInSuit[DDS_HANDS][DDS_SUITS];
  unsigned short int aggr[DDS_SUITS];
  unsigned char length[DDS_HANDS][DDS_SUITS];
  int handDist[DDS_HANDS];

  unsigned short int winRanks[50][DDS_SUITS];
  /* Cards that win by rank, firstindex is depth. */
  int first[50];
  /* Hand that leads the trick for each ply */
  moveType move[50];
  /* Presently winning move */
  int handRelFirst;
  /* The current hand, relative first hand */
  int tricksMAX;
  /* Aggregated tricks won by MAX */
  highCardType winner[DDS_SUITS];
  /* Winning rank of trick. */
  highCardType secondBest[DDS_SUITS];
  /* Second best rank. */
};'''

new = '''struct pos
{
  unsigned short int rankInSuit[DDS_HANDS][DDS_SUITS];
  unsigned short int aggr[DDS_SUITS];
  unsigned char length[DDS_HANDS][DDS_SUITS];
  int handDist[DDS_HANDS];

  highCardType winner[DDS_SUITS];
  /* Winning rank of trick. */
  highCardType secondBest[DDS_SUITS];
  /* Second best rank. */
  int first[50];
  /* Hand that leads the trick for each ply */
  moveType move[50];
  /* Presently winning move */
  unsigned short int winRanks[50][DDS_SUITS];
  /* Cards that win by rank, firstindex is depth. */
  int tricksMAX;
  /* Aggregated tricks won by MAX */
  int handRelFirst;
  /* The current hand, relative first hand */
};'''

content = content.replace(old, new)
with open(path, 'w') as f:
    f.write(content)
print('Applied 8.5 pos field reorder')
"
run_benchmark "2_pos_reorder"

# --- Variant 3: +8.4 Packed moveType ---
echo "=== Applying 8.4: Packed moveType (int -> short) ==="
cd "$ROOT"
git checkout 170e566 -- src/dds.h src/Memory.h src/ABsearch.cpp src/ABsearch_m1max.cpp src/ABsearch.h
python3 -c "
path = '$SRC/dds.h'
with open(path) as f:
    content = f.read()

old = '''struct moveType
{
  int suit;
  int rank;
  int sequence; /* Whether or not this move is the
                                     first in a sequence */
  int weight; /* Weight used at sorting */
};'''

new = '''struct moveType
{
  short suit;
  short rank;
  short sequence; /* Whether or not this move is the
                                       first in a sequence */
  short weight; /* Weight used at sorting */
};

static_assert(sizeof(moveType) == 8, \"moveType should remain compact\");'''

content = content.replace(old, new)
with open(path, 'w') as f:
    f.write(content)
print('Applied 8.4 packed moveType')
"
run_benchmark "3_packed_movetype"

# --- Restore HEAD ---
echo "=== Restoring HEAD ==="
cd "$ROOT"
git checkout fa2280e -- src/

# --- Summary ---
echo ""
echo "========================================="
echo "PMU BENCHMARK LADDER SUMMARY"
echo "========================================="
for f in "$OUTDIR"/*.log; do
  label=$(basename "$f" .log)
  line=$(grep "ALPHA_MU_BENCHMARK method=" "$f" 2>/dev/null || true)
  if [ -n "$line" ]; then
    echo ""
    echo "[$label]"
    echo "$line" | tr ' ' '\n' | grep -E "cpu_seconds|pmu_"
  fi
done
echo ""
echo "Done."

