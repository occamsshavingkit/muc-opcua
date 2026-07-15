<!-- markdownlint-disable MD013 -->

# Research: Server Nano Core Service Behaviour CUs

## Decision 1: Treat manifest defaults as canonical for profile claims

**Decision**: Claim nano-default support only for in-scope CUs whose manifest record has `profile_defaults.nano: true`. For this feature, the nano-default CUs are Discovery Get Endpoints, Discovery Find Servers Self, View TranslateBrowsePath, and View Basic 2. Write, Session Change User, and Diagnostics CUs remain optional or micro+ according to the manifest.

**Rationale**: The roadmap title says nano service behaviour, but the manifest is the project source of truth for profile defaults. Overriding it in the spec would create false nano conformance claims.

**Alternatives considered**: Mark all ten CUs nano-default because the roadmap lists them under spec 006. Rejected because it conflicts with `profiles/opcua-profile-manifest.yaml` and Constitution Principle V.

## Decision 2: Start implementation with a CU audit

**Decision**: Tasks must first classify each in-scope CU as already satisfied, needs a symbol, needs tests, or needs missing behaviour.

**Rationale**: Existing handlers and tests already cover parts of Discovery, View, Write, Session, and Diagnostics. Blindly adding code risks duplicate service paths and oversized changes.

**Alternatives considered**: Implement each CU from scratch. Rejected because it ignores existing repository evidence and increases regression risk.

## Decision 3: Use named CU symbols, not broad legacy service aliases

**Decision**: Each claimed CU gets a dedicated `MUC_OPCUA_CU_*` symbol and backing tests; broad aliases can remain only as generated/profile-derived consequences where already established.

**Rationale**: Roadmap item 006 exists because several behaviours are functionally present but not claimable at CU granularity. Named symbols preserve profile honesty and compile-out behaviour.

**Alternatives considered**: Continue claiming aggregate service symbols. Rejected because aggregate claims cannot express optional CU defaults or backing-test traceability.

## Decision 4: Split diagnostics into address-space and service-return surfaces

**Decision**: Plan `opc_cu_3192` around ServerDiagnostics address-space exposure and `opc_cu_3983` around diagnostics returned through service response diagnostics.

**Rationale**: The manifest notes and OPC references define two related but distinct observable behaviours. Existing tests cover live ServerDiagnostics nodes but do not by themselves prove `returnDiagnostics` handling.

**Alternatives considered**: Treat diagnostics as one CU. Rejected because it would hide the Base Services Diagnostics requirement.

## Decision 5: Validation level and fixture strategy

**Decision**: Use unit tests for service handlers and manifest symbols, integration tests for service request/response round trips, binary fixtures for wire-visible encodings where practical, and profile tests for compile-out/default behaviour.

**Rationale**: The affected CUs change service behaviour, StatusCodes, generated configuration, and conformance claims. A single test layer cannot cover all required risks.

**Alternatives considered**: Validate only through profile tests. Rejected because profile tests cannot identify service-level StatusCode and encoding regressions.

## Decision 6: External-system and mock strategy

**Decision**: No external OPC UA server or network dependency is required for this feature's core validation. Tests may use in-process server structures, encoded request fixtures, and deterministic endpoint/session/diagnostics state.

**Rationale**: All in-scope behaviours are local server semantics. External CTT remains out of scope and conformance status stays profile-targeting.

**Alternatives considered**: Require CTT or .NET interop for this slice. Rejected because it would exceed the roadmap scope and block small CU-level iteration.
