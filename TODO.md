<!-- markdownlint-disable MD013 MD022 MD032 MD056 MD060 -->

# TODO — muc-opcua

**Updated**: 2026-10-04

## Pending

| ID | Item | Notes |
|----|------|-------|
| C-483 | 483 documented CUs need implementation | Audited down from 611 → 128 claimed. 483 lack code. Each needs CU-level implementation, Kconfig symbol, `#ifdef` guard, and backing test. |
| C-11 | 11 deferred CUs | Historical events, structured data, A&C shelving/suppression |
| G-AGGREGATOR | Remove aggregator `#if` guards from C code | `MUC_OPCUA_CU_DATA_ACCESS`, `MUC_OPCUA_CU_EVENTS` etc. still gate shared files. Replace with individual CU guards per Principle VIII. |
| D-ECC | ECC cert subtypes | 6 ECC-specific CertificateType subtypes |
| D-TRUST | TrustList integration | Full TrustList management + CRL parsing |
| D-PUSH | Push certificate model | ServerConfigurationType reverse-direction management |
| S-363-1 | too-many-branches (17/15) in `scripts/profile_manifest/generated_kconfig_policy.py:46` | Pre-existing style debt surfaced by Codacy on PR #363; generated-policy parser complexity. |
| S-363-2 | too-many-locals (24/15) in `scripts/profile_manifest/generated_kconfig_policy.py:46` | Pre-existing style debt surfaced by Codacy on PR #363; same generated-policy parser. |
| S-363-3 | too-many-arguments (6/5) in `scripts/profile_manifest/model.py:106` | Pre-existing style debt surfaced by Codacy on PR #363; stable internal model API. |
| S-363-4 | too-many-positional-arguments (6/5) in `scripts/profile_manifest/model.py:106` | Pre-existing style debt surfaced by Codacy on PR #363; same stable model API. |
| S-363-5 | Explicit `validate_manifest(...) == []` comparison in `scripts/profile_manifest/test_model.py:27` | Pre-existing style debt surfaced by Codacy on PR #363; explicit assertion style. |
| S-363-6 | Explicit `errors == []` comparison in `scripts/profile_manifest/test_model.py:191` | Pre-existing style debt surfaced by Codacy on PR #363; pre-range line shifted by a new test. |
| S-363-7 | too-many-locals (18/15) in `scripts/profile_manifest/test_profile_requirements.py:71` | Pre-existing style debt surfaced by Codacy on PR #363; pre-existing test readability debt. |

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
