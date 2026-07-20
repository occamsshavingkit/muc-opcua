<!-- markdownlint-disable MD013 MD022 MD032 MD056 MD060 -->

# TODO — muc-opcua

**Updated**: 2026-07-20 — PG18/PG19/PG20 all complete. Backlog empty.

## Pending

| ID | Feature | OPC Ref | Notes |
|----|---------|---------|-------|
| PG24 | User Token — Issued Token Server Facet | OPC-10000-7 v1.05.02 | Kerberos — **deprecated** in v1.05.02 per OPC Foundation. Skipped. |

## Recently Completed

| PR | Feature | Notes |
|----|---------|-------|
| #331 | PG20 — Certificate Management (CU 2105) | Type nodes + adapter. GDS deferred. |
| #330 | PG19 — User Role Management (CU 2080) | AddRole/RemoveRole/AddIdentity/RemoveIdentity. 13 tests. |
| #329 | PG18 — KeyCredential Service (CU 2113) | GetEncryptingKey + CRUD methods. 11 tests. |
| #328 | Authorization Service / JWT (CU 1629/1697) | OAuth2 Resource Server. mbedTLS/wolfSSL backends. |
| #327 | CU 5801 Diagnostics Part 2 | 4 type trees: SubDiag, SesDiagVar, SesDiagObj, ServerRedundancy |

## Deferred (low priority)

| ID | Item | Notes |
|----|------|-------|
| D-1629 | CU 1629 AuthorizationServiceConfigurationType nodes | GDS namespace 2 type system |
| D-GDS | Full GDS Push/Pull model | Certificate enrollment workflow, CRL management |
