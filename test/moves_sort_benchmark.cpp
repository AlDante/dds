#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <random>
#include <vector>
#include <time.h>

#ifdef __APPLE__
#include <pthread/qos.h>
#endif

#include "../src/Moves.h"

namespace
{
  static const int kMaxSortMoves = 12;
  static const int kPatternCount = 5;
  static const int kWarmupRounds = 1;
  static const int kMeasuredRounds = 9;
  static const int kSamplesPerCase = 4096;

  volatile uint64_t gSink = 0;

  enum SortImplementation
  {
    SORT_IMPLEMENTATION_MERGE = 0,
    SORT_IMPLEMENTATION_CYCLE = 1
  };

  struct SortSample
  {
    moveType values[kMaxSortMoves];
    int count;
    int pattern;
  };

  typedef void (*SortFunction)(moveType *, int);

  double ThreadCpuSeconds()
  {
    struct timespec ts;
    clock_gettime(CLOCK_THREAD_CPUTIME_ID, &ts);
    return static_cast<double>(ts.tv_sec) +
      static_cast<double>(ts.tv_nsec) / 1e9;
  }

  const char * PatternName(const int pattern)
  {
    switch (pattern)
    {
      case 0:
        return "random_wide";
      case 1:
        return "random_dup_heavy";
      case 2:
        return "already_desc";
      case 3:
        return "reverse_asc";
      case 4:
        return "almost_desc";
      default:
        return "unknown";
    }
  }

  const char * SortName(const SortImplementation impl)
  {
    return (impl == SORT_IMPLEMENTATION_CYCLE ? "CycleSort" : "MergeSort");
  }

  uint64_t MixChecksum(
    uint64_t checksum,
    const moveType& move,
    const int index)
  {
    checksum ^= static_cast<uint64_t>(static_cast<unsigned short>(move.weight))
      + (static_cast<uint64_t>(static_cast<unsigned short>(move.rank)) << 16)
      + (static_cast<uint64_t>(static_cast<unsigned short>(move.suit)) << 24)
      + (static_cast<uint64_t>(static_cast<unsigned short>(move.sequence)) << 32)
      + (static_cast<uint64_t>(index) << 48);
    checksum *= 1099511628211ULL;
    return checksum;
  }

  uint64_t SampleChecksum(const SortSample& sample)
  {
    uint64_t checksum = 1469598103934665603ULL;
    checksum ^= static_cast<uint64_t>(sample.count);
    checksum *= 1099511628211ULL;
    checksum ^= static_cast<uint64_t>(sample.pattern);
    checksum *= 1099511628211ULL;

    for (int i = 0; i < sample.count; i++)
      checksum = MixChecksum(checksum, sample.values[i], i);

    return checksum;
  }

  void FillWeights(
    short weights[kMaxSortMoves],
    const int count,
    const int pattern,
    std::mt19937& rng)
  {
    switch (pattern)
    {
      case 0:
      {
        std::uniform_int_distribution<int> weightDist(-96, 160);
        for (int i = 0; i < count; i++)
          weights[i] = static_cast<short>(weightDist(rng));
        break;
      }
      case 1:
      {
        std::uniform_int_distribution<int> weightDist(-12, 20);
        for (int i = 0; i < count; i++)
          weights[i] = static_cast<short>(weightDist(rng));
        break;
      }
      case 2:
      {
        for (int i = 0; i < count; i++)
          weights[i] = static_cast<short>(120 - 3 * i);
        break;
      }
      case 3:
      {
        for (int i = 0; i < count; i++)
          weights[i] = static_cast<short>(-24 + 4 * i);
        break;
      }
      case 4:
      {
        for (int i = 0; i < count; i++)
          weights[i] = static_cast<short>(96 - 5 * i);
        if (count >= 4)
        {
          std::swap(weights[count / 2], weights[count / 2 - 1]);
          weights[count - 1] = weights[count - 2];
        }
        break;
      }
      default:
      {
        for (int i = 0; i < count; i++)
          weights[i] = 0;
        break;
      }
    }
  }

