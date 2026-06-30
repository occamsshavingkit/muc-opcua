# micro-opcua profile build presets.
#
# Thin wrappers over CMake that produce a minimal_server example binary configured
# for each OPC UA server-profile tier. The profiles differ in the feature set that
# is compiled in (see the MICRO_OPCUA_* CMake options).
#
#   make nano      Nano Embedded Device 2017 Server Profile — SecurityPolicy None,
#                  the full Nano service surface (Discovery, Session, Read, the View
#                  Service Set) + the standard Base Information node set, subscriptions
#                  OFF. Complete.
#   make micro     Micro profile configuration — Nano + the data-change subscription
#                  engine (MICRO_OPCUA_SUBSCRIPTIONS=ON: the Embedded Data Change
#                  Subscription Server Facet). Distinct from nano. (Concurrent ≥2-session
#                  support is the remaining Micro item.)
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
COMMON := -DMICRO_OPCUA_BUILD_EXAMPLES=ON -DMICRO_OPCUA_OPTIMIZE_SIZE=ON
PROFILE_CACHE_RESET := -U MU_MAX_SUBSCRIPTIONS -U MU_MAX_MONITORED_ITEMS \
	-U MU_MAX_PUBLISH_REQUESTS -U MU_MONITORED_QUEUE_DEPTH \
	-U MU_MAX_TRIGGER_LINKS
EMBEDDED_CAPS := -DMU_MAX_SUBSCRIPTIONS=2 -DMU_MAX_MONITORED_ITEMS=100 \
	-DMU_MAX_PUBLISH_REQUESTS=5 -DMU_MONITORED_QUEUE_DEPTH=2 \
	-DMU_MAX_TRIGGER_LINKS=4

.PHONY: help nano micro embedded all-profiles test clean

help:
	@echo "micro-opcua profile builds:"
	@echo "  make nano      Nano 2017 profile (None, subscriptions OFF)  -> $(BUILD)/nano"
	@echo "  make micro     Micro profile (None + subscriptions ON)      -> $(BUILD)/micro"
	@echo "  make embedded  Embedded 2017 profile target                 -> $(BUILD)/embedded"
	@echo "  make test      build with tests and run ctest"
	@echo "  make clean     remove profile build directories"

nano:
	@echo ">> NANO profile: SecurityPolicy None, full Nano service surface, subscriptions OFF"
	$(CMAKE) -S . -B $(BUILD)/nano $(PROFILE_CACHE_RESET) $(COMMON) -DMICRO_OPCUA_PROFILE=nano
	$(CMAKE) --build $(BUILD)/nano

micro:
	@echo ">> MICRO profile: Nano + data-change subscriptions (MICRO_OPCUA_SUBSCRIPTIONS=ON)"
	$(CMAKE) -S . -B $(BUILD)/micro $(PROFILE_CACHE_RESET) $(COMMON) -DMICRO_OPCUA_PROFILE=micro
	$(CMAKE) --build $(BUILD)/micro

embedded:
	@echo ">> EMBEDDED profile: Basic256Sha256 + Standard subscriptions + Base Info Type System"
	$(CMAKE) -S . -B $(BUILD)/embedded $(PROFILE_CACHE_RESET) $(COMMON) -DMICRO_OPCUA_PROFILE=embedded $(EMBEDDED_CAPS)
	$(CMAKE) --build $(BUILD)/embedded

all-profiles: nano micro embedded

test:
	$(CMAKE) -S . -B $(BUILD)/test -DMICRO_OPCUA_BUILD_TESTS=ON
	$(CMAKE) --build $(BUILD)/test
	cd $(BUILD)/test && ctest --output-on-failure

clean:
	rm -rf $(BUILD)/nano $(BUILD)/micro $(BUILD)/embedded $(BUILD)/test
