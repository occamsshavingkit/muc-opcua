# Implementation Plan: Core Feature Expansion

**Branch**: `009-core-feature-expansion` | **Date**: 2026-06-28 | **Spec**: [spec.md](spec.md)
**Input**: Feature specification from `/specs/009-core-feature-expansion/spec.md`

## Summary

This plan outlines the core expansion of the server library, adding support for the Write Service (OPC-10000-4 §5.10.4), multiple concurrent TCP connections, modern security policies (Aes128_Sha256_RsaOaep and Aes256_Sha256_RsaPss), X.509 user certificate authentication, and Alarms & Conditions (Events). Each feature is gated using compile flags to maintain the library's "pay-for-what-you-use" footprint design.

## Technical Context

**Language/Version**: C11  
**Primary Dependencies**: None (freestanding core)  
**Storage**: N/A (caller provided)  
**Testing**: Unity (host), CTest  
**Target Platform**: Arm Cortex-M0+ (RP2040), Linux/POSIX host  
**Project Type**: library  
**Performance Goals**: Sub-millisecond write handling and connection multiplexing  
**Constraints**: Zero dynamic heap allocations on the hot path; transient stack usage under 512 bytes  
**Scale/Scope**: Write service set, support up to 3 concurrent TCP connections, AES-128/256 security policies, certificate user token authentication, simple alarm events.  
**OPC UA Normative References**: OPC-10000-4 §5.10.4 (Write), §7.36.4 (Certificate Identity Token), §5.12.1 (Events), OPC-10000-7 §6.2 (Security Policies), OPC-10000-6 §7.38.3.  
**Target OPC UA Profile/Conformance Units**: User Token-UserName Server Facet, User Token-X509 Server Facet, Core 2017 Server Facet, Standard DataChange Subscription 2017.  
**Conformance Status Target**: profile-targeting  

## Embedded Size Budget

**Flash Impact**: Under 10 KB of ARM Thumb code with all options enabled (typically 2 KB for Write, 1 KB for multi-conn, 3 KB for Aes128/256 policy additions, 2 KB for certificate user auth, 2 KB for simple events).  
**RAM Impact**: Configurable via capacity macros; transient stack usage under 512 bytes.  
**Heap Use**: None (zero heap allocations).  
**Static Tables Added**: Configurable endpoint list additions (~512 bytes flash).  
**Transport Buffers**: Caller-owned buffers (default 2 × 8 KiB).  
**Crypto Dependency Impact**: None new; utilizes existing RSA and hash primitives in the platform crypto adapter.  

## Constitution Check

- **Spec Fidelity**: Yes. Write, secure channel, and certificate user auth parsing and status returns conform to the cited OPC UA standard sections.
- **Embedded C Core**: Yes. Free of OS dependencies; standard C11 core.
- **Memory Model**: Yes. No dynamic allocations; all buffers and connection/session memory are allocated statically or supplied by the caller.
- **Minimal OPC UA Surface**: Yes. Features are gated behind CMake options (`MICRO_OPCUA_SERVICE_WRITE`, `MICRO_OPCUA_MULTIPLE_CONNECTIONS`, etc.) to keep the surface minimal when not required.
- **Profile Research**: Yes. Targets the respective standard profiles and conformance units.
- **Correctness Gates**: Yes. Incorporates binary decoder, decryption, and end-to-end integration tests.
- **Security Honesty**: Yes. Enforces valid signatures for certificate user token validation.
- **Toolchain Discipline**: Yes. Managed via CMake and testable locally on the host.
- **Size Discipline**: Yes. Footprint remains well within target flash and RAM limits.

## Project Structure

### Documentation (this feature)

```text
specs/009-core-feature-expansion/
├── plan.md              # This file
├── research.md          # Research findings
├── data-model.md        # Wire and memory layout design
├── quickstart.md        # Integration guide
└── tasks.md             # Implementation tasks
```

### Source Code (repository root)

```text
include/
└── micro_opcua/
    └── server.h         # Write callback config and connection limits
src/
├── core/
│   ├── server.c         # Multiple connection multiplexing
│   └── service_dispatch.c # Write request handling, certificate verification
├── encoding/
│   └── binary_reader.c   # Certificate token decoder
├── services/
│   ├── session.c        # Certificate-based session activation
│   └── write.c          # Write service logic
└── security/
    └── security_policy.c # AES-128/256 policy definitions
```

**Structure Decision**: Code additions will be integrated directly into the appropriate core modules (decoding in encoding, service routing in service_dispatch, multiplexing in server) and isolated using `#ifdef` gates.

## Complexity Tracking

*No violations of the Constitution.*

## Size Impact (US4)

- **Flash Impact**: The CertificateIdentityToken decoder (`mu_binary_read_certificate_identity_token`) compiles to **48 bytes** of ARM Thumb code. The signature verification and integration within `handle_activate_session` add approximately **200 bytes** of code.
- **RAM Impact**: Adds `opcua_byte_t server_nonce[32]` to the `mu_session_t` struct, resulting in **32 bytes** of additional static RAM per session slot, which is fully caller-provided.
- **Heap Use**: **0 bytes** (zero heap).

## Size Impact (US5 - Events)

- **Flash Impact**: The Event notification and filtering logic compiles to approximately **600 bytes** of ARM Thumb code.
- **RAM Impact**: Adds circular queue storage (`mu_event_notification_t` array) and select clauses tracking to `mu_subscription_t` and `mu_monitored_item_t`. This increases the static memory requirement by **384 bytes** per subscription slot, managed via the `MU_EVENTS_STORAGE_BYTES` configuration macro.
- **Heap Use**: **0 bytes** (zero heap).

