.PHONY: docs profile profile-clean perf-build perf-check perf-bench perf

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
# All targets use the release build (build/) with -O3 -flto.
# The benchmark uses 8 board workers to saturate the M1 Max P-cores.

PERF_BOARD_WORKERS ?= 8
PERF_HANDS         ?= hands/list9.txt
PERF_DEPTH         ?= 2
PERF_LOG_DIR       ?= test/build/perf_runs

perf-build:
	$(MAKE) -C src macos
	$(MAKE) -C test -f Makefiles/Makefile_Mac_clang dtest regression_api alpha_mu

DDS_LIB_DIR  = src/build

perf-check: perf-build
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

