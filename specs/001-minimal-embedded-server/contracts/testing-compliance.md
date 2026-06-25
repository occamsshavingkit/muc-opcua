# Contract: Testing, Traceability, and Compliance

## Purpose

The project must prove that a small implementation is correct for the surface it claims and honest about everything it does not support. This contract defines required validation artifacts and gates for protocol work.

## Required Test Layers

### Unit Tests

Location: `tests/unit/`

Required coverage:

- Binary primitive encoding/decoding.
- String length handling, including null String, empty String, 64 byte String, and over-limit String.
- NodeId variants supported in v1.
- ExtensionObject header decoding and malformed input rejection.
- Variant and DataValue encoding for Boolean, Int32, UInt32, Float, and bounded String.
- StatusCode mapping.
- Static address-space validation.
- Service state-machine transitions.

### Byte Fixtures

Location: `tests/fixtures/`

Required coverage:

- Hello and Acknowledge messages.
- UASC MessageChunk headers and sequence headers.
- OpenSecureChannel request/response bodies for SecurityPolicy None phase.
- FindServers and GetEndpoints request/response bodies.
- CreateSession, ActivateSession, CloseSession request/response bodies.
- Browse request/response for the default static address space.
- Read request/response for each v1 scalar type.
- Unsupported-service request and `Bad_ServiceUnsupported` response.

Fixture requirements:

- Every fixture has a Markdown sidecar naming the OPC UA type/message, encoding NodeId when applicable, byte length, source section, and expected decoder result.
- Fixtures may be generated, but generation must be reproducible and size-reviewed.

### Malformed Input Tests

Location: `tests/unit/` and `tests/integration/`

Required coverage:

- Message size smaller than header.
- Message size larger than RX buffer or negotiated limit.
- Invalid chunk type or message type.
- Chunk boundary split at every header and payload field.
- Negative, excessive, and truncated String lengths.
- Invalid array lengths and truncated array elements.
- Invalid NodeId encoding masks.
- Invalid ExtensionObject encoding and body length.
- Browse before active Session.
- Read before active Session.
- Session service before SecureChannel.
- Unsupported service IDs.
- Second active client while the single slot is occupied.

### Fuzz and Property Tests

Location: `tests/fuzz/`

Required targets:

- Binary reader entry point.
- MessageChunk parser.
- NodeId decoder.
- String and array length decoder.
- ExtensionObject decoder.
- Variant/DataValue decoder.
- Service dispatch predecoder.

Requirements:

- libFuzzer is preferred when Clang supports it.
- AFL++ may be used as an alternate target.
- CI must skip fuzz execution gracefully when tooling is unavailable, but deterministic malformed-input tests remain mandatory.

### Integration Tests

Location: `tests/integration/`

Required flows:

- Host minimal server starts with caller-owned buffers.
- Client completes Hello/Acknowledge.
- Client calls FindServers and GetEndpoints on the DiscoveryEndpoint before any Session exists.
- Client opens SecureChannel for SecurityPolicy None phase.
- Client creates and activates Session using the selected v1 identity token.
- Client calls FindServers and GetEndpoints.
- Client browses the tiny static address space.
- Client receives documented behavior for Browse limits, absent continuation points, and unsupported BrowseNext.
- Client reads Boolean, Int32, UInt32, Float, and bounded String variables.
- Client receives correct StatusCode or cited transport behavior for unsupported services, unsupported writes, unsupported service sets, and unsupported alternate transports/encodings.

## CI Gates

Required jobs:

- CMake configure and build on host.
- Unity/CTest unit and integration tests.
- Sanitizer build where compiler support exists.
- Fuzz target compile where compiler support exists.
- `clang-format` check.
- Compiler warnings as errors.
- `cppcheck`.
- `clang-tidy` where useful and not too noisy for generated code.
- Pico SDK cross-compile for the library and minimal example.
- Size report generation for host and Pico builds once implementation exists.

## Traceability Table Contract

Location: `docs/traceability/`

Minimum generated tables:

- `sections-to-files.md`: OPC UA section to source/header/test artifacts.
- `files-to-sections.md`: source/header/test artifact to OPC UA sections.
- `statuscodes.md`: internal condition to OPC UA StatusCode or TCP error behavior.
- `fixtures.md`: byte fixture to OPC UA type/message/section.
- `opcua-mcp-queries.md`: OPC UA MCP query provenance for profile, service, encoding, transport, StatusCode, and conformance decisions.

Target profile/facet/conformance-unit evidence is maintained in `docs/conformance/profile-nano.md` (see Conformance Status Contract below); it is not duplicated as a separate traceability table.

Each row must include:

- Artifact path.
- Implemented or tested behavior.
- OPC UA part and exact section.
- Conformance unit if applicable.
- Test evidence.
- Current status: planned, implemented, tested, verified.

## Conformance Status Contract

Location: `docs/conformance/`

Required documents:

- `status.md`: current label, one of experimental, profile-targeting, profile-compliant, or CTT-verified.
- `profile-nano.md`: Nano Embedded Device Server Profile evidence and conformance-unit checklist.
- `services.md`: supported and unsupported services with StatusCode behavior.
- `security.md`: SecurityPolicy and user token policy behavior.
- `identity-policy.md`: profile-derived accepted identity-token policy and rejection behavior before endpoint/session implementation.
- `interop.md`: client/tool versions used for interoperability tests.

Rules:

- Do not claim `profile-compliant` until every mandatory selected profile item is implemented, tested, and traced.
- Do not claim `CTT-verified` without actual CTT evidence.
- Compatibility claims must name exact supported services, encoding, transport profile URI, SecurityPolicy behavior, and tested conformance units.

## Size Reporting Contract

Location: `docs/size/`

Required reports:

- `pico-minimal-server.md`: Pico SDK size output for the default minimal example.
- `host-minimal-server.md`: host build size as a development signal.
- `feature-size-ledger.md`: feature additions with flash, RAM, stack, buffer, static table, heap, and crypto impact.

Budget rules:

- Default v1 budget: <= 128 KiB flash for core plus minimal example, excluding board TCP/IP stack and unrelated application code.
- Default v1 budget: <= 32 KiB RAM for core static data, peak stack estimate, and default RX/TX buffers, excluding platform TCP/IP buffers.
- Default RX/TX buffers: 8192 bytes each unless validated and documented otherwise.
- Heap use in protocol hot path: zero mandatory allocation.

## async-opcua Compliance Inventory

Source: `occamsshavingkit/async-opcua`

Required inventory targets:

- `.devcontainer/devcontainer.json`
- `codegen-tests`
- `dotnet-tests`
- `fuzz`
- `schemas`
- `tools`
- Any GitHub Actions workflows or scripts that run OPC UA interoperability/compliance checks.

Adoption gate:

- Inventory during planning/task creation.
- Do not require async-opcua tests in CI until micro-opcua host integration can complete minimal connect/discover/browse/read.
- Once adopted, each imported or adapted test must be mapped to a `docs/traceability/` row and a conformance-status document.

## Test-First Rule

Any task touching one of the following areas must create or update tests before implementation code:

- Binary encoders/decoders.
- MessageChunk parsing and assembly.
- SecureChannel or Session state machines.
- Service dispatch.
- Browse and Read handlers.
- StatusCode mapping.
- Unsupported service behavior.
- Static address-space lookup.
- SecurityPolicy or identity-token handling.
