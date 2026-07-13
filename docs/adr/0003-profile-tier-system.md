# ADR 0003: Profile Tier System via Kconfig Feature Flags

**Date**: 2026-07-05
**Status**: Accepted

## Context

Micro-opcua serves integrators with wildly different requirements: a tiny sensor that only needs Read and Browse (Nano Embedded 2017 Server Profile), a controller that also needs data-change subscriptions (Micro), a secured gateway with events and multiple connections (Embedded), up to the full Standard 2017 UA Server Profile with DataAccess, Methods, Auditing, ComplexTypes, Aggregates, ReverseConnect, and TimeSync. Compiling every feature into every build would waste flash and RAM on constrained targets and pull in unused crypto, event, and subscription machinery.

## Decision

Feature gating uses a tiered Kconfig profile system. The `MUC_OPCUA_PROFILE` CMake cache variable accepts `nano`, `micro`, `embedded`, `standard`, `full`, or `custom`, then seeds Kconfig from `configs/<profile>.defconfig`. Kconfig owns feature defaults, dependency containment, and profile-derived compile definitions; CMake consumes the generated `muc_opcua_config.cmake` for source selection and `muc_opcua_autoconf.h` for non-CMake/external builds. Integrators can refine the seed with `menuconfig`, additional defconfig fragments via `MUC_OPCUA_KCONFIG_CONFIG`, or by selecting `custom` and enabling facets directly.

## Consequences

Integrators get exactly the footprint they need while keeping feature dependencies in one declarative configuration graph. The `custom` escape hatch allows hand-picked facets not covered by a predefined tier. The trade-off is that adding a new feature now requires updating `Kconfig`, the relevant defconfig/profile defaults, and any source-selection consumers; this is preferable to duplicating profile state across CMake option lists and compile-time guard macros.
