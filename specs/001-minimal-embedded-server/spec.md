# Feature Specification: Minimal Embedded Server

**Feature Branch**: `001-minimal-embedded-server`
**Created**: 2026-06-25
**Status**: Draft
**Input**: User description: "Create the initial complete micro-opcua implementation: a smallest-practical embedded OPC UA server stack for large Arduino-class boards, Raspberry Pi Pico/RP2040-class boards, and comparable microcontrollers."

## Clarifications

### Session 2026-06-25

- Q: Should the occamsshavingkit/async-opcua Codespaces testing suite be noted for future compliance testing? -> A: Yes; inventory and adopt or adapt it when compliance testing is planned.
- Q: When should the async-opcua Codespaces testing suite become part of micro-opcua validation? -> A: Inventory during planning; adopt or adapt tests after minimal connect/discover/browse/read works.
- Q: Should the available OPC UA reference MCP server be recorded as a required research source? -> A: Yes; use it to ground every OPC UA decision in exact normative specification references.
- Q: How many active client/SecureChannel/Session contexts must the first version support? -> A: One active client/SecureChannel/Session by default; increase only if the selected OPC UA profile requires it.
- Q: Which user identity-token scope should the first version support? -> A: Defer identity-token scope to OPC-10000-7 profile research.
- Q: Which scalar OPC UA variable data types must the first-version minimal address space expose? -> A: Boolean, Int32, UInt32, Float, and bounded String.
- Q: What maximum encoded length must a first-version bounded String variable support? -> A: 64 UTF-8 bytes.

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Build and Verify the Portable Core (Priority: P1)

An embedded firmware developer can obtain the project, build the portable
micro-opcua library on a host machine, run the host validation suite, and confirm
that the same core can be cross-compiled for an RP2040-class target without an
operating system dependency or mandatory heap allocation in the protocol hot path.

**Why this priority**: A host-buildable and host-testable core is the minimum
foundation for every later interoperability and embedded milestone.

**Independent Test**: Build the project on a host machine, run the required host
checks, and run the embedded cross-compile target. The story passes only when the
host tests pass, static checks complete, and the embedded target compiles without
requiring application heap allocation in the protocol hot path.

**Acceptance Scenarios**:

1. **Given** a fresh checkout on a supported host, **When** the maintainer runs the documented build and test flow, **Then** the portable core builds and all host tests pass.
2. **Given** the RP2040-class embedded target is selected, **When** the maintainer runs the documented cross-compile flow, **Then** the library and minimal example compile for the embedded target.
3. **Given** the protocol hot path is reviewed, **When** memory ownership is inspected through the public configuration surface and traceability records, **Then** protocol buffers and address-space storage are static or caller-provided.

---

### User Story 2 - Configure a Tiny Static Address Space (Priority: P2)

An embedded firmware developer can configure server identity, endpoint URL,
caller-provided transport buffers, and a tiny static address space containing
the five first-version read-only scalar value kinds with fixed values or read
callbacks.

**Why this priority**: A useful embedded server must expose application data while
preserving explicit memory ownership and predictable resource use.

**Independent Test**: Configure a host-based minimal server with a small static
address space, run it with caller-provided buffers, and verify that configured
variables are visible to the server core and remain read-only.

**Acceptance Scenarios**:

1. **Given** an application provides server identity, endpoint URL, buffers, and read-only variables covering Boolean, Int32, UInt32, Float, and bounded String values, **When** the server starts, **Then** the configured identity, endpoint, and variables are available to supported OPC UA services.
2. **Given** the configured address space contains read-only variables, **When** an unsupported write path is requested, **Then** the server returns the specified OPC UA StatusCode and does not mutate application data.
3. **Given** application-owned storage is used, **When** the server processes requests, **Then** it does not take ownership of application memory beyond documented lifetimes.

---

### User Story 3 - Connect, Discover, Browse, and Read (Priority: P3)

