# Performance Log

This file records standardized post-change performance runs from `test/standard_performance.py`.

Each entry links to a timestamped result bundle under `test/build/performance_runs/`.

![Standardized performance trend graph](performance-log.svg)

_The graph uses two aligned logarithmic panels: cumulative runtime at the top and per-board runtime at the bottom for entries that record board counts._

_Entries that include a `Timing stabilization` section use warmup runs plus adaptive repeat counts for short workloads so the reported medians are stable to about `0.1 s` or better._

_If an entry includes `Graph outliers`, those workload values remain recorded below but are shown as hollow X markers and excluded from the corresponding trend line in the graph._

## 2026-04-12 10:10:11 — commit `2bec9d9` (dirty)

- Output bundle: `test/build/performance_runs/20260412-095932`
- Platform: `macOS-26.4-arm64-arm-64bit`
- Repeats per workload: `3`

| Workload | Median (s) | Mean (s) | Min (s) | Max (s) |
| --- | ---: | ---: | ---: | ---: |
| `regression_api_smoke` | 207.714 | 209.947 | 200.656 | 221.469 |
| `dtest_solve_list10` | 0.144 | 0.166 | 0.121 | 0.233 |
| `dtest_solve_list100` | 1.188 | 1.222 | 1.187 | 1.290 |
| `play_analysis_benchmark` | 0.107 | 0.106 | 0.103 | 0.109 |
| `alpha_mu_prototype_default` | 0.150 | 0.250 | 0.149 | 0.452 |
| `alpha_mu_prototype_bridge_dds` | 0.431 | 0.430 | 0.424 | 0.434 |

## 2026-04-12 11:21:28 — commit `2bec9d9` (dirty)

- Output bundle: `test/build/performance_runs/20260412-standard-baseline`
- Platform: `macOS-26.4-arm64-arm-64bit`
- Repeats per workload: `1`

| Workload | Median (s) | Mean (s) | Min (s) | Max (s) |
| --- | ---: | ---: | ---: | ---: |
| `regression_api_smoke` | 231.683 | 231.683 | 231.683 | 231.683 |
| `dtest_solve_list10` | 0.225 | 0.225 | 0.225 | 0.225 |
| `dtest_solve_list100` | 1.239 | 1.239 | 1.239 | 1.239 |
| `play_analysis_benchmark` | 0.177 | 0.177 | 0.177 | 0.177 |
| `alpha_mu_prototype_default` | 0.193 | 0.193 | 0.193 | 0.193 |
| `alpha_mu_prototype_bridge_dds` | 0.581 | 0.581 | 0.581 | 0.581 |

## 2026-04-12 12:45:42 — commit `e537788` (dirty)

- Output bundle: `test/build/performance_runs/20260412-124110`
- Platform: `macOS-26.4-arm64-arm-64bit`
- Repeats per workload: `1`
- Graph outliers: `alpha_mu_prototype_bridge_dds`
- Note: `alpha_mu_prototype_bridge_dds` used a temporary overly heavy targeted regression variant before the targeted-scope trim, so it is not directly comparable with later stabilized bridge-dds timings.

| Workload | Median (s) | Mean (s) | Min (s) | Max (s) |
| --- | ---: | ---: | ---: | ---: |
| `regression_api_smoke` | 222.035 | 222.035 | 222.035 | 222.035 |
| `dtest_solve_list10` | 0.253 | 0.253 | 0.253 | 0.253 |
| `dtest_solve_list100` | 1.243 | 1.243 | 1.243 | 1.243 |
| `play_analysis_benchmark` | 0.178 | 0.178 | 0.178 | 0.178 |
| `alpha_mu_prototype_default` | 0.422 | 0.422 | 0.422 | 0.422 |
| `alpha_mu_prototype_bridge_dds` | 44.806 | 44.806 | 44.806 | 44.806 |

## 2026-04-12 12:50:43 — commit `e537788` (dirty)

- Output bundle: `test/build/performance_runs/20260412-124655`
- Platform: `macOS-26.4-arm64-arm-64bit`
- Repeats per workload: `1`

| Workload | Median (s) | Mean (s) | Min (s) | Max (s) |
| --- | ---: | ---: | ---: | ---: |
| `regression_api_smoke` | 224.060 | 224.060 | 224.060 | 224.060 |
| `dtest_solve_list10` | 0.192 | 0.192 | 0.192 | 0.192 |
| `dtest_solve_list100` | 1.275 | 1.275 | 1.275 | 1.275 |
| `play_analysis_benchmark` | 0.106 | 0.106 | 0.106 | 0.106 |
| `alpha_mu_prototype_default` | 0.249 | 0.249 | 0.249 | 0.249 |
| `alpha_mu_prototype_bridge_dds` | 0.472 | 0.472 | 0.472 | 0.472 |