  SortSample BuildSample(
    const int count,
    const int pattern,
    std::mt19937& rng)
  {
    SortSample sample;
    sample.count = count;
    sample.pattern = pattern;

    short weights[kMaxSortMoves];
    FillWeights(weights, count, pattern, rng);

    std::uniform_int_distribution<int> suitDist(0, DDS_SUITS - 1);
    std::uniform_int_distribution<int> rankDist(2, 14);
    std::uniform_int_distribution<int> sequenceDist(0, 0x1fff);

    for (int i = 0; i < count; i++)
    {
      sample.values[i].weight = weights[i];
      sample.values[i].rank = static_cast<short>(rankDist(rng));
      sample.values[i].suit = static_cast<short>(suitDist(rng));
      sample.values[i].sequence = static_cast<short>(sequenceDist(rng));
    }

    for (int i = count; i < kMaxSortMoves; i++)
    {
      sample.values[i].weight = 0;
      sample.values[i].rank = 0;
      sample.values[i].suit = 0;
      sample.values[i].sequence = 0;
    }

    return sample;
  }

  std::vector<SortSample> BuildCorpus()
  {
    std::vector<SortSample> corpus;
    corpus.reserve(static_cast<size_t>(11 * kPatternCount * kSamplesPerCase));

    std::mt19937 rng(0x5eed1234U);
    for (int count = 2; count <= kMaxSortMoves; count++)
    {
      for (int pattern = 0; pattern < kPatternCount; pattern++)
      {
        for (int sampleNo = 0; sampleNo < kSamplesPerCase; sampleNo++)
          corpus.push_back(BuildSample(count, pattern, rng));
      }
    }

    return corpus;
  }

  bool SameMoves(
    const moveType * lhs,
    const moveType * rhs,
    const int count)
  {
    for (int i = 0; i < count; i++)
    {
      if (lhs[i].weight != rhs[i].weight ||
          lhs[i].rank != rhs[i].rank ||
          lhs[i].suit != rhs[i].suit ||
          lhs[i].sequence != rhs[i].sequence)
        return false;
    }

    return true;
  }

  void PrintMoves(
    const char * label,
    const moveType * moves,
    const int count)
  {
    std::fprintf(stderr, "%s", label);
    for (int i = 0; i < count; i++)
    {
      std::fprintf(stderr, " [%d:%d/%d/%d/%d]",
        i,
        static_cast<int>(moves[i].weight),
        static_cast<int>(moves[i].rank),
        static_cast<int>(moves[i].suit),
        static_cast<int>(moves[i].sequence));
    }
    std::fprintf(stderr, "\n");
  }

  void VerifyExactEquivalence(const std::vector<SortSample>& corpus)
  {
    for (size_t sampleNo = 0; sampleNo < corpus.size(); sampleNo++)
    {
      moveType mergeMoves[kMaxSortMoves];
      moveType cycleMoves[kMaxSortMoves];
      const SortSample& sample = corpus[sampleNo];

      std::memcpy(mergeMoves, sample.values,
        static_cast<size_t>(sample.count) * sizeof(moveType));
      std::memcpy(cycleMoves, sample.values,
        static_cast<size_t>(sample.count) * sizeof(moveType));

      Moves::MergeSort(mergeMoves, sample.count);
      Moves::CycleSort(cycleMoves, sample.count);

      if (! SameMoves(mergeMoves, cycleMoves, sample.count))
      {
        std::fprintf(stderr,
          "Mismatch for sample %zu count=%d pattern=%s\n",
          sampleNo, sample.count, PatternName(sample.pattern));
        PrintMoves("input ", sample.values, sample.count);
        PrintMoves("merge ", mergeMoves, sample.count);
        PrintMoves("cycle ", cycleMoves, sample.count);
        std::exit(1);
      }
    }
  }

  uint64_t RunCopyPass(
    const std::vector<SortSample>& source,
    std::vector<SortSample>& work)
  {
    work = source;

    uint64_t checksum = 1469598103934665603ULL;
    for (size_t i = 0; i < work.size(); i++)
      checksum ^= SampleChecksum(work[i]) + 0x9e3779b97f4a7c15ULL * (i + 1);

    return checksum;
  }

