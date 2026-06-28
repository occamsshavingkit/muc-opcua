# Research: OPC UA PubSub (Part 14) UADP / UDP

## Decision: Transport
- **Decision**: UADP over UDP (Unicast/Multicast).
- **Rationale**: Minimal overhead for embedded sensors. Fits perfectly in the connectionless model. OPC-10000-14 section 7.3.
- **Alternatives**: MQTT or AMQP (requires a broker and heavy TCP/TLS stacks, completely out of scope for Nano).

## Decision: Memory Model
- **Decision**: Static buffers for PubSub DataSetMessages and NetworkMessages.
- **Rationale**: Strict zero-heap compliance. Publisher uses `mu_server_config_t` provided buffers for UDP payload construction.
- **Alternatives**: Dynamic allocation (violates core principles).

## Decision: NetworkMessage mapping (OPC 10000-14, 7.2)
- **Decision**: Support only the mandatory UADP fields for a Publisher. 
- **Rationale**: Reduces code size and complexity for embedded targets.