An OPC UA client/operator can connect to the minimal server over OPC UA TCP using
opc.tcp, complete the required connection, SecureChannel, and Session flow for
the selected profile, discover the server endpoint, browse the tiny static
address space, and read simple variable values.

**Why this priority**: The first externally valuable milestone is an interoperable
server that exposes real device data through the smallest standards-based surface.

**Independent Test**: Run the host minimal server and use a standards-compliant
OPC UA client or scripted test client to complete endpoint discovery, connection,
session establishment, browsing, and reads of configured variables.

**Acceptance Scenarios**:

1. **Given** the minimal server is running with a configured opc.tcp endpoint, **When** a client requests endpoint discovery, **Then** the server returns its own endpoint description according to the cited OPC UA Discovery rules.
2. **Given** the client connects to the advertised endpoint, **When** the connection, SecureChannel, and Session flow completes, **Then** the client can call supported browse and read behavior within the selected profile scope.
3. **Given** the address space includes simple variable values, **When** the client reads those variables, **Then** it receives the configured values with the expected OPC UA success status.

---

### User Story 4 - Reject Unsupported and Malformed Requests Correctly (Priority: P4)

A project maintainer or client/operator can verify that unsupported services,
out-of-scope features, malformed binary payloads, invalid lengths, invalid
NodeIds, invalid ExtensionObjects, invalid arrays, invalid strings, invalid
chunks, and invalid request sequences produce deterministic OPC UA failures.

**Why this priority**: A small implementation must be honest and predictable when
it receives behavior outside its supported surface.

**Independent Test**: Run the malformed-input, unsupported-service, and invalid
state-sequence tests and confirm each case returns the expected OPC UA StatusCode
or closes the connection when required by the cited standard section.

**Acceptance Scenarios**:

1. **Given** a request for an unsupported service, **When** the server receives it, **Then** it returns the specified OPC UA unsupported-service result without partial execution.
2. **Given** malformed OPC UA Binary data, **When** the server decodes it, **Then** it rejects the message deterministically and preserves server availability for subsequent valid requests when the standard permits.
3. **Given** a request arrives in an invalid channel or session state, **When** the server evaluates the request, **Then** it returns or signals the cited OPC UA failure behavior.

---

### User Story 5 - Audit Conformance, Traceability, and Size (Priority: P5)

A project maintainer can inspect project documentation and generated reports to
see the current conformance status, targeted profile or conformance units,
supported services, encoding, transport, SecurityPolicy behavior, test evidence,
and embedded size impact.

**Why this priority**: The project must avoid overstating OPC UA compatibility and
must leave useful room for embedded application logic.

**Independent Test**: Review the traceability and conformance documentation after
the validation suite runs. The story passes only when every implemented
wire-visible behavior maps to cited OPC UA sections and size results are recorded
against the agreed embedded budget.

**Acceptance Scenarios**:

1. **Given** a completed build and test run, **When** the maintainer opens the traceability documentation, **Then** implemented services, messages, encodings, StatusCodes, and tests are mapped to exact OPC UA sections.
2. **Given** compatibility documentation is reviewed, **When** conformance status is stated, **Then** it is labeled as experimental, profile-targeting, profile-compliant, or CTT-verified and does not claim verification that has not occurred.
3. **Given** the embedded example is measured, **When** flash and RAM usage are recorded, **Then** the report shows remaining application headroom against the selected RP2040-class budget.

### Edge Cases

- A received OPC UA message exceeds the negotiated or configured receive buffer.
- A message chunk header declares a size that is too small, too large, or
  inconsistent with the actual received data.
- A request uses an unsupported service, unsupported encoding, unsupported
  transport, unsupported SecurityPolicy, or out-of-scope feature.
- A request arrives before the required connection, SecureChannel, or Session
  state has been established.
- A client presents a user identity token type that is not included in the
  researched minimal profile scope.
