.PHONY: docs profile profile-clean check api-check instrumented-build instrumented-check \
	asan-build asan-check ubsan-build ubsan-check tsan-build tsan-check \
	perf-build perf-check perf-bench perf apple-backend-compare

# ---------- Documentation ----------

docs:
	$(MAKE) -C docs html

# ---------- Profiling build ----------

profile:
	$(MAKE) -C src macos_profile
	$(MAKE) -C test -f Makefiles/Makefile_Mac_clang profile_binaries

profile-clean:
	rm -rf src/build-profile test/build-profile

# ---------- Performance evaluation targets ----------
#
# Usage:
#   make perf          -- full build + correctness + benchmark cycle
#   make perf-build    -- compile release library and test binaries
#   make perf-check    -- run correctness suite (dtest solve + regression_api)
#   make perf-bench    -- run list9 depth-2 board-parallel benchmark (8 P-cores)
#
# All targets use the standard release build (build/).
# On Apple arm64, that standard build is the M1 Max-tuned release profile
# rather than the older generic-tuned -O3 -flto configuration.
# The benchmark uses 8 board workers to saturate the M1 Max P-cores.

PERF_BOARD_WORKERS ?= 8
PERF_HANDS         ?= hands/list9.txt
PERF_DEPTH         ?= 2
PERF_LOG_DIR       ?= test/build/perf_runs
APPLE_COMPARE_HANDS ?= hands/list10.txt
APPLE_COMPARE_DEPTH ?= 3
APPLE_COMPARE_MAX_BOARDS ?= 0
APPLE_COMPARE_SKIP_BOARDS ?= 2
APPLE_COMPARE_PARALLEL ?= board
APPLE_COMPARE_BOARD_WORKERS ?= 4
APPLE_COMPARE_ROOT_WORKERS ?= 1
APPLE_COMPARE_DDS_THREAD_ID ?= 0
APPLE_COMPARE_WARMUPS ?= 1
APPLE_COMPARE_REPEATS ?= 5
APPLE_COMPARE_WARN_CV ?= 0.10
APPLE_COMPARE_OUTPUT_DIR ?=
DDS_LIB_DIR        = src/build
INSTRUMENTED_DDS_LIB_DIR = src/build-instrumented
ASAN_DDS_LIB_DIR   = src/build-asan
UBSAN_DDS_LIB_DIR  = src/build-ubsan
TSAN_DDS_LIB_DIR   = src/build-tsan
INSTRUMENTED_REGRESSION_HANDS = ../hands/list10.txt
INSTRUMENTED_DDS_BEHAVIOR = -DDDS_ALPHA_MU_STATS -DDDS_TIMING -DDDS_MOVES -DDDS_TT_STATS
INSTRUMENTED_EXTRA_COMPILE_FLAGS = -DDDS_ALPHA_MU_STATS -DDDS_TIMING -DDDS_MOVES -DDDS_TT_STATS
ASAN_FLAGS         = -fsanitize=address,undefined -fno-omit-frame-pointer
UBSAN_FLAGS        = -fsanitize=undefined -fno-omit-frame-pointer
TSAN_FLAGS         = -fsanitize=thread -fno-omit-frame-pointer

perf-build:
	$(MAKE) -C src macos
	$(MAKE) -C test -f Makefiles/Makefile_Mac_clang dtest regression_api alpha_mu

api-check: check

check: perf-build
	@echo "=== Minimum correctness gate: regression_api ==="
	cd test && DYLD_LIBRARY_PATH=../$(DDS_LIB_DIR) ./build/regression_api ../hands/list10.txt ../hands/thomas1.txt

instrumented-build:
	$(MAKE) -C src BUILD_DIR=build-instrumented DDS_BEHAVIOR="$(INSTRUMENTED_DDS_BEHAVIOR)" macos
	$(MAKE) -C test -f Makefiles/Makefile_Mac_clang \
		BUILD_DIR=build-instrumented \
		DDS_BUILD_DIR=../src/build-instrumented \
		RPATH_DIR=@loader_path/../../src/build-instrumented \
		EXTRA_COMPILE_FLAGS="$(INSTRUMENTED_EXTRA_COMPILE_FLAGS)" \
		regression_api dtest play_analysis_benchmark alpha_mu

instrumented-check: instrumented-build
	@echo "=== Instrumented correctness: regression_api ==="
	cd test && DYLD_LIBRARY_PATH=../$(INSTRUMENTED_DDS_LIB_DIR) ./build-instrumented/regression_api $(INSTRUMENTED_REGRESSION_HANDS)
	@echo ""
	@echo "=== Instrumented correctness: play_analysis_benchmark ==="
	cd test && DYLD_LIBRARY_PATH=../$(INSTRUMENTED_DDS_LIB_DIR) ./build-instrumented/play_analysis_benchmark

asan-build:
	$(MAKE) -C src PROFILE_BUILD=1 BUILD_DIR=build-asan EXTRA_COMPILE_FLAGS="$(ASAN_FLAGS)" EXTRA_LINK_FLAGS="$(ASAN_FLAGS)" macos
	$(MAKE) -C test -f Makefiles/Makefile_Mac_clang PROFILE_BUILD=1 \
		BUILD_DIR=build-asan \
		DDS_BUILD_DIR=../src/build-asan \
		RPATH_DIR=@loader_path/../../src/build-asan \
		EXTRA_COMPILE_FLAGS="$(ASAN_FLAGS)" \
		EXTRA_LINK_FLAGS="$(ASAN_FLAGS)" \
		regression_api alpha_mu

