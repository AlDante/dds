// pmu_counters.h — Lightweight Apple Silicon PMU counter access via kpc
// Requires running as root (sudo) or with performance monitoring entitlement
#ifndef PMU_COUNTERS_H
#define PMU_COUNTERS_H

#include <cstdint>
#include <cstdio>
#include <dlfcn.h>

// KPC constants
#define KPC_CLASS_FIXED          (0)
#define KPC_CLASS_CONFIGURABLE   (1)
#define KPC_CLASS_FIXED_MASK     (1u << KPC_CLASS_FIXED)
#define KPC_CLASS_CONFIGURABLE_MASK (1u << KPC_CLASS_CONFIGURABLE)

// Max counters (M1 has 2 fixed + 8 configurable per core)
#define KPC_MAX_COUNTERS 16

struct PmuCounterSet
{
  uint64_t cycles;
  uint64_t instructions;
  uint64_t branchMispredictions;
  uint64_t l1dCacheMissLd;
  uint64_t l1dCacheMissSt;
};

class PmuCounters
{
public:
  PmuCounters()
    : initialized_(false), kperfHandle_(nullptr)
  {
  }

  bool Init()
  {
    kperfHandle_ = dlopen(
      "/System/Library/PrivateFrameworks/kperf.framework/kperf",
      RTLD_NOW);
    if (!kperfHandle_)
    {
      fprintf(stderr, "PMU: Cannot load kperf framework: %s\n", dlerror());
      return false;
    }

    // Load function pointers
    kpc_get_thread_counters_ = reinterpret_cast<kpc_get_thread_counters_fn>(
      dlsym(kperfHandle_, "kpc_get_thread_counters"));
    kpc_set_counting_ = reinterpret_cast<kpc_set_counting_fn>(
      dlsym(kperfHandle_, "kpc_set_counting"));
    kpc_set_thread_counting_ = reinterpret_cast<kpc_set_thread_counting_fn>(
      dlsym(kperfHandle_, "kpc_set_thread_counting"));
    kpc_set_config_ = reinterpret_cast<kpc_set_config_fn>(
      dlsym(kperfHandle_, "kpc_set_config"));
    kpc_get_config_count_ = reinterpret_cast<kpc_get_config_count_fn>(
      dlsym(kperfHandle_, "kpc_get_config_count"));
    kpc_get_counter_count_ = reinterpret_cast<kpc_get_counter_count_fn>(
      dlsym(kperfHandle_, "kpc_get_counter_count"));
    kpc_force_all_ctrs_set_ = reinterpret_cast<kpc_force_all_ctrs_set_fn>(
      dlsym(kperfHandle_, "kpc_force_all_ctrs_set"));

    if (!kpc_get_thread_counters_ || !kpc_set_counting_ ||
        !kpc_set_thread_counting_ || !kpc_set_config_ ||
        !kpc_get_config_count_ || !kpc_get_counter_count_ ||
        !kpc_force_all_ctrs_set_)
    {
      fprintf(stderr, "PMU: Cannot resolve kpc symbols\n");
      return false;
    }

    // Force access to all counters
    int ret = kpc_force_all_ctrs_set_(1);
    if (ret != 0)
    {
      fprintf(stderr, "PMU: kpc_force_all_ctrs_set failed (%d). Run with sudo.\n", ret);
      return false;
    }

    // Configure counters:
    // M1 configurable events (from a14.plist):
    //   BRANCH_MISPRED_NONSPEC = 0xcb
    //   L1D_CACHE_MISS_LD_NONSPEC = 0xd3
    //   L1D_CACHE_MISS_ST_NONSPEC = 0xd4
    //   INST_ALL = 0x8c (but fixed counter 0 is instructions)
    // Fixed counters: counter 0 = cycles (FIXED_CYCLES), counter 1 = instructions (INST_ALL)

    uint32_t configCount = kpc_get_config_count_(KPC_CLASS_CONFIGURABLE_MASK);
    numConfigurable_ = configCount;

    // Configure configurable counters for our events.
    // From cpu_100000c_2_1b588bb3.plist (M1 Max):
    //   BRANCH_MISPRED_NONSPEC=203 (counters_mask=224, bits 5,6,7)
    //   L1D_CACHE_MISS_LD=163 (no mask restriction = any counter)
    //   L1D_CACHE_MISS_ST=162 (no mask restriction = any counter)
    uint64_t config[KPC_MAX_COUNTERS] = {};
    config[0] = 163;  // L1D_CACHE_MISS_LD (speculative, any counter)
    config[1] = 162;  // L1D_CACHE_MISS_ST (speculative, any counter)
    config[5] = 203;  // BRANCH_MISPRED_NONSPEC (counters_mask=224 -> bits 5,6,7)

    uint32_t classes = KPC_CLASS_FIXED_MASK | KPC_CLASS_CONFIGURABLE_MASK;

    ret = kpc_set_config_(classes, config);
    if (ret != 0)
    {
      fprintf(stderr, "PMU: kpc_set_config failed (%d)\n", ret);
      return false;
    }

    ret = kpc_set_counting_(classes);
    if (ret != 0)
    {
      fprintf(stderr, "PMU: kpc_set_counting failed (%d)\n", ret);
      return false;
    }

    ret = kpc_set_thread_counting_(classes);
    if (ret != 0)
    {
      fprintf(stderr, "PMU: kpc_set_thread_counting failed (%d)\n", ret);
      return false;
    }

    numCounters_ = kpc_get_counter_count_(classes);
    initialized_ = true;
    fprintf(stderr, "PMU: Initialized with %u configurable, %u total counters\n",
      numConfigurable_, numCounters_);
    return true;
  }