- A binary payload contains malformed primitive values, invalid NodeId encodings,
  invalid ExtensionObjects, negative or excessive string lengths, invalid arrays,
  or truncated structures.
- A configured bounded String value exceeds the first-version maximum of 64 UTF-8
  bytes.
- A client requests write, subscription, method, historical access, alarms/events,
  PubSub, XML, JSON, HTTPS, WebSockets, dynamic address-space mutation, or
  companion-spec behavior in the first-version server.
- A configured address-space variable callback fails, times out, or reports that
  data is temporarily unavailable.
- A second client attempts to connect while the single active
  client/SecureChannel/Session slot is already in use.
- A minimal embedded build lacks optional fuzzing or sanitizer tooling; the
  project must document the unavailable tool and still run deterministic tests.
- Embedded measurement shows the default example exceeds the agreed flash/RAM
  budget or leaves insufficient application headroom.

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: System MUST provide a portable library that can be built and tested
  on a host machine and cross-compiled for an RP2040-class embedded target.
- **FR-002**: System MUST expose a public configuration surface for server
  identity, endpoint URL, caller-provided memory, transport buffers, static
  address-space nodes, and read-only variable values or read callbacks.
- **FR-003**: System MUST avoid mandatory heap allocation in the protocol hot path
  and document all required application-owned buffers and lifetimes.
- **FR-004**: System MUST provide narrow adapter contracts for TCP I/O, timers,
  entropy, persistence, and optional cryptography.
- **FR-005**: System MUST include a host transport/test adapter that enables
  integration testing without embedded hardware.
- **FR-006**: System MUST include RP2040/Pico SDK adapter and example support for
  the first embedded target.
- **FR-007**: System MUST include an Arduino/PlatformIO adapter skeleton so
  Arduino-class integration boundaries are documented for later completion.
- **FR-008**: System MUST include a minimal example server exposing a tiny static
  address space with simple read-only variables.
- **FR-009**: System MUST support OPC UA TCP over opc.tcp as the initial transport
  and OPC UA Binary as the initial encoding.
- **FR-010**: System MUST support enough endpoint discovery behavior for a client
  to discover the server endpoint required by the selected minimal profile.
- **FR-011**: System MUST support the connection, SecureChannel, and Session flow
  required by the selected minimal profile before accepting browse/read requests.
- **FR-012**: System MUST support browsing the configured tiny static address
  space within the selected minimal profile scope.
- **FR-013**: System MUST support reading configured simple variable values from
  the tiny static address space.
- **FR-014**: System MUST reject unsupported services and out-of-scope features
  with the correct OPC UA StatusCode or cited connection behavior.
- **FR-015**: System MUST reject malformed OPC UA Binary payloads, invalid message
  chunks, invalid lengths, invalid NodeIds, invalid ExtensionObjects, invalid
  arrays, invalid strings, and invalid protocol state sequences deterministically.
- **FR-016**: System MUST include deterministic round-trip tests, boundary tests,
  and byte fixtures for implemented binary encoders and decoders.
- **FR-017**: System MUST include malformed-input tests for decoder, chunk,
  service dispatch, StatusCode, and state-machine behavior.
- **FR-018**: System MUST include fuzz or property tests for parser and decoder
  surfaces where host tooling is available, and document unavailable tooling when
  it is not available.
- **FR-019**: System MUST include validation steps for host tests, static checks,
  formatting, sanitizer build where available, and at least one embedded
  cross-compile.
- **FR-020**: System MUST maintain traceability documentation that maps every
  implemented wire-visible behavior, service response, encoding rule, StatusCode,
  implementation area, and test to exact OPC UA normative sections.
- **FR-021**: System MUST document conformance status as experimental,
  profile-targeting, profile-compliant, or CTT-verified and MUST NOT claim CTT
  verification until actual CTT evidence exists.
- **FR-022**: System MUST document supported role, services, encoding, transport
  profile URI, SecurityPolicy behavior, and tested conformance units in any
  compatibility claim.
