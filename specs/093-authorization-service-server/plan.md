# Implementation Plan: Authorization Service Server Facet

**Feature Branch**: `093-authorization-service-server`  
**Created**: 2026-07-19  
**Spec**: [spec.md](./spec.md)  
**Constitution**: [.specify/memory/constitution.md](../../.specify/memory/constitution.md)

## Technical Context

- **Language**: C11 (freestanding, no heap in hot path)
- **Build**: CMake + Kconfig, profile-gated (`MUC_OPCUA_CU_USER_TOKEN_JWT`, `MUC_OPCUA_CU_AUTHORIZATION_SERVICE_SERVER`)
- **Target profiles**: full (by default), standard (opt-in), embedded (opt-in)
- **Dependencies**: Platform crypto adapter (RSA/ECDSA verify, SHA-256), existing `activate_session.c` user token dispatch
- **Performance**: JWT validation must complete in <10ms on embedded-class hardware
- **Size budget**: ≤5 KB `.text` growth in standard profile

## Constitution Check

| Principle | Status | Notes |
|-----------|--------|-------|
| I. Spec Fidelity | ✅ | All JWT behavior cites OPC-10000-4 §5.7.3, OPC-10000-7 CU 1629/1697, RFC 7519/7515 |
| II. Embedded-First C Core | ✅ | Freestanding C11, no heap in JWT hot path, caller-provided buffers |
| III. Minimal OPC UA Surface | ✅ | JWT is an *additional* UserIdentityToken type; existing auth paths unchanged |
| IV. Protocol Correctness Gates | ✅ | Every rejection path returns correct OPC UA StatusCode |
| V. Security Honesty | ✅ | JWT validation is cryptographic (signature verify, expiry check) |
| VI. Reproducible Builds | ✅ | Kconfig-gated, no external JWT library, no network calls |
| VII. Size Discipline | ✅ | ≤5 KB budget, compiles out entirely when both symbols undefined |

## Phase 0: Research

Complete. See [research.md](./research.md) for:
- JWT library decision (minimal freestanding parser)
- Key configuration (DER blobs in server config)
- Claims processing (streaming key-value scanner)
- ActivateSession integration (user token dispatch hook)
- Kconfig gating (two symbols, default full-only)
- Algorithm support (RS256 minimum, ECDSA optional)

## Phase 1: Design & Contracts

### Data Model

See [data-model.md](./data-model.md) for:
- `mu_jwt_config_t` and `mu_jwt_issuer_t` configuration structures
- `mu_jwt_alg_t` algorithm enumeration
- `mu_jwt_claims_t` extracted claims
- `mu_jwt_result_t` validation result codes
- Kconfig symbol dependencies
- File map

### Interface Contracts

No external API beyond existing `mu_server_config_t`. The JWT configuration is embedded in the server config struct. See `data-model.md` for field definitions.

### Validation Path

See [quickstart.md](./quickstart.md) for:
- Build commands per profile
- Test execution
- Manual JWT verification with OpenSSL/Python
- Profile size measurement

## Design Artifacts

- Internal object design: `./data-model.md` (populated — entities and file map)
- Service sequences: `N/A` — single internal function call (ActivateSession → jwt_validate), no cross-boundary sequencing
- Behavior draft: `N/A` — not applicable (no UI, no visual requirements)
- BDD contracts: `N/A` — not applicable (embedded protocol feature, no user-facing UI)
- Expected UIF contracts: `N/A` — not applicable
- Behavior contracts: `N/A` — not applicable
- Data model: `./data-model.md`
- Interface contracts: `N/A` — configuration-only (server config struct extension)
- Validation path: `./quickstart.md`

## Task Sequence

1. Kconfig symbols + CMake gating
2. Base64url decoder (`base64url.c`)
3. JWT parser + claims scanner (`jwt.c`)
4. RSA/ECDSA signature verification integration (crypto adapter wrappers)
5. `mu_jwt_validate()` public API
6. ActivateSession JWT hook
7. AuthorizationServiceConfigurationType address-space nodes (CU 1629)
8. Unit tests (parser, claims, signature, error paths)
9. Integration test (ActivateSession with JWT)
10. Size measurement and README update

## Size Budget

| Item | Estimated `.text` |
|------|-------------------|
| Base64url decoder | ~0.3 KB |
| JWT parser + scanner | ~1.5 KB |
| Signature verify wrappers | ~0.5 KB |
| ActivateSession hook | ~0.3 KB |
| AuthorizationServiceConfig nodes | ~1.2 KB |
| **Total** | **~3.8 KB** |
