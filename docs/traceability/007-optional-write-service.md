# Traceability: Optional Attribute Write Service (Feature 007)

Maps the Write Value Server Facet conformance units to OPC-10000 sections and to the implementation/test files that satisfy them.

**Target profile**: Write Value Server Facet — `http://opcfoundation.org/UA-Profile/Server/WriteValue` (OPC-10000-7 §6.6.6) is targeted under the `embedded-write` profile (with `MICRO_OPCUA_SERVICE_WRITE=ON`). Status: `profile-targeting`.

## Conformance-unit → OPC section → file map

| Conformance unit / Feature | OPC-10000 citation | Impl file(s) | Test(s) | Status |
|---|---|---|---|---|
| `Write Value Service` | OPC-10000-4 §5.11.4 | `src/core/service_dispatch.c` | `tests/integration/test_write_scalar.c`, `tests/integration/test_write_batch.c` | COMPLETED |
| `Write Value Attribute` | OPC-10000-4 §5.11.4.2 | `src/core/service_dispatch.c` | `tests/unit/test_write_errors.c`, `tests/integration/test_write_validation.c` | COMPLETED |
| `Write Request/Response Binary` | OPC-10000-6 §7.36 | `src/encoding/binary_reader.c` | `tests/unit/test_write_decoder.c` | COMPLETED |

## Unsupported / out-of-scope (cited rejection)

| Feature | Why out | Expected StatusCode |
|---|---|---|
| Writing non-Value attributes (§5.11.4.2) | Out of scope for Write Value facet | `Bad_AttributeIdInvalid` |
| indexRange writes (§5.11.4.2) | Partial array writes not supported in this tier | `Bad_WriteNotSupported` |
| Writing when service disabled | Build flag `MICRO_OPCUA_SERVICE_WRITE=OFF` | `Bad_ServiceUnsupported` |
