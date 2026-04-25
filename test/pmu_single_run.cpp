// pmu_single_run.cpp — Run a single serial alpha-mu benchmark board with PMU counters.
// Compile alongside the existing alpha_mu object files.
// Must run with sudo for PMU access.

#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <mach/mach_time.h>
#include "pmu_counters.h"
#include "alpha_mu_core.h"

using namespace alpha_mu;

int main(int argc, char* argv[])
{
  if (argc < 4)
  {
    fprintf(stderr, "Usage: sudo %s <hands_file> <depth> <max_boards>\n", argv[0]);
    return 1;
  }

  const char* handsFile = argv[1];
  int depth = atoi(argv[2]);
  int maxBoards = atoi(argv[3]);

  // Init PMU
  PmuCounters pmu;
  bool hasPmu = pmu.Init();
  if (!hasPmu)
    fprintf(stderr, "WARNING: PMU not available (run with sudo); will report times only.\n");

  // Run the benchmark through the existing API, wrapping with PMU
  AlphaMuBenchmarkOptions opts;
  opts.handFile = handsFile;
  opts.depth = depth;
  opts.maxBoards = maxBoards;
  opts.parallelMode = ALPHA_MU_PARALLEL_SERIAL;
  opts.boardWorkers = 1;
  opts.rootWorkers = 1;
  opts.ddsThreadId = 0;

  // CPU time
  struct timespec cpuStart, cpuEnd;
  clock_gettime(CLOCK_THREAD_CPUTIME_ID, &cpuStart);

  // Wall time
  uint64_t wallStart = mach_absolute_time();

  // PMU start
  PmuCounterSet pmuStart = {};
  if (hasPmu) pmuStart = pmu.Read();

  // Run
  BenchmarkMethodSummary summary = BenchmarkAlphaMuExactBoards(opts);

  // PMU end
  PmuCounterSet pmuEnd = {};
  if (hasPmu) pmuEnd = pmu.Read();

  // Wall time end
  uint64_t wallEnd = mach_absolute_time();

  // CPU time end
  clock_gettime(CLOCK_THREAD_CPUTIME_ID, &cpuEnd);

  // Compute
  PmuCounterSet d = PmuCounters::Diff(pmuEnd, pmuStart);

  mach_timebase_info_data_t tbInfo;
  mach_timebase_info(&tbInfo);
  double wallSec = static_cast<double>(wallEnd - wallStart)
    * tbInfo.numer / tbInfo.denom / 1e9;
  double cpuSec = static_cast<double>(cpuEnd.tv_sec - cpuStart.tv_sec)
    + static_cast<double>(cpuEnd.tv_nsec - cpuStart.tv_nsec) / 1e9;

  int boards = static_cast<int>(summary.boardsTested);
  double wallPerBoard = (boards > 0) ? wallSec / boards : 0;
  double cpuPerBoard = (boards > 0) ? cpuSec / boards : 0;
  double ipc = (d.cycles > 0)
    ? static_cast<double>(d.instructions) / static_cast<double>(d.cycles)
    : 0.0;

  printf("\n=== PMU Single Run Results ===\n");
  printf("Boards:          %d\n", boards);
  printf("Mismatches:      %u\n", summary.mismatches);
  printf("Wall total:      %.2f s\n", wallSec);
  printf("CPU total:       %.2f s\n", cpuSec);
  printf("Wall/board:      %.2f s\n", wallPerBoard);
  printf("CPU/board:       %.2f s\n", cpuPerBoard);

  if (hasPmu)
  {
    printf("Cycles:          %.2f B\n", d.cycles / 1e9);
    printf("Instructions:    %.2f B\n", d.instructions / 1e9);
    printf("IPC:             %.2f\n", ipc);
    printf("Branch Mispred:  %.0f M\n", d.branchMispredictions / 1e6);
    printf("L1D Miss Ld:     %.3f B\n", d.l1dCacheMissLd / 1e9);
    printf("L1D Miss St:     %.3f B\n", d.l1dCacheMissSt / 1e9);
  }

  printf("\n| Label | Wall/board (s) | CPU/board (s) | Cycles (B) | Instructions (B) | IPC | Branch Mispred (M) | L1D Miss Ld (B) | L1D Miss St (B) |\n");
  printf("|-------|---------------|--------------|-----------|-----------------|-----|-------------------|----------------|----------------|\n");
  printf("| Current HEAD | %.2f | %.2f | %.2f | %.2f | %.2f | %.0f | %.3f | %.3f |\n",
    wallPerBoard, cpuPerBoard,
    d.cycles / 1e9, d.instructions / 1e9, ipc,
    d.branchMispredictions / 1e6,
    d.l1dCacheMissLd / 1e9,
    d.l1dCacheMissSt / 1e9);

  return 0;
}
