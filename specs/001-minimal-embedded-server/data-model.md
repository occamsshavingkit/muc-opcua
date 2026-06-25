# Data Model: Minimal Embedded Server

## Model Scope

The v1 data model describes the in-memory contracts for a single embedded OPC UA server instance. It is not a dynamic information-model database. The application owns storage for configuration, buffers, static nodes, references, values, callbacks, and platform adapters.

## Entities

### ServerConfig

Represents immutable or init-time server settings.

**Fields**

- `application_uri`: non-empty String used in discovery responses.
- `product_uri`: non-empty String used in discovery responses.
- `application_name`: LocalizedText-compatible server display name.
- `endpoint_url`: `opc.tcp://` URL advertised by GetEndpoints.
- `transport_profile_uri`: fixed to `http://opcfoundation.org/UA-Profile/Transport/uatcp-uasc-uabinary`.
- `security_policy_uri`: SecurityPolicy None URI for the v1 interoperability phase unless profile research requires another policy.
- `security_mode`: MessageSecurityMode None for the v1 interoperability phase unless profile research requires another mode.
- `user_token_policies`: bounded array selected from profile research, initially anonymous only if permitted.
- `limits`: `ServerLimits` instance.
- `address_space`: `StaticAddressSpace` instance.
- `platform`: `PlatformAdapter` instance.

**Validation**

- `endpoint_url` must use `opc.tcp`.
- All strings must fit configured compile-time or caller-provided maxima.
- SecurityPolicy None must be documented as profile-permitted or non-production.

### ServerLimits

Represents configured compile-time and runtime bounds.

**Fields**

- `max_clients`: default 1.
- `max_secure_channels`: default 1.
- `max_sessions`: default 1.
- `rx_buffer_len`: caller-provided receive buffer length.
- `tx_buffer_len`: caller-provided transmit buffer length.
- `max_string_value_len`: default and v1 maximum 64 encoded UTF-8 bytes.
- `max_nodes`: number of static nodes supplied by the application.
- `max_references`: number of static references supplied by the application.
- `max_browse_results_per_node`: maximum references returned for one Browse target.
- `max_read_nodes_per_request`: bounded operation count for Read.
- `max_browse_nodes_per_request`: bounded operation count for Browse.

**Validation**

- Default RX/TX buffers are 8192 bytes each.
- Any smaller buffer must pass OPC UA TCP Hello/Acknowledge legality checks for the advertised endpoint.
- All operation maxima must be finite and test-covered.

### PlatformAdapter

Narrow integration boundary between the portable core and host or embedded platform code.

**Fields**

- `tcp`: nonblocking or bounded-blocking TCP read/write/close callbacks.
- `time`: monotonic and UTC time callbacks where available.
- `entropy`: callback for nonce/session identifiers where required.
- `persistence`: optional key/value or blob callbacks for future persisted identity/certificates.
- `crypto`: optional crypto provider callbacks.
- `log`: optional diagnostic callback compiled out or stubbed for small builds.

**Validation**

- Core must compile when optional callbacks are null if the selected configuration does not require them.
- TCP callbacks must report short reads/writes without assuming heap-backed buffering.

### StaticAddressSpace

Application-owned static node and reference collection.

**Fields**

- `namespace_uris`: bounded static array; namespace index 0 is the OPC UA namespace.
- `nodes`: array of `StaticNode`.
- `references`: array of `StaticReference`.
- `node_count`: count of valid nodes.
- `reference_count`: count of valid references.

**Validation**

- NodeIds must be unique within the configured namespace/index form.
- References must point to existing nodes or a documented built-in namespace node.
- The default example must include enough standard root/object/server structure for discovery, browsing, and metadata reads needed by tested clients.

### StaticNode

Represents one Object or Variable node.

**Fields**

- `node_id`: `UaNodeId`, limited to numeric and string forms needed by v1.
- `node_class`: Object or Variable for v1 application nodes.
- `browse_name`: QualifiedName-compatible value.
- `display_name`: LocalizedText-compatible value.
- `description`: optional LocalizedText-compatible value.
- `write_mask`: zero for read-only v1 nodes unless profile evidence requires otherwise.
- `user_write_mask`: zero for read-only v1 nodes.
- `type_definition`: NodeId of type definition where required.
- `variable`: optional `VariableAttributes` for Variable nodes.

**Validation**

- Object nodes must have Object-compatible mandatory attributes.
- Variable nodes must have Variable-compatible mandatory attributes.
- Unsupported NodeClass values must be rejected at configuration time.

