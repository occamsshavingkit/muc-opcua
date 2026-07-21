<!-- markdownlint-disable MD013 MD022 MD032 MD056 MD060 -->

# TODO — muc-opcua

**Updated**: 2026-07-20 — Backlog empty. All features complete.

## Pending
*(none)*

## Recently Completed

| PR | Feature | Notes |
|----|---------|-------|
| #332 | D-GDS Certificate Pull Management (CU 1631) | 8 types + 3 instances + 4 Methods + adapter. 14 tests. |
| #331 | PG20 Certificate Management (CU 2105) | Type nodes + adapter. GDS deferred (later completed in #332). |
| #330 | PG19 User Role Management (CU 2080) | AddRole/RemoveRole/AddIdentity/RemoveIdentity. 13 tests. |
| #329 | PG18 KeyCredential Service (CU 2113) | GetEncryptingKey + CRUD methods. 11 tests. |
| #328 | Authorization Service / JWT (CU 1629/1697) | OAuth2 Resource Server. mbedTLS/wolfSSL backends. |

## Deferred

| ID | Item | Notes |
|----|------|-------|
| D-1629 | AuthorizationServiceConfigurationType nodes | GDS namespace 2 type system |
| D-ECC | ECC cert subtypes | 6 ECC-specific CertificateType subtypes |
| D-TRUST | TrustList integration | Full TrustList management + CRL parsing |
| D-PUSH | Push certificate model | ServerConfigurationType reverse-direction management |
