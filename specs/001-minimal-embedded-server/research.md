# Research: Minimal Embedded Server

## Research Inputs

- OPC UA reference MCP server is the primary source for normative section lookup. The OPC Foundation documents the MCP endpoint and tools at https://reference.opcfoundation.org/ai-help.
- OPC Foundation online reference sections are used for exact OPC-10000 citations in the plan, contracts, traceability, and future tasks.
- OPC Foundation profile metadata is used to identify profile names and URIs where the MCP conformance-unit search does not expose a compact profile listing.
- Sister project `occamsshavingkit/async-opcua` is recorded as a future compliance-testing input. Its `.devcontainer`, `codegen-tests`, `dotnet-tests`, `fuzz`, `schemas`, and `tools` areas must be inventoried before micro-opcua adopts or adapts those tests.

## Decision R1: Initial OPC UA Profile Target

**Decision**: Target the Nano Embedded Device Server Profile family first, profile URI `http://opcfoundation.org/UA-Profile/Server/NanoEmbeddedDevice`, and label the implementation `profile-targeting` until all mandatory profile facets and conformance units are implemented and tested.

**Rationale**: OPC-10000-7 4.2 defines conformance units and conformance groups as the basis for conformance claims. OPC-10000-7 4.4 and 4.6 describe profile categories and profile definition conventions. OPC Foundation profile metadata identifies Nano Embedded Device Server Profile as the embedded profile with URI `http://opcfoundation.org/UA-Profile/Server/NanoEmbeddedDevice`; Micro Embedded Device Server Profile metadata states that Micro builds on Nano and adds subscriptions. Because subscriptions are out of scope for the smallest v1 server, Nano is the smallest profile family that matches the product goal.

**Alternatives Considered**:

- **Micro Embedded Device Server Profile**: rejected for v1 because it builds on Nano and adds subscriptions, which exceed the minimal connect/discover/browse/read target.
- **Core Server Facet or Standard UA Server Profile**: rejected for v1 because they imply a broader server surface than the smallest embedded target.
- **No profile target**: rejected because the constitution requires profile/conformance honesty and a justified OPC-10000-7 target.

**Follow-up Evidence Required**: Before any `profile-compliant` claim, create `docs/conformance/profile-nano.md` with the exact mandatory profile/facet/conformance-unit checklist and trace each item to implementation/tests.

## Decision R2: Transport and Encoding

**Decision**: Implement OPC UA TCP over `opc.tcp` with UASC message chunks and OPC UA Binary message bodies. Advertise transport profile URI `http://opcfoundation.org/UA-Profile/Transport/uatcp-uasc-uabinary`.

**Rationale**: OPC-10000-6 7.2 defines OPC UA TCP, 7.1.2.3 and 7.1.2.4 define Hello/Acknowledge negotiation, 6.7.1 through 6.7.4 define Secure Conversation chunking and SecureChannel establishment, and 5.2.1 defines OPC UA Binary DataEncoding. This is the smallest transport/encoding combination aligned with embedded performance and the selected profile family.

**Alternatives Considered**:

- **HTTPS, WebSockets, XML, JSON**: rejected because they add code size and are explicitly out of v1 scope.
- **Raw service calls without UASC framing**: rejected because it would not be OPC UA TCP conformant.

## Decision R3: Service Surface

**Decision**: Implement only the services required for endpoint discovery, SecureChannel/session setup and teardown, browsing, and reading:

- Discovery: `FindServers` and `GetEndpoints` from OPC-10000-4 5.5.2 and 5.5.4.
- SecureChannel: `OpenSecureChannel` and `CloseSecureChannel` from OPC-10000-4 5.6.2 and 5.6.3.
- Session: `CreateSession`, `ActivateSession`, and `CloseSession` from OPC-10000-4 5.7.2, 5.7.3, and 5.7.4.
- View: `Browse` from OPC-10000-4 5.9.2.
- Attribute: `Read` from OPC-10000-4 5.11.2.

**Rationale**: OPC-10000-4 5.5.1 requires each Server to have a DiscoveryEndpoint accessible without a Session. Browse and Read are the smallest useful service pair for exposing a tiny static address space and simple variable values. The service set is also consistent with the user's explicit first-version outcomes.

**Alternatives Considered**:

- **BrowseNext and continuation points**: deferred unless profile requirements or client tests prove it is required. The v1 address-space and operation limits should keep Browse responses small. If a request would require a continuation point that cannot be allocated, return the cited Browse operation-level StatusCode from OPC-10000-4 5.9.2.4 and 7.9.
- **Write, subscriptions, methods, history, alarms/events, PubSub**: rejected for v1 unless profile research proves they are mandatory.
- **Session-less invocation**: rejected for v1 because it adds a separate invocation model and is not needed for the minimal embedded flow.

## Decision R4: Security and Identity

**Decision**: Keep cryptography behind a pluggable adapter. Implement SecurityPolicy None only as a non-production interoperability phase unless Nano profile evidence explicitly permits the claim. Support AnonymousIdentityToken if permitted by the selected profile; reject all unsupported identity token types with cited OPC UA StatusCodes.

**Rationale**: OPC-10000-4 5.6.2 covers OpenSecureChannel, 7.40.1 and 7.40.3 define user identity token concepts including AnonymousIdentityToken, and 7.41 defines UserTokenPolicy. OPC-10000-4 5.7.3 defines ActivateSession and its identity-token handling. The constitution requires conformance honesty and a future crypto backend boundary.