## 2026-04-12 17:12:16 — commit `04290a6` (dirty)

- Output bundle: `test/build/performance_runs/20260412-170734`
- Platform: `macOS-26.4-arm64-arm-64bit`
- Repeats per workload: `1`
- Graph outliers: `dtest_solve_list10`
- Note: `dtest_solve_list10` was a single-run wall-clock startup/scheduling outlier; repeated reruns and the program's own internal timing remained near the historical ~0.1-0.2 s range.

| Workload | Median (s) | Mean (s) | Min (s) | Max (s) |
| --- | ---: | ---: | ---: | ---: |
| `regression_api_smoke` | 227.501 | 227.501 | 227.501 | 227.501 |
| `dtest_solve_list10` | 7.579 | 7.579 | 7.579 | 7.579 |
| `dtest_solve_list100` | 2.874 | 2.874 | 2.874 | 2.874 |
| `play_analysis_benchmark` | 0.208 | 0.208 | 0.208 | 0.208 |
| `alpha_mu_prototype_default` | 0.341 | 0.341 | 0.341 | 0.341 |
| `alpha_mu_prototype_bridge_dds` | 0.571 | 0.571 | 0.571 | 0.571 |

## 2026-04-13 06:47:57 — commit `6a0afe7` (dirty)

- Output bundle: `test/build/performance_runs/20260413-064240`
- Platform: `macOS-26.4-arm64-arm-64bit`
- Requested repeats per workload: `1`
- Timing stabilization:
  - `dtest_solve_list10`: 1 unmeasured warmup run and at least 3 measured repeats to reduce short-run startup noise.
  - `dtest_solve_list100`: 1 unmeasured warmup run and at least 3 measured repeats to reduce short-run startup noise.
- Graph outliers: `alpha_mu_prototype_default`
- Note: `alpha_mu_prototype_default` was a single-run wall-clock outlier; repeated reruns remained near the historical ~0.15-0.2 s range and the later stabilized entry supersedes it for trend interpretation.

| Workload | Median (s) | Mean (s) | Min (s) | Max (s) |
| --- | ---: | ---: | ---: | ---: |
| `regression_api_smoke` | 221.221 | 221.221 | 221.221 | 221.221 |
| `dtest_solve_list10` | 0.129 | 0.149 | 0.116 | 0.201 |
| `dtest_solve_list100` | 1.239 | 1.233 | 1.211 | 1.248 |
| `play_analysis_benchmark` | 0.150 | 0.150 | 0.150 | 0.150 |
| `alpha_mu_prototype_default` | 16.432 | 16.432 | 16.432 | 16.432 |
| `alpha_mu_prototype_bridge_dds` | 0.443 | 0.443 | 0.443 | 0.443 |

## 2026-04-13 09:54:02 — commit `e83103f` (dirty)

- Output bundle: `test/build/performance_runs/20260413-094756`
- Platform: `macOS-26.4-arm64-arm-64bit`
- Requested repeats per workload: `1`
- Timing stabilization:
  - `dtest_solve_list10`: 1 unmeasured warmup run; 6 measured repeats; minimum measured repeat count raised from requested 1 to 3; cumulative measured wall time target ≥ 1.0 s; automatic repeats capped at 10 unless the user requests more.
  - `dtest_solve_list100`: 1 unmeasured warmup run; 3 measured repeats; minimum measured repeat count raised from requested 1 to 3; cumulative measured wall time target ≥ 1.0 s; automatic repeats capped at 10 unless the user requests more.
  - `play_analysis_benchmark`: 1 unmeasured warmup run; 10 measured repeats; minimum measured repeat count raised from requested 1 to 3; cumulative measured wall time target ≥ 1.0 s; automatic repeats capped at 10 unless the user requests more.
  - `alpha_mu_prototype_default`: 1 unmeasured warmup run; 7 measured repeats; minimum measured repeat count raised from requested 1 to 3; cumulative measured wall time target ≥ 1.0 s; automatic repeats capped at 10 unless the user requests more.
  - `alpha_mu_prototype_bridge_dds`: 1 unmeasured warmup run; 3 measured repeats; minimum measured repeat count raised from requested 1 to 3; cumulative measured wall time target ≥ 1.0 s; automatic repeats capped at 10 unless the user requests more.

