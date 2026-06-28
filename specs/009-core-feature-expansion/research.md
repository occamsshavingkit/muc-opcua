# Research: Core Feature Expansion

**Feature**: Core Feature Expansion  
**Feature Branch**: `009-core-feature-expansion`

## Decisions and Rationales

### 1. Write Service (OPC-10000-4 §5.10.4)
* **Decision**: Implement a customizable write callback handler (`mu_write_handler_t`) registered in the server configuration.
* **Rationale**: The core library cannot know the location or mutability rules of all caller variables (some might write to hardware registers, some to flash, some to local RAM). A pluggable callback allows the integrator to intercept write requests and validate/apply them securely.
* **Alternatives Considered**: In-place memory modification of static nodes (rejected because some variables represent hardware actuators that require side-effects, which only a handler callback can provide).

### 2. Multiple Concurrent TCP Connections
* **Decision**: Expand the connection manager to store an array of size `MU_MAX_CONNECTIONS` representing active TCP sockets.
* **Rationale**: Multiplexing connections statically avoids heap allocations while allowing up to `MU_MAX_CONNECTIONS` clients to connect concurrently.
* **Alternatives Considered**: Bounded dynamic heap allocation (rejected as it violates the project's zero-heap constitution rule).

### 3. Modern Security Policies (Aes128_Sha256_RsaOaep and Aes256_Sha256_RsaPss)
* **Decision**: Incorporate policy blocks in `src/security/security_policy.c` mapping these standard URIs.
* **Rationale**: Uses standard RSA-OAEP / RSA-PSS algorithms via the caller-supplied platform crypto adapter, without adding library-side crypto code.
* **Alternatives Considered**: Bundling a specific crypto library (rejected to maintain platform independence and small code size).

### 4. X.509 Certificate User Authentication (OPC-10000-4 §7.36.4)
* **Decision**: Implement decoding of `CertificateIdentityToken` (NodeId 327) and signature verification.
* **Rationale**: Standard OPC UA user certificate authentication verifies the client's signature over the server's nonce.
* **Alternatives Considered**: Plaintext certificate verification (rejected as insecure).

### 5. Alarms & Conditions (Events) (OPC-10000-9)
* **Decision**: Add a pre-allocated static circular queue for events in each subscription state structure.
* **Rationale**: Prevents data loss during publish intervals while satisfying the zero-heap constraint.
* **Alternatives Considered**: Dynamic event list (rejected due to heap constraint).
