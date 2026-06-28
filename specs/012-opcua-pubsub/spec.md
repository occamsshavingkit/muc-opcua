# Feature Specification: OPC UA PubSub (UADP/UDP)

**Feature Branch**: `012-opcua-pubsub`  
**Created**: 2026-06-28  
**Status**: Draft  
**Input**: User description: "OPC UA PubSub (Part 14) - UADP / UDP connectionless Publish-Subscribe"

## User Scenarios & Testing *(mandatory)*

### User Story 1 - UDP Broadcasting (Priority: P1)

A sensor device needs to broadcast its current temperature every 1 second over UDP without accepting incoming TCP connections to preserve memory and power.

**Why this priority**: Connectionless PubSub is the primary use case for IoT sensors using OPC UA.

**Independent Test**: Can be tested by running the server, capturing UDP packets on the network using Wireshark, and decoding them as UADP.

**Acceptance Scenarios**:

1. **Given** the PubSub module is enabled and configured to broadcast temperature on UDP port 4840, **When** the server ticks, **Then** a valid UADP packet containing the temperature variant is sent.

### Edge Cases

- What happens when a packet exceeds the MTU (Maximum Transmission Unit)?
- How does the system handle rapid value changes that exceed the publish interval?
- How does the feature preserve application flash/RAM headroom on the target microcontroller class?

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: System MUST support sending UADP NetworkMessages over UDP (Publisher).
- **FR-002**: System MUST allow configuration of PublisherId, WriterGroupId, and DataSetWriterId.
- **FR-003**: System MUST encode variables using the OPC UA Binary encoding standard (UADP DataSetMessage).
- **FR-004**: System MUST NOT require a TCP connection or Session to publish data.
- **FR-005**: System MUST NOT use dynamic memory allocation (heap) for PubSub state or encoding buffers.

### OPC UA Normative Scope *(mandatory for protocol features)*

- **OPC-001**: Target OPC UA role is PubSub Publisher over UADP/UDP (OPC 10000-14).
- **OPC-002**: Implemented features are UADP NetworkMessage mapping (OPC 10000-14 section 7.2).
- **OPC-003**: Subscriber capabilities (receiving PubSub messages) are out of scope for v1.
- **OPC-004**: Wire encoding requirements cite OPC-10000-14 section 7.2.2.

### Scope Boundaries *(mandatory)*

- **In Scope**: UADP Publisher over UDP (Unicast and Multicast).
- **Out of Scope**: UADP Subscriber, MQTT/JSON mapping, AMQP mapping, Security for UADP.
- **Compatibility Claim**: OPC UA PubSub UADP UDP Publisher.
- **Application Headroom Goal**: Less than 3KB of flash and 500 bytes of RAM.

### Key Entities

- **PubSubConnection**: The UDP transport and addressing info.
- **WriterGroup**: Groups multiple DataSetWriters, provides timing and NetworkMessage settings.
- **DataSetWriter**: Links to a PublishedDataSet and generates DataSetMessages.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: UADP packets are successfully decoded by UaExpert or Wireshark without errors.
- **SC-002**: UDP broadcasting functions correctly on host and embedded (e.g., Raspberry Pi Pico W) platforms.
- **SC-003**: PubSub layer does not allocate memory on the heap and respects `MU_SERVER_STORAGE_BYTES`.
- **SC-004**: RAM footprint of PubSub feature is bounded and documented.

## Assumptions

- No PubSub security (encryption/signing) is required for this initial MVP.
- Multicast support depends on the platform adapter's UDP socket capabilities.
