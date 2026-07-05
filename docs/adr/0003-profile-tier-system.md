# ADR 0003: Profile Tier System via CMake Feature Flags

**Date**: 2026-07-05
**Status**: Accepted

## Context

Micro-opcua serves integrators with wildly different requirements: a tiny sensor that only needs Read and Browse (Nano Embedded 2017 Server Profile), a controller that also needs data-change subscriptions (Micro), a secured gateway with events and multiple connections (Embedded), up to the full Standard 2017 UA Server Profile with DataAccess, Methods, Auditing, ComplexTypes, Aggregates, ReverseConnect, and TimeSync. Compiling every feature into every build would waste flash and RAM on constrained targets and pull in unused crypto, event, and subscription machinery.

## Decision

Feature gating uses a tiered CMake profile system. The `MUC_OPCUA_PROFILE` cache variable accepts `nano`, `micro`, `embedded`, `standard`, `full`, or `custom`, and maps each tier to a set of `MUC_OPCUA_*` compile definitions. The mapping is centralized in `CMakeLists.txt` and the `MUC_OPCUA_PROFILE_CONTROLLED_OPTIONS` list. For non-`custom` profiles, all controlled options are first set to `OFF`, then the tier-specific subset is enabled. This guarantees that unrequested features are definitively off rather than inheriting stale CMake cache values. Dependency guards in `include/muc_opcua/features.h` use `#error` to reject illegal combinations at compile time (e.g., `MUC_OPCUA_EVENTS` without `MUC_OPCUA_SUBSCRIPTIONS`, `MUC_OPCUA_STANDARD_PROFILE` without `MUC_OPCUA_PROFILE=standard` or `full`). When tests or examples are built and no explicit profile is given, the default silently promotes from `nano` to `full` for the broadest test coverage.

## Consequences

Integrators get exactly the footprint they need: `nano` fits in under 16 KB flash; `full` includes the entire feature set for host development and certification testing. The `custom` escape hatch allows integrators to hand-pick features not covered by any predefined tier. The explicit-reset-then-enable pattern prevents stale cache poisoning. The trade-off is that adding a new feature flag requires touching three places: the controlled-options list, the tier assignments, and `features.h` guard rules — a manual coordination cost that has been manageable to date.