| Workload | Median (s) | Mean (s) | Min (s) | Max (s) |
| --- | ---: | ---: | ---: | ---: |
| `regression_api_smoke` | 219.642 | 219.642 | 219.642 | 219.642 |
| `dtest_solve_list10` | 0.194 | 0.191 | 0.171 | 0.199 |
| `dtest_solve_list100` | 2.347 | 2.346 | 2.344 | 2.348 |
| `play_analysis_benchmark` | 0.107 | 0.107 | 0.103 | 0.114 |
| `alpha_mu_prototype_default` | 0.156 | 0.157 | 0.154 | 0.162 |
| `alpha_mu_prototype_bridge_dds` | 0.449 | 0.450 | 0.448 | 0.454 |

## 2026-04-13 21:05:52 — commit `75252de`

- Output bundle: `test/build/performance_runs/20260413-210045`
- Platform: `macOS-26.4-arm64-arm-64bit`
- Requested repeats per workload: `1`
- Timing stabilization:
  - `dtest_solve_list10`: 1 unmeasured warmup run; 9 measured repeats; minimum measured repeat count raised from requested 1 to 3; cumulative measured wall time target ≥ 1.0 s; automatic repeats capped at 10 unless the user requests more.
  - `dtest_solve_list100`: 1 unmeasured warmup run; 3 measured repeats; minimum measured repeat count raised from requested 1 to 3; cumulative measured wall time target ≥ 1.0 s; automatic repeats capped at 10 unless the user requests more.
  - `play_analysis_benchmark`: 1 unmeasured warmup run; 10 measured repeats; minimum measured repeat count raised from requested 1 to 3; cumulative measured wall time target ≥ 1.0 s; automatic repeats capped at 10 unless the user requests more.
  - `alpha_mu_prototype_default`: 1 unmeasured warmup run; 7 measured repeats; minimum measured repeat count raised from requested 1 to 3; cumulative measured wall time target ≥ 1.0 s; automatic repeats capped at 10 unless the user requests more.
  - `alpha_mu_prototype_bridge_dds`: 1 unmeasured warmup run; 3 measured repeats; minimum measured repeat count raised from requested 1 to 3; cumulative measured wall time target ≥ 1.0 s; automatic repeats capped at 10 unless the user requests more.

| Workload | Median (s) | Mean (s) | Min (s) | Max (s) |
| --- | ---: | ---: | ---: | ---: |
| `regression_api_smoke` | 252.324 | 252.324 | 252.324 | 252.324 |
| `dtest_solve_list10` | 0.121 | 0.124 | 0.109 | 0.143 |
| `dtest_solve_list100` | 1.250 | 1.249 | 1.202 | 1.296 |
| `play_analysis_benchmark` | 0.106 | 0.108 | 0.098 | 0.131 |
| `alpha_mu_prototype_default` | 0.147 | 0.147 | 0.139 | 0.154 |
| `alpha_mu_prototype_bridge_dds` | 0.449 | 0.448 | 0.437 | 0.458 |

## 2026-04-16 17:10:55 — commit `e11ce28` (dirty)

- Output log: `test/build/alpha_mu_depth2_list10.log`
- Status sidecar: `test/build/alpha_mu_depth2_list10.log.status.json`
- Platform: `macOS-26.4.1-arm64-arm-64bit`
- Benchmark mode: `alpha_mu_prototype benchmark_alpha`
- Checkpoint interval: `30 s`
- Heartbeat interval: `30 s`

| Workload | Boards | Total (s) | Per board (s) |
| --- | ---: | ---: | ---: |
| `alpha_mu_prototype_list10_depth2` | 10 | 653.456 | 65.346 |

## 2026-04-16 19:19:08 — commit `b12ad9e`

- Output log: `test/build/list10_alpha_mu_depth2_20260416-190836.log`
- Status sidecar: `test/build/list10_alpha_mu_depth2_20260416-190836.log.status.json`
- Platform: `macOS-26.4.1-arm64-arm-64bit`
- Benchmark mode: `alpha_mu_prototype benchmark_alpha`
- Checkpoint interval: `30 s`
- Heartbeat interval: `30 s`

| Workload | Boards | Total (s) | Per board (s) |
| --- | ---: | ---: | ---: |
| `alpha_mu_prototype_list10_depth2` | 10 | 631.195 | 63.120 |

