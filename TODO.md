<!-- markdownlint-disable MD013 MD022 MD032 MD056 MD060 -->

# TODO — muc-opcua

**Updated**: 2026-07-19 (CU 5801 deferred tests resolved — 124/124 pass)

## Pending

| ID | Feature | OPC Ref | Notes |
|----|---------|---------|-------|
| PG18 | KeyCredential Service Server Facet | OPC-10000-7 v1.05.02 | KeyCredential management for broker auth |
| PG19 | User Role Management Server Facet | OPC-10000-7 v1.05.02 | Server user CRUD management |
| PG20 | Global Certificate Management Server Facet | OPC-10000-7 v1.05.02 | Certificate enrollment/renewal via GDS |
| PG24 | User Token — Issued Token Server Facet | OPC-10000-7 v1.05.02 | Kerberos (deprecated in 1.05.02) |

## Recently Completed

| PR | Feature | Notes |
|----|---------|-------|
| #328 | Authorization Service / JWT (CU 1629/1697) | OAuth2 Resource Server — JWT validation at ActivateSession. CU 1629 type nodes deferred (GDS namespace). Non-OpenSSL backends not yet supported. |
| #327 | CU 5801 Diagnostics Part 2 | SubscriptionDiagnosticsType, SessionDiagnosticsVariableType, SessionDiagnosticsObjectType, ServerRedundancyType |

## Follow-ups from Code Review

| Source | Item | Status |
|--------|------|--------|
| PR #327 | 3 deferred test functions (browse_name assertion mismatch) | ✅ Fixed — root cause was 62 wrong length constants + shifted NodeId mapping + 3 missing DA-off nodes |
| PR #328 | JWT success-path E2E test needs non-None SecurityPolicy fixture | Pending |
| PR #328 | CU 1629 AuthorizationServiceConfigurationType type nodes (GDS ns 2) | Deferred |
| PR #328 | Non-OpenSSL JWT crypto backends (mbedTLS/wolfSSL) | Deferred |
| PR #328 | Issuer key rotation (evaluate all matching issuers, not first) | Deferred |
| PR #328 | Refactor high-CCN functions (mu_jwt_validate, mu_claim_scan) | Deferred |