  uint64_t RunSortPass(
    const std::vector<SortSample>& source,
    std::vector<SortSample>& work,
    SortFunction sorter)
  {
    work = source;

    uint64_t checksum = 1469598103934665603ULL;
    for (size_t i = 0; i < work.size(); i++)
    {
      sorter(work[i].values, work[i].count);
      checksum ^= SampleChecksum(work[i]) + 0x9e3779b97f4a7c15ULL * (i + 1);
    }

    return checksum;
  }

  double TimeCopyPasses(
    const std::vector<SortSample>& source,
    std::vector<SortSample>& work,
    const int passes,
    uint64_t& checksum)
  {
    checksum = 1469598103934665603ULL;
    const double start = ThreadCpuSeconds();
    for (int pass = 0; pass < passes; pass++)
      checksum = checksum * 1099511628211ULL + RunCopyPass(source, work);
    const double elapsed = ThreadCpuSeconds() - start;
    gSink ^= checksum;
    return elapsed;
  }

  double TimeSortPasses(
    const std::vector<SortSample>& source,
    std::vector<SortSample>& work,
    const int passes,
    SortFunction sorter,
    uint64_t& checksum)
  {
    checksum = 1469598103934665603ULL;
    const double start = ThreadCpuSeconds();
    for (int pass = 0; pass < passes; pass++)
      checksum = checksum * 1099511628211ULL + RunSortPass(source, work, sorter);
    const double elapsed = ThreadCpuSeconds() - start;
    gSink ^= checksum;
    return elapsed;
  }

  double Median(std::vector<double> values)
  {
    const size_t mid = values.size() / 2;
    std::nth_element(values.begin(),
      values.begin() + static_cast<std::ptrdiff_t>(mid), values.end());
    const double high = values[mid];
    if ((values.size() & 1U) != 0U)
      return high;

    std::nth_element(values.begin(),
      values.begin() + static_cast<std::ptrdiff_t>(mid - 1), values.end());
    return 0.5 * (values[mid - 1] + high);
  }

  int ChoosePasses(const std::vector<SortSample>& source)
  {
    std::vector<SortSample> work(source.size());
    int passes = 1;

    while (passes < 256)
    {
      uint64_t checksum = 0;
      const double elapsed = TimeSortPasses(
        source, work, passes, &Moves::MergeSort, checksum);
      if (elapsed >= 0.20)
        return passes;
      passes *= 2;
    }

    return passes;
  }
}


