# Traceability: Core Feature Expansion (Feature 009)

Maps the core feature expansion (Write, Multiple Connections, AES-128/256 policies, X.509 user auth, events) to OPC-10000 sections.

**Target profile**: Core 2017 Server Facet, User Token-X509 Server Facet. Status: `profile-targeting`.

## Conformance-unit → OPC section → file map

| Conformance unit / Feature | OPC-10000 citation | Impl file(s) | Test(s) | Status |
|---|---|---|---|---|
| `Attribute Write` | OPC-10000-4 §5.10.4 | `src/services/write.c`, `src/core/service_dispatch.c` | `tests/unit/test_write_decoder.c`, `tests/integration/test_write_service.c` | PASSED |
| `Multiple Connections` | OPC-10000-4 §5.13 | `src/core/server.c` | `tests/unit/test_connection_multiplex.c`, `tests/integration/test_multiple_connections.c` | PASSED |
| `Security Policies` | OPC-10000-7 §6.2 | `src/security/security_policy.c` | `tests/unit/test_security_policy_selection.c`, `tests/integration/test_secure_handshake_modern.c` | PASSED |
| `Certificate User Auth` | OPC-10000-4 §7.36.4 | `src/encoding/binary_reader.c`, `src/core/service_dispatch.c` | `tests/unit/test_cert_token_decoder.c`, `tests/integration/test_user_auth_certificate.c` | PASSED |
| `Alarms & Events` | OPC-10000-4 §5.12.1 | `src/services/subscription.c` | `tests/unit/test_event_serializer.c`, `tests/integration/test_event_notifications.c` | PASSED |

## Unsupported / out-of-scope (cited rejection)

| Feature | Why out | Expected StatusCode |
|---|---|---|
| Writing non-value attributes | Not required for base variables control | `Bad_NotWritable` |
| Historic events | Out of scope for embedded A&C | `Bad_ServiceUnsupported` |