asan-check: asan-build
	cd test && DYLD_LIBRARY_PATH=../$(ASAN_DDS_LIB_DIR) ./build-asan/regression_api ../hands/list10.txt ../hands/thomas1.txt

ubsan-build:
	$(MAKE) -C src PROFILE_BUILD=1 BUILD_DIR=build-ubsan EXTRA_COMPILE_FLAGS="$(UBSAN_FLAGS)" EXTRA_LINK_FLAGS="$(UBSAN_FLAGS)" macos
	$(MAKE) -C test -f Makefiles/Makefile_Mac_clang PROFILE_BUILD=1 \
		BUILD_DIR=build-ubsan \
		DDS_BUILD_DIR=../src/build-ubsan \
		RPATH_DIR=@loader_path/../../src/build-ubsan \
		EXTRA_COMPILE_FLAGS="$(UBSAN_FLAGS)" \
		EXTRA_LINK_FLAGS="$(UBSAN_FLAGS)" \
		regression_api alpha_mu

ubsan-check: ubsan-build
	cd test && DYLD_LIBRARY_PATH=../$(UBSAN_DDS_LIB_DIR) ./build-ubsan/regression_api ../hands/list10.txt ../hands/thomas1.txt

tsan-build:
	$(MAKE) -C src PROFILE_BUILD=1 BUILD_DIR=build-tsan EXTRA_COMPILE_FLAGS="$(TSAN_FLAGS)" EXTRA_LINK_FLAGS="$(TSAN_FLAGS)" macos
	$(MAKE) -C test -f Makefiles/Makefile_Mac_clang PROFILE_BUILD=1 \
		BUILD_DIR=build-tsan \
		DDS_BUILD_DIR=../src/build-tsan \
		RPATH_DIR=@loader_path/../../src/build-tsan \
		EXTRA_COMPILE_FLAGS="$(TSAN_FLAGS)" \
		EXTRA_LINK_FLAGS="$(TSAN_FLAGS)" \
		regression_api alpha_mu

tsan-check: tsan-build
	cd test && DYLD_LIBRARY_PATH=../$(TSAN_DDS_LIB_DIR) ./build-tsan/regression_api ../hands/list10.txt ../hands/thomas1.txt

perf-check: check
	@echo "=== Correctness: dtest solve list10 ==="
	cd test && DYLD_LIBRARY_PATH=../$(DDS_LIB_DIR) ./build/dtest -f ../hands/list10.txt -s solve
	@echo ""
	@echo "=== Correctness: regression_api ==="
	cd test && DYLD_LIBRARY_PATH=../$(DDS_LIB_DIR) ./build/regression_api
	@echo ""
	@echo "=== Correctness: alpha_mu list9 depth 2 (serial, verify only) ==="
	cd test && DYLD_LIBRARY_PATH=../$(DDS_LIB_DIR) ./build/alpha_mu benchmark_alpha \
		../$(PERF_HANDS) $(PERF_DEPTH) 0 \
		--parallel serial --board-workers 1 --root-workers 1 --dds-thread-id 0

perf-bench: perf-build
	@mkdir -p $(PERF_LOG_DIR)
	@TIMESTAMP=$$(date +%Y%m%d-%H%M%S); \
	LOG="$(PERF_LOG_DIR)/list9_depth2_board$(PERF_BOARD_WORKERS)_$$TIMESTAMP.log"; \
	echo "=== Performance benchmark: $(PERF_HANDS) depth $(PERF_DEPTH), $(PERF_BOARD_WORKERS) board workers ==="; \
	echo "Log: $$LOG"; \
	cd test && DYLD_LIBRARY_PATH=../$(DDS_LIB_DIR) ./build/alpha_mu benchmark_alpha \
		../$(PERF_HANDS) $(PERF_DEPTH) 0 \
		--parallel board --board-workers $(PERF_BOARD_WORKERS) 2>&1 | tee "../$$LOG"; \
	echo ""; \
	grep -q "mismatches=0" "../$$LOG" && echo "CORRECTNESS: PASS (mismatches=0)" \
		|| (echo "CORRECTNESS: FAIL — mismatches detected!"; exit 1)

perf: perf-check perf-bench
	@echo ""
	@echo "=== All performance checks passed ==="

apple-backend-compare: perf-build
	python3 test/alpha_mu_backend_compare.py \
		--hand-file $(APPLE_COMPARE_HANDS) \
		--depth $(APPLE_COMPARE_DEPTH) \
		--max-boards $(APPLE_COMPARE_MAX_BOARDS) \
		--skip-boards "$(APPLE_COMPARE_SKIP_BOARDS)" \
		--parallel $(APPLE_COMPARE_PARALLEL) \
		--board-workers $(APPLE_COMPARE_BOARD_WORKERS) \
		--root-workers $(APPLE_COMPARE_ROOT_WORKERS) \
		--dds-thread-id $(APPLE_COMPARE_DDS_THREAD_ID) \
		--warmups $(APPLE_COMPARE_WARMUPS) \
		--repeats $(APPLE_COMPARE_REPEATS) \
		--warn-cv $(APPLE_COMPARE_WARN_CV) \
		--output-dir "$(APPLE_COMPARE_OUTPUT_DIR)"

