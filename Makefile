# muc-opcua profile build presets.
#
# Thin wrappers over CMake that produce a minimal_server example binary configured
# for each OPC UA server-profile tier. The profiles differ in the feature set that
# is compiled in (see the MUC_OPCUA_* CMake options).
#
#   make nano      Nano Embedded Device 2017 Server Profile — SecurityPolicy None,
#                  the full Nano service surface (Discovery, Session, Read, the View
#                  Service Set) + the standard Base Information node set, subscriptions
#                  OFF. Complete.
#   make micro     Micro profile configuration — Nano + the data-change subscription
#                  engine (MUC_OPCUA_SUBSCRIPTIONS=ON: the Embedded Data Change
#                  Subscription Server Facet). Distinct from nano, with bounded
#                  multi-session capacity controlled by MU_MAX_SESSIONS.
#   make embedded  Embedded profile configuration — Micro + Basic256Sha256,
#                  Standard DataChange Subscription 2017 facet capacities, and
#                  Base Info Type System exposure.
#   make all-profiles   build nano, micro and embedded
#   make test           configure with tests and run the full ctest suite
#   make clean          remove the profile build directories
#
# Example binary lands at build/<profile>/examples/minimal_server.

CMAKE ?= cmake
BUILD  ?= build
COMMON := -DMUC_OPCUA_BUILD_EXAMPLES=ON -DMUC_OPCUA_OPTIMIZE_SIZE=ON
PROFILE_CACHE_RESET := -U MU_MAX_SUBSCRIPTIONS -U MU_MAX_MONITORED_ITEMS \
	-U MU_MAX_PUBLISH_REQUESTS -U MU_MONITORED_QUEUE_DEPTH \
	-U MU_MAX_TRIGGER_LINKS
EMBEDDED_CAPS := -DMU_MAX_SUBSCRIPTIONS=2 -DMU_MAX_MONITORED_ITEMS=100 \
	-DMU_MAX_PUBLISH_REQUESTS=5 -DMU_MONITORED_QUEUE_DEPTH=2 \
	-DMU_MAX_TRIGGER_LINKS=4
SPEED_LAB_FLAGS ?= --cpu 11 --use-sudo --require-affinity --require-realtime \
	--require-isolated-cpu --shield-cpu
SPEED_SMOKE_FLAGS ?= --cpu 11 --realtime-priority 0

.PHONY: help nano micro embedded all-profiles test test-nano test-micro test-embedded test-full test-profiles speed-matrix speed-smoke speed-current speed-baseline speed-compare clean

help:
	@echo "muc-opcua profile builds:"
	@echo "  make nano      Nano 2017 profile (None, subscriptions OFF)  -> $(BUILD)/nano"
	@echo "  make micro     Micro profile (None + subscriptions ON)      -> $(BUILD)/micro"
	@echo "  make embedded  Embedded 2017 profile target                 -> $(BUILD)/embedded"
	@echo "  make test      build with tests and run ctest"
	@echo "  make speed-smoke     run non-strict speed benchmark smoke"
	@echo "  make speed-current   run lab speed/resource benchmark matrix"
	@echo "  make speed-baseline  refresh docs/benchmarks/speed-baseline.json in lab mode"
	@echo "  make speed-compare   compare lab speed/resource results to baseline"
	@echo "  make clean     remove profile build directories"

nano:
	@echo ">> NANO profile: SecurityPolicy None, full Nano service surface, subscriptions OFF"
	$(CMAKE) -S . -B $(BUILD)/nano $(PROFILE_CACHE_RESET) $(COMMON) -DMUC_OPCUA_PROFILE=nano
	$(CMAKE) --build $(BUILD)/nano

micro:
	@echo ">> MICRO profile: Nano + data-change subscriptions (MUC_OPCUA_SUBSCRIPTIONS=ON)"
	$(CMAKE) -S . -B $(BUILD)/micro $(PROFILE_CACHE_RESET) $(COMMON) -DMUC_OPCUA_PROFILE=micro
	$(CMAKE) --build $(BUILD)/micro

embedded:
	@echo ">> EMBEDDED profile: Basic256Sha256 + Standard subscriptions + Base Info Type System"
	$(CMAKE) -S . -B $(BUILD)/embedded $(PROFILE_CACHE_RESET) $(COMMON) -DMUC_OPCUA_PROFILE=embedded $(EMBEDDED_CAPS)
	$(CMAKE) --build $(BUILD)/embedded

all-profiles: nano micro embedded

test:
	$(CMAKE) -S . -B $(BUILD)/test -DMUC_OPCUA_BUILD_TESTS=ON
	$(CMAKE) --build $(BUILD)/test
	cd $(BUILD)/test && ctest --output-on-failure

# Feature 028: per-profile test execution. An explicit -DMUC_OPCUA_PROFILE is
# honored (the "force full" resolution only applies to the default profile), so
# these build+run the tests that survive each profile's RUN_TEST gates against
# that profile's actual library — the per-profile conformance evidence.
TEST_COMMON := -DMUC_OPCUA_BUILD_TESTS=ON -DMUC_OPCUA_PLATFORM=host \
	-DMUC_OPCUA_HAVE_MBEDTLS=ON -DMUC_OPCUA_HAVE_WOLFSSL=ON

test-nano:
	$(CMAKE) -S . -B $(BUILD)/test-nano $(PROFILE_CACHE_RESET) $(TEST_COMMON) -DMUC_OPCUA_PROFILE=nano
	$(CMAKE) --build $(BUILD)/test-nano
	cd $(BUILD)/test-nano && ctest --output-on-failure

test-micro:
	$(CMAKE) -S . -B $(BUILD)/test-micro $(PROFILE_CACHE_RESET) $(TEST_COMMON) -DMUC_OPCUA_PROFILE=micro
	$(CMAKE) --build $(BUILD)/test-micro
	cd $(BUILD)/test-micro && ctest --output-on-failure

test-embedded:
	$(CMAKE) -S . -B $(BUILD)/test-embedded $(PROFILE_CACHE_RESET) $(TEST_COMMON) -DMUC_OPCUA_PROFILE=embedded $(EMBEDDED_CAPS)
	$(CMAKE) --build $(BUILD)/test-embedded
	cd $(BUILD)/test-embedded && ctest --output-on-failure

test-full:
	$(CMAKE) -S . -B $(BUILD)/test-full $(PROFILE_CACHE_RESET) $(TEST_COMMON) -DMUC_OPCUA_PROFILE=full $(EMBEDDED_CAPS)
	$(CMAKE) --build $(BUILD)/test-full
	cd $(BUILD)/test-full && ctest --output-on-failure

test-profiles: test-nano test-micro test-embedded test-full

speed-matrix:
	python3 scripts/run_speed_bench.py --matrix

speed-smoke:
	python3 scripts/run_speed_bench.py $(SPEED_SMOKE_FLAGS)

speed-current:
	python3 scripts/run_speed_bench.py $(SPEED_LAB_FLAGS)

speed-baseline:
	python3 scripts/run_speed_bench.py $(SPEED_LAB_FLAGS) --update-baseline

speed-compare:
	python3 scripts/run_speed_bench.py $(SPEED_LAB_FLAGS) --compare

clean:
	rm -rf $(BUILD)/nano $(BUILD)/micro $(BUILD)/embedded $(BUILD)/test