  PmuCounterSet Read()
  {
    PmuCounterSet result = {};
    if (!initialized_) return result;

    uint64_t counters[KPC_MAX_COUNTERS] = {};
    int ret = kpc_get_thread_counters_(0, numCounters_, counters);
    if (ret != 0) return result;

    // Fixed counters: [0]=cycles, [1]=instructions
    // Configurable: config[0] at idx 2, config[1] at idx 3, config[5] at idx 7
    result.cycles = counters[0];
    result.instructions = counters[1];
    result.l1dCacheMissLd = counters[2];        // config[0] = L1D_CACHE_MISS_LD
    result.l1dCacheMissSt = counters[3];        // config[1] = L1D_CACHE_MISS_ST
    result.branchMispredictions = counters[7];  // config[5] = BRANCH_MISPRED
    return result;
  }

  static PmuCounterSet Diff(const PmuCounterSet& end, const PmuCounterSet& start)
  {
    PmuCounterSet d;
    d.cycles = end.cycles - start.cycles;
    d.instructions = end.instructions - start.instructions;
    d.branchMispredictions = end.branchMispredictions - start.branchMispredictions;
    d.l1dCacheMissLd = end.l1dCacheMissLd - start.l1dCacheMissLd;
    d.l1dCacheMissSt = end.l1dCacheMissSt - start.l1dCacheMissSt;
    return d;
  }

  bool IsInitialized() const { return initialized_; }

  ~PmuCounters()
  {
    if (kperfHandle_)
    {
      if (kpc_force_all_ctrs_set_)
        kpc_force_all_ctrs_set_(0);
      dlclose(kperfHandle_);
    }
  }

private:
  bool initialized_;
  void * kperfHandle_;
  uint32_t numConfigurable_ = 0;
  uint32_t numCounters_ = 0;

  using kpc_get_thread_counters_fn = int (*)(int, uint32_t, uint64_t *);
  using kpc_set_counting_fn = int (*)(uint32_t);
  using kpc_set_thread_counting_fn = int (*)(uint32_t);
  using kpc_set_config_fn = int (*)(uint32_t, uint64_t *);
  using kpc_get_config_count_fn = uint32_t (*)(uint32_t);
  using kpc_get_counter_count_fn = uint32_t (*)(uint32_t);
  using kpc_force_all_ctrs_set_fn = int (*)(int);

  kpc_get_thread_counters_fn kpc_get_thread_counters_ = nullptr;
  kpc_set_counting_fn kpc_set_counting_ = nullptr;
  kpc_set_thread_counting_fn kpc_set_thread_counting_ = nullptr;
  kpc_set_config_fn kpc_set_config_ = nullptr;
  kpc_get_config_count_fn kpc_get_config_count_ = nullptr;
  kpc_get_counter_count_fn kpc_get_counter_count_ = nullptr;
  kpc_force_all_ctrs_set_fn kpc_force_all_ctrs_set_ = nullptr;
};

#endif // PMU_COUNTERS_H