- **FR-023**: System MUST keep cryptography pluggable through a narrow contract
  and must document SecurityPolicy None as profile-permitted or non-production
  interoperability behavior until stronger security is implemented.
- **FR-024**: System MUST measure or estimate flash impact, RAM impact, static
  tables, stack use, heap use, transport buffers, and crypto dependency impact for
  the default minimal example.
- **FR-025**: System MUST preserve documented application flash/RAM headroom on
  the selected RP2040-class target for the default minimal example.
- **FR-026**: System MUST record the occamsshavingkit/async-opcua Codespaces
  testing suite as a future compliance-testing input, require planning to
  inventory reusable tests, fixtures, and tooling, and defer adoption or
  adaptation until the minimal connect/discover/browse/read path works.
- **FR-027**: System MUST use the available OPC UA reference MCP server during
  planning and implementation research so every OPC UA behavior, StatusCode,
  profile claim, transport rule, encoding rule, and conformance decision is
  grounded in exact OPC UA normative references.
- **FR-028**: System MUST support one active client, SecureChannel, and Session by
  default for the first version and MUST expand that limit only if the selected
  OPC UA profile requires more concurrent contexts.
- **FR-029**: System MUST defer supported user identity-token types to
  OPC-10000-7 profile research and MUST reject any token type outside the selected
  profile scope with cited OPC UA behavior.
- **FR-030**: System MUST support read-only scalar Variable values for Boolean,
  Int32, UInt32, Float, and bounded String in the first-version static address
  space.
- **FR-031**: System MUST limit first-version bounded String Variable values to a
  maximum encoded length of 64 UTF-8 bytes and reject longer values according to
  documented, tested behavior selected during planning.

### OPC UA Normative Scope *(mandatory for protocol features)*

- **OPC-001**: Target OPC UA role is embedded OPC UA server. Target
  profile/conformance-unit set is profile-targeting, specifically targeting the
  Nano Embedded Device Server Profile family (including NanoEmbeddedDevice and
  NanoEmbeddedDevice2017 profiles), until planning verifies all required conformance
  units from OPC-10000-7, including OPC-10000-7 4.2 for the conformance-unit model.
- **OPC-002**: Implemented first-version service areas are Discovery,
  SecureChannel, Session, View/Browse, and Attribute/Read only to the extent
  required by the selected minimal profile and the tiny static address-space use
  case. Planning must cite exact OPC-10000-4 service sections for each included
  service and operation.
- **OPC-003**: Unsupported services and features return the exact OPC UA
  StatusCode or connection behavior cited during planning. Unsupported-service
  behavior must include an explicit citation to the relevant OPC UA StatusCode
  definition and service-dispatch rule.
- **OPC-004**: Wire encoding and transport requirements include OPC-10000-6 5.2.1
  for OPC UA Binary DataEncoding, OPC-10000-6 6.7.2 for MessageChunk structure,
  OPC-10000-6 7.1.2.3 for Hello Message behavior, and OPC-10000-6 7.2 for OPC UA
  TCP. Planning must add exact sections for Acknowledge, OpenSecureChannel,
  sequence headers, error handling, message body encoding, and any implemented
  built-in type encodings.
- **OPC-005**: SecurityPolicy and conformance status are profile-targeting for the
  first implementation. SecurityPolicy None may be used only if profile research
  permits it or if the server labels it as a non-production interoperability
  phase. The project makes no CTT-verified claim in this feature.
- **OPC-006**: Planning and implementation research must query the available OPC
  UA reference MCP server to ground protocol and conformance decisions in exact
  OPC-10000 sections, as specified in FR-027.
- **OPC-007**: Supported user identity-token types are not predetermined by this
  specification. Planning must derive them from OPC-10000-7 profile research and
  cite exact OPC UA sections for accepted and rejected token behavior.

### Scope Boundaries *(mandatory)*

