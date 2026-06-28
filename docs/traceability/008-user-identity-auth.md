# Traceability: UserName/Password User Identity Token Authentication (Feature 008)

Maps the User Token-UserName Server Facet conformance units to OPC-10000 sections and to the implementation/test files that satisfy them.

**Target profile**: User Token-UserName Server Facet — `http://opcfoundation.org/UA-Profile/Server/UserToken-UserName` (OPC-10000-7 §6.6.14) is targeted under the `user-identity-auth` feature (with `MICRO_OPCUA_USER_AUTH=ON`). Status: `profile-targeting`.

## Conformance-unit → OPC section → file map

| Conformance unit / Feature | OPC-10000 citation | Impl file(s) | Test(s) | Status |
|---|---|---|---|---|
| `User Token-UserName Server Facet` | OPC-10000-7 §6.6.14 | `src/services/session.c`, `src/core/service_dispatch.c` | `tests/unit/test_session_auth.c`, `tests/integration/test_user_auth_plaintext.c`, `tests/integration/test_user_auth_encrypted.c` | COMPLETE |
| `UserNameIdentityToken Binary` | OPC-10000-6 §7.38.3 | `src/encoding/binary_reader.c` | `tests/unit/test_user_token_decoder.c`, `tests/unit/test_user_token_encrypted.c` | COMPLETE |
| `GetEndpoints UserTokenPolicy` | OPC-10000-4 §7.37 | `src/services/discovery.c` | `tests/unit/test_discovery_services.c`, `tests/unit/test_discovery_encode.c` | COMPLETE |

## Unsupported / out-of-scope (cited rejection)

| Feature | Why out | Expected StatusCode |
|---|---|---|
| X509 / Issued Tokens | Out of scope for this facet | `Bad_IdentityTokenInvalid` |
| ws-security tokens | WS-Security signatures not supported | `Bad_IdentityTokenInvalid` |
| Invalid credentials | Password/Username mismatch | `Bad_IdentityTokenRejected` |
