#!/bin/bash
# PMU benchmark ladder with 9b7d54a broken into individual stages.
# Must be run with sudo.
# Usage: sudo bash test/run_pmu_ladder_v2.sh

set -e
cd "$(dirname "$0")/.."
ROOT="$(pwd)"
SRC="$ROOT/src"
TEST_BUILD="$ROOT/test/build"
HANDS="$ROOT/hands/list9.txt"
OUTDIR="$TEST_BUILD/pmu_ladder_v2"
mkdir -p "$OUTDIR"

echo "=== PMU Benchmark Ladder v2 (with broken-out cache stages) ==="
echo "Root: $ROOT"
echo ""

run_benchmark() {
  local label="$1"
  local logfile="$OUTDIR/${label}.log"
  echo "--- Running: $label ---"
  cd "$SRC" && make -j8 2>&1 | tail -1
  cp "$SRC/build/libdds.so" "$TEST_BUILD/build/"
  cd "$TEST_BUILD"
  ./alpha_mu benchmark_alpha "$HANDS" 2 1 \
    --parallel serial --board-workers 1 --root-workers 1 \
    2>&1 | tee "$logfile"
  echo ""
}

# --- 0: Baseline (170e566) ---
echo "=== 0: Baseline (170e566) ==="
cd "$ROOT"
git checkout 170e566 -- src/
run_benchmark "0_baseline"

# --- 1: +8.1 Hot/cold ThreadData split ---
echo "=== 1: +8.1 Hot/cold ThreadData split ==="
cd "$ROOT"
git checkout 170e566 -- src/
python3 -c "
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
run_benchmark "1_hotcold_8p1"

# --- 2: +8.5 pos field reorder ---
echo "=== 2: +8.5 pos field reorder ==="
cd "$ROOT"
git checkout 170e566 -- src/
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
run_benchmark "2_pos_reorder_8p5"

# --- 3: +8.4 Packed moveType ---
echo "=== 3: +8.4 Packed moveType ==="
cd "$ROOT"
git checkout 170e566 -- src/
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
run_benchmark "3_packed_movetype_8p4"

# --- 4: Full 9b7d54a (all cache-layout + DepthLocal) ---
echo "=== 4: Full 9b7d54a ==="
cd "$ROOT"
git checkout 9b7d54a -- src/
run_benchmark "4_cache_layout_full_9b7d54a"

# --- 5: 2b4b27c Revert DepthLocal ---
echo "=== 5: 2b4b27c Revert DepthLocal ==="
cd "$ROOT"
git checkout 2b4b27c -- src/
run_benchmark "5_revert_depthlocal_2b4b27c"

# --- 6: 96d02a3 Phase 1: NEON ---
echo "=== 6: 96d02a3 Phase 1: NEON ==="
cd "$ROOT"
git checkout 96d02a3 -- src/
run_benchmark "6_neon_96d02a3"

# --- 7: 4f218e6 Phase 2: CLZ ---
echo "=== 7: 4f218e6 Phase 2: CLZ ==="
cd "$ROOT"
git checkout 4f218e6 -- src/
run_benchmark "7_clz_4f218e6"

# --- 8: bcdd518 Modernisation ---
echo "=== 8: bcdd518 Modernisation ==="
cd "$ROOT"
git checkout bcdd518 -- src/
run_benchmark "8_modernisation_bcdd518"

# --- 9: fa2280e HEAD ---
echo "=== 9: fa2280e HEAD ==="
cd "$ROOT"
git checkout fa2280e -- src/
run_benchmark "9_head_fa2280e"

# --- Restore HEAD ---
echo "=== Restoring HEAD ==="
cd "$ROOT"
git checkout fa2280e -- src/

# --- Summary ---
echo ""
echo "========================================="
echo "PMU LADDER v2 SUMMARY"
echo "========================================="
for f in "$OUTDIR"/*.log; do
  label=$(basename "$f" .log)
  line=$(grep "ALPHA_MU_BENCHMARK method=" "$f" 2>/dev/null | grep -v BOARD || true)
  if [ -n "$line" ]; then
    echo ""
    echo "[$label]"
    echo "$line" | tr ' ' '\n' | grep -E "total_seconds|cpu_seconds|pmu_"
  fi
done
echo ""
echo "Done."