int main()
{
#ifdef __APPLE__
  (void) pthread_set_qos_class_self_np(QOS_CLASS_USER_INITIATED, 0);
#endif

  const std::vector<SortSample> corpus = BuildCorpus();
  VerifyExactEquivalence(corpus);

  const int passes = ChoosePasses(corpus);
  std::vector<SortSample> work(corpus.size());

  std::vector<double> copyRuns;
  std::vector<double> mergeRuns;
  std::vector<double> cycleRuns;
  std::vector<double> mergeNetRuns;
  std::vector<double> cycleNetRuns;

  copyRuns.reserve(kMeasuredRounds);
  mergeRuns.reserve(kMeasuredRounds);
  cycleRuns.reserve(kMeasuredRounds);
  mergeNetRuns.reserve(kMeasuredRounds);
  cycleNetRuns.reserve(kMeasuredRounds);

  uint64_t mergeReferenceChecksum = 0;
  uint64_t cycleReferenceChecksum = 0;

  for (int round = 0; round < kWarmupRounds + kMeasuredRounds; round++)
  {
    uint64_t copyChecksum = 0;
    const double copyElapsed = TimeCopyPasses(corpus, work, passes, copyChecksum);

    uint64_t firstChecksum = 0;
    uint64_t secondChecksum = 0;
    double firstElapsed;
    double secondElapsed;
    SortImplementation firstImpl;

    if ((round & 1) == 0)
    {
      firstImpl = SORT_IMPLEMENTATION_MERGE;
      firstElapsed = TimeSortPasses(corpus, work, passes, &Moves::MergeSort, firstChecksum);
      secondElapsed = TimeSortPasses(corpus, work, passes, &Moves::CycleSort, secondChecksum);
    }
    else
    {
      firstImpl = SORT_IMPLEMENTATION_CYCLE;
      firstElapsed = TimeSortPasses(corpus, work, passes, &Moves::CycleSort, firstChecksum);
      secondElapsed = TimeSortPasses(corpus, work, passes, &Moves::MergeSort, secondChecksum);
    }

    uint64_t mergeChecksum = 0;
    uint64_t cycleChecksum = 0;
    double mergeElapsed = 0.0;
    double cycleElapsed = 0.0;

    if (firstImpl == SORT_IMPLEMENTATION_MERGE)
    {
      mergeChecksum = firstChecksum;
      mergeElapsed = firstElapsed;
      cycleChecksum = secondChecksum;
      cycleElapsed = secondElapsed;
    }
    else
    {
      cycleChecksum = firstChecksum;
      cycleElapsed = firstElapsed;
      mergeChecksum = secondChecksum;
      mergeElapsed = secondElapsed;
    }

    if (round == 0)
    {
      mergeReferenceChecksum = mergeChecksum;
      cycleReferenceChecksum = cycleChecksum;
    }
    else if (mergeChecksum != mergeReferenceChecksum ||
             cycleChecksum != cycleReferenceChecksum ||
             mergeChecksum != cycleChecksum)
    {
      std::fprintf(stderr, "Checksum mismatch across benchmark rounds\n");
      return 1;
    }

    if (round < kWarmupRounds)
      continue;

    copyRuns.push_back(copyElapsed);
    mergeRuns.push_back(mergeElapsed);
    cycleRuns.push_back(cycleElapsed);
    mergeNetRuns.push_back(mergeElapsed - copyElapsed);
    cycleNetRuns.push_back(cycleElapsed - copyElapsed);
  }

  const double copyMedian = Median(copyRuns);
  const double mergeMedian = Median(mergeRuns);
  const double cycleMedian = Median(cycleRuns);
  const double mergeNetMedian = Median(mergeNetRuns);
  const double cycleNetMedian = Median(cycleNetRuns);
  const double speedupPct = 100.0 * (mergeNetMedian - cycleNetMedian) /
    mergeNetMedian;

  std::printf("# moves_sort_benchmark\n\n");
  std::printf("- Corpus: %zu samples (%d patterns x 11 sizes x %d samples/case)\n",
    corpus.size(), kPatternCount, kSamplesPerCase);
  std::printf("- Sizes covered: 2..12\n");
  std::printf("- Patterns: ");
  for (int pattern = 0; pattern < kPatternCount; pattern++)
  {
    std::printf("%s%s", PatternName(pattern),
      (pattern + 1 == kPatternCount ? "\n" : ", "));
  }
  std::printf("- Timing method: CLOCK_THREAD_CPUTIME_ID medians over %d measured rounds after %d warmup round\n",
    kMeasuredRounds, kWarmupRounds);
  std::printf("- Passes per round: %d\n", passes);
  std::printf("- Exact output match: yes\n");
  std::printf("- Merge checksum: 0x%016llx\n",
    static_cast<unsigned long long>(mergeReferenceChecksum));
  std::printf("- Cycle checksum: 0x%016llx\n\n",
    static_cast<unsigned long long>(cycleReferenceChecksum));

  std::printf("| Implementation | Median total CPU (s) | Median net sort CPU (s) | Relative vs Merge net |\n");
  std::printf("| --- | ---: | ---: | ---: |\n");
  std::printf("| Copy baseline | %.6f | 0.000000 | -- |\n", copyMedian);
  std::printf("| %s | %.6f | %.6f | baseline |\n",
    SortName(SORT_IMPLEMENTATION_MERGE), mergeMedian, mergeNetMedian);
  std::printf("| %s | %.6f | %.6f | %+.2f%% |\n\n",
    SortName(SORT_IMPLEMENTATION_CYCLE), cycleMedian, cycleNetMedian,
    -speedupPct);

  if (cycleNetMedian < mergeNetMedian)
  {
    std::printf("Result: CycleSort is faster by %.2f%% on median net sort CPU time.\n",
      speedupPct);
  }
  else
  {
    std::printf("Result: CycleSort is slower by %.2f%% on median net sort CPU time.\n",
      -speedupPct);
  }

  return 0;
}