### VariableAttributes

Represents v1 read-only scalar variable metadata and value source.

**Fields**

- `data_type`: NodeId for Boolean, Int32, UInt32, Float, or String.
- `value_rank`: scalar.
- `access_level`: current-read only.
- `user_access_level`: current-read only or more restrictive.
- `historizing`: false.
- `minimum_sampling_interval`: omitted or fixed to unsupported/no sampling behavior.
- `value_source`: `VariableValueSource`.

**Validation**

- String values must be <= 64 encoded UTF-8 bytes.
- Arrays are not valid v1 variable values.
- Write access bits must not be advertised in v1 default nodes.

### VariableValueSource

Represents either a fixed read-only value or an application callback.

**Fields**

- `kind`: fixed value or callback.
- `type`: Boolean, Int32, UInt32, Float, or bounded String.
- `fixed_value`: static scalar storage when `kind` is fixed.
- `read_callback`: bounded callback when `kind` is callback.
- `user_context`: opaque application pointer passed to callback.

**Validation**

- Callback must return a StatusCode and never transfer ownership of transient buffers to the core.
- Callback String output must fit the configured String limit.

### StaticReference

Represents one reference edge used by Browse.

**Fields**

- `source_node_id`: source NodeId.
- `reference_type_id`: NodeId for the reference type.
- `is_forward`: Boolean.
- `target_node_id`: target NodeId.
- `target_node_class`: NodeClass used in ReferenceDescription.
- `target_browse_name`: QualifiedName used in ReferenceDescription.
- `target_display_name`: LocalizedText used in ReferenceDescription.
- `target_type_definition`: ExpandedNodeId used in ReferenceDescription where applicable.

**Validation**

- Source and target must be available from the static model or recognized namespace 0 built-ins.
- No Browse response may exceed configured TX buffer limits.

### ConnectionContext

Represents one active OPC UA TCP connection.

**Fields**

- `state`: `Disconnected`, `HelloReceived`, `Acknowledged`, `SecureChannelOpen`, `Closing`, or `Error`.
- `remote_endpoint`: platform-defined peer identity if available.
- `rx_buffer`: caller-owned receive buffer view.
- `tx_buffer`: caller-owned transmit buffer view.
- `receive_buffer_size`: negotiated receive size.
- `send_buffer_size`: negotiated send size.
- `max_message_size`: negotiated max message size.
- `max_chunk_count`: negotiated max chunk count.
- `secure_channel`: `SecureChannelContext`.
- `session`: `SessionContext`.

**Validation**

- A second connection while one is active must return OPC UA TCP busy/error behavior.
- No message chunk may be accepted if it exceeds negotiated or configured bounds.

### SecureChannelContext

Represents UASC channel state.

**Fields**

- `state`: `Closed`, `Opening`, `Open`, `Closing`, or `Error`.
- `channel_id`: server-assigned UInt32.
- `security_token_id`: UInt32.
- `security_policy_uri`: String.
- `security_mode`: MessageSecurityMode.
- `client_protocol_version`: UInt32.
- `revised_lifetime_ms`: UInt32.
- `sequence_number_window`: bounded tracking for sequence validation.

**Validation**

- OpenSecureChannel is required before Session services.
- Security header must match the selected SecurityPolicy behavior.
- Message security must be verified before interpreting secured message bodies when security is enabled.

### SessionContext

Represents one OPC UA Session.

**Fields**

- `state`: `None`, `Created`, `Activated`, `Closing`, or `Closed`.
- `session_id`: NodeId or server handle.
- `authentication_token`: NodeId or opaque token used in RequestHeader.
- `client_description`: bounded ApplicationDescription subset.
- `session_name`: bounded String.
- `revised_session_timeout_ms`: UInt32.
- `identity_policy_id`: String selected during ActivateSession.

**Validation**

- Browse and Read require an activated Session unless a cited profile/service rule permits otherwise.
- Authentication token must match the active Session.
- Unsupported identity token types are rejected with cited ActivateSession behavior.

### ServiceRequest

Represents decoded request metadata before service-specific handling.

**Fields**

- `service_id`: NodeId of request type encoding.
- `request_header`: decoded RequestHeader subset.
- `payload_view`: bounded byte span for service-specific decode.
- `requires_session`: Boolean from service dispatch table.
- `normative_section`: traceability reference.

**Validation**

- Unknown service IDs return `Bad_ServiceUnsupported`.
- Request body decoding must fail without reading past payload bounds.

### ServiceResponse

Represents encoded service output metadata.