**Alternatives Considered**:

- **Implement production crypto in v1**: deferred because the smallest embedded surface should first establish correct framing, service state, and traceability. The adapter must allow later mbedTLS, PSA Crypto, wolfSSL, or platform crypto.
- **Hard-code anonymous access without profile evidence**: rejected; anonymous behavior must be tied to endpoint policy and profile evidence.

## Decision R5: Memory, Buffers, and Size Budget

**Decision**: Use one active client, one SecureChannel, and one Session by default; caller-provided RX/TX buffers; no mandatory heap allocation in the protocol hot path; and fixed default limits documented in public configuration.

**Rationale**: The first embedded target is RP2040-class hardware with 264 KiB RAM, so predictable memory ownership is more important than broad concurrency. OPC-10000-6 7.1.2.3 and 7.1.2.4 define buffer negotiation through Hello and Acknowledge messages. Default 8192 byte RX and TX buffers leave enough space for one endpoint/session flow while avoiding dynamic chunk buffers.

**Alternatives Considered**:

- **Multiple concurrent Sessions**: rejected unless the selected profile requires it.
- **Heap-backed message assembly**: rejected because it violates the protocol hot-path memory requirement.
- **Very small default buffers**: rejected until exact legal minima are proven for the advertised endpoint/security behavior and interoperability clients.

## Decision R6: Address Space and Data Types

**Decision**: Model a tiny static address space with Object and Variable nodes, read-only scalar Variable values, and fixed references. First-version scalar values are Boolean, Int32, UInt32, Float, and bounded String with a maximum encoded payload of 64 UTF-8 bytes.

**Rationale**: OPC-10000-3 5.2.1 defines base NodeClass concepts, 5.5.1 covers Object NodeClass, 5.6.2 covers Variable NodeClass and the Value/DataType/ValueRank/AccessLevel relationship, and 5.9 summarizes NodeClass attributes. OPC-10000-6 5.2.2.4 defines String binary encoding, and 5.2.2.16 and 5.2.2.17 define Variant and DataValue encodings needed for Read responses.

**Alternatives Considered**:

- **Dynamic address-space mutation**: rejected because it adds ownership, locking, and allocation complexity.
- **Arrays and ExtensionObject variable values**: rejected for v1 variable data, but decoders must still safely reject or skip malformed ExtensionObjects as required by the parser scope.

## Decision R7: Unsupported and Malformed Behavior

**Decision**: Route unsupported services to `Bad_ServiceUnsupported` from OPC-10000-4 7.38.2. Route a second concurrent client to the OPC UA TCP error behavior in OPC-10000-6 7.1.5, using `TcpServerTooBusy` unless profile research selects a more specific behavior. Map malformed Binary, chunk, NodeId, ExtensionObject, array, String, and state-machine failures to cited service-level, operation-level, or transport-level behavior.

**Rationale**: A small stack must be honest and deterministic. OPC-10000-4 7.38.2 provides common StatusCodes, Browse/Read have service-specific result sections, and OPC-10000-6 7.1.5 defines TCP error handling before service decoding is possible.

**Alternatives Considered**:

- **Silently ignore unsupported features**: rejected because it creates false compatibility.
- **Close every malformed request immediately**: rejected because OPC UA distinguishes service errors, operation errors, decode errors, and transport errors depending on where the failure occurs.

## Decision R8: Test Framework and Validation Strategy

**Decision**: Use Unity as the default unit-test framework, with CMake/CTest wrappers. Add libFuzzer or AFL++ targets for host fuzzing where the toolchain supports them. Use byte fixtures tied to OPC UA type definitions for all encoder/decoder behavior.

**Rationale**: Unity is tiny C, easy to vendor or fetch reproducibly, and suitable for host and embedded builds. The parser, chunker, and state machine are high-risk surfaces and require deterministic fixtures plus malformed-input tests before implementation.

**Alternatives Considered**:

- **CMocka**: acceptable fallback, but Unity has a smaller footprint and simpler embedded story.
- **Only integration tests**: rejected because decoders and status mapping require boundary and malformed-input coverage at unit-test granularity.

## Decision R9: Generated Tables

**Decision**: Permit optional generated tables only for compact, auditable OPC UA NodeId/type/status mappings. Generated files must be reproducible, checked by tests, and reported in size output.

**Rationale**: Hand-maintaining many numeric NodeIds and encoding IDs is error-prone, but broad generated type systems could consume too much flash. A small generated table bounded by v1 services/data types balances correctness with size.

**Alternatives Considered**:

- **Generate the full OPC UA type universe**: rejected for size and auditability.
- **Hard-code all numbers inline**: rejected because it weakens traceability and increases typo risk.

## Decision R10: async-opcua Compliance Suite Integration

**Decision**: Inventory `occamsshavingkit/async-opcua` compliance assets during planning and defer adoption or adaptation until micro-opcua can complete minimal connect/discover/browse/read on the host transport.

**Rationale**: The sister project appears to have a strong Codespaces-based OPC UA test environment, including `.devcontainer`, `codegen-tests`, `dotnet-tests`, `fuzz`, `schemas`, and `tools`. Importing it before the minimal protocol path exists would obscure core bring-up; inventorying it now preserves a path to stronger compliance testing.

**Alternatives Considered**:

- **Adopt async-opcua tests immediately**: rejected because v1 needs small, test-first C parser/service milestones before broad external suites.
- **Ignore the suite until after release**: rejected because the spec explicitly records it as future compliance input.