- **In Scope**: Embedded OPC UA server role; OPC UA TCP over opc.tcp; OPC UA
  Binary encoding; minimal endpoint discovery; required SecureChannel and Session
  flow for the selected profile; browsing a tiny static address space; reading
  simple read-only variable values; host validation; Pico SDK adapter/example;
  Arduino/PlatformIO adapter skeleton; conformance and size documentation.
- **Out of Scope**: OPC UA client functionality; subscriptions; writes; methods;
  historical access; alarms/events; PubSub; XML encoding; JSON encoding; HTTPS;
  WebSockets; dynamic address-space mutation; full companion-spec modeling; CTT
  verification; production security policy support beyond pluggable architecture
  and any minimum required by researched profile scope.
- **Compatibility Claim**: After this feature ships, the project may claim only a
  profile-targeting embedded OPC UA server with documented supported services,
  OPC UA Binary encoding, OPC UA TCP/opc.tcp transport, documented SecurityPolicy
  behavior, and tested conformance units. It may not claim profile-compliance or
  CTT verification without later evidence.
- **Concurrency Scope**: The first-version server supports one active client,
  SecureChannel, and Session by default. Additional concurrent contexts are out of
  scope unless required by the selected OPC UA profile.
- **Identity Scope**: User identity-token support is determined during
  OPC-10000-7 profile research. Token types outside the selected profile scope are
  out of scope for the first version.
- **Application Headroom Goal**: The default minimal embedded example must remain
  within a conservative RP2040-class flash/RAM budget selected during planning and
  must document remaining application headroom.

### Key Entities *(include if feature involves data)*

- **Server Configuration**: User-provided identity, endpoint URL, resource limits,
  transport buffers, platform adapters, and SecurityPolicy/conformance labels used
  to start the server.
- **Platform Adapter**: Contract supplied by the embedding application or example
  target for TCP I/O, timing, entropy, persistence, and optional cryptography.
- **Static Address Space**: Application-defined collection of browseable nodes and
  simple read-only variables exposed by the server.
- **Variable Node**: Read-only value or callback-backed data item with NodeId,
  browse name, data type, access metadata, and current value behavior. First-
  version scalar data types are Boolean, Int32, UInt32, Float, and bounded String;
  bounded String values support up to 64 UTF-8 bytes.
- **Connection Context**: Per-client transport state, negotiated limits, and
  message-chunk processing state; one active instance is supported by default in
  the first version.
- **SecureChannel Context**: Channel state required by the selected profile,
  including token, sequence, security mode, and failure handling metadata; one
  active instance is supported by default in the first version.
- **Session Context**: Client session state required before supported browse and
  read requests are accepted; one active instance is supported by default in the
  first version.
- **Identity Policy**: Profile-derived set of accepted user identity-token types
  and cited rejection behavior for unsupported token types.
- **Service Request**: Decoded client request with service identifier, request
  header, payload, and dispatch result.
- **Status Mapping**: Mapping from unsupported, malformed, or invalid-state cases
  to cited OPC UA StatusCodes or connection behavior.
- **Traceability Record**: Documentation entry linking a requirement,
  implementation area, fixture, test, and OPC UA section.
- **OPC UA Reference MCP Query**: Recorded research lookup against the available
  OPC UA reference MCP server used to identify exact normative OPC-10000 sections
  for a protocol or conformance decision.
- **Size Report**: Record of flash, RAM, stack, buffer, static table, heap, and
  crypto dependency impact for the default example and feature areas.
- **External Compliance Suite**: Reusable tests, fixtures, scripts, and
  Codespaces configuration from occamsshavingkit/async-opcua that may be adopted
  or adapted after micro-opcua's minimal connect/discover/browse/read path works.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: A new maintainer can build the host validation target and run all
  deterministic host tests from documented instructions in under 15 minutes on a
  prepared development machine.