**Fields**

- `service_id`: NodeId of response type encoding.
- `response_header`: ResponseHeader subset.
- `service_status`: StatusCode.
- `operation_results`: optional bounded array of per-operation StatusCodes.
- `payload_writer`: bounded writer state.

**Validation**

- Response must fit TX buffer or return the cited too-large/error behavior.
- ResponseHeader service result and operation results must follow the service section.

### StatusMapping

Maps implementation conditions to OPC UA result codes or transport errors.

**Fields**

- `condition`: internal enum.
- `status_code`: OPC UA StatusCode or TCP Error symbolic ID.
- `section`: exact OPC UA citation.
- `scope`: transport, service, operation, decoder, or configuration.

**Validation**

- All public wire-visible failures must be represented in the mapping table and covered by tests.

### TraceabilityRecord

Maps implementation files and tests to OPC UA sources.

**Fields**

- `artifact`: source file, header, test, fixture, generated table, or doc.
- `behavior`: implemented behavior or tested edge case.
- `opcua_reference`: exact part and section.
- `conformance_unit`: optional selected profile/conformance-unit entry.
- `status`: planned, implemented, tested, or verified.

**Validation**

- No protocol task may be marked done without a traceability record.

### SizeReport

Captures embedded build size and memory evidence.

**Fields**

- `target`: host, pico, or arduino-skeleton.
- `configuration`: CMake preset/options.
- `flash_bytes`: measured or estimated.
- `static_ram_bytes`: measured or estimated.
- `stack_bytes`: measured, estimated, or bounded by analysis.
- `rx_buffer_bytes`: configured.
- `tx_buffer_bytes`: configured.
- `heap_bytes`: measured or expected zero.
- `headroom_notes`: remaining budget and caveats.

**Validation**

- Default minimal example must report against the initial 128 KiB flash and 32 KiB RAM micro-opcua budget.

### ExternalComplianceSuite

Represents an external compliance or interoperability test source.

**Fields**

- `source`: `occamsshavingkit/async-opcua` or OPC UA CTT/manual client.
- `location`: repository path, devcontainer path, or external tool version.
- `adoption_phase`: inventory, adapted, required, or deferred.
- `coverage`: services/encoding/state covered.
- `gaps`: missing coverage or incompatibilities.

**Validation**

- async-opcua remains inventory/deferred until host minimal connect/discover/browse/read works.

## Relationships

- `ServerConfig` owns references to `ServerLimits`, `StaticAddressSpace`, and `PlatformAdapter`.
- `ConnectionContext` contains one `SecureChannelContext` and one `SessionContext`.
- `ServiceRequest` dispatch consults `SessionContext`, `SecureChannelContext`, `StaticAddressSpace`, and `StatusMapping`.
- `Browse` reads `StaticReference` entries and produces ReferenceDescription values.
- `Read` reads `StaticNode` and `VariableAttributes` entries and produces DataValue values.
- `TraceabilityRecord` links every implementation and test artifact back to OPC UA sections and optional conformance units.

## State Machines

### Connection and Session Flow

```text
Disconnected
  -> HelloReceived
  -> Acknowledged
  -> SecureChannelOpen
  -> SessionCreated
  -> SessionActivated
  -> Closing
  -> Closed
```

Invalid transitions return the cited OPC UA service error or TCP error:

- Service message before Hello/Acknowledge: transport error.
- Session service before SecureChannelOpen: SecureChannel/session StatusCode.
- Browse or Read before SessionActivated: session StatusCode.
- Second connection while one is active: OPC UA TCP busy/error behavior.

### SecureChannel Flow

```text
Closed -> Opening -> Open -> Closing -> Closed
Closed -> Opening -> Error -> Closed
Open -> Error -> Closed
```

OpenSecureChannel creates or renews the active channel only within the single-channel limit. CloseSecureChannel tears down channel and dependent session state according to the cited service behavior.

### Session Flow

```text
None -> Created -> Activated -> Closing -> Closed
None -> Created -> Closing -> Closed
```

CreateSession allocates the fixed Session slot, ActivateSession associates the selected identity token, and CloseSession releases the slot. Unsupported or invalid identity tokens fail without activating the Session.

## Derived Test Obligations

- Every entity validator has deterministic unit tests.
- Every state-machine transition has valid and invalid sequence tests.
- Every decoder entity with a length field has boundary tests and fuzz coverage where available.
- Every StatusMapping entry has one positive or negative service/transport test.
- Every byte fixture cites the OPC UA type, message, and section that defines it.
