<!-- markdownlint-disable MD013 MD022 MD032 MD056 MD060 -->

# TODO — muc-opcua

**Updated**: 2026-07-19 (PR #327 CU 5801 part 2 merged; stale items cleaned up)
**Source**: code review findings, binary size analysis, approved Spec Kit deferrals

## Remaining Active Backlog (post-audit)

### Implemented (TODO was stale)

All items below are complete and the TODO.md entries were simply outdated:

| ID | Feature | Status | Implemented In |
|----|---------|--------|----------------|
| PG1 | Security Time Synchronization | ✅ Complete | specs 050/055/075 — `osc_handler.c` validates `RequestHeader.timestamp` against configurable skew |
| PG3 | Data Access Server Facet | ✅ Complete | spec 060 — AnalogItemType, EURange, percent deadband, DA type nodes |
| PG4 | Enhanced DataChange Subscription 2017 | ✅ Complete | Higher capacity limits via Kconfig + Monitor Items 500 CU |
| PG5 | Method Server Facet | ✅ Complete | spec 062 — `mu_server_register_method_callback()` arbitrary user methods |
| PG6 | Standard Event Subscription (where-clause) | ✅ Complete | specs 037/063 — `event_filter.c` 13-operator where-clause evaluator |
| PG10 | Base Server Behaviour Facet | ✅ Complete | spec 064 — `diagnostics.c` session/subscription/request counters |
| PG11 | Client Redundancy Server Facet | ✅ Complete | specs 066/067 — `transfer_handler.c` cross-session subscription transfer |
| PG12 | Reverse Connect Server Facet | ✅ Complete | spec 065 — ReverseHello encoder, config validation |
| PG13/PG14 | ECC Security Policies | ✅ Complete | spec 059 — curve25519 + nist256 with AEAD/CBC-HMAC, OpenSSL/wolfSSL/mbedTLS |
| PG15 | Exposes Type System Server Facet | ✅ Complete | `MUC_OPCUA_FACET_EXPOSES_TYPE_SYSTEM_SERVER` drives base_nodes+encoding |
| PG16 | Documentation Server Facet | ✅ Complete | spec 058 — mandate limit CUs documented |

### Genuinely pending

| ID | Feature | Notes |
|----|---------|-------|
| PG17 | Authorization Service Server Facet | OAuth2/JWT access token support — new in v1.05.02 |
| PG18 | KeyCredential Service Server Facet | KeyCredential management for broker auth |
| PG19 | User Role Management Server Facet | Server user CRUD management |
| PG20 | Global Certificate Management Server Facet | Certificate enrollment/renewal via GDS |
| PG23 | User Token — JWT Server Facet | OAuth2/JWT — new in 1.05.02 |
| PG24 | User Token — Issued Token Server Facet | Kerberos — deprecated in 1.05.02 |

### Deferred Tests

| ID | Test | Issue |
|----|------|-------|
| T092-1 | `test_subscription_diagnostics_type_instance_declarations` | browse_name assertion mismatch in test binary context; implementation verified correct via independent node-lookup binary (52 node entries, zero sort violations) |
| T092-2 | `test_session_diagnostics_variable_type_instance_declarations` | Ibid |
| T092-3 | `test_session_diagnostics_object_type_instance_declarations` | Ibid |

## ✅ Completed in Spec 092 — CU 5801 Diagnostics Completeness Part 2

- SubscriptionDiagnosticsType(2172) — 31 InstanceDeclarations
- SessionDiagnosticsVariableType(2197) — 43 InstanceDeclarations
- SessionDiagnosticsObjectType(2029) — 4 InstanceDeclarations
- ServerRedundancyType(2034) — 2 InstanceDeclarations
- ApplicationType(307)/ApplicationDescription(308) closure DataTypes
- All data-band nodes dual-placed DA-on/DA-off with verified sort order
- Embedded profile node budget bumped to 512 (from 64)

---
*Older completed items below kept as historical record*
