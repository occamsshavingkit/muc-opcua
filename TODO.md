<!-- markdownlint-disable MD013 MD022 MD032 MD056 MD060 -->

# TODO — muc-opcua

**Updated**: 2026-07-27

## Pending

| ID | Item | Notes |
|----|------|-------|
| C-483 | 483 documented CUs need implementation | Audited down from 611 → 128 claimed. 483 lack code. Each needs CU-level implementation, Kconfig symbol, `#ifdef` guard, and backing test. |
| C-11 | 11 deferred CUs | Historical events, structured data, A&C shelving/suppression |
| G-AGGREGATOR | Remove aggregator `#if` guards from C code | `MUC_OPCUA_CU_DATA_ACCESS`, `MUC_OPCUA_CU_EVENTS` etc. still gate shared files. Replace with individual CU guards per Principle VIII. |
| D-ECC | ECC cert subtypes | 6 ECC-specific CertificateType subtypes |
| D-TRUST | TrustList integration | Full TrustList management + CRL parsing |
| D-PUSH | Push certificate model | ServerConfigurationType reverse-direction management |

## Recently Completed

| Commit | Feature | Notes |
|--------|---------|-------|
| 5-step cleanup | Manifest integrity sweep | Per-CU kconfig_symbol, dead code removal, aggregator cleanup, implementation audit, gate audit |
| #302 | CTT Gauntlet compliance | 42/49 failures fixed. 520 OPC CUs claimed (later audited). |
| #360 | CU claiming sweep | 119→568 claimed, 525 OPC CUs tracked |
| Constitution v1.0.3 | Principle VIII — CU-Level Kconfig Gating | Every CU gets its own Kconfig symbol, `#ifdef` gating, no invented middlemen |

## State

| Category | Count |
|----------|-------|
| Claimed (implemented) | 128 |
| Documented (known, not done) | 488 |
| Deferred | 11 |
| Total OPC server CUs tracked | 525 |

## Deferred

| ID | Item | Notes |
|----|------|-------|
| D-ECC | ECC cert subtypes | 6 ECC-specific CertificateType subtypes |
| D-TRUST | TrustList integration | Full TrustList management + CRL parsing |
| D-PUSH | Push certificate model | ServerConfigurationType reverse-direction management |
