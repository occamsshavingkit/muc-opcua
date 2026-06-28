# Implementation Plan: UserName/Password User Identity Token Authentication

**Branch**: `008-user-identity-auth` | **Date**: 2026-06-28 | **Spec**: [spec.md](file:///home/quackdcs/micro-opcua/specs/008-user-identity-auth/spec.md)
**Input**: Feature specification from `/specs/008-user-identity-auth/spec.md`

## Summary

This plan adds support for UserName/Password User Identity Token authentication during `ActivateSession` (OPC-10000-4 §5.7.3). We will implement binary decoding of `UserNameIdentityToken` (NodeId 324) according to OPC-10000-6 §7.38.3, register a pluggable user authentication callback in the server configuration, validate client credentials against this callback, and reject unauthorized sessions with `Bad_IdentityTokenRejected`.

## Technical Context

**Language/Version**: C11  
**Primary Dependencies**: None (freestanding core)  
**Storage**: N/A (application managed)  
**Testing**: Unity (host), Python/asyncua interop tests  
**Target Platform**: RP2040 (Cortex-M0+), Linux/POSIX host  
**Project Type**: library  
**Performance Goals**: Sub-millisecond validation and session activation  
**Constraints**: Zero dynamic heap allocations, zero mutable globals  
**Scale/Scope**: < 1.5 KB flash impact  
**OPC UA Normative References**: OPC-10000-4 §5.7.3, §7.36, OPC-10000-6 §7.38.3  
**Target OPC UA Profile/Conformance Units**: User Token-UserName Server Facet (OPC-10000-7 §6.6.14)  
**Conformance Status Target**: profile-targeting  

## Embedded Size Budget

**Flash Impact**: Under 1.5 KB of ARM Thumb code  
**RAM Impact**: 0 bytes static BSS; transient stack usage under 256 bytes  
**Heap Use**: None (zero heap allocations)  
**Static Tables Added**: N/A  
**Transport Buffers**: N/A  
**Crypto Dependency Impact**: None new; utilizes existing RSA decryption primitives when decrypting encrypted user passwords.  

## Constitution Check

- **Spec Fidelity**: Yes. User identity token decoding and session activation match OPC UA specifications exactly.
- **Embedded C Core**: Yes. Standard C11, fully portable freestanding code.
- **Memory Model**: Yes. No dynamic allocations; parsing of the token is done in-place or using stack buffers.
- **Minimal OPC UA Surface**: Yes. Username/password user authentication is gated using CMake options.
- **Profile Research**: Yes. Targets the User Token-UserName Server Facet.
- **Correctness Gates**: Yes. Incorporates binary packet decoder tests and integration authentication tests.
- **Security Honesty**: Yes. Enforces standard password decryption / authentication rules.
- **Toolchain Discipline**: Yes. Integrates into host CMake build.
- **Size Discipline**: Yes. Establishes a code footprint budget under 1.5 KB.

## Project Structure

### Documentation

```text
specs/008-user-identity-auth/
├── plan.md              # This file
├── spec.md              # Feature specification
├── research.md          # Design research
├── data-model.md        # Wire structures
├── quickstart.md        # Integration guide
└── tasks.md             # Implementation tasks
```

### Source Code

```text
include/
└── micro_opcua/
    └── server.h         # Register user authentication callback
src/
├── core/
│   └── service_dispatch.c # Read token type and call session activate
├── encoding/
│   ├── binary_reader.c   # Decode UserNameIdentityToken
│   └── binary_reader.h
└── services/
    ├── session.c        # Check credentials in mu_session_activate
    └── session.h
```

**Structure Decision**: Pluggable authentication callback registered on the server configuration. Binary decoding added to encoding module. Validation logic routed during `ActivateSession` dispatch.

## Complexity Tracking

No violations of the Constitution.
