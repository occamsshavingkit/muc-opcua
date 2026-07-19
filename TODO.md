<!-- markdownlint-disable MD013 MD022 MD032 MD056 MD060 -->

# TODO — muc-opcua

**Updated**: 2026-07-19 (post-audit cleanup — all PG1-PG16 were already implemented; TODO was ~10 days stale)

## Pending

| ID | Feature | OPC Ref | Notes |
|----|---------|---------|-------|
| PG17 | Authorization Service Server Facet | OPC-10000-7 v1.05.02 | OAuth2/JWT access token support |
| PG18 | KeyCredential Service Server Facet | OPC-10000-7 v1.05.02 | KeyCredential management for broker auth |
| PG19 | User Role Management Server Facet | OPC-10000-7 v1.05.02 | Server user CRUD management |
| PG20 | Global Certificate Management Server Facet | OPC-10000-7 v1.05.02 | Certificate enrollment/renewal via GDS |
| PG23 | User Token — JWT Server Facet | OPC-10000-7 v1.05.02 | OAuth2/JWT |
| PG24 | User Token — Issued Token Server Facet | OPC-10000-7 v1.05.02 | Kerberos (deprecated in 1.05.02) |

## Deferred Tests (PR #327)

| ID | Test | Issue |
|----|------|-------|
| T092-1 | `test_subscription_diagnostics_type_instance_declarations` | browse_name assertion mismatch; implementation verified correct independently |
| T092-2 | `test_session_diagnostics_variable_type_instance_declarations` | ibid |
| T092-3 | `test_session_diagnostics_object_type_instance_declarations` | ibid |
