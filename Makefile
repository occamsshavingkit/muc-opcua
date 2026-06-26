# micro-opcua profile build presets.
#
# Thin wrappers over CMake that produce a minimal_server example binary configured
# for each OPC UA server-profile tier. The profiles differ in the feature set that
# is compiled in (see the MICRO_OPCUA_* CMake options).
#
#   make nano      Nano Embedded Device 2017 Server Profile — SecurityPolicy None,
#                  the full Nano service surface (Discovery, Session, Read, the View
#                  Service Set) + the standard Base Information node set. Complete.
#   make micro     Micro profile configuration. Micro = Nano + subscriptions; the
#                  subscription/MonitoredItem engine is NOT yet implemented, so this
#                  currently builds the Nano-equivalent config (a placeholder until
#                  MICRO_OPCUA_SUBSCRIPTIONS lands).
#   make embedded  Embedded profile configuration — turns SecurityPolicy
#                  Basic256Sha256 ON (its sign/encrypt is implemented). The Standard
#                  DataChange Subscription facet and full type-system exposure are
#                  still pending.
#   make all-profiles   build nano, micro and embedded
#   make test           configure with tests and run the full ctest suite
#   make clean          remove the profile build directories
#
# Example binary lands at build/<profile>/examples/minimal_server/minimal_server.

CMAKE ?= cmake
BUILD  ?= build
COMMON := -DMICRO_OPCUA_BUILD_EXAMPLES=ON -DMICRO_OPCUA_OPTIMIZE_SIZE=ON

.PHONY: help nano micro embedded all-profiles test clean

help:
	@echo "micro-opcua profile builds:"
	@echo "  make nano      Nano 2017 profile (SecurityPolicy None)   -> $(BUILD)/nano"
	@echo "  make micro     Micro config (subscriptions not yet built) -> $(BUILD)/micro"
	@echo "  make embedded  Embedded config (Basic256Sha256 on)        -> $(BUILD)/embedded"
	@echo "  make test      build with tests and run ctest"
	@echo "  make clean     remove profile build directories"

nano:
	@echo ">> NANO profile: SecurityPolicy None, full Nano service surface"
	$(CMAKE) -S . -B $(BUILD)/nano $(COMMON) -DMICRO_OPCUA_SECURITY=OFF
	$(CMAKE) --build $(BUILD)/nano

micro:
	@echo ">> MICRO profile config (NOTE: subscriptions/MonitoredItems not yet implemented)"
	$(CMAKE) -S . -B $(BUILD)/micro $(COMMON) -DMICRO_OPCUA_SECURITY=OFF
	$(CMAKE) --build $(BUILD)/micro

embedded:
	@echo ">> EMBEDDED profile config: Basic256Sha256 ON (NOTE: subscriptions/full type system pending)"
	$(CMAKE) -S . -B $(BUILD)/embedded $(COMMON) -DMICRO_OPCUA_SECURITY=ON
	$(CMAKE) --build $(BUILD)/embedded

all-profiles: nano micro embedded

test:
	$(CMAKE) -S . -B $(BUILD)/test -DMICRO_OPCUA_BUILD_TESTS=ON
	$(CMAKE) --build $(BUILD)/test
	cd $(BUILD)/test && ctest --output-on-failure

clean:
	rm -rf $(BUILD)/nano $(BUILD)/micro $(BUILD)/embedded $(BUILD)/test