- **SC-002**: The default minimal server example cross-compiles for the selected
  RP2040-class target and reports flash/RAM usage with documented remaining
  application headroom.
- **SC-003**: A standards-compliant OPC UA client can discover the server endpoint,
  complete the required connection/session flow, browse the configured tiny static
  address space, and read at least three configured simple read-only variables.
- **SC-004**: One hundred percent of implemented wire-visible services, messages,
  encodings, StatusCodes, and protocol state transitions have traceability records
  pointing to exact OPC UA normative sections.
- **SC-005**: One hundred percent of specified malformed-input and unsupported
  feature tests return the cited OPC UA StatusCode or cited connection behavior.
- **SC-006**: The binary encoding/decoding suite includes round-trip, boundary,
  and byte-fixture coverage for every implemented built-in type, structure, and
  message required by the first-version server.
- **SC-007**: The validation suite documents execution of host tests, static
  checks, formatting checks, sanitizer build where available, fuzz/property tests
  where available, and at least one embedded cross-compile.
- **SC-008**: Compatibility documentation contains no profile-compliant or
  CTT-verified claim unless matching evidence exists; first-version status is
  clearly labeled profile-targeting unless planning and validation prove a
  stronger status.
- **SC-009**: Compliance-planning documentation identifies whether the
  occamsshavingkit/async-opcua Codespaces testing suite was inventoried, which
  parts are reusable for micro-opcua, which parts are deferred with rationale, and
  what minimal connect/discover/browse/read milestone must pass before adoption.
- **SC-010**: One hundred percent of OPC UA protocol and conformance decisions in
  the plan cite exact normative sections discovered or verified through the
  available OPC UA reference MCP server.
- **SC-011**: Validation includes a test showing the documented behavior when a
  second client attempts to connect while the single active client/channel/session
  slot is already in use.
- **SC-012**: Planning documentation identifies the profile-derived user
  identity-token scope and validation includes rejection coverage for token types
  outside that scope.
- **SC-013**: Validation proves that a client can read at least one Boolean,
  Int32, UInt32, Float, and bounded String value from the configured static
  address space.
- **SC-014**: Validation covers bounded String reads at 0 bytes, 64 UTF-8 bytes,
  and over-limit input behavior.

## Assumptions

- The first implementation is a server-only feature; client functionality is a
  separate future feature.
- The first target class is RP2040/Pico SDK, with Arduino/PlatformIO represented
  by adapter skeletons until a later feature completes board-specific validation.
- The initial profile and conformance-unit target will be selected during
  planning after reviewing OPC-10000-7; until then the project is
  profile-targeting only.
- SecurityPolicy None is acceptable only as profile-permitted behavior or as a
  clearly labeled non-production interoperability phase.
- The first address space is static and tiny by design: enough to prove discovery,
  browsing, and reads without dynamic mutation or companion-spec modeling.
- Exact flash/RAM budgets will be set during planning after toolchain and target
  measurement commands are defined.
- Fuzzing and sanitizer checks are required where host tooling is available; when
  unavailable, the project records the reason and still runs deterministic
  malformed-input tests.
- The occamsshavingkit/async-opcua Codespaces testing suite is a sibling-project
  resource for future compliance testing; the first planning pass must inventory
  it, but adoption or adaptation begins only after micro-opcua's minimal
  connect/discover/browse/read path works.
- The available OPC UA reference MCP server is the required first research path
  for OPC UA specification lookups; final decisions still cite exact OPC-10000
  part and section references in project documentation.
- The first version optimizes for one active client, SecureChannel, and Session to
  preserve embedded memory headroom unless OPC UA profile research requires more.
- User identity-token support is intentionally deferred to OPC-10000-7 profile
  research; no token type is assumed before that research is completed.
- The first-version static address space exposes scalar Boolean, Int32, UInt32,
  Float, and bounded String values; bounded String values support up to 64 UTF-8
  bytes. Additional OPC UA data types are future scope unless profile research
  requires them.
